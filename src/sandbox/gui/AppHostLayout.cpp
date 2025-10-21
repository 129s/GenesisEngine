#include "sandbox/gui/AppHost.hpp"

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace Genesis::Sandbox::Gui
{

namespace
{
    void PushActiveButtonStyle(bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.80f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.88f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
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

void AppHost::drawDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

    ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id);
    if (!dock_layout_initialized_ && (rootNode == nullptr || (!rootNode->IsSplitNode() && rootNode->Windows.Size == 0)))
    {
        dock_layout_initialized_ = true;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.23f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.28f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Browser", dock_left);
        ImGui::DockBuilderDockWindow("Main View", dock_main);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }
}

void AppHost::drawStatusBar()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.FramePadding.y + 4.0f;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
    if (ImGui::BeginViewportSideBar("Status Bar", viewport, ImGuiDir_Up, height, flags))
    {
        ImGui::AlignTextToFramePadding();

        if (runtime_bridge_)
        {
            const bool paused = runtime_bridge_->paused();
            ImGui::TextUnformatted(paused ? "状态：Paused" : "状态：Running");
        }
        else
        {
            ImGui::TextUnformatted("状态：Runtime offline");
        }

        ImGui::SameLine(0.0f, 18.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 18.0f);

        ImGui::Text("Speed %.2fx", static_cast<float>(speed_multiplier_ui_));

        auto drawNavButton = [&](const char* label, MainViewTab tab, BrowserSection section) {
            ImGui::SameLine(0.0f, 12.0f);
            const bool active = (ui_state_.main_view_active_tab == tab);
            PushActiveButtonStyle(active);
            if (ImGui::Button(label))
            {
                ui_state_.main_view_active_tab = tab;
                ui_state_.browser_active_section = section;
            }
            PopActiveButtonStyle(active);
        };

        drawNavButton("Monitor", MainViewTab::Monitor, BrowserSection::Monitor);
        drawNavButton("Scene", MainViewTab::Scene, BrowserSection::Scene);
        drawNavButton("World", MainViewTab::World, BrowserSection::World);
        drawNavButton("Settings", MainViewTab::Settings, BrowserSection::LayoutsThemes);

        ImGui::SameLine(0.0f, 18.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 18.0f);

        if (latest_snapshot_)
        {
            const auto& tick = latest_snapshot_->telemetry;
            ImGui::Text("Step %llu", static_cast<unsigned long long>(tick.step));
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Agents %zu", tick.agents.size());
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Resources %zu", tick.resources.size());
            ImGui::SameLine(0.0f, 12.0f);
            const std::size_t alertCount = latest_snapshot_->events.size();
            if (alertCount > 0)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "Alerts %zu", alertCount);
            }
            else
            {
                ImGui::TextDisabled("Alerts 0");
            }
        }
        else
        {
            ImGui::TextUnformatted("等待首帧快照…");
        }

        ImGui::SameLine(0.0f, 18.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextUnformatted("Help ▸ F1");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void AppHost::drawControlBar()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.FramePadding.y * 2.2f;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, style.FramePadding.y));
    const bool open = ImGui::BeginViewportSideBar("Control Bar", viewport, ImGuiDir_Down, height, flags);
    if (open)
    {
        if (runtime_bridge_)
        {
            bool paused = runtime_bridge_->paused();
            if (ImGui::Button(paused ? "继续" : "暂停"))
            {
                runtime_bridge_->setPaused(!paused);
                pushToast(paused ? "Resume" : "Pause", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            }

            ImGui::SameLine(0.0f, 10.0f);
            if (ImGui::Button("单步"))
            {
                runtime_bridge_->requestStep(1);
                pushToast("Step x1", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
            }

            ImGui::SameLine();
            if (ImGui::Button("快进×10"))
            {
                runtime_bridge_->requestStep(10);
                pushToast("Step x10", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
            }

            ImGui::SameLine(0.0f, 18.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0.0f, 18.0f);

            float speed = static_cast<float>(speed_multiplier_ui_);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("速度倍率", &speed, 0.25f, 8.0f, "%.2fx"))
            {
                speed_multiplier_ui_ = speed;
                runtime_bridge_->setSpeedMultiplier(speed_multiplier_ui_);
            }

            ImGui::SameLine(0.0f, 18.0f);
            if (ImGui::Checkbox("VSync", &config_.vsync))
            {
                glfwSwapInterval(config_.vsync ? 1 : 0);
            }

            ImGui::SameLine(0.0f, 18.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0.0f, 18.0f);

            if (latest_snapshot_)
            {
                const auto& tick = latest_snapshot_->telemetry;
                ImGui::Text("Step %llu  |  FPS %.1f",
                            static_cast<unsigned long long>(tick.step),
                            ImGui::GetIO().Framerate);
            }
            else
            {
                ImGui::TextUnformatted("等待快照同步…");
            }
        }
        else
        {
            ImGui::TextUnformatted("RuntimeBridge unavailable.");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void AppHost::drawBrowserPanel()
{
    if (!ImGui::Begin("Browser"))
    {
        ImGui::End();
        return;
    }

    const RuntimeBridge::WorldAtlas* atlas = runtime_bridge_ ? &runtime_bridge_->atlas() : nullptr;
    const RuntimeBridge::Snapshot* snapshot = latest_snapshot_ ? &*latest_snapshot_ : nullptr;

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

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
    for (int i = 0; i < static_cast<int>(std::size(sections)); ++i)
    {
        if (i > 0)
        {
            ImGui::SameLine();
        }

        const bool active = ui_state_.browser_active_section == sections[i].section;
        PushActiveButtonStyle(active);
        if (ImGui::Button(sections[i].label))
        {
            ui_state_.browser_active_section = sections[i].section;
            ui_state_.main_view_active_tab = sections[i].target;
        }
        PopActiveButtonStyle(active);
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    switch (ui_state_.browser_active_section)
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
                const bool selected = ui_state_.scene_selected_node == node.id.value;
                std::string label = node.name.empty() ? ("Node " + std::to_string(node.id.value))
                                                      : node.name;
                label += "##SceneBrowserNode";
                label += std::to_string(node.id.value);
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    ui_state_.scene_selected_node = node.id.value;
                    ui_state_.map_selected_node = node.id.value;
                    ui_state_.scene_view_mode = SceneViewMode::Node;
                    ui_state_.main_view_active_tab = MainViewTab::Scene;
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
        ImGui::TextUnformatted("世界生成 / 加载 / 保存");
        ImGui::Separator();

        ImGui::Text("配置文件：%s", ui_state_.worldgen_config_buffer.data());
        ImGui::Text("输出目录：%s", ui_state_.worldgen_output_buffer.data());
        ImGui::Text("最近状态：%s", ui_state_.world_command_status.empty() ? "—" : ui_state_.world_command_status.c_str());
        if (!ui_state_.world_load_status.empty())
        {
            ImGui::Text("加载：%s", ui_state_.world_load_status.c_str());
        }
        if (!ui_state_.world_save_status.empty())
        {
            ImGui::Text("保存：%s", ui_state_.world_save_status.c_str());
        }

        if (ImGui::Button("打开世界面板"))
        {
            ui_state_.main_view_active_tab = MainViewTab::World;
            ui_state_.browser_active_section = BrowserSection::World;
        }

        if (runtime_bridge_)
        {
            if (auto lastGen = runtime_bridge_->lastGeneration(); lastGen)
            {
                ImGui::Separator();
                ImGui::Text("最近生成：%s", lastGen->success ? "Success" : "Failed");
                ImGui::Text("Seed：%llu", static_cast<unsigned long long>(lastGen->seed.value));
                ImGui::Text("节点：%zu  |  边：%zu", lastGen->locationCount, lastGen->edgeCount);
                if (!lastGen->error.empty())
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", lastGen->error.c_str());
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
            ui_state_.main_view_active_tab = MainViewTab::Monitor;
            ui_state_.browser_active_section = BrowserSection::Monitor;
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
            ui_state_.main_view_active_tab = MainViewTab::Settings;
            ui_state_.browser_active_section = BrowserSection::LayoutsThemes;
        }
        break;
    }
    }

    ImGui::End();
}

void AppHost::drawMainViewPanel()
{
    if (!ImGui::Begin("Main View", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    auto drawTabButton = [&](const char* label, MainViewTab tab, BrowserSection section) {
        const bool active = (ui_state_.main_view_active_tab == tab);
        PushActiveButtonStyle(active);
        if (ImGui::Button(label, ImVec2(0.0f, 0.0f)))
        {
            ui_state_.main_view_active_tab = tab;
            ui_state_.browser_active_section = section;
        }
        PopActiveButtonStyle(active);
    };

    drawTabButton("Scene", MainViewTab::Scene, BrowserSection::Scene);
    ImGui::SameLine();
    drawTabButton("World", MainViewTab::World, BrowserSection::World);
    ImGui::SameLine();
    drawTabButton("Monitor", MainViewTab::Monitor, BrowserSection::Monitor);
    ImGui::SameLine();
    drawTabButton("Settings", MainViewTab::Settings, BrowserSection::LayoutsThemes);

    ImGui::Separator();

    if (ImGui::BeginChild("MainViewContent", ImVec2(0.0f, 0.0f), false))
    {
        switch (ui_state_.main_view_active_tab)
        {
        case MainViewTab::Scene:
            drawSceneTabContent();
            break;
        case MainViewTab::World:
            drawWorldTabContent();
            break;
        case MainViewTab::Monitor:
            drawMonitorTabContent();
            break;
        case MainViewTab::Settings:
            drawSettingsTabContent();
            break;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace Genesis::Sandbox::Gui
