#include "sandbox/gui/ui/BrowserView.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/LayoutHelpers.hpp"
#include "../FilesystemHelpers.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <ctime>

#include <imgui.h>
#include <imgui_internal.h>

namespace Genesis::Sandbox::Gui
{
namespace
{
    ImVec4 lerpColor(const ImVec4& from, const ImVec4& to, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return ImVec4(from.x + (to.x - from.x) * t,
                      from.y + (to.y - from.y) * t,
                      from.z + (to.z - from.z) * t,
                      from.w + (to.w - from.w) * t);
    }

    std::string toLowerCopy(std::string_view text)
    {
        std::string lowered;
        lowered.reserve(text.size());
        for (char ch : text)
        {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return lowered;
    }

    std::string relativeKey(const std::filesystem::path& absolute, const std::filesystem::path& root)
    {
        std::error_code ec;
        auto rel = std::filesystem::relative(absolute, root, ec);
        if (ec || rel.empty())
        {
            return ".";
        }
        auto canonical = rel.lexically_normal().generic_string();
        if (canonical.empty() || canonical == ".")
        {
            return ".";
        }
        return canonical;
    }

    std::string humanReadableSize(std::uintmax_t bytes)
    {
        static const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
        double value = static_cast<double>(bytes);
        std::size_t index = 0;
        const std::size_t maxIndex = sizeof(suffixes) / sizeof(suffixes[0]);
        while (value >= 1024.0 && (index + 1) < maxIndex)
        {
            value /= 1024.0;
            ++index;
        }

        std::ostringstream oss;
        if (index == 0)
        {
            oss << static_cast<std::uintmax_t>(value) << ' ' << suffixes[index];
        }
        else
        {
            oss << std::fixed << std::setprecision(value >= 10.0 ? 1 : 2) << value << ' ' << suffixes[index];
        }
        return oss.str();
    }

    std::string formatTimestamp(const std::filesystem::file_time_type& tp)
    {
        const auto fileNow = std::filesystem::file_time_type::clock::now();
        const auto systemNow = std::chrono::system_clock::now();
        const auto systemTime =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp - fileNow + systemNow);

        const std::time_t cTime = std::chrono::system_clock::to_time_t(systemTime);
        std::tm localTm{};
#if defined(_WIN32)
        localtime_s(&localTm, &cTime);
#else
        localtime_r(&cTime, &localTm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    struct LeafColorSet
    {
        ImVec4 normal;
        ImVec4 hover;
        ImVec4 active;
    };

    LeafColorSet colorForLeaf(const std::filesystem::path& path)
    {
        (void)path;
        const ImVec4 surface = Style::DesignTokens::color(Style::ColorToken::Surface);
        const ImVec4 success = Style::DesignTokens::color(Style::ColorToken::Success);
        const ImVec4 successBright = lerpColor(success, ImVec4(1.0f, 1.0f, 1.0f, success.w), 0.35f);
        const ImVec4 successGlow = lerpColor(success, ImVec4(1.0f, 1.0f, 1.0f, success.w), 0.55f);

        return {
            lerpColor(surface, success, 0.28f),
            lerpColor(surface, successBright, 0.48f),
            lerpColor(surface, successGlow, 0.68f),
        };
    }

    struct BrowserNode
    {
        std::filesystem::path path;
        bool isDirectory{false};
        bool selfMatches{false};
        std::vector<BrowserNode> children;
    };

    std::optional<BrowserNode> buildNode(const std::filesystem::path& current,
                                         const std::filesystem::path& root,
                                         const std::string& filterLower,
                                         bool hasFilter,
                                         std::vector<std::string>& warnings)
    {
        std::error_code ec;
        const bool isDir = std::filesystem::is_directory(current, ec);
        if (ec)
        {
            warnings.emplace_back("无法访问：" + current.generic_string());
            return std::nullopt;
        }

        BrowserNode node;
        node.path = current;
        node.isDirectory = isDir;

        const std::string keyLower = toLowerCopy(relativeKey(current, root));
        const std::string nameLower = toLowerCopy(current.filename().generic_string());
        node.selfMatches = hasFilter && (!filterLower.empty()) &&
                           ((keyLower.find(filterLower) != std::string::npos) ||
                            (nameLower.find(filterLower) != std::string::npos));

        if (node.isDirectory)
        {
            std::vector<std::filesystem::directory_entry> entries;
            std::filesystem::directory_iterator dirIt{current, ec};
            if (ec)
            {
                warnings.emplace_back("无法列出目录：" + current.generic_string());
            }
            else
            {
                const std::filesystem::directory_iterator end;
                for (auto it = dirIt; it != end; ++it)
                {
                    entries.push_back(*it);
                }
            }

            std::sort(entries.begin(), entries.end(),
                      [](const std::filesystem::directory_entry& lhs, const std::filesystem::directory_entry& rhs) {
                          const bool lhsDir = lhs.is_directory();
                          const bool rhsDir = rhs.is_directory();
                          if (lhsDir != rhsDir)
                          {
                              return lhsDir > rhsDir;
                          }
                          return lhs.path().filename() < rhs.path().filename();
                      });

            for (const auto& entry : entries)
            {
                std::error_code childEc;
                const bool childDir = entry.is_directory(childEc);
                if (childEc)
                {
                    warnings.emplace_back("无法访问：" + entry.path().generic_string());
                    continue;
                }
                const bool childFile = entry.is_regular_file(childEc);
                if (childEc)
                {
                    warnings.emplace_back("无法访问：" + entry.path().generic_string());
                    continue;
                }
                if (!childDir && !childFile)
                {
                    continue;
                }

                if (auto child = buildNode(entry.path(), root, filterLower, hasFilter, warnings))
                {
                    node.children.push_back(std::move(*child));
                }
            }
        }

        if (hasFilter && !node.selfMatches && node.children.empty())
        {
            return std::nullopt;
        }

        return node;
    }

    void drawNode(const BrowserNode& node,
                  UiContext& ctx,
                  const std::filesystem::path& root,
                  bool hasFilter,
                  const ImVec4& highlightColor)
    {
        const std::string key = relativeKey(node.path, root);
        const bool hasChildren = !node.children.empty();
        const bool isLeaf = !hasChildren;
        const bool isSelected =
            (!ctx.state.browser_selected_path.empty() && ctx.state.browser_selected_path == key);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding |
                                   ImGuiTreeNodeFlags_OpenOnArrow;
        if (isLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (isSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (ctx.state.browser_expanded_paths.contains(key))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        else if (hasFilter && node.selfMatches)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }

        std::string label;
        if (node.path.has_filename())
        {
            label = node.path.filename().generic_string();
        }
        if (label.empty())
        {
            label = key;
        }

        const bool highlightText = hasFilter && node.selfMatches;
        std::string nodeId = key + "##browser_tree";
        LeafColorSet leafColors{};
        if (isLeaf)
        {
            leafColors = colorForLeaf(node.path);
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            const ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
            const float rowHeight = ImGui::GetFrameHeight();
            const ImVec2 rowMin(window->WorkRect.Min.x, cursorScreen.y);
            const ImVec2 rowMax(window->WorkRect.Max.x, cursorScreen.y + rowHeight);
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(leafColors.normal));
            ImGui::PushStyleColor(ImGuiCol_Header, leafColors.active);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, leafColors.hover);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, leafColors.active);
        }
        if (highlightText)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, highlightColor);
        }

        const bool open = ImGui::TreeNodeEx(nodeId.c_str(), flags, "%s", label.c_str());
        Ui::applyClickableCursorToLastItem();
        bool renderChildren = open && !isLeaf;
        const ImGuiID itemId = ImGui::GetItemID();

        if (isLeaf)
        {
            ImGui::PopStyleColor(3);
        }

        if (highlightText)
        {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ctx.state.browser_selected_path = key;
            if (!isLeaf)
            {
                const bool targetOpen = !open;
                ImGui::TreeNodeSetOpen(itemId, targetOpen);
                if (targetOpen && !open)
                {
                    ImGui::TreePush(nodeId.c_str());
                    renderChildren = true;
                }
                else if (!targetOpen && open)
                {
                    ImGui::TreePop();
                    renderChildren = false;
                }

                if (targetOpen)
                {
                    ctx.state.browser_expanded_paths.insert(key);
                }
                else
                {
                    ctx.state.browser_expanded_paths.erase(key);
                }
            }
        }
        else if (!isLeaf && ImGui::IsItemToggledOpen())
        {
            if (open)
            {
                ctx.state.browser_expanded_paths.insert(key);
            }
            else
            {
                ctx.state.browser_expanded_paths.erase(key);
            }
        }

        if (!isLeaf && renderChildren)
        {
            for (const auto& child : node.children)
            {
                drawNode(child, ctx, root, hasFilter, highlightColor);
            }
            ImGui::TreePop();
        }
    }

    void drawDetailCard(UiContext& ctx,
                        const std::filesystem::path& dataRoot,
                        const std::vector<std::string>& warnings,
                        float detailHeight)
    {
        Style::Layout::CardLayoutConfig layout = Style::Layout::detailCard();
        layout.padding.y = Style::DesignTokens::spacing(Style::SpacingToken::Xs);
        layout.headerGap = Style::DesignTokens::spacing(Style::SpacingToken::None);
        Style::Layout::CardScope card("BrowserDetailCard",
                                      layout,
                                      ImGuiWindowFlags_NoScrollbar,
                                      ImVec2(0.0f, detailHeight));
        if (!card.isOpen())
        {
            return;
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);

        const float headerSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Sm);
        ImGui::Dummy(ImVec2(0.0f, headerSpacing));
        ImGui::TextUnformatted("详情");
        ImGui::Dummy(ImVec2(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Xs)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, headerSpacing));

        if (!warnings.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Style::DesignTokens::color(Style::ColorToken::Warning));
            ImGui::TextUnformatted("访问警告");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, layout.lineGap));
            ImGui::Indent(layout.indent);
            for (std::size_t i = 0; i < warnings.size(); ++i)
            {
                ImGui::TextWrapped("%s", warnings[i].c_str());
                if (i + 1 < warnings.size())
                {
                    ImGui::Dummy(ImVec2(0.0f, layout.lineGap));
                }
            }
            ImGui::Unindent(layout.indent);

            ImGui::Dummy(ImVec2(0.0f, layout.headerGap));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, layout.headerGap));
        }

        const std::string selectionKey =
            ctx.state.browser_selected_path.empty() ? "." : ctx.state.browser_selected_path;
        const std::filesystem::path selectedPath =
            (selectionKey == ".") ? dataRoot : (dataRoot / std::filesystem::path(selectionKey));

        std::error_code existsEc;
        if (!std::filesystem::exists(selectedPath, existsEc) || existsEc)
        {
            ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning),
                               "所选条目不存在或无法访问。");
        }
        else
        {
            const bool isDir = std::filesystem::is_directory(selectedPath, existsEc);

            ImGui::Text("相对路径：%s", selectionKey.c_str());
            ImGui::Dummy(ImVec2(0.0f, layout.lineGap));

            ImGui::TextWrapped("绝对路径：%s", selectedPath.generic_string().c_str());
            ImGui::Dummy(ImVec2(0.0f, layout.lineGap));

            ImGui::Text("类型：%s", isDir ? "文件夹" : "文件");

            if (!isDir)
            {
                std::error_code sizeEc;
                const auto fileSize = std::filesystem::file_size(selectedPath, sizeEc);
                if (!sizeEc)
                {
                    ImGui::Dummy(ImVec2(0.0f, layout.lineGap));
                    ImGui::Text("大小：%s", humanReadableSize(fileSize).c_str());
                }
            }

            std::error_code timeEc;
            const auto lastWrite = std::filesystem::last_write_time(selectedPath, timeEc);
            if (!timeEc)
            {
                ImGui::Dummy(ImVec2(0.0f, layout.lineGap));
                ImGui::Text("最后修改：%s", formatTimestamp(lastWrite).c_str());
            }
        }

        ImGui::PopTextWrapPos();
    }
} // namespace

void BrowserView::render(UiContext& ctx)
{
    const Style::Layout::WindowLayoutConfig windowLayout = Style::Layout::mainViewWindow();
    Style::Layout::WindowStyleScope windowScope(windowLayout);
    if (!ImGui::Begin("Browser"))
    {
        ImGui::End();
        return;
    }
    Ui::drawDockAnchorOverlay("BrowserDockAnchor", ctx.state.layout_mode_enabled);

    std::filesystem::path dataRoot = locateAsset(std::filesystem::path("data"));
    if (dataRoot.empty())
    {
        ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Danger), "未找到 data 目录。");
        ImGui::End();
        return;
    }

    std::error_code canonicalEc;
    auto canonicalRoot = std::filesystem::weakly_canonical(dataRoot, canonicalEc);
    if (!canonicalEc)
    {
        dataRoot = canonicalRoot;
    }

    if (ctx.state.browser_selected_path.empty())
    {
        ctx.state.browser_selected_path = ".";
    }
    if (!ctx.state.browser_expanded_paths.contains("."))
    {
        ctx.state.browser_expanded_paths.insert(".");
    }

    float available = ImGui::GetContentRegionAvail().x;
    bool hasFilter = ctx.state.browser_filter_buffer[0] != '\0';
    if (hasFilter)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::CalcTextSize("清除").x + style.FramePadding.x * 2.0f;
        available = std::max(available - buttonWidth - style.ItemSpacing.x, 120.0f);
    }
    ImGui::SetNextItemWidth(available);
    if (ImGui::InputTextWithHint("##BrowserFilter",
                                 "搜索文件或文件夹...",
                                 ctx.state.browser_filter_buffer.data(),
                                 ctx.state.browser_filter_buffer.size()))
    {
        // 输入框直接更新缓冲区
    }

    hasFilter = ctx.state.browser_filter_buffer[0] != '\0';
    if (hasFilter)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("清除"))
        {
            ctx.state.browser_filter_buffer.fill('\0');
            hasFilter = false;
        }
        Ui::applyClickableCursorToLastItem();
    }

    const std::string filterLower = toLowerCopy(ctx.state.browser_filter_buffer.data());
    std::vector<std::string> warnings;
    auto rootNode = buildNode(dataRoot, dataRoot, filterLower, hasFilter, warnings);
    if (!rootNode)
    {
        ImGui::TextUnformatted("data 目录为空。");
        ImGui::End();
        return;
    }

    const ImVec4 highlightColor = Style::DesignTokens::color(Style::ColorToken::Accent);
    const ImVec4 accentColor = Style::DesignTokens::color(Style::ColorToken::Accent);
    const ImVec4 accentActiveColor = Style::DesignTokens::color(Style::ColorToken::AccentActive);
    const ImVec4 borderColor = Style::DesignTokens::color(Style::ColorToken::BorderSoft);

    const ImVec2 initialCursor = ImGui::GetCursorPos();
    const ImVec2 windowContentMin = ImGui::GetWindowContentRegionMin();
    const ImVec2 windowContentMax = ImGui::GetWindowContentRegionMax();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const float contentWidth = std::max(0.0f, windowContentMax.x - windowContentMin.x);
    const float contentHeight = std::max(0.0f, windowContentMax.y - windowContentMin.y);

    const float splitterThickness = 2.0f;
    const float splitterHoverExtend = Style::DesignTokens::spacing(Style::SpacingToken::Sm);
    float minDetailHeight =
        std::max(ImGui::GetTextLineHeightWithSpacing() * 12.0f, Style::DesignTokens::spacing(Style::SpacingToken::Xl));
    float minTreeHeight = ImGui::GetTextLineHeightWithSpacing() * 6.0f;
    float detailHeight =
        ctx.state.browser_detail_height > 0.0f ? ctx.state.browser_detail_height
                                               : std::max(minDetailHeight, contentHeight * 0.40f);
    float treeHeight = 0.0f;
    bool splitterHovered = false;
    bool splitterActive = false;

    if (contentHeight <= 0.0f)
    {
        detailHeight = 0.0f;
        treeHeight = 0.0f;
        ctx.state.browser_detail_height = detailHeight;
    }
    else
    {
        float minDetailForClamp = minDetailHeight;
        float minTreeForClamp = minTreeHeight;
        const float minCombined = minDetailHeight + minTreeHeight;

        if (contentHeight < minCombined)
        {
            const float scale = contentHeight / std::max(minCombined, 1.0f);
            minDetailForClamp *= scale;
            minTreeForClamp *= scale;
            detailHeight = minDetailForClamp;
            treeHeight = minTreeForClamp;
        }
        else
        {
            detailHeight = std::clamp(detailHeight, minDetailHeight, contentHeight - minTreeHeight);
            treeHeight = contentHeight - detailHeight;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImVec2 cursorBackup = window->DC.CursorPos;
        const ImVec2 cursorMaxBackup = window->DC.CursorMaxPos;
        const float splitterYLocal = windowContentMin.y + treeHeight;

        window->DC.CursorPos = ImVec2(windowContentMin.x, splitterYLocal - splitterThickness * 0.5f);
        const ImRect splitterBb(ImVec2(windowPos.x + windowContentMin.x,
                                       windowPos.y + splitterYLocal - splitterThickness * 0.5f),
                                ImVec2(windowPos.x + windowContentMax.x,
                                       windowPos.y + splitterYLocal + splitterThickness * 0.5f));
        const ImGuiID splitterId = window->GetID("BrowserDetailSplitter");
        ImGui::KeepAliveID(splitterId);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::SplitterBehavior(splitterBb,
                                splitterId,
                                ImGuiAxis_Y,
                                &treeHeight,
                                &detailHeight,
                                minTreeForClamp,
                                minDetailForClamp,
                                splitterHoverExtend,
                                0.0f,
                                splitterThickness);
        ImGui::PopStyleVar();
        splitterHovered = ImGui::IsItemHovered();
        splitterActive = ImGui::IsItemActive();

        window->DC.CursorPos = cursorBackup;
        window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, cursorMaxBackup);

        treeHeight = std::clamp(treeHeight, 0.0f, contentHeight);
        detailHeight = std::clamp(detailHeight, 0.0f, contentHeight);
        if (treeHeight + detailHeight > 0.0f)
        {
            // 保持和可用高度一致，避免累计误差
            const float sum = treeHeight + detailHeight;
            treeHeight = treeHeight * contentHeight / sum;
            detailHeight = contentHeight - treeHeight;
        }
        ctx.state.browser_detail_height = detailHeight;

        const float splitterY = windowPos.y + windowContentMin.y + treeHeight;
        const ImVec2 splitterLineMin(windowPos.x + windowContentMin.x, splitterY - splitterThickness * 0.5f);
        const ImVec2 splitterLineMax(windowPos.x + windowContentMax.x, splitterY + splitterThickness * 0.5f);
        const ImU32 splitterColor =
            ImGui::GetColorU32(splitterActive ? accentActiveColor : (splitterHovered ? accentColor : borderColor));
        ImGui::GetForegroundDrawList()->AddRectFilled(splitterLineMin, splitterLineMax, splitterColor);
    }

    ImGui::SetCursorPos(initialCursor);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Sm), 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Xs),
                               Style::DesignTokens::spacing(Style::SpacingToken::Xs)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Xs), 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, Style::DesignTokens::spacing(Style::SpacingToken::Lg));

    if (ImGui::BeginChild("BrowserTreePane", ImVec2(0.0f, treeHeight), true))
    {
        if (rootNode->children.empty())
        {
            ImGui::TextUnformatted("data 目录为空。");
        }
        else
        {
            for (const auto& child : rootNode->children)
            {
                drawNode(child, ctx, dataRoot, hasFilter, highlightColor);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(4);

    if (detailHeight > 0.0f)
    {
        const float detailCursorY = windowContentMin.y + treeHeight;
        ImGui::SetCursorPos(ImVec2(windowContentMin.x, detailCursorY));
        drawDetailCard(ctx, dataRoot, warnings, detailHeight);
    }

    ImGui::End();
}
} // namespace Genesis::Sandbox::Gui
