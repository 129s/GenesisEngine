#include "sandbox/gui/ui/BrowserView.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

#include <algorithm>
#include <numeric>
#include <string>
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
            ctx.state.browser_active_section = sections[i].section;
            ctx.state.main_view_active_tab = sections[i].target;
        }
        PopActiveButtonStyle(active);
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    switch (ctx.state.browser_active_section)
    {
    case BrowserSection::Scene:
    {
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
            std::vector<std::size_t> nodeIndices(atlas->nodes.size());
            std::iota(nodeIndices.begin(), nodeIndices.end(), 0);
            std::sort(nodeIndices.begin(), nodeIndices.end(), [&](std::size_t lhs, std::size_t rhs) {
                const auto& a = atlas->nodes[lhs];
                const auto& b = atlas->nodes[rhs];
                if (a.name == b.name)
                {
                    return a.id.value < b.id.value;
                }
                return a.name < b.name;
            });

            for (std::size_t idx : nodeIndices)
            {
                const auto& node = atlas->nodes[idx];
                const bool selected = ctx.state.scene_selected_node == node.id.value;
                std::string label = node.name.empty() ? ("Node " + std::to_string(node.id.value))
                                                      : node.name;
                label += "##SceneBrowserNode";
                label += std::to_string(node.id.value);
                if (ImGui::Selectable(label.c_str(), selected))
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
