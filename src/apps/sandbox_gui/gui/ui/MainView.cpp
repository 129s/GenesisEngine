#include "sandbox/gui/ui/MainView.hpp"
#include "../CommandUiHelpers.hpp"
#include "../ImGuiLogSink.hpp"
#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/LayoutHelpers.hpp"
#include "MainViewHelpers.hpp"

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
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace genesis::sandbox::gui
{
    using json = nlohmann::json;
    void MainView::render(UiContext &ctx, InspectorView &inspector)
    {
        const Style::Layout::WindowLayoutConfig windowLayout = Style::Layout::mainViewWindow();
        Style::Layout::WindowStyleScope windowScope(windowLayout);
        if (!ImGui::Begin("Main View", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }
        Ui::drawDockAnchorOverlay("MainViewDockAnchor", ctx.state.layout_mode_enabled);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Sm),
                                   Style::DesignTokens::spacing(Style::SpacingToken::Sm)));
        if (ImGui::BeginChild("MainViewContent", ImVec2(0.0f, 0.0f), false))
        {
            switch (ctx.state.main_view_active_tab)
            {
            case MainViewTab::Scene:
                drawSceneTab(ctx, inspector);
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
        ImGui::PopStyleVar();

        ImGui::End();
    }

    void MainView::drawSceneTab(UiContext &ctx, InspectorView &inspector)
    {
        drawSceneUnified(ctx, inspector);
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
        const Style::Layout::CardLayoutConfig cardLayout = Style::Layout::detailCard();
        const float cardSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Lg);

        auto drawCardHeader = [&](const char *title) {
            ImGui::TextUnformatted(title);
            ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
        };

        auto drawFullWidthInput = [&](const char *label, const char *id, std::string_view hint, char *buffer, std::size_t size) {
            ImGui::TextUnformatted(label);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (!hint.empty())
            {
                ImGui::InputTextWithHint(id, hint.data(), buffer, size);
            }
            else
            {
                ImGui::InputText(id, buffer, size);
            }
        };

        const std::string configInput(ctx.state.worldgen_config_buffer.data());
        const std::string outputInput(ctx.state.worldgen_output_buffer.data());
        const std::string loadInput(ctx.state.world_load_buffer.data());
        const std::string saveInput(ctx.state.world_save_buffer.data());
        const std::string scriptPath(ctx.state.command_script_buffer.data());
        const bool hasConfig = worldVm.hasConfigPath;

        {
            Style::Layout::CardScope card("WorldGenerationCard",
                                          cardLayout,
                                          ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("世界生成");

                drawFullWidthInput("配置路径", "##WorldGenConfigPath", "data/worldgen/default.toml", ctx.state.worldgen_config_buffer.data(), ctx.state.worldgen_config_buffer.size());
                drawFullWidthInput("输出路径", "##WorldGenOutputPath", "生成文件输出目录（可选）", ctx.state.worldgen_output_buffer.data(), ctx.state.worldgen_output_buffer.size());

                ImGui::Checkbox("使用随机种子", &ctx.state.worldgen_use_random_seed);
                Ui::applyClickableCursorToLastItem();
                if (ctx.state.worldgen_use_random_seed)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("刷新种子"))
                    {
                        ctx.state.worldgen_seed = static_cast<std::uint64_t>(std::random_device{}());
                    }
                    Ui::applyClickableCursorToLastItem();
                    ImGui::SameLine();
                    ImGui::Text("Seed %llu", static_cast<unsigned long long>(ctx.state.worldgen_seed));
                }
                else
                {
                    ImGui::Dummy(ImVec2(0.0f, cardLayout.lineGap));
                    ImGui::TextUnformatted("固定种子");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##WorldGenFixedSeed", ImGuiDataType_U64, &ctx.state.worldgen_seed);
                }

                if (!runtimeReady)
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "运行时未连接，无法执行生成命令。");
                }
                if (!hasConfig)
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写配置文件路径");
                }

                ImGui::Dummy(ImVec2(0.0f, cardLayout.sectionGap));

                const bool disableGenerate = !runtimeReady || !hasConfig;
                if (disableGenerate)
                {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("生成世界"))
                {
                    if (runtimeReady)
                    {
                        json command = {
                            {"action", "world.db.generate"},
                            {"configPath", configInput},
                        };
                        if (!ctx.state.worldgen_use_random_seed)
                        {
                            command["seed"] = ctx.state.worldgen_seed;
                        }
                        if (!outputInput.empty())
                        {
                            command["folder"] = outputInput;
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
                Ui::applyClickableCursorToLastItem();
                if (disableGenerate)
                {
                    ImGui::EndDisabled();
                }

                if (!ctx.state.world_command_status.empty())
                {
                    ImGui::Dummy(ImVec2(0.0f, cardLayout.lineGap));
                    ImGui::TextWrapped("%s", ctx.state.world_command_status.c_str());
                }

                if (runtimeReady)
                {
                    if (auto resultOpt = ctx.runtime_bridge->lastGeneration(); resultOpt)
                    {
                        const auto &result = *resultOpt;
                        ImGui::Dummy(ImVec2(0.0f, cardLayout.sectionGap));
                        ImGui::Separator();
                        ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));

                        if (result.success)
                        {
                            ImGui::TextUnformatted("最近一次生成成功");
                            ImGui::BulletText("配置：%s", result.configPath.string().c_str());
                            ImGui::BulletText("种子：%llu", static_cast<unsigned long long>(result.seed.value));
                            ImGui::BulletText("节点：%zu · 边：%zu", result.locationCount, result.edgeCount);
                            ImGui::BulletText("耗时：%.2f ms", result.durationMs);
                            if (result.outputPath)
                            {
                                ImGui::BulletText("输出：%s", result.outputPath->string().c_str());
                            }
                            else
                            {
                                ImGui::BulletText("输出：内存");
                            }
                        }
                        else
                        {
                            ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Danger), "生成失败：%s", result.error.c_str());
                        }

                        if (!result.logs.empty())
                        {
                            ImGui::Dummy(ImVec2(0.0f, cardLayout.lineGap));
                            if (ImGui::BeginChild("WorldGenLogs", ImVec2(0.0f, 160.0f), true))
                            {
                                for (const auto &msg : result.logs)
                                {
                                    ImGui::TextUnformatted(msg.c_str());
                                }
                            }
                            ImGui::EndChild();
                        }
                    }
                }

                ImGui::PopTextWrapPos();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, cardSpacing));

        {
            Style::Layout::CardScope card("WorldIoCard",
                                          cardLayout,
                                          ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("世界 DB（v2）加载 / 保存");

                // Load section
                ImGui::TextUnformatted("世界目录（包含 world.json 与 map_#.json）");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##WorldLoadPath", ctx.state.world_load_buffer.data(), ctx.state.world_load_buffer.size());

                if (loadInput.empty())
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写加载路径");
                }

                const bool disableLoad = !runtimeReady || loadInput.empty();
                if (disableLoad)
                {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("加载世界"))
                {
                    if (runtimeReady)
                    {
                        json command = {
                            {"action", "world.db.load"},
                            {"folder", loadInput},
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
                Ui::applyClickableCursorToLastItem();
                if (disableLoad)
                {
                    ImGui::EndDisabled();
                }
                if (!ctx.state.world_load_status.empty())
                {
                    ImGui::TextWrapped("%s", ctx.state.world_load_status.c_str());
                }

                ImGui::Dummy(ImVec2(0.0f, cardLayout.sectionGap));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));

                // Save section
                ImGui::TextUnformatted("保存目录（写出 world.json 与 map_#.json）");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##WorldSavePath", ctx.state.world_save_buffer.data(), ctx.state.world_save_buffer.size());

                if (saveInput.empty())
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写保存路径");
                }

                const bool disableSave = !runtimeReady || saveInput.empty();
                if (disableSave)
                {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("保存世界"))
                {
                    if (runtimeReady)
                    {
                        json command = {
                            {"action", "world.db.save"},
                            {"folder", saveInput},
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
                Ui::applyClickableCursorToLastItem();
                if (disableSave)
                {
                    ImGui::EndDisabled();
                }
                if (!ctx.state.world_save_status.empty())
                {
                    ImGui::TextWrapped("%s", ctx.state.world_save_status.c_str());
                }

                ImGui::PopTextWrapPos();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, cardSpacing));

        {
            Style::Layout::CardScope card("WorldScriptCard",
                                          cardLayout,
                                          ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("命令脚本");

                ImGui::TextUnformatted("脚本路径");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##CommandScriptPath", ctx.state.command_script_buffer.data(), ctx.state.command_script_buffer.size());

                if (scriptPath.empty())
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "请填写脚本路径");
                }

                const bool disableScript = !runtimeReady;
                if (disableScript)
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
                Ui::applyClickableCursorToLastItem();
                if (disableScript)
                {
                    ImGui::EndDisabled();
                }
                if (!ctx.state.command_script_status.empty())
                {
                    ImGui::TextWrapped("%s", ctx.state.command_script_status.c_str());
                }

                ImGui::PopTextWrapPos();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, cardSpacing));

        // --- Entities (v2) ---
        {
            Style::Layout::CardScope card("EntitiesV2Card",
                                          cardLayout,
                                          ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("实体（v2）");

                const bool runtimeReady2 = (ctx.runtime_bridge != nullptr);
                if (!runtimeReady2)
                {
                    ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Warning), "运行时未连接");
                }

                bool firstSection = true;
                CardSectionHeader(cardLayout, "创建", firstSection);

                ImGui::Text("mapId"); ImGui::SameLine();
                ImGui::InputScalar("##CreateMapId", ImGuiDataType_U32, &ctx.state.agent_create_mapId);
                ImGui::Text("x"); ImGui::SameLine(); ImGui::InputFloat("##CreateX", &ctx.state.agent_create_x);
                ImGui::Text("y"); ImGui::SameLine(); ImGui::InputFloat("##CreateY", &ctx.state.agent_create_y);
                ImGui::Checkbox("创建后立即移动", &ctx.state.agent_create_with_move);
                if (ctx.state.agent_create_with_move)
                {
                    ImGui::Text("to.x"); ImGui::SameLine(); ImGui::InputFloat("##CreateMoveX", &ctx.state.agent_create_move_x);
                    ImGui::Text("to.y"); ImGui::SameLine(); ImGui::InputFloat("##CreateMoveY", &ctx.state.agent_create_move_y);
                    ImGui::Text("speed"); ImGui::SameLine(); ImGui::InputFloat("##CreateMoveSpeed", &ctx.state.agent_create_move_speed);
                }

                if (!runtimeReady2) ImGui::BeginDisabled();
                if (ImGui::Button("创建实体"))
                {
                    json cmd = {
                        {"action","agent.create2d"},
                        {"mapId", ctx.state.agent_create_mapId},
                        {"x", ctx.state.agent_create_x},
                        {"y", ctx.state.agent_create_y}
                    };
                    if (ctx.state.agent_create_with_move)
                    {
                        cmd["move"] = {
                            {"mapId", ctx.state.agent_create_mapId},
                            {"x", ctx.state.agent_create_move_x},
                            {"y", ctx.state.agent_create_move_y},
                            {"speed", ctx.state.agent_create_move_speed}
                        };
                    }
                    std::string err;
                    if (!ctx.runtime_bridge->enqueueCommandFromJson(cmd, "ui", err))
                    {
                        ctx.state.pushToast(std::string("创建失败：") + err, Style::DesignTokens::color(Style::ColorToken::Danger));
                    }
                }
                Ui::applyClickableCursorToLastItem();
                if (!runtimeReady2) ImGui::EndDisabled();

                if (ctx.latest_snapshot && !ctx.latest_snapshot->telemetry.resources.empty())
                {
                    if (ImGui::BeginTable("ResourcesV2Table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("InteractionId");
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Current");
                        ImGui::TableSetupColumn("Capacity");
                        ImGui::TableHeadersRow();
                        for (const auto &r : ctx.latest_snapshot->telemetry.resources)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%u", r.interactionId);
                            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.name.c_str());
                            ImGui::TableSetColumnIndex(2); ImGui::Text("%u", r.current);
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%u", r.capacity);
                        }
                        ImGui::EndTable();
                    }
                }

                CardSectionHeader(cardLayout, "移动", firstSection);
                ImGui::Text("entityId"); ImGui::SameLine();
                ImGui::InputScalar("##MoveEntityId", ImGuiDataType_U32, &ctx.state.agent_move_entityId);
                ImGui::Text("mapId"); ImGui::SameLine(); ImGui::InputScalar("##MoveMapId", ImGuiDataType_U32, &ctx.state.agent_move_mapId);
                ImGui::Text("x"); ImGui::SameLine(); ImGui::InputFloat("##MoveX", &ctx.state.agent_move_x);
                ImGui::Text("y"); ImGui::SameLine(); ImGui::InputFloat("##MoveY", &ctx.state.agent_move_y);
                ImGui::Text("speed"); ImGui::SameLine(); ImGui::InputFloat("##MoveSpeed", &ctx.state.agent_move_speed);

                if (!runtimeReady2) ImGui::BeginDisabled();
                if (ImGui::Button("移动实体"))
                {
                    json cmd = {
                        {"action","agent.move2d"},
                        {"entityId", ctx.state.agent_move_entityId},
                        {"mapId", ctx.state.agent_move_mapId},
                        {"x", ctx.state.agent_move_x},
                        {"y", ctx.state.agent_move_y},
                        {"speed", ctx.state.agent_move_speed}
                    };
                    std::string err;
                    if (!ctx.runtime_bridge->enqueueCommandFromJson(cmd, "ui", err))
                    {
                        ctx.state.pushToast(std::string("移动失败：") + err, Style::DesignTokens::color(Style::ColorToken::Danger));
                    }
                }
                Ui::applyClickableCursorToLastItem();
                if (!runtimeReady2) ImGui::EndDisabled();

                CardSectionHeader(cardLayout, "停止", firstSection);
                ImGui::Text("entityId"); ImGui::SameLine();
                ImGui::InputScalar("##StopEntityId", ImGuiDataType_U32, &ctx.state.agent_stop_entityId);
                if (!runtimeReady2) ImGui::BeginDisabled();
                if (ImGui::Button("停止移动"))
                {
                    json cmd = {
                        {"action","agent.stop2d"},
                        {"entityId", ctx.state.agent_stop_entityId},
                    };
                    std::string err;
                    if (!ctx.runtime_bridge->enqueueCommandFromJson(cmd, "ui", err))
                    {
                        ctx.state.pushToast(std::string("停止失败：") + err, Style::DesignTokens::color(Style::ColorToken::Danger));
                    }
                }
                Ui::applyClickableCursorToLastItem();
                if (!runtimeReady2) ImGui::EndDisabled();

                CardSectionHeader(cardLayout, "传送", firstSection);
                ImGui::Text("entityId"); ImGui::SameLine(); ImGui::InputScalar("##TpEntityId", ImGuiDataType_U32, &ctx.state.agent_tp_entityId);
                ImGui::Text("mapId"); ImGui::SameLine(); ImGui::InputScalar("##TpMapId", ImGuiDataType_U32, &ctx.state.agent_tp_mapId);
                ImGui::Text("x"); ImGui::SameLine(); ImGui::InputFloat("##TpX", &ctx.state.agent_tp_x);
                ImGui::Text("y"); ImGui::SameLine(); ImGui::InputFloat("##TpY", &ctx.state.agent_tp_y);
                if (!runtimeReady2) ImGui::BeginDisabled();
                if (ImGui::Button("传送实体"))
                {
                    json cmd = {
                        {"action","agent.teleport2d"},
                        {"entityId", ctx.state.agent_tp_entityId},
                        {"mapId", ctx.state.agent_tp_mapId},
                        {"x", ctx.state.agent_tp_x},
                        {"y", ctx.state.agent_tp_y}
                    };
                    std::string err;
                    if (!ctx.runtime_bridge->enqueueCommandFromJson(cmd, "ui", err))
                    {
                        ctx.state.pushToast(std::string("传送失败：") + err, Style::DesignTokens::color(Style::ColorToken::Danger));
                    }
                }
                Ui::applyClickableCursorToLastItem();
                if (!runtimeReady2) ImGui::EndDisabled();

                CardSectionHeader(cardLayout, "资源（v2）", firstSection);
                ImGui::Text("interactionId"); ImGui::SameLine(); ImGui::InputScalar("##ResInterId", ImGuiDataType_U32, &ctx.state.resource_consume_interactionId);
                ImGui::Text("amount"); ImGui::SameLine(); ImGui::InputScalar("##ResAmount", ImGuiDataType_U32, &ctx.state.resource_consume_amount);
                if (!runtimeReady2) ImGui::BeginDisabled();
                if (ImGui::Button("消耗资源"))
                {
                    json cmd = {
                        {"action","resource.consume"},
                        {"interactionId", ctx.state.resource_consume_interactionId},
                        {"amount", ctx.state.resource_consume_amount}
                    };
                    std::string err;
                    if (!ctx.runtime_bridge->enqueueCommandFromJson(cmd, "ui", err))
                    {
                        ctx.state.pushToast(std::string("消耗失败：") + err, Style::DesignTokens::color(Style::ColorToken::Danger));
                    }
                }
                Ui::applyClickableCursorToLastItem();
                if (!runtimeReady2) ImGui::EndDisabled();

                CardSectionHeader(cardLayout, "当前实体", firstSection);
                if (ctx.latest_snapshot && !ctx.latest_snapshot->telemetry.agents.empty())
                {
                    if (ImGui::BeginTable("AgentsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("ID");
                        ImGui::TableSetupColumn("Map");
                        ImGui::TableSetupColumn("X");
                        ImGui::TableSetupColumn("Y");
                        ImGui::TableHeadersRow();
                        for (const auto &a : ctx.latest_snapshot->telemetry.agents)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%u", a.entityId);
                            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", a.mapId);
                            ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", a.position.x);
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", a.position.y);
                        }
                        ImGui::EndTable();
                    }
                }
                else
                {
                    ImGui::TextDisabled("无实体");
                }

                ImGui::PopTextWrapPos();
            }
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


    void MainView::drawMonitorTab(UiContext &ctx)
    {
        const auto cardLayout = Style::Layout::detailCard();
        const float cardSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Lg);

        auto drawCardHeader = [&](const char *title) {
            ImGui::TextUnformatted(title);
            ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
        };

        {
            Style::Layout::CardScope card("MonitorTelemetryCard",
                                          cardLayout,
                                          ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("运行概览");
                drawMonitorTelemetry(ctx, cardLayout);
                ImGui::PopTextWrapPos();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, cardSpacing));

        {
            Style::Layout::CardScope card("MonitorLogCard",
                                          cardLayout,
                                          ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("运行日志");
                drawMonitorLog(ctx, cardLayout);
                ImGui::PopTextWrapPos();
            }
        }
    }

void MainView::drawSettingsTab(UiContext &ctx)
    {
        const auto cardLayout = Style::Layout::detailCard();
        const float cardSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Lg);

        auto drawCardHeader = [&](const char *title) {
            ImGui::TextUnformatted(title);
            ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
        };

        {
            Style::Layout::CardScope card("SettingsDisplayCard", cardLayout, ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("显示设置");

                if (ImGui::Checkbox("启用 VSync", &ctx.config.vsync))
                {
                    glfwSwapInterval(ctx.config.vsync ? 1 : 0);
                }
                Ui::applyClickableCursorToLastItem();

                ImGui::PopTextWrapPos();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, cardSpacing));

        {
            Style::Layout::CardScope card("SettingsDesignTokensCard", cardLayout, ImGuiWindowFlags_NoScrollbar);
            if (card.isOpen())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                drawCardHeader("设计令牌预览");

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
                        ImGui::ColorButton(label,
                                           color,
                                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                                           ImVec2(40.0f, 18.0f));
                        ImGui::SameLine();
                        ImGui::TextUnformatted(label);
                    }
                    ImGui::EndTable();
                }

                ImGui::Dummy(ImVec2(0.0f, cardLayout.sectionGap));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));

                ImGui::Text("间距设置");
                ImGui::Text("窗口内边距：%.1f / %.1f", style.WindowPadding.x, style.WindowPadding.y);
                ImGui::Text("元素间距：%.1f / %.1f", style.ItemSpacing.x, style.ItemSpacing.y);
                ImGui::Text("控件圆角：%.1f", style.FrameRounding);

                ImGui::Dummy(ImVec2(0.0f, cardLayout.sectionGap));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, cardLayout.headerGap));
                ImGui::TextWrapped(
                    "后续任务将补充：主题切换、布局预设管理、快捷键自定义等功能。当前阶段仅提供设计指标预览，方便在开发过程中校准 UI 令牌。");

                ImGui::PopTextWrapPos();
            }
        }
    }

    void MainView::drawMonitorTelemetry(UiContext &ctx, const Style::Layout::CardLayoutConfig &layout)
    {
        MonitorPresenterInput presenterInput{
            ctx.latest_snapshot ? &*ctx.latest_snapshot : nullptr};
        const MonitorTelemetryViewModel telemetryVm = monitor_presenter_.buildTelemetryViewModel(presenterInput);

        if (!telemetryVm.hasSnapshot)
        {
            ImGui::TextUnformatted("等待监控数据…");
            return;
        }

        bool firstSection = true;
        CardSectionHeader(layout, "基础数据", firstSection);
        ImGui::Text("步数：%llu", static_cast<unsigned long long>(telemetryVm.step));
        ImGui::Text("实体：%zu", telemetryVm.agentCount);
        ImGui::Text("执行命令：%zu", telemetryVm.actionCount);
        ImGui::Text("需求项：%zu", telemetryVm.needCount);

        if (telemetryVm.needCount > 0)
        {
            CardSectionHeader(layout, "需求统计", firstSection);
            ImGui::Text("平均需求值：%.2f", telemetryVm.averageNeed);
            ImGui::Text("危急需求：%u", telemetryVm.criticalNeedCount);
        }

        if (!telemetryVm.resources.empty())
        {
            CardSectionHeader(layout, "资源监控", firstSection);
            if (ImGui::BeginTable("MonitorResourceTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("节点");
                ImGui::TableSetupColumn("名称");
                ImGui::TableSetupColumn("库存");
                ImGui::TableHeadersRow();

                for (const auto &resource : telemetryVm.resources)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("#%u", resource.locationId);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(resource.name.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u / %u", resource.current, resource.capacity);
                }

                ImGui::EndTable();
            }
        }
    }

    void MainView::drawMonitorLog(UiContext &ctx, const Style::Layout::CardLayoutConfig &layout)
    {
        if (!ctx.state.log_sink)
        {
            ImGui::TextUnformatted("日志缓冲不可用。");
            return;
        }

        bool firstSection = true;
        CardSectionHeader(layout, "控制", firstSection);
        ImGui::Checkbox("自动滚动", &ctx.state.log_auto_scroll);
        Ui::applyClickableCursorToLastItem();

        CardSectionHeader(layout, "日志流", firstSection);

        const auto lines = ctx.state.log_sink->snapshot();

        const float logHeight = std::max(180.0f, ImGui::GetTextLineHeightWithSpacing() * 12.0f);
        if (ImGui::BeginChild("LogConsole.ScrollRegion", ImVec2(0.0f, logHeight), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
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
        }
        ImGui::EndChild();

        ctx.state.log_last_line_count = lines.size();
    }
}
