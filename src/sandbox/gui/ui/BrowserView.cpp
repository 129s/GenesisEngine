#include "sandbox/gui/ui/BrowserView.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>

namespace Genesis::Sandbox::Gui
{
namespace
{
    void PushActiveButtonStyle(bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Style::DesignTokens::color(Style::ColorToken::Primary));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::DesignTokens::color(Style::ColorToken::PrimaryHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::DesignTokens::color(Style::ColorToken::PrimaryActive));
        }
    }

    void PopActiveButtonStyle(bool active)
    {
        if (active)
        {
            ImGui::PopStyleColor(3);
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

    const RuntimeBridge::WorldAtlas* atlas = ctx.runtime_bridge ? &ctx.runtime_bridge->atlas() : nullptr;
    const RuntimeBridge::Snapshot* snapshot = ctx.latest_snapshot ? &*ctx.latest_snapshot : nullptr;

    struct SectionButton
    {
        BrowserSection section;
        const char* label;
        MainViewTab target;
    };
    const SectionButton sections[] = {
        {BrowserSection::Scene, "Scene", MainViewTab::Scene},
        {BrowserSection::World, "World", MainViewTab::World},
        {BrowserSection::Monitor, "Monitor", MainViewTab::Monitor},
        {BrowserSection::LayoutsThemes, "Layouts & Themes", MainViewTab::Settings},
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Sm),
                               Style::DesignTokens::spacing(Style::SpacingToken::Sm)));
    bool sectionChanged = false;
    const BrowserSection originalSection = ctx.state.browser_active_section;
    for (int i = 0; i < static_cast<int>(std::size(sections)); ++i)
    {
        if (i > 0)
        {
            ImGui::SameLine();
        }

        const bool active = ctx.state.browser_active_section == sections[i].section;
        PushActiveButtonStyle(active);
        if (ImGui::Button(sections[i].label))
        {
            if (ctx.state.browser_active_section != sections[i].section)
            {
                ctx.state.browser_active_section = sections[i].section;
                ctx.state.main_view_active_tab = sections[i].target;
                sectionChanged = true;
            }
        }
        PopActiveButtonStyle(active);
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    if (sectionChanged && ctx.state.browser_active_section != BrowserSection::Scene)
    {
        ctx.state.browser_search_buffer.fill('\0');
    }

    auto trimCopy = [](std::string_view text) -> std::string {
        const auto begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos)
        {
            return std::string{};
        }
        const auto end = text.find_last_not_of(" \t\r\n");
        return std::string{text.substr(begin, end - begin + 1)};
    };

    auto toLowerCopy = [](std::string_view text) -> std::string {
        std::string lowered;
        lowered.reserve(text.size());
        for (char ch : text)
        {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return lowered;
    };

    switch (ctx.state.browser_active_section)
    {
    case BrowserSection::Scene:
    {
        const char* searchHint = "搜索节点或 ID...";
        ImGui::SetNextItemWidth(-ImGui::GetStyle().IndentSpacing);
        if (ImGui::InputTextWithHint("##BrowserSceneSearch", searchHint, ctx.state.browser_search_buffer.data(), ctx.state.browser_search_buffer.size()))
        {
            // 输入框已直接更新搜索缓冲
        }
        if (ctx.state.browser_search_buffer[0] != '\0')
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("清除"))
            {
                ctx.state.browser_search_buffer.fill('\0');
            }
        }
        ImGui::Separator();

        const std::string searchText = trimCopy(std::string_view(ctx.state.browser_search_buffer.data()));
        const std::string searchLower = toLowerCopy(searchText);
        const bool hasSearch = !searchLower.empty();

        auto matchesNode = [&](const RuntimeBridge::WorldAtlas::Node& node) -> bool {
            if (!hasSearch)
            {
                return false;
            }
            if (!node.name.empty() && toLowerCopy(node.name).find(searchLower) != std::string::npos)
            {
                return true;
            }
            const std::string idStr = std::to_string(node.id.value);
            return idStr.find(searchLower) != std::string::npos;
        };

        if (snapshot)
        {
            ImGui::Text("实体：%zu  |  资源：%zu", snapshot->telemetry.agents.size(), snapshot->telemetry.resources.size());
        }
        if (atlas)
        {
            ImGui::Text("节点：%zu  |  边：%zu", atlas->nodes.size(), atlas->edges.size());
        }
        ImGui::Separator();

        if (atlas && ImGui::BeginChild("BrowserSceneTree", ImVec2(0.0f, 0.0f), true))
        {
            struct SceneTreeEntry
            {
                const RuntimeBridge::WorldAtlas::Node* node{nullptr};
                std::vector<SceneTreeEntry> children;
                bool selfMatch{false};
            };

            std::unordered_map<std::uint32_t, std::vector<const RuntimeBridge::WorldAtlas::Node*>> childrenMap;
            std::unordered_set<std::uint32_t> nodeIds;
            childrenMap.reserve(atlas->nodes.size());
            nodeIds.reserve(atlas->nodes.size());
            for (const auto& node : atlas->nodes)
            {
                childrenMap[node.parent.value].push_back(&node);
                nodeIds.insert(node.id.value);
            }

            auto comparator = [](const RuntimeBridge::WorldAtlas::Node* lhs, const RuntimeBridge::WorldAtlas::Node* rhs) {
                if (lhs->name == rhs->name)
                {
                    return lhs->id.value < rhs->id.value;
                }
                return lhs->name < rhs->name;
            };
            for (auto& [_, vec] : childrenMap)
            {
                std::sort(vec.begin(), vec.end(), comparator);
            }

            std::function<std::optional<SceneTreeEntry>(const RuntimeBridge::WorldAtlas::Node*)> buildEntry;
            buildEntry = [&](const RuntimeBridge::WorldAtlas::Node* nodePtr) -> std::optional<SceneTreeEntry> {
                SceneTreeEntry entry;
                entry.node = nodePtr;
                entry.selfMatch = matchesNode(*nodePtr);

                auto childIt = childrenMap.find(nodePtr->id.value);
                if (childIt != childrenMap.end())
                {
                    for (const auto* child : childIt->second)
                    {
                        if (auto childEntry = buildEntry(child))
                        {
                            entry.children.push_back(std::move(*childEntry));
                        }
                    }
                }

                if (hasSearch && !entry.selfMatch && entry.children.empty())
                {
                    return std::nullopt;
                }

                return entry;
            };

            std::vector<const RuntimeBridge::WorldAtlas::Node*> roots;
            roots.reserve(atlas->nodes.size());
            for (const auto& node : atlas->nodes)
            {
                if (node.parent.value == 0 || !nodeIds.contains(node.parent.value))
                {
                    roots.push_back(&node);
                }
            }
            std::sort(roots.begin(), roots.end(), comparator);

            std::vector<SceneTreeEntry> tree;
            tree.reserve(roots.size());
            for (const auto* root : roots)
            {
                if (auto rootEntry = buildEntry(root))
                {
                    tree.push_back(std::move(*rootEntry));
                }
            }

            if (tree.empty())
            {
                ImGui::TextUnformatted(hasSearch ? "未找到匹配的节点。" : "暂无导航数据。");
            }
            else
            {
                const ImVec4 highlightColor = Style::DesignTokens::color(Style::ColorToken::Accent);
                std::function<void(const SceneTreeEntry&)> drawEntry;
                drawEntry = [&](const SceneTreeEntry& entry) {
                    const auto& node = *entry.node;
                    const bool hasChildren = !entry.children.empty();
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                               ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (!hasChildren)
                    {
                        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }
                    if (ctx.state.scene_selected_node == node.id.value)
                    {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    if ((hasChildren && ctx.state.browser_scene_expanded_nodes.contains(node.id.value)) ||
                        (hasSearch && (entry.selfMatch || hasChildren)))
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                    }

                    std::string displayName = node.name.empty() ? ("节点 " + std::to_string(node.id.value))
                                                                : node.name;
                    if (hasChildren)
                    {
                        displayName += " (" + std::to_string(entry.children.size()) + ")";
                    }

                    const bool highlightText = hasSearch && entry.selfMatch;
                    if (highlightText)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, highlightColor);
                    }

                    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node.id.value)),
                                                        flags,
                                                        "%s", displayName.c_str());

                    if (highlightText)
                    {
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::IsItemClicked())
                    {
                        ctx.state.scene_selected_node = node.id.value;
                        ctx.state.map_selected_node = node.id.value;
                        ctx.state.scene_view_mode = SceneViewMode::Node;
                        ctx.state.scene_focus_node_request = node.id.value;
                        ctx.state.main_view_active_tab = MainViewTab::Scene;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("节点 #%u", node.id.value);
                    }

                    if (hasChildren)
                    {
                        if (ImGui::IsItemToggledOpen())
                        {
                            if (open)
                            {
                                ctx.state.browser_scene_expanded_nodes.insert(node.id.value);
                            }
                            else
                            {
                                ctx.state.browser_scene_expanded_nodes.erase(node.id.value);
                            }
                        }
                        else if (hasSearch && open)
                        {
                            ctx.state.browser_scene_expanded_nodes.insert(node.id.value);
                        }
                    }

                    if (open && hasChildren)
                    {
                        for (const auto& child : entry.children)
                        {
                            drawEntry(child);
                        }
                        ImGui::TreePop();
                    }
                };

                for (const auto& entry : tree)
                {
                    drawEntry(entry);
                }
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::TextUnformatted("暂无导航数据。");
        }
        break;
    }
    case BrowserSection::World:
    {
        ImGui::TextUnformatted("世界生成与配置");
        ImGui::Separator();
        ImGui::Text("最近状态：%s", ctx.state.world_command_status.empty() ? "—" : ctx.state.world_command_status.c_str());
        if (!ctx.state.world_load_status.empty())
        {
            ImGui::Text("加载：%s", ctx.state.world_load_status.c_str());
        }
        if (!ctx.state.world_save_status.empty())
        {
            ImGui::Text("保存：%s", ctx.state.world_save_status.c_str());
        }

        if (ImGui::Button("打开世界面板"))
        {
            ctx.state.main_view_active_tab = MainViewTab::World;
            ctx.state.browser_active_section = BrowserSection::World;
        }

        if (ctx.runtime_bridge)
        {
            if (auto lastGen = ctx.runtime_bridge->lastGeneration(); lastGen)
            {
                ImGui::Separator();
                ImGui::Text("最近生成：%s", lastGen->success ? "Success" : "Failed");
                ImGui::Text("Seed：%llu", static_cast<unsigned long long>(lastGen->seed.value));
                ImGui::Text("节点：%zu  |  边：%zu", lastGen->locationCount, lastGen->edgeCount);
                if (!lastGen->error.empty())
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Danger), "%s", lastGen->error.c_str());
                }
            }
        }
        break;
    }
    case BrowserSection::Monitor:
    {
        ImGui::TextUnformatted("运行状态与告警总览");
        ImGui::Separator();
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("UI FPS %.1f", io.Framerate);

        if (snapshot)
        {
            const auto& tick = snapshot->telemetry;
            ImGui::Text("命令队列：%zu", tick.actions.size());
            ImGui::Text("事件：%zu", snapshot->events.size());
        }
        else
        {
            ImGui::TextUnformatted("等待 Runtime 数据…");
        }

        if (ImGui::Button("跳转 Monitor 面板"))
        {
            ctx.state.main_view_active_tab = MainViewTab::Monitor;
            ctx.state.browser_active_section = BrowserSection::Monitor;
        }
        break;
    }
    case BrowserSection::LayoutsThemes:
    {
        ImGui::TextUnformatted("布局与主题");
        ImGui::Separator();
        ImGui::TextWrapped(
            "后续任务将提供布局/主题的导入导出与预设管理。当前可通过 Main View > Settings 预览设计令牌。");
        if (ImGui::Button("打开 Settings 面板"))
        {
            ctx.state.main_view_active_tab = MainViewTab::Settings;
            ctx.state.browser_active_section = BrowserSection::LayoutsThemes;
        }
        break;
    }
    }

    ImGui::End();
}

} // namespace Genesis::Sandbox::Gui
