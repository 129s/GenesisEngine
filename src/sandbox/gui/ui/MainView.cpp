#include "sandbox/gui/ui/MainView.hpp"
#include "../CommandUiHelpers.hpp"
#include "../ImGuiLogSink.hpp"
#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Genesis::Sandbox::Gui
{
    using json = nlohmann::json;

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

        void DrawOverlayToggleButton(const char *id, bool &value, const char *label, const char *tooltip)
        {
            const bool initiallyActive = value;
            PushActiveButtonStyle(initiallyActive);
            std::string buttonLabel = std::string(label) + "##" + id;
            if (ImGui::Button(buttonLabel.c_str()))
            {
                value = !value;
            }
            if (tooltip && tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                ImGui::SetTooltip("%s", tooltip);
            }
            PopActiveButtonStyle(initiallyActive);
        }
    } // namespace

    void MainView::render(UiContext &ctx)
    {
        if (!ImGui::Begin("Main View", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginChild("MainViewContent", ImVec2(0.0f, 0.0f), false))
        {
            switch (ctx.state.main_view_active_tab)
            {
            case MainViewTab::Scene:
                drawSceneTab(ctx);
                break;
            case MainViewTab::World:
                drawWorldTab(ctx);
                break;
            case MainViewTab::Monitor:
                drawMonitorTab(ctx);
                break;
            case MainViewTab::Settings:
                drawSettingsTab(ctx);
                break;
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    void MainView::drawSceneTab(UiContext &ctx)
    {
        drawSceneUnified(ctx);
    }

    void MainView::drawWorldTab(UiContext &ctx)
    {
        const bool runtimeReady = (ctx.runtime_bridge != nullptr);
        std::vector<RuntimeBridge::CommandProgress> commandStatuses;
        if (runtimeReady)
        {
            commandStatuses = ctx.runtime_bridge->commandStatusSnapshot();
            ctx.updateWorldCommandStatuses(commandStatuses);
        }

        WorldPresenterInput presenterInput{
            ctx.state,
            ctx.runtime_bridge,
            runtimeReady ? &commandStatuses : nullptr};
        const WorldViewModel worldVm = world_presenter_.buildWorldViewModel(presenterInput);

        ImGui::TextUnformatted("世界生成 / 加载 / 保存");
        ImGui::Separator();

        ImGui::InputText("配置路径", ctx.state.worldgen_config_buffer.data(), ctx.state.worldgen_config_buffer.size());
        ImGui::InputText("输出路径", ctx.state.worldgen_output_buffer.data(), ctx.state.worldgen_output_buffer.size());

        if (ImGui::Checkbox("随机种子", &ctx.state.worldgen_use_random_seed))
        {
            if (ctx.state.worldgen_use_random_seed)
            {
                ctx.state.worldgen_seed = static_cast<std::uint64_t>(std::random_device{}());
            }
        }

        if (ctx.state.worldgen_use_random_seed)
        {
            ImGui::SameLine();
            if (ImGui::Button("刷新种子"))
            {
                ctx.state.worldgen_seed = static_cast<std::uint64_t>(std::random_device{}());
            }
            ImGui::SameLine();
            ImGui::Text("Seed %llu", static_cast<unsigned long long>(ctx.state.worldgen_seed));
        }
        else
        {
            ImGui::InputScalar("固定种子", ImGuiDataType_U64, &ctx.state.worldgen_seed);
        }

        const std::string configInput(ctx.state.worldgen_config_buffer.data());
        const std::string outputInput(ctx.state.worldgen_output_buffer.data());
        const bool hasConfig = worldVm.hasConfigPath;
        if (!hasConfig)
        {
            ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写配置文件路径");
        }

        if (!runtimeReady || !hasConfig)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("生成世界"))
        {
            if (runtimeReady)
            {
                json command = {
                    {"action", "world.generate"},
                    {"configPath", configInput},
                };
                if (!ctx.state.worldgen_use_random_seed)
                {
                    command["seed"] = ctx.state.worldgen_seed;
                }
                if (!outputInput.empty())
                {
                    command["outputPath"] = outputInput;
                }

                std::string error;
                if (auto id = ctx.runtime_bridge->enqueueCommandFromJson(command, "ui", error))
                {
                    ctx.state.worldgen_command_id = id;
                    ctx.state.world_command_status = "命令已提交 #" + std::to_string(*id);
                }
                else
                {
                    ctx.state.world_command_status = "提交失败：" + error;
                }
            }
        }
        if (!runtimeReady || !hasConfig)
        {
            ImGui::EndDisabled();
        }
        if (!ctx.state.world_command_status.empty())
        {
            ImGui::TextWrapped("%s", ctx.state.world_command_status.c_str());
        }

        if (runtimeReady)
        {
            if (auto resultOpt = ctx.runtime_bridge->lastGeneration(); resultOpt)
            {
                const auto &result = *resultOpt;
                ImGui::Separator();
                if (result.success)
                {
                    ImGui::TextUnformatted("最近一次生成成功");
                    ImGui::BulletText("Config: %s", result.configPath.string().c_str());
                    ImGui::BulletText("Seed: %llu", static_cast<unsigned long long>(result.seed.value));
                    ImGui::BulletText("Locations: %zu · Edges: %zu", result.locationCount, result.edgeCount);
                    ImGui::BulletText("Duration: %.2f ms", result.durationMs);
                    if (result.outputPath)
                    {
                        ImGui::BulletText("Output: %s", result.outputPath->string().c_str());
                    }
                    else
                    {
                        ImGui::BulletText("Output: in-memory");
                    }
                }
                else
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Danger), "生成失败：%s", result.error.c_str());
                }

                if (!result.logs.empty())
                {
                    if (ImGui::BeginChild("WorldGenLogs", ImVec2(0.0f, 160.0f), true))
                    {
                        for (const auto &entry : result.logs)
                        {
                            ImGui::TextUnformatted(entry.message.c_str());
                        }
                    }
                    ImGui::EndChild();
                }
            }
        }

        ImGui::Separator();
        ImGui::InputText("加载路径", ctx.state.world_load_buffer.data(), ctx.state.world_load_buffer.size());
        const std::string loadInput(ctx.state.world_load_buffer.data());
        if (loadInput.empty())
        {
            ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写加载路径");
        }

        if (!runtimeReady || loadInput.empty())
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("加载世界"))
        {
            if (runtimeReady)
            {
                json command = {
                    {"action", "world.load"},
                    {"path", loadInput},
                };
                std::string error;
                if (auto id = ctx.runtime_bridge->enqueueCommandFromJson(command, "ui", error))
                {
                    ctx.state.world_load_command_id = id;
                    ctx.state.world_load_status = "加载任务已提交 #" + std::to_string(*id);
                }
                else
                {
                    ctx.state.world_load_status = "加载失败：" + error;
                }
            }
        }
        if (!runtimeReady || loadInput.empty())
        {
            ImGui::EndDisabled();
        }
        if (!ctx.state.world_load_status.empty())
        {
            ImGui::TextWrapped("%s", ctx.state.world_load_status.c_str());
        }

        ImGui::InputText("保存路径", ctx.state.world_save_buffer.data(), ctx.state.world_save_buffer.size());
        const std::string saveInput(ctx.state.world_save_buffer.data());
        if (saveInput.empty())
        {
            ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写保存路径");
        }

        if (!runtimeReady || saveInput.empty())
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("保存世界"))
        {
            if (runtimeReady)
            {
                json command = {
                    {"action", "world.save"},
                    {"path", saveInput},
                };
                std::string error;
                if (auto id = ctx.runtime_bridge->enqueueCommandFromJson(command, "ui", error))
                {
                    ctx.state.world_save_command_id = id;
                    ctx.state.world_save_status = "保存任务已提交 #" + std::to_string(*id);
                }
                else
                {
                    ctx.state.world_save_status = "保存失败：" + error;
                }
            }
        }
        if (!runtimeReady || saveInput.empty())
        {
            ImGui::EndDisabled();
        }
        if (!ctx.state.world_save_status.empty())
        {
            ImGui::TextWrapped("%s", ctx.state.world_save_status.c_str());
        }

        ImGui::Separator();
        ImGui::InputText("脚本路径", ctx.state.command_script_buffer.data(), ctx.state.command_script_buffer.size());
        const std::string scriptPath(ctx.state.command_script_buffer.data());
        if (scriptPath.empty())
        {
            ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写脚本路径");
        }

        if (!runtimeReady)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("运行脚本"))
        {
            if (scriptPath.empty())
            {
                ctx.state.command_script_status = "请先填写脚本路径";
            }
            else if (runtimeReady)
            {
                std::string error;
                if (ctx.runtime_bridge->enqueueCommandScript(std::filesystem::path(scriptPath), "script", error))
                {
                    ctx.state.command_script_status = "脚本已入队";
                }
                else
                {
                    ctx.state.command_script_status = "脚本执行失败：" + error;
                }
            }
        }
        if (!runtimeReady)
        {
            ImGui::EndDisabled();
        }
        if (!ctx.state.command_script_status.empty())
        {
            ImGui::TextWrapped("%s", ctx.state.command_script_status.c_str());
        }

        if (!worldVm.commands.empty())
        {
            ImGui::Separator();
            ImGui::TextUnformatted("命令状态");
            if (ImGui::BeginChild("WorldCommandStatusList", ImVec2(0.0f, 180.0f), true))
            {
                for (const auto &command : worldVm.commands)
                {
                    ImGui::TextColored(commandStateColor(command.state), "%s", command.summary.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("源：%s", command.source.c_str());
                    if (!command.message.empty())
                    {
                        ImGui::BulletText("%s", command.message.c_str());
                    }
                    if (command.payloadJson && !command.payloadJson->empty())
                    {
                        ImGui::PushTextWrapPos();
                        ImGui::TextWrapped("%s", command.payloadJson->c_str());
                        ImGui::PopTextWrapPos();
                    }
                }
            }
            ImGui::EndChild();
        }
    }

    void InspectorView::render(UiContext &ctx)
    {
        if (!ctx.state.show_inspector)
        {
            return;
        }

        if (!ImGui::Begin("Inspector", &ctx.state.show_inspector))
        {
            ImGui::End();
            return;
        }

        if (!ctx.latest_snapshot)
        {
            ImGui::TextUnformatted("Waiting for snapshot...");
            ImGui::End();
            return;
        }

        const auto &snapshot = *ctx.latest_snapshot;
        const auto &tick = snapshot.telemetry;
        const RuntimeBridge::WorldAtlas *atlasPtr = ctx.runtime_bridge ? &ctx.runtime_bridge->atlas() : nullptr;

        const auto resourceTypeName = [](genesis::world::ResourceType type) -> const char *
        {
            switch (type)
            {
            case genesis::world::ResourceType::Food:
                return "Food";
            case genesis::world::ResourceType::Drink:
                return "Drink";
            case genesis::world::ResourceType::Social:
                return "Social";
            default:
                return "Unknown";
            }
        };

        ImGui::Separator();

        switch (ctx.state.inspector_selection_type)
        {
        case UiState::InspectorSelectionType::None:
            ImGui::TextUnformatted("请使用 Browser 选择实体以查看详情。");
            break;
        case UiState::InspectorSelectionType::Agent:
        {
            const genesis::telemetry::AgentSnapshot *agent = nullptr;
            for (const auto &candidate : tick.agents)
            {
                if (candidate.entityId == ctx.state.inspector_selected_primary)
                {
                    agent = &candidate;
                    break;
                }
            }

            if (!agent)
            {
                ImGui::Text("Agent #%u is not present in the current snapshot.", ctx.state.inspector_selected_primary);
                break;
            }

            std::string nodeName = "(Unknown)";
            const RuntimeBridge::WorldAtlas::Node *nodeInfo = nullptr;
            if (atlasPtr)
            {
                for (const auto &node : atlasPtr->nodes)
                {
                    if (node.id.value == agent->location.value)
                    {
                        nodeName = node.name;
                        nodeInfo = &node;
                        break;
                    }
                }
            }

            if (!agent->name.empty())
            {
                ImGui::Text("%s", agent->name.c_str());
            }
            else
            {
                ImGui::Text("Agent #%u", agent->entityId);
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            if (ImGui::Button("定位地图##agentFocus"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.browser_active_section = BrowserSection::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Node;
                ctx.state.scene_tile_selection.reset();
                ctx.state.inspector_highlight_node = agent->location.value;
                ctx.state.map_selected_node = agent->location.value;
                ctx.state.scene_focus_node_request = agent->location.value;
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("打开节点视图##agentScene"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.browser_active_section = BrowserSection::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Tile;
                ctx.state.scene_selected_node = agent->location.value;
                ctx.state.map_selected_node = agent->location.value;
                ctx.state.scene_tile_selection.reset();
                ctx.state.scene_focus_node_request = agent->location.value;
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            bool followChanged = ImGui::Checkbox("Follow##agentFollow", &ctx.state.inspector_follow_selection);
            if (followChanged && ctx.state.inspector_follow_selection)
            {
                ctx.state.scene_selected_node = agent->location.value;
                ctx.state.map_selected_node = agent->location.value;
                ctx.state.scene_tile_selection.reset();
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("Copy JSON##agentCopy"))
            {
                json agentJson;
                agentJson["type"] = "agent";
                agentJson["step"] = tick.step;
                agentJson["snapshotVersion"] = snapshot.version;

                auto &agentNode = agentJson["agent"];
                agentNode["entityId"] = agent->entityId;
                if (!agent->name.empty())
                {
                    agentNode["name"] = agent->name;
                }
                auto &locationJson = agentNode["location"];
                locationJson["id"] = agent->location.value;
                locationJson["name"] = nodeName;
                if (nodeInfo)
                {
                    locationJson["parent"] = nodeInfo->parent.value;
                    locationJson["kind"] = static_cast<unsigned int>(nodeInfo->kind);
                    locationJson["position"] = {{"x", nodeInfo->position.x}, {"y", nodeInfo->position.y}};
                }

                json needsJson = json::array();
                for (const auto &need : tick.needs)
                {
                    if (need.entityId == agent->entityId)
                    {
                        needsJson.push_back({{"name", need.needName},
                                             {"value", need.value},
                                             {"critical", need.critical}});
                    }
                }
                if (!needsJson.empty())
                {
                    agentJson["needs"] = std::move(needsJson);
                }

                const genesis::telemetry::ActionSnapshot *actionPtr = nullptr;
                for (const auto &entry : tick.actions)
                {
                    if (entry.entityId == agent->entityId)
                    {
                        actionPtr = &entry;
                        break;
                    }
                }
                if (actionPtr)
                {
                    json actionJson{
                        {"label", actionPtr->currentAction},
                        {"queueLength", actionPtr->queueLength},
                        {"target", actionPtr->target.value},
                        {"speed", actionPtr->speed},
                        {"resourceType", resourceTypeName(actionPtr->resource)},
                        {"amount", actionPtr->amount},
                        {"reliefPerUnit", actionPtr->reliefPerUnit}};
                    agentJson["action"] = std::move(actionJson);
                }

                const genesis::telemetry::PlannerSnapshot *plannerPtr = nullptr;
                for (const auto &entry : tick.plannerDecisions)
                {
                    if (entry.entityId == agent->entityId)
                    {
                        plannerPtr = &entry;
                        break;
                    }
                }
                if (plannerPtr)
                {
                    agentJson["planner"] = {
                        {"target", plannerPtr->target.value},
                        {"travelCost", plannerPtr->travelCost},
                        {"score", plannerPtr->score}};
                }

                const genesis::telemetry::MovementProgressSnapshot *movementPtr = nullptr;
                for (const auto &entry : tick.movementProgress)
                {
                    if (entry.entityId == agent->entityId)
                    {
                        movementPtr = &entry;
                        break;
                    }
                }
                if (movementPtr)
                {
                    agentJson["movement"] = {
                        {"from", movementPtr->from.value},
                        {"to", movementPtr->to.value},
                        {"progress", movementPtr->t01}};
                }

                json resourcesJson = json::array();
                for (const auto &res : tick.resources)
                {
                    if (res.location.value == agent->location.value)
                    {
                        resourcesJson.push_back({{"name", res.name},
                                                 {"type", resourceTypeName(res.type)},
                                                 {"current", res.current},
                                                 {"capacity", res.capacity}});
                    }
                }
                if (!resourcesJson.empty())
                {
                    agentJson["resourcesAtLocation"] = std::move(resourcesJson);
                }

                std::vector<std::uint32_t> peers;
                for (const auto &other : tick.agents)
                {
                    if (other.entityId != agent->entityId && other.location.value == agent->location.value)
                    {
                        peers.push_back(other.entityId);
                    }
                }
                if (!peers.empty())
                {
                    agentJson["otherAgentsAtLocation"] = peers;
                }

                std::string serialized = agentJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                ctx.pushToast("Agent snapshot copied", Style::DesignTokens::color(Style::ColorToken::Success));
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            ImGui::Text("ID: %u", agent->entityId);
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            ImGui::Text("Location: #%u %s", agent->location.value, nodeName.c_str());

            ImGui::Separator();

            std::vector<const genesis::telemetry::NeedSnapshot *> needs;
            for (const auto &need : tick.needs)
            {
                if (need.entityId == agent->entityId)
                {
                    needs.push_back(&need);
                }
            }
            if (!needs.empty())
            {
                if (ImGui::BeginTable("NeedsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Need");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableSetupColumn("Critical");
                    ImGui::TableHeadersRow();

                    for (const auto *need : needs)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(need->needName.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.2f", need->value);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(need->critical ? "Yes" : "No");
                    }

                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextUnformatted("No need data.");
            }

            const genesis::telemetry::ActionSnapshot *action = nullptr;
            for (const auto &entry : tick.actions)
            {
                if (entry.entityId == agent->entityId)
                {
                    action = &entry;
                    break;
                }
            }

            if (action)
            {
                ImGui::Separator();
                ImGui::Text("Current Action: %s", action->currentAction.c_str());
                ImGui::Text("Target Node: #%u", action->target.value);
                ImGui::Text("Queue Length: %u", action->queueLength);
                ImGui::Text("Speed: %.2f", action->speed);
                ImGui::Text("Resource: %s · Amount %u", resourceTypeName(action->resource), action->amount);
            }

            const genesis::telemetry::PlannerSnapshot *planner = nullptr;
            for (const auto &entry : tick.plannerDecisions)
            {
                if (entry.entityId == agent->entityId)
                {
                    planner = &entry;
                    break;
                }
            }
            if (planner)
            {
                ImGui::Separator();
                ImGui::Text("Planner Target: #%u", planner->target.value);
                ImGui::Text("Travel Cost: %.2f", planner->travelCost);
                ImGui::Text("Score: %.2f", planner->score);
            }

            const genesis::telemetry::MovementProgressSnapshot *movement = nullptr;
            for (const auto &entry : tick.movementProgress)
            {
                if (entry.entityId == agent->entityId)
                {
                    movement = &entry;
                    break;
                }
            }
            if (movement)
            {
                ImGui::Separator();
                ImGui::Text("Movement Progress: %u → %u (%.2f)", movement->from.value, movement->to.value, movement->t01);
            }

            if (snapshot.diff)
            {
                bool printedHeader = false;
                for (const auto &change : snapshot.diff->needChanges)
                {
                    const auto *afterNeed = change.after ? &(*change.after) : nullptr;
                    const auto *beforeNeed = change.before ? &(*change.before) : nullptr;
                    if ((afterNeed && afterNeed->entityId == agent->entityId) || (beforeNeed && beforeNeed->entityId == agent->entityId))
                    {
                        if (!printedHeader)
                        {
                            ImGui::Separator();
                            ImGui::TextUnformatted("Need changes this frame");
                            printedHeader = true;
                        }
                        char delta[160];
                        if (beforeNeed && afterNeed)
                        {
                            std::snprintf(delta, sizeof(delta), "%s: %.2f → %.2f", afterNeed->needName.c_str(), beforeNeed->value, afterNeed->value);
                        }
                        else if (afterNeed)
                        {
                            std::snprintf(delta, sizeof(delta), "%s: added %.2f", afterNeed->needName.c_str(), afterNeed->value);
                        }
                        else
                        {
                            std::snprintf(delta, sizeof(delta), "%s: removed (%.2f)", beforeNeed->needName.c_str(), beforeNeed->value);
                        }
                        ImGui::BulletText("%s", delta);
                    }
                }
            }
            break;
        }
        case UiState::InspectorSelectionType::Resource:
        {
            if (ctx.state.inspector_selected_primary >= tick.resources.size())
            {
                ImGui::TextUnformatted("Selected resource index is stale.");
                break;
            }

            const auto &resource = tick.resources[ctx.state.inspector_selected_primary];
            std::string nodeName = "(Unknown)";
            const RuntimeBridge::WorldAtlas::Node *nodeInfo = nullptr;
            if (atlasPtr)
            {
                for (const auto &node : atlasPtr->nodes)
                {
                    if (node.id.value == resource.location.value)
                    {
                        nodeName = node.name;
                        nodeInfo = &node;
                        break;
                    }
                }
            }
            ImGui::Text("%s", resource.name.c_str());
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            if (ImGui::Button("定位地图##resourceFocus"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.browser_active_section = BrowserSection::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Node;
                ctx.state.scene_tile_selection.reset();
                ctx.state.inspector_highlight_node = resource.location.value;
                ctx.state.map_selected_node = resource.location.value;
                ctx.state.scene_focus_node_request = resource.location.value;
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("打开节点视图##resourceScene"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.browser_active_section = BrowserSection::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Tile;
                ctx.state.scene_selected_node = resource.location.value;
                ctx.state.map_selected_node = resource.location.value;
                ctx.state.scene_tile_selection.reset();
                ctx.state.scene_focus_node_request = resource.location.value;
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("Copy JSON##resourceCopy"))
            {
                json resourceJson;
                resourceJson["type"] = "resource";
                resourceJson["step"] = tick.step;
                resourceJson["snapshotVersion"] = snapshot.version;

                auto &resourceNode = resourceJson["resource"];
                resourceNode["name"] = resource.name;
                resourceNode["type"] = resourceTypeName(resource.type);
                resourceNode["current"] = resource.current;
                resourceNode["capacity"] = resource.capacity;

                auto &locationJson = resourceNode["location"];
                locationJson["id"] = resource.location.value;
                locationJson["name"] = nodeName;
                if (nodeInfo)
                {
                    locationJson["parent"] = nodeInfo->parent.value;
                    locationJson["kind"] = static_cast<unsigned int>(nodeInfo->kind);
                    locationJson["position"] = {{"x", nodeInfo->position.x}, {"y", nodeInfo->position.y}};
                }

                json spawnsJson = json::array();
                if (atlasPtr)
                {
                    for (const auto &spawn : atlasPtr->spawns)
                    {
                        if (spawn.resource.location.value == resource.location.value && spawn.resource.name == resource.name)
                        {
                            json spawnJson{
                                {"position", {{"x", spawn.position.x}, {"y", spawn.position.y}}},
                                {"capacity", spawn.resource.capacity},
                                {"ratePerStep", spawn.resource.ratePerStep}};
                            if (spawn.resource.local_coord.has_value())
                            {
                                spawnJson["localCoord"] = {
                                    {"x", spawn.resource.local_coord->first},
                                    {"y", spawn.resource.local_coord->second}};
                            }
                            spawnsJson.push_back(std::move(spawnJson));
                        }
                    }
                }
                if (!spawnsJson.empty())
                {
                    resourceJson["spawns"] = std::move(spawnsJson);
                }

                json consumers = json::array();
                for (const auto &action : tick.actions)
                {
                    if (action.target.value == resource.location.value)
                    {
                        consumers.push_back({{"entityId", action.entityId},
                                             {"action", action.currentAction}});
                    }
                }
                if (!consumers.empty())
                {
                    resourceJson["activeAgents"] = std::move(consumers);
                }

                std::string serialized = resourceJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                ctx.pushToast("Resource snapshot copied", Style::DesignTokens::color(Style::ColorToken::Success));
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            ImGui::Text("Node: #%u %s", resource.location.value, nodeName.c_str());

            ImGui::Text("Type: %s", resourceTypeName(resource.type));
            ImGui::Text("Inventory: %u / %u", resource.current, resource.capacity);
            break;
        }
        case UiState::InspectorSelectionType::Node:
        {
            if (!atlasPtr)
            {
                ImGui::TextUnformatted("暂无节点信息。");
                break;
            }

            const RuntimeBridge::WorldAtlas::Node *selectedNode = nullptr;
            for (const auto &node : atlasPtr->nodes)
            {
                if (node.id.value == ctx.state.inspector_selected_primary)
                {
                    selectedNode = &node;
                    break;
                }
            }

            if (!selectedNode)
            {
                ImGui::Text("节点 #%u 不存在。", ctx.state.inspector_selected_primary);
                break;
            }

            ImGui::Text("%s", selectedNode->name.c_str());
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            if (ImGui::Button("定位地图##nodeFocus"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.browser_active_section = BrowserSection::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Node;
                ctx.state.scene_tile_selection.reset();
                ctx.state.inspector_highlight_node = selectedNode->id.value;
                ctx.state.map_selected_node = selectedNode->id.value;
                ctx.state.scene_focus_node_request = selectedNode->id.value;
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("Copy JSON##nodeCopy"))
            {
                json nodeJson;
                nodeJson["type"] = "node";
                nodeJson["step"] = tick.step;
                nodeJson["snapshotVersion"] = snapshot.version;

                auto &nodeObj = nodeJson["node"];
                nodeObj["id"] = selectedNode->id.value;
                nodeObj["name"] = selectedNode->name;
                nodeObj["parent"] = selectedNode->parent.value;
                nodeObj["kind"] = static_cast<unsigned int>(selectedNode->kind);
                nodeObj["position"] = {{"x", selectedNode->position.x}, {"y", selectedNode->position.y}};

                json children = json::array();
                for (const auto &n : atlasPtr->nodes)
                {
                    if (n.parent.value == selectedNode->id.value && n.id.value != selectedNode->id.value)
                    {
                        children.push_back({{"id", n.id.value}, {"name", n.name}});
                    }
                }
                if (!children.empty())
                {
                    nodeObj["children"] = std::move(children);
                }

                json edges = json::array();
                for (const auto &edge : atlasPtr->edges)
                {
                    if (edge.from.value == selectedNode->id.value || edge.to.value == selectedNode->id.value)
                    {
                        edges.push_back({{"from", edge.from.value},
                                         {"to", edge.to.value},
                                         {"bidirectional", edge.bidirectional}});
                    }
                }
                if (!edges.empty())
                {
                    nodeJson["edges"] = std::move(edges);
                }

                json resourcesJson = json::array();
                for (const auto &res : tick.resources)
                {
                    if (res.location.value == selectedNode->id.value)
                    {
                        resourcesJson.push_back({{"name", res.name},
                                                 {"type", resourceTypeName(res.type)},
                                                 {"current", res.current},
                                                 {"capacity", res.capacity}});
                    }
                }
                if (!resourcesJson.empty())
                {
                    nodeJson["resources"] = std::move(resourcesJson);
                }

                json agentsJson = json::array();
                for (const auto &agent : tick.agents)
                {
                    if (agent.location.value == selectedNode->id.value)
                    {
                        agentsJson.push_back({{"entityId", agent.entityId},
                                              {"name", agent.name}});
                    }
                }
                if (!agentsJson.empty())
                {
                    nodeJson["agents"] = std::move(agentsJson);
                }

                std::string serialized = nodeJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                ctx.pushToast("Node snapshot copied", Style::DesignTokens::color(Style::ColorToken::Success));
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            ImGui::Text("ID: %u", selectedNode->id.value);

            ImGui::Text("Parent: %u", selectedNode->parent.value);
            ImGui::Text("Kind: %u", static_cast<unsigned int>(selectedNode->kind));
            break;
        }
        }

        ImGui::Separator();
        if (!snapshot.events.empty())
        {
            ImGui::TextUnformatted("Runtime events this frame");
            if (ImGui::BeginChild("InspectorEventsLog", ImVec2(0, 140.0f), true))
            {
                for (const auto &evt : snapshot.events)
                {
                    ImGui::TextColored(evt.success ? Style::DesignTokens::color(Style::ColorToken::Success) : Style::DesignTokens::color(Style::ColorToken::Danger),
                                       "[#%llu] %s", static_cast<unsigned long long>(evt.id), evt.label.c_str());
                    if (!evt.message.empty())
                    {
                        ImGui::BulletText("%s", evt.message.c_str());
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void MainView::drawSceneUnified(UiContext &ctx)
{
    ScenePresenterInput presenterInput{
        ctx.state,
        ctx.runtime_bridge ? &ctx.runtime_bridge->atlas() : nullptr,
        ctx.latest_snapshot ? &*ctx.latest_snapshot : nullptr};
    const RuntimeBridge::WorldAtlas *atlasPtr = presenterInput.atlas;

    if (ctx.state.scene_selected_node == 0 && atlasPtr && !atlasPtr->nodes.empty())
    {
        ctx.state.scene_selected_node = atlasPtr->nodes.front().id.value;
        if (ctx.state.inspector_selection_type == UiState::InspectorSelectionType::None)
        {
            ctx.state.inspector_selection_type = UiState::InspectorSelectionType::Node;
            ctx.state.inspector_selected_primary = ctx.state.scene_selected_node;
            ctx.state.inspector_selected_secondary = 0;
            ctx.state.inspector_highlight_node = ctx.state.scene_selected_node;
        }
    }

    SceneNodeViewModel nodeVm = scene_presenter_.buildNodeViewModel(presenterInput);
    if (!nodeVm.active && !nodeVm.nodes.empty())
    {
        ctx.state.scene_selected_node = nodeVm.nodes.front().id;
        nodeVm = scene_presenter_.buildNodeViewModel(presenterInput);
        if (ctx.state.inspector_selection_type == UiState::InspectorSelectionType::None)
        {
            ctx.state.inspector_selection_type = UiState::InspectorSelectionType::Node;
            ctx.state.inspector_selected_primary = ctx.state.scene_selected_node;
            ctx.state.inspector_selected_secondary = 0;
            ctx.state.inspector_highlight_node = ctx.state.scene_selected_node;
        }
    }

    const SceneNodeDetails *detailsPtr = nodeVm.active ? &*nodeVm.active : nullptr;
    const SceneNodeGridInfo *gridPtr = detailsPtr ? &detailsPtr->grid : nullptr;

    const float toggleSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Sm);
    auto drawToolButton = [&](const char *label, SceneSelectionTool tool)
    {
        const bool active = (ctx.state.scene_selection_tool == tool);
        PushActiveButtonStyle(active);
        if (ImGui::Button(label))
        {
            ctx.state.scene_selection_tool = tool;
        }
        PopActiveButtonStyle(active);
    };

    drawToolButton("任意", SceneSelectionTool::Any);
    ImGui::SameLine(0.0f, toggleSpacing);
    drawToolButton("节点", SceneSelectionTool::Node);
    ImGui::SameLine(0.0f, toggleSpacing);
    drawToolButton("瓦片", SceneSelectionTool::Tile);

    ImGui::SameLine(0.0f, toggleSpacing);
    ImGui::TextDisabled("图层：");
    ImGui::SameLine(0.0f, toggleSpacing);
    DrawOverlayToggleButton("SceneOverlayGrid", ctx.state.scene_show_grid, "网格", "Ctrl+G 切换网格显示");
    ImGui::SameLine(0.0f, toggleSpacing);
    DrawOverlayToggleButton("SceneOverlayPortals", ctx.state.scene_show_graph, "传送门", "Ctrl+1 切换传送门显示");
    ImGui::SameLine(0.0f, toggleSpacing);
    DrawOverlayToggleButton("SceneOverlayAnchors", ctx.state.scene_show_anchors, "锚点", "Ctrl+A 切换锚点显示");
    ImGui::SameLine(0.0f, toggleSpacing);
    DrawOverlayToggleButton("SceneOverlayResources", ctx.state.scene_show_resources, "资源", "Ctrl+R 切换资源叠加");
    ImGui::SameLine(0.0f, toggleSpacing);
    DrawOverlayToggleButton("SceneOverlayRuler", ctx.state.scene_show_ruler, "标尺", "Ctrl+6 切换标尺（左键设起点，右键清除）");

    bool resetRequested = false;
    ImGui::SameLine(0.0f, toggleSpacing);
    if (detailsPtr)
    {
        if (ImGui::Button("重置视图"))
        {
            resetRequested = true;
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("重置视图");
        ImGui::EndDisabled();
    }
    ImGui::SameLine(0.0f, toggleSpacing);
    ImGui::TextDisabled("滚轮缩放｜右键拖拽");

    ImGui::Spacing();

    const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 canvasExtent{std::max(160.0f, canvasAvail.x), std::max(160.0f, canvasAvail.y)};
    const ImVec2 canvasMax{canvasPos.x + canvasExtent.x, canvasPos.y + canvasExtent.y};
    constexpr float canvasMargin = 24.0f;

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    auto drawCanvasBackground = [&]()
    {
        drawList->AddRectFilled(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_WindowBg));
        drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));
    };
    auto drawCanvasMessage = [&](const char *text)
    {
        drawCanvasBackground();
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 textPos{
            canvasPos.x + (canvasExtent.x - textSize.x) * 0.5f,
            canvasPos.y + (canvasExtent.y - textSize.y) * 0.5f};
        drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), text);
    };

    if (!atlasPtr)
    {
        drawCanvasMessage("RuntimeBridge 未就绪。");
        return;
    }

    if (nodeVm.nodes.empty())
    {
        drawCanvasMessage("暂无地图节点。");
        return;
    }

    if (!detailsPtr || !gridPtr)
    {
        drawCanvasMessage("请选择包含瓦片数据的节点。");
        return;
    }

    const SceneNodeDetails &details = *detailsPtr;
    const SceneNodeGridInfo &grid = *gridPtr;

    drawCanvasBackground();

    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("SceneCanvas", canvasExtent, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasActive = ImGui::IsItemActive();
    ImGuiIO &io = ImGui::GetIO();

    drawList->PushClipRect(canvasPos, canvasMax, true);

    bool shouldRecenter = resetRequested;
    if (ctx.state.scene_camera_node != details.nodeId)
    {
        ctx.state.scene_camera_node = details.nodeId;
        shouldRecenter = true;
    }
    if (ctx.state.scene_focus_node_request && *ctx.state.scene_focus_node_request == details.nodeId)
    {
        ctx.state.scene_focus_node_request.reset();
        shouldRecenter = true;
    }

    auto centerCamera = [&](float zoom)
    {
        const float cell = grid.baseTileSize * zoom;
        const float mapWidthPx = (grid.maxX - grid.minX + 1) * cell;
        const float mapHeightPx = (grid.maxY - grid.minY + 1) * cell;
        const float viewWidth = canvasExtent.x - canvasMargin * 2.0f;
        const float viewHeight = canvasExtent.y - canvasMargin * 2.0f;
        ctx.state.scene_cam_offset_x = (viewWidth - mapWidthPx) * 0.5f;
        ctx.state.scene_cam_offset_y = (viewHeight - mapHeightPx) * 0.5f;
    };

    auto enforceCameraBounds = [&]()
    {
        const float cell = grid.baseTileSize * ctx.state.scene_cam_zoom;
        const float mapWidthPx = (grid.maxX - grid.minX + 1) * cell;
        const float mapHeightPx = (grid.maxY - grid.minY + 1) * cell;
        const float viewWidth = canvasExtent.x - canvasMargin * 2.0f;
        const float viewHeight = canvasExtent.y - canvasMargin * 2.0f;

        auto clampOffset = [&](float mapSize, float viewSize, float &offset)
        {
            if (mapSize <= viewSize)
            {
                offset = (viewSize - mapSize) * 0.5f;
            }
            else
            {
                const float minOffset = viewSize - mapSize;
                const float maxOffset = 0.0f;
                if (offset < minOffset)
                {
                    offset = minOffset;
                }
                else if (offset > maxOffset)
                {
                    offset = maxOffset;
                }
            }
        };

        clampOffset(mapWidthPx, viewWidth, ctx.state.scene_cam_offset_x);
        clampOffset(mapHeightPx, viewHeight, ctx.state.scene_cam_offset_y);
    };

    if (shouldRecenter)
    {
        ctx.state.scene_cam_zoom = 1.0f;
        centerCamera(ctx.state.scene_cam_zoom);
        enforceCameraBounds();
    }

    const float prevZoom = ctx.state.scene_cam_zoom;
    if (io.MouseWheel != 0.0f)
    {
        const float zoomFactor = (io.MouseWheel > 0.0f) ? 1.1f : 0.9f;
        float newZoom = prevZoom * zoomFactor;
        newZoom = std::clamp(newZoom, 0.25f, 6.0f);

        if (std::abs(newZoom - prevZoom) > 1e-4f)
        {
            const ImVec2 focus = canvasHovered ? io.MousePos : ImVec2(canvasPos.x + canvasExtent.x * 0.5f, canvasPos.y + canvasExtent.y * 0.5f);
            const float prevCell = grid.baseTileSize * prevZoom;
            const float newCell = grid.baseTileSize * newZoom;
            const float localX = focus.x - (canvasPos.x + canvasMargin + ctx.state.scene_cam_offset_x);
            const float localY = focus.y - (canvasPos.y + canvasMargin + ctx.state.scene_cam_offset_y);

            ctx.state.scene_cam_zoom = newZoom;
            ctx.state.scene_cam_offset_x = localX - (localX / prevCell) * newCell;
            ctx.state.scene_cam_offset_y = localY - (localY / prevCell) * newCell;
            enforceCameraBounds();
        }
    }

    if (canvasActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        const ImVec2 delta = io.MouseDelta;
        ctx.state.scene_cam_offset_x += delta.x;
        ctx.state.scene_cam_offset_y += delta.y;
        enforceCameraBounds();
    }

    const float cellPx = grid.baseTileSize * ctx.state.scene_cam_zoom;
    if (cellPx <= 0.01f)
    {
        drawList->AddText(canvasPos, ImGui::GetColorU32(ImGuiCol_Text), "瓦片尺寸异常。");
        drawList->PopClipRect();
        ImGui::Dummy(canvasExtent);
        return;
    }

    auto toScreenRaw = [&](float gx, float gy) -> ImVec2
    {
        const float px = canvasPos.x + canvasMargin + ctx.state.scene_cam_offset_x + (gx - static_cast<float>(grid.minX)) * cellPx;
        const float py = canvasPos.y + canvasMargin + ctx.state.scene_cam_offset_y + (gy - static_cast<float>(grid.minY)) * cellPx;
        return ImVec2(px, py);
    };
    auto toScreen = [&](float gx, float gy) -> ImVec2
    {
        ImVec2 raw = toScreenRaw(gx, gy);
        return ImVec2(std::floor(raw.x) + 0.5f, std::floor(raw.y) + 0.5f);
    };
    auto tileFromScreen = [&](const ImVec2 &screen) -> std::optional<std::pair<int, int>>
    {
        const float localX = screen.x - (canvasPos.x + canvasMargin + ctx.state.scene_cam_offset_x);
        const float localY = screen.y - (canvasPos.y + canvasMargin + ctx.state.scene_cam_offset_y);
        const float gx = localX / cellPx + static_cast<float>(grid.minX);
        const float gy = localY / cellPx + static_cast<float>(grid.minY);
        const int tileX = static_cast<int>(std::floor(gx));
        const int tileY = static_cast<int>(std::floor(gy));
        if (tileX < grid.minX || tileX > grid.maxX || tileY < grid.minY || tileY > grid.maxY)
        {
            return std::nullopt;
        }
        return std::pair{tileX, tileY};
    };

    if (details.tilemap)
    {
        const ImU32 tileColorA = ImGui::GetColorU32(ImVec4(0.18f, 0.25f, 0.32f, 0.65f));
        const ImU32 tileColorB = ImGui::GetColorU32(ImVec4(0.15f, 0.21f, 0.28f, 0.65f));
        for (int y = 0; y < details.tilemap->height; ++y)
        {
            for (int x = 0; x < details.tilemap->width; ++x)
            {
                const float gx0 = static_cast<float>(grid.minX + x);
                const float gy0 = static_cast<float>(grid.minY + y);
                ImVec2 cellMin = toScreenRaw(gx0, gy0);
                ImVec2 cellMax = toScreenRaw(gx0 + 1.0f, gy0 + 1.0f);
                const ImU32 fill = ((x + y) % 2 == 0) ? tileColorA : tileColorB;
                drawList->AddRectFilled(cellMin, cellMax, fill);
            }
        }
    }
    else
    {
        const ImVec2 min = toScreenRaw(static_cast<float>(grid.minX), static_cast<float>(grid.minY));
        const ImVec2 max = toScreenRaw(static_cast<float>(grid.maxX + 1), static_cast<float>(grid.maxY + 1));
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.14f, 0.16f, 0.20f, 0.65f)));
    }

    if (ctx.state.scene_show_grid)
    {
        const ImU32 gridColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
        for (int x = grid.minX; x <= grid.maxX + 1; ++x)
        {
            drawList->AddLine(toScreen(static_cast<float>(x), static_cast<float>(grid.minY)),
                              toScreen(static_cast<float>(x), static_cast<float>(grid.maxY + 1)),
                              gridColor,
                              1.0f);
        }
        for (int y = grid.minY; y <= grid.maxY + 1; ++y)
        {
            drawList->AddLine(toScreen(static_cast<float>(grid.minX), static_cast<float>(y)),
                              toScreen(static_cast<float>(grid.maxX + 1), static_cast<float>(y)),
                              gridColor,
                              1.0f);
        }
    }

    std::optional<std::pair<int, int>> hoveredTile;
    if (canvasHovered)
    {
        hoveredTile = tileFromScreen(io.MousePos);
    }

    const ImU32 highlightColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Highlight));
    if (ctx.state.scene_tile_selection && ctx.state.scene_tile_selection->nodeId == details.nodeId)
    {
        const auto &sel = *ctx.state.scene_tile_selection;
        ImVec2 selMin = toScreenRaw(static_cast<float>(sel.tileX), static_cast<float>(sel.tileY));
        ImVec2 selMax = toScreenRaw(static_cast<float>(sel.tileX + 1), static_cast<float>(sel.tileY + 1));
        drawList->AddRect(selMin, selMax, highlightColor, 0.0f, 0, 2.0f);
    }

    if (ctx.state.ui_fps_display > 0.0f)
    {
        const int fpsRounded = static_cast<int>(std::lround(ctx.state.ui_fps_display));
        char fpsBuffer[16];
        std::snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS %d", fpsRounded);
        const ImVec2 textSize = ImGui::CalcTextSize(fpsBuffer);
        const ImVec2 padding{6.0f, 4.0f};
        const ImVec2 textPos{canvasPos.x + 10.0f, canvasPos.y + 10.0f};
        const ImVec2 bgMin{textPos.x - padding.x, textPos.y - padding.y};
        const ImVec2 bgMax{textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y};
        drawList->AddRectFilled(bgMin, bgMax, ImGui::GetColorU32(ImVec4(0.08f, 0.09f, 0.12f, 0.85f)), 4.0f);
        drawList->AddRect(bgMin, bgMax, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, ImDrawFlags_RoundCornersAll, 1.0f);
        drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), fpsBuffer);
    }

    if (hoveredTile)
    {
        ImVec2 hMin = toScreenRaw(static_cast<float>(hoveredTile->first), static_cast<float>(hoveredTile->second));
        ImVec2 hMax = toScreenRaw(static_cast<float>(hoveredTile->first + 1), static_cast<float>(hoveredTile->second + 1));
        drawList->AddRect(hMin, hMax, highlightColor, 0.0f, 0, 1.2f);
    }

    auto nodeNameById = [&](std::uint32_t id) -> std::string
    {
        const auto it = std::find_if(nodeVm.nodes.begin(), nodeVm.nodes.end(), [&](const SceneNodeSummary &entry) {
            return entry.id == id;
        });
        if (it != nodeVm.nodes.end())
        {
            return it->name.empty() ? ("节点 " + std::to_string(it->id)) : it->name;
        }
        for (const auto &node : atlasPtr->nodes)
        {
            if (node.id.value == id)
            {
                return node.name.empty() ? ("节点 " + std::to_string(node.id.value)) : node.name;
            }
        }
        return "#" + std::to_string(id);
    };

    const bool nodeToolActive = ctx.state.scene_selection_tool == SceneSelectionTool::Any ||
                                ctx.state.scene_selection_tool == SceneSelectionTool::Node;
    const bool tileToolActive = ctx.state.scene_selection_tool == SceneSelectionTool::Any ||
                                ctx.state.scene_selection_tool == SceneSelectionTool::Tile;

    const float portalRadius = std::max(6.0f, cellPx * 0.25f);
    const ImU32 portalColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
    std::optional<SceneNodePortal> hoveredPortal;
    if (ctx.state.scene_show_graph)
    {
        for (const auto &portal : details.portals)
        {
            const ImVec2 center = toScreen(portal.x + 0.5f, portal.y + 0.5f);
            drawList->AddCircleFilled(center, portalRadius, portalColor, 24);
            drawList->AddCircle(center, portalRadius, ImGui::GetColorU32(ImGuiCol_Border), 24, 1.2f);

            const std::string label = "→ " + nodeNameById(portal.targetNodeId);
            drawList->AddText(ImVec2(center.x + portalRadius + 4.0f, center.y - ImGui::GetTextLineHeight() * 0.5f),
                              ImGui::GetColorU32(ImGuiCol_Text),
                              label.c_str());

            if (canvasHovered)
            {
                const float dx = io.MousePos.x - center.x;
                const float dy = io.MousePos.y - center.y;
                if ((dx * dx + dy * dy) <= (portalRadius * portalRadius))
                {
                    hoveredPortal = portal;
                    }
                }
            }
        }

    if (hoveredPortal && nodeToolActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (hoveredPortal->targetNodeId != 0)
        {
            ctx.state.scene_selected_node = hoveredPortal->targetNodeId;
            ctx.state.map_selected_node = hoveredPortal->targetNodeId;
            ctx.state.scene_tile_selection.reset();
            ctx.state.scene_focus_node_request = hoveredPortal->targetNodeId;
            ctx.state.scene_camera_node = 0;
        }
    }

    if (hoveredPortal)
    {
        ImGui::BeginTooltip();
        ImGui::Text("前往：%s", nodeNameById(hoveredPortal->targetNodeId).c_str());
        ImGui::EndTooltip();
    }

    if (tileToolActive && hoveredTile && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !hoveredPortal)
    {
        ctx.state.scene_tile_selection = SceneTileSelection{details.nodeId, hoveredTile->first, hoveredTile->second};
        ctx.state.scene_selected_node = details.nodeId;
    }

    if (!ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        const ImVec2 drag = io.MouseDragMaxDistanceAbs[ImGuiMouseButton_Right];
        if (drag.x < 4.0f && drag.y < 4.0f)
        {
            ctx.state.scene_tile_selection.reset();
        }
    }

    if (ctx.state.scene_show_resources)
    {
        auto colorForResource = [](genesis::world::ResourceType type) -> ImU32
        {
            switch (type)
            {
            case genesis::world::ResourceType::Food:
                return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Warning));
            case genesis::world::ResourceType::Drink:
                return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
            case genesis::world::ResourceType::Social:
                return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Success));
            default:
                return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Muted));
            }
        };

        for (const auto &resource : details.resources)
        {
            const ImVec2 center = toScreen(resource.x + 0.5f, resource.y + 0.5f);
            const float radius = std::max(3.5f, cellPx * 0.25f);
            const ImU32 fillColor = colorForResource(resource.type);
            drawList->AddCircleFilled(center, radius, fillColor, 16);
            drawList->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Border), 16, 1.1f);
        }
    }

    if (ctx.state.scene_show_anchors)
    {
        const ImU32 anchorColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::AccentActive));
        for (const auto &anchor : details.anchors)
        {
            const ImVec2 base = toScreen(anchor.x + 0.5f, anchor.y + 0.5f);
            const float size = std::max(4.0f, cellPx * 0.2f);
            const ImVec2 a{base.x - size, base.y};
            const ImVec2 b{base.x + size, base.y};
            const ImVec2 c{base.x, base.y + size * 1.6f};
            drawList->AddTriangleFilled(a, b, c, anchorColor);
            drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
            const std::string label = std::string("↔ ") + nodeNameById(anchor.targetNodeId);
            drawList->AddText(ImVec2(base.x + size + 4.0f, base.y - ImGui::GetTextLineHeight() * 0.5f),
                              ImGui::GetColorU32(ImGuiCol_Text),
                              label.c_str());
        }
    }

    if (ctx.state.scene_show_ruler)
    {
        const ImU32 rulerColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
        const ImVec2 cursor = io.MousePos;
        const bool inside = cursor.x >= canvasPos.x && cursor.x <= canvasMax.x &&
                            cursor.y >= canvasPos.y && cursor.y <= canvasMax.y;
        if (inside)
        {
            drawList->AddLine(ImVec2(cursor.x, canvasPos.y), ImVec2(cursor.x, canvasMax.y), rulerColor, 1.0f);
            drawList->AddLine(ImVec2(canvasPos.x, cursor.y), ImVec2(canvasMax.x, cursor.y), rulerColor, 1.0f);
        }

        if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (auto tile = tileFromScreen(cursor))
            {
                ctx.state.scene_ruler_anchor = RuntimeBridge::Vector2{
                    static_cast<float>(tile->first),
                    static_cast<float>(tile->second)};
            }
            else
            {
                ctx.state.scene_ruler_anchor.reset();
            }
        }
        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ctx.state.scene_ruler_anchor.reset();
        }

        if (ctx.state.scene_ruler_anchor)
        {
            const RuntimeBridge::Vector2 anchorTile = *ctx.state.scene_ruler_anchor;
            const ImVec2 anchorScreen = toScreen(anchorTile.x + 0.5f, anchorTile.y + 0.5f);
            drawList->AddCircleFilled(anchorScreen, 4.0f, rulerColor, 24);
            drawList->AddLine(anchorScreen, cursor, rulerColor, 1.3f);

            if (auto tile = tileFromScreen(cursor))
            {
                const float dx = static_cast<float>(tile->first) - anchorTile.x;
                const float dy = static_cast<float>(tile->second) - anchorTile.y;
                const float dist = std::hypot(dx, dy);
                char buffer[96];
                std::snprintf(buffer, sizeof(buffer), "Δ (%d, %d)  dist %.2f", static_cast<int>(dx), static_cast<int>(dy), dist);
                drawList->AddText(ImVec2(cursor.x + 12.0f, cursor.y - 18.0f),
                                  ImGui::GetColorU32(ImGuiCol_Text),
                                  buffer);
            }
        }
    }

    if (hoveredTile)
    {
        char coordBuffer[48];
        std::snprintf(coordBuffer, sizeof(coordBuffer), "Tile (%d, %d)", hoveredTile->first, hoveredTile->second);
        drawList->AddText(ImVec2(canvasPos.x + 10.0f, canvasMax.y - ImGui::GetTextLineHeightWithSpacing() - 6.0f),
                          ImGui::GetColorU32(ImGuiCol_TextDisabled),
                          coordBuffer);
    }

    drawList->PopClipRect();
}
void MainView::drawMonitorTab(UiContext &ctx)
    {
        ImGui::TextUnformatted("运行概览");
        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
        if (ctx.latest_snapshot)
        {
            const auto &tick = ctx.latest_snapshot->telemetry;
            ImGui::Text("Step %llu | Agents %zu | Actions %zu",
                        static_cast<unsigned long long>(tick.step),
                        tick.agents.size(),
                        tick.actions.size());
        }
        else
        {
            ImGui::TextUnformatted("(等待快照)");
        }

        ImGui::Separator();

        const float totalHeight = std::max(240.0f, ImGui::GetContentRegionAvail().y);
        const float telemetryHeight = totalHeight * 0.45f;

        if (ImGui::BeginChild("MonitorTelemetry", ImVec2(0.0f, telemetryHeight), true))
        {
            drawMonitorTelemetry(ctx);
        }
        ImGui::EndChild();

        ImGui::Spacing();

        if (ImGui::BeginChild("MonitorLogs", ImVec2(0.0f, 0.0f), true))
        {
            drawMonitorLog(ctx);
        }
        ImGui::EndChild();
    }

    void MainView::drawSettingsTab(UiContext &ctx)
    {
        ImGui::TextUnformatted("显示设置");
        ImGui::Separator();

        if (ImGui::Checkbox("启用 VSync", &ctx.config.vsync))
        {
            glfwSwapInterval(ctx.config.vsync ? 1 : 0);
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("设计令牌预览");
        ImGui::Separator();

        const ImGuiStyle &style = ImGui::GetStyle();

        ImGui::Text("颜色样本");
        if (ImGui::BeginTable("SettingsColors", 4, ImGuiTableFlags_SizingFixedFit))
        {
            const std::array<std::pair<const char *, ImGuiCol>, 4> swatches = {
                std::pair{"WindowBg", ImGuiCol_WindowBg},
                std::pair{"Header", ImGuiCol_Header},
                std::pair{"Button", ImGuiCol_Button},
                std::pair{"Accent", ImGuiCol_TabActive}};
            for (const auto &[label, col] : swatches)
            {
                ImGui::TableNextColumn();
                const ImVec4 color = style.Colors[col];
                ImGui::ColorButton(label, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(40.0f, 18.0f));
                ImGui::SameLine();
                ImGui::TextUnformatted(label);
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("间距设置");
        ImGui::Text("窗口内边距：%.1f / %.1f", style.WindowPadding.x, style.WindowPadding.y);
        ImGui::Text("元素间距：%.1f / %.1f", style.ItemSpacing.x, style.ItemSpacing.y);
        ImGui::Text("控件圆角：%.1f", style.FrameRounding);

        ImGui::Separator();
        ImGui::TextWrapped(
            "后续任务将补充：主题切换、布局预设管理、快捷键自定义等功能。当前阶段仅提供设计指标预览，方便在开发过程中校准 UI 令牌。");
    }

    void MainView::drawMonitorTelemetry(UiContext &ctx)
    {
        MonitorPresenterInput presenterInput{
            ctx.latest_snapshot ? &*ctx.latest_snapshot : nullptr};
        const MonitorTelemetryViewModel telemetryVm = monitor_presenter_.buildTelemetryViewModel(presenterInput);

        if (!telemetryVm.hasSnapshot)
        {
            ImGui::TextUnformatted("等待监控数据…");
            return;
        }

        ImGui::Text("步数：%llu", static_cast<unsigned long long>(telemetryVm.step));
        ImGui::Text("实体：%zu", telemetryVm.agentCount);
        ImGui::Text("执行命令：%zu", telemetryVm.actionCount);
        ImGui::Text("需求项：%zu", telemetryVm.needCount);

        if (telemetryVm.needCount > 0)
        {
            ImGui::Separator();
            ImGui::Text("平均需求值：%.2f", telemetryVm.averageNeed);
            ImGui::Text("危急需求：%u", telemetryVm.criticalNeedCount);
        }

        if (!telemetryVm.resources.empty())
        {
            ImGui::Separator();
            for (const auto &resource : telemetryVm.resources)
            {
                ImGui::Text("#%u %s (%u / %u)",
                            resource.locationId,
                            resource.name.c_str(),
                            resource.current,
                            resource.capacity);
            }
        }
    }

    void MainView::drawMonitorLog(UiContext &ctx)
    {
        if (!ctx.state.log_sink)
        {
            ImGui::TextUnformatted("日志缓冲不可用。");
            return;
        }

        ImGui::Checkbox("自动滚动", &ctx.state.log_auto_scroll);
        ImGui::Separator();

        const auto lines = ctx.state.log_sink->snapshot();

        ImGui::BeginChild("LogConsole.ScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        const bool stickToBottom = ctx.state.log_auto_scroll &&
                                   (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f || ctx.state.log_last_line_count == 0);

        for (const auto &line : lines)
        {
            ImGui::TextUnformatted(line.c_str());
        }

        if (stickToBottom && !lines.empty())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ctx.state.log_last_line_count = lines.size();

        ImGui::EndChild();
    }
}
