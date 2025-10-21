#include "sandbox/gui/AppHost.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "CommandUiHelpers.hpp"
#include "FilesystemHelpers.hpp"

namespace Genesis::Sandbox::Gui
{
using json = nlohmann::json;

void AppHost::drawWorldTabContent()
{
    const bool bridgeReady = runtime_bridge_ != nullptr;
    std::vector<RuntimeBridge::CommandProgress> commandStatuses;
    if (bridgeReady)
    {
        commandStatuses = runtime_bridge_->commandStatusSnapshot();
        refreshCommandStatusTexts(commandStatuses);
    }

    ImGui::TextUnformatted("世界生成 / 加载 / 保存");
    ImGui::Separator();

    ImGui::InputText("配置路径", ui_state_.worldgen_config_buffer.data(), ui_state_.worldgen_config_buffer.size());
    ImGui::InputText("输出路径", ui_state_.worldgen_output_buffer.data(), ui_state_.worldgen_output_buffer.size());

    if (ImGui::Checkbox("随机种子", &ui_state_.worldgen_use_random_seed))
    {
        if (ui_state_.worldgen_use_random_seed)
        {
            ui_state_.worldgen_seed = static_cast<std::uint64_t>(std::random_device{}());
        }
    }

    if (ui_state_.worldgen_use_random_seed)
    {
        ImGui::SameLine();
        if (ImGui::Button("刷新种子"))
        {
            ui_state_.worldgen_seed = static_cast<std::uint64_t>(std::random_device{}());
        }
        ImGui::SameLine();
        ImGui::Text("Seed %llu", static_cast<unsigned long long>(ui_state_.worldgen_seed));
    }
    else
    {
        ImGui::InputScalar("固定种子", ImGuiDataType_U64, &ui_state_.worldgen_seed);
    }

    const std::string configInput(ui_state_.worldgen_config_buffer.data());
    const std::string outputInput(ui_state_.worldgen_output_buffer.data());
    const bool hasConfig = !configInput.empty();
    if (!hasConfig)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "请填写配置文件路径");
    }

    if (!bridgeReady || !hasConfig)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("生成世界"))
    {
        if (bridgeReady)
        {
            json command = {
                {"action", "world.generate"},
                {"configPath", configInput},
            };
            if (!ui_state_.worldgen_use_random_seed)
            {
                command["seed"] = ui_state_.worldgen_seed;
            }
            if (!outputInput.empty())
            {
                command["outputPath"] = outputInput;
            }

            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                ui_state_.worldgen_command_id = id;
                ui_state_.world_command_status = "命令已提交 #" + std::to_string(*id);
            }
            else
            {
                ui_state_.world_command_status = "提交失败：" + error;
            }
        }
    }
    if (!bridgeReady || !hasConfig)
    {
        ImGui::EndDisabled();
    }
    if (!ui_state_.world_command_status.empty())
    {
        ImGui::TextWrapped("%s", ui_state_.world_command_status.c_str());
    }

    if (bridgeReady)
    {
        if (auto resultOpt = runtime_bridge_->lastGeneration(); resultOpt)
        {
            const auto& result = *resultOpt;
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
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "生成失败：%s", result.error.c_str());
            }

            if (!result.logs.empty())
            {
                if (ImGui::BeginChild("WorldGenLogs", ImVec2(0.0f, 160.0f), true))
                {
                    for (const auto& entry : result.logs)
                    {
                        ImGui::TextUnformatted(entry.message.c_str());
                    }
                }
                ImGui::EndChild();
            }
        }
    }

    ImGui::Separator();
    ImGui::InputText("加载路径", ui_state_.world_load_buffer.data(), ui_state_.world_load_buffer.size());
    const std::string loadInput(ui_state_.world_load_buffer.data());
    if (loadInput.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "请填写加载路径");
    }

    if (!bridgeReady || loadInput.empty())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("加载世界"))
    {
        if (bridgeReady)
        {
            json command = {
                {"action", "world.load"},
                {"path", loadInput},
            };
            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                ui_state_.world_load_command_id = id;
                ui_state_.world_load_status = "加载任务已提交 #" + std::to_string(*id);
            }
            else
            {
                ui_state_.world_load_status = "加载失败：" + error;
            }
        }
    }
    if (!bridgeReady || loadInput.empty())
    {
        ImGui::EndDisabled();
    }
    if (!ui_state_.world_load_status.empty())
    {
        ImGui::TextWrapped("%s", ui_state_.world_load_status.c_str());
    }

    ImGui::InputText("保存路径", ui_state_.world_save_buffer.data(), ui_state_.world_save_buffer.size());
    const std::string saveInput(ui_state_.world_save_buffer.data());
    if (saveInput.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "请填写保存路径");
    }

    if (!bridgeReady || saveInput.empty())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("保存世界"))
    {
        if (bridgeReady)
        {
            json command = {
                {"action", "world.save"},
                {"path", saveInput},
            };
            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                ui_state_.world_save_command_id = id;
                ui_state_.world_save_status = "保存任务已提交 #" + std::to_string(*id);
            }
            else
            {
                ui_state_.world_save_status = "保存失败：" + error;
            }
        }
    }
    if (!bridgeReady || saveInput.empty())
    {
        ImGui::EndDisabled();
    }
    if (!ui_state_.world_save_status.empty())
    {
        ImGui::TextWrapped("%s", ui_state_.world_save_status.c_str());
    }

    ImGui::Separator();
    ImGui::InputText("脚本路径", ui_state_.command_script_buffer.data(), ui_state_.command_script_buffer.size());
    const std::string scriptPath(ui_state_.command_script_buffer.data());
    if (scriptPath.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "请填写脚本路径");
    }

    if (!bridgeReady)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("运行脚本"))
    {
        if (scriptPath.empty())
        {
            ui_state_.command_script_status = "请先填写脚本路径";
        }
        else if (bridgeReady)
        {
            std::string error;
            if (runtime_bridge_->enqueueCommandScript(std::filesystem::path(scriptPath), "script", error))
            {
                ui_state_.command_script_status = "脚本已入队";
            }
            else
            {
                ui_state_.command_script_status = "脚本执行失败：" + error;
            }
        }
    }
    if (!bridgeReady)
    {
        ImGui::EndDisabled();
    }
    if (!ui_state_.command_script_status.empty())
    {
        ImGui::TextWrapped("%s", ui_state_.command_script_status.c_str());
    }
}

void AppHost::refreshCommandStatusTexts(const std::vector<RuntimeBridge::CommandProgress>& commands)
{
    auto findCommand = [&commands](std::uint64_t id) -> const RuntimeBridge::CommandProgress* {
        for (const auto& command : commands)
        {
            if (command.id == id)
            {
                return &command;
            }
        }
        return nullptr;
    };

    if (ui_state_.worldgen_command_id)
    {
        if (const auto* command = findCommand(*ui_state_.worldgen_command_id))
        {
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                ui_state_.world_command_status = commandStateSummary(*command);
                break;
            case RuntimeBridge::CommandState::Succeeded:
                ui_state_.world_command_status = commandStateSummary(*command);
                if (runtime_bridge_)
                {
                    if (auto latestOpt = runtime_bridge_->lastGeneration(); latestOpt && latestOpt->success)
                    {
                        const auto& latest = *latestOpt;
                        if (latest.outputPath)
                        {
                            const auto text = latest.outputPath->string();
                            std::snprintf(ui_state_.world_load_buffer.data(), ui_state_.world_load_buffer.size(), "%s", text.c_str());
                        }
                        if (ui_state_.worldgen_use_random_seed && latest.seed.value != 0)
                        {
                            ui_state_.worldgen_seed = latest.seed.value;
                        }
                    }
                }
                pushToast("World generated", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
                ui_state_.worldgen_command_id.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                ui_state_.world_command_status = commandStateSummary(*command);
                pushToast("World generation failed", ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
                ui_state_.worldgen_command_id.reset();
                break;
            }
        }
    }

    auto updateStatus = [&](std::optional<std::uint64_t>& idHolder, std::string& statusText, auto onSuccess) {
        if (!idHolder)
        {
            return;
        }
        if (const auto* command = findCommand(*idHolder))
        {
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                statusText = commandStateSummary(*command);
                break;
            case RuntimeBridge::CommandState::Succeeded:
                statusText = commandStateSummary(*command);
                onSuccess();
                idHolder.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                statusText = commandStateSummary(*command);
                idHolder.reset();
                break;
            }
        }
    };

    updateStatus(ui_state_.world_load_command_id, ui_state_.world_load_status, [this]() {
        resetSceneForNewWorld();
        pushToast("World loaded", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });

    updateStatus(ui_state_.world_save_command_id, ui_state_.world_save_status, [this]() {
        pushToast("World saved", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });
}

void AppHost::resetSceneForNewWorld()
{
    latest_snapshot_.reset();
    ui_state_.agent_trails.clear();
    ui_state_.inspector_selection_type = UiState::InspectorSelectionType::None;
    ui_state_.inspector_selected_primary = 0;
    ui_state_.inspector_selected_secondary = 0;
    ui_state_.inspector_highlight_node.reset();
    ui_state_.inspector_follow_selection = false;
    ui_state_.scene_selected_node = 0;
    ui_state_.scene_cam_offset_x = 0.0f;
    ui_state_.scene_cam_offset_y = 0.0f;
    ui_state_.scene_cam_zoom = 1.5f;
    resetMapViewCamera();
    ui_state_.map_selected_node.reset();
}

void AppHost::resetMapViewCamera()
{
    ui_state_.map_zoom = 1.0f;
    ui_state_.map_pan_x = 0.0f;
    ui_state_.map_pan_y = 0.0f;
}

void AppHost::refreshDefaultWorldgenConfig()
{
    std::fill(ui_state_.worldgen_config_buffer.begin(), ui_state_.worldgen_config_buffer.end(), '\0');
    std::fill(ui_state_.worldgen_output_buffer.begin(), ui_state_.worldgen_output_buffer.end(), '\0');
    std::fill(ui_state_.world_load_buffer.begin(), ui_state_.world_load_buffer.end(), '\0');
    std::fill(ui_state_.world_save_buffer.begin(), ui_state_.world_save_buffer.end(), '\0');
    std::fill(ui_state_.command_script_buffer.begin(), ui_state_.command_script_buffer.end(), '\0');
    ui_state_.world_load_status.clear();
    ui_state_.world_save_status.clear();
    ui_state_.world_command_status.clear();
    ui_state_.command_script_status.clear();
    ui_state_.worldgen_command_id.reset();
    ui_state_.world_load_command_id.reset();
    ui_state_.world_save_command_id.reset();

    const std::filesystem::path defaultConfig{"data/worldgen/default.toml"};
    if (auto resolved = locateAsset(defaultConfig); !resolved.empty())
    {
        resolved.make_preferred();
        const auto text = resolved.string();
        std::snprintf(ui_state_.worldgen_config_buffer.data(), ui_state_.worldgen_config_buffer.size(), "%s", text.c_str());
    }
    else if (std::filesystem::exists(defaultConfig))
    {
        auto preferred = defaultConfig;
        preferred.make_preferred();
        const auto text = preferred.string();
        std::snprintf(ui_state_.worldgen_config_buffer.data(), ui_state_.worldgen_config_buffer.size(), "%s", text.c_str());
    }

    const std::filesystem::path defaultOutput{"data/world/generated/generated_world.json"};
    auto preferredOutput = defaultOutput;
    preferredOutput.make_preferred();
    const auto outputText = preferredOutput.string();
    std::snprintf(ui_state_.worldgen_output_buffer.data(), ui_state_.worldgen_output_buffer.size(), "%s", outputText.c_str());
    std::snprintf(ui_state_.world_load_buffer.data(), ui_state_.world_load_buffer.size(), "%s", outputText.c_str());
    std::snprintf(ui_state_.world_save_buffer.data(), ui_state_.world_save_buffer.size(), "%s", outputText.c_str());
}

} // namespace Genesis::Sandbox::Gui
