#include "sandbox/gui/ui/BrowserView.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"
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

namespace Genesis::Sandbox::Gui
{
namespace
{
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
        const bool isRoot = (key == ".");
        const bool hasChildren = !node.children.empty();
        const bool isSelected =
            (!ctx.state.browser_selected_path.empty() && ctx.state.browser_selected_path == key) ||
            (ctx.state.browser_selected_path.empty() && isRoot);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (!node.isDirectory)
        {
            flags |= ImGuiTreeNodeFlags_Bullet;
        }
        if (isSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (ctx.state.browser_expanded_paths.contains(key))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }
        else if (hasFilter && node.selfMatches)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }

        std::string label;
        if (isRoot)
        {
            label = root.filename().generic_string();
            if (label.empty())
            {
                label = root.generic_string();
            }
        }
        else
        {
            label = node.path.filename().generic_string();
            if (label.empty())
            {
                label = node.path.generic_string();
            }
        }

        const bool highlightText = hasFilter && node.selfMatches;
        std::string nodeId = key + "##browser_tree";
        if (highlightText)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, highlightColor);
        }
        const bool open = ImGui::TreeNodeEx(nodeId.c_str(), flags, "%s", label.c_str());
        if (highlightText)
        {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemClicked())
        {
            ctx.state.browser_selected_path = key;
        }

        if (hasChildren)
        {
            if (ImGui::IsItemToggledOpen())
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

            if (open)
            {
                for (const auto& child : node.children)
                {
                    drawNode(child, ctx, root, hasFilter, highlightColor);
                }
                ImGui::TreePop();
            }
        }
    }
} // namespace

void BrowserView::render(UiContext& ctx)
{
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

    ImGui::TextDisabled("根目录：%s", dataRoot.generic_string().c_str());

    const ImGuiStyle& style = ImGui::GetStyle();
    float available = ImGui::GetContentRegionAvail().x;
    bool hasFilter = ctx.state.browser_filter_buffer[0] != '\0';
    if (hasFilter)
    {
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
    }

    ImGui::Separator();

    const std::string filterLower = toLowerCopy(ctx.state.browser_filter_buffer.data());
    std::vector<std::string> warnings;
    auto rootNode = buildNode(dataRoot, dataRoot, filterLower, hasFilter, warnings);
    if (!rootNode)
    {
        ImGui::TextUnformatted("data 目录为空。");
        ImGui::End();
        return;
    }

    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float detailReserve = ImGui::GetTextLineHeightWithSpacing() * 7.0f;
    const float treeHeight = std::max(availableHeight - detailReserve, ImGui::GetTextLineHeightWithSpacing() * 8.0f);
    if (ImGui::BeginChild("BrowserTree", ImVec2(0.0f, treeHeight), true))
    {
        drawNode(*rootNode, ctx, dataRoot, hasFilter, Style::DesignTokens::color(Style::ColorToken::Accent));
    }
    ImGui::EndChild();

    if (!warnings.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "访问警告：");
        for (const auto& warning : warnings)
        {
            ImGui::BulletText("%s", warning.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("详情");
    ImGui::Separator();

    const std::string selectionKey = ctx.state.browser_selected_path.empty() ? "." : ctx.state.browser_selected_path;
    const std::filesystem::path selectedPath =
        (selectionKey == ".") ? dataRoot : (dataRoot / std::filesystem::path(selectionKey));

    std::error_code existsEc;
    if (!std::filesystem::exists(selectedPath, existsEc) || existsEc)
    {
        ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "所选条目不存在或无法访问。");
    }
    else
    {
        const bool isDir = std::filesystem::is_directory(selectedPath, existsEc);
        ImGui::Text("相对路径：%s", selectionKey.c_str());
        ImGui::Text("绝对路径：%s", selectedPath.generic_string().c_str());
        ImGui::Text("类型：%s", isDir ? "文件夹" : "文件");

        if (!isDir)
        {
            std::error_code sizeEc;
            const auto fileSize = std::filesystem::file_size(selectedPath, sizeEc);
            if (!sizeEc)
            {
                ImGui::Text("大小：%s", humanReadableSize(fileSize).c_str());
            }
        }

        std::error_code timeEc;
        const auto lastWrite = std::filesystem::last_write_time(selectedPath, timeEc);
        if (!timeEc)
        {
            ImGui::Text("最后修改：%s", formatTimestamp(lastWrite).c_str());
        }
    }

    ImGui::End();
}
} // namespace Genesis::Sandbox::Gui
