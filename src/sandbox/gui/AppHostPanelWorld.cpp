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

    ImGui::InputText("配置路径", worldgen_config_buffer_.data(), worldgen_config_buffer_.size());
    ImGui::InputText("输出路径", worldgen_output_buffer_.data(), worldgen_output_buffer_.size());

    if (ImGui::Checkbox("随机种子", &worldgen_use_random_seed_))
    {
        if (worldgen_use_random_seed_)
        {
            worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
        }
    }

    if (worldgen_use_random_seed_)
    {
        ImGui::SameLine();
        if (ImGui::Button("刷新种子"))
        {
            worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
        }
        ImGui::SameLine();
        ImGui::Text("Seed %llu", static_cast<unsigned long long>(worldgen_seed_));
    }
    else
    {
        ImGui::InputScalar("固定种子", ImGuiDataType_U64, &worldgen_seed_);
    }

    const std::string configInput(worldgen_config_buffer_.data());
    const std::string outputInput(worldgen_output_buffer_.data());
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
            if (!worldgen_use_random_seed_)
            {
                command["seed"] = worldgen_seed_;
            }
            if (!outputInput.empty())
            {
                command["outputPath"] = outputInput;
            }

            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                worldgen_command_id_ = id;
                world_command_status_ = "命令已提交 #" + std::to_string(*id);
            }
            else
            {
                world_command_status_ = "提交失败：" + error;
            }
        }
    }
    if (!bridgeReady || !hasConfig)
    {
        ImGui::EndDisabled();
    }
    if (!world_command_status_.empty())
    {
        ImGui::TextWrapped("%s", world_command_status_.c_str());
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
    ImGui::InputText("加载路径", world_load_buffer_.data(), world_load_buffer_.size());
    const std::string loadInput(world_load_buffer_.data());
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
                world_load_command_id_ = id;
                world_load_status_ = "加载任务已提交 #" + std::to_string(*id);
            }
            else
            {
                world_load_status_ = "加载失败：" + error;
            }
        }
    }
    if (!bridgeReady || loadInput.empty())
    {
        ImGui::EndDisabled();
    }
    if (!world_load_status_.empty())
    {
        ImGui::TextWrapped("%s", world_load_status_.c_str());
    }

    ImGui::InputText("保存路径", world_save_buffer_.data(), world_save_buffer_.size());
    const std::string saveInput(world_save_buffer_.data());
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
                world_save_command_id_ = id;
                world_save_status_ = "保存任务已提交 #" + std::to_string(*id);
            }
            else
            {
                world_save_status_ = "保存失败：" + error;
            }
        }
    }
    if (!bridgeReady || saveInput.empty())
    {
        ImGui::EndDisabled();
    }
    if (!world_save_status_.empty())
    {
        ImGui::TextWrapped("%s", world_save_status_.c_str());
    }

    ImGui::Separator();
    ImGui::InputText("脚本路径", command_script_buffer_.data(), command_script_buffer_.size());
    const std::string scriptPath(command_script_buffer_.data());
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
            command_script_status_ = "请先填写脚本路径";
        }
        else if (bridgeReady)
        {
            std::string error;
            if (runtime_bridge_->enqueueCommandScript(std::filesystem::path(scriptPath), "script", error))
            {
                command_script_status_ = "脚本已入队";
            }
            else
            {
                command_script_status_ = "脚本执行失败：" + error;
            }
        }
    }
    if (!bridgeReady)
    {
        ImGui::EndDisabled();
    }
    if (!command_script_status_.empty())
    {
        ImGui::TextWrapped("%s", command_script_status_.c_str());
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

    if (worldgen_command_id_)
    {
        if (const auto* command = findCommand(*worldgen_command_id_))
        {
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                world_command_status_ = commandStateSummary(*command);
                break;
            case RuntimeBridge::CommandState::Succeeded:
                world_command_status_ = commandStateSummary(*command);
                if (runtime_bridge_)
                {
                    if (auto latestOpt = runtime_bridge_->lastGeneration(); latestOpt && latestOpt->success)
                    {
                        const auto& latest = *latestOpt;
                        if (latest.outputPath)
                        {
                            const auto text = latest.outputPath->string();
                            std::snprintf(world_load_buffer_.data(), world_load_buffer_.size(), "%s", text.c_str());
                        }
                        if (worldgen_use_random_seed_ && latest.seed.value != 0)
                        {
                            worldgen_seed_ = latest.seed.value;
                        }
                    }
                }
                pushToast("World generated", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
                worldgen_command_id_.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                world_command_status_ = commandStateSummary(*command);
                pushToast("World generation failed", ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
                worldgen_command_id_.reset();
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

    updateStatus(world_load_command_id_, world_load_status_, [this]() {
        resetSceneForNewWorld();
        pushToast("World loaded", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });

    updateStatus(world_save_command_id_, world_save_status_, [this]() {
        pushToast("World saved", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });
}

void AppHost::resetSceneForNewWorld()
{
    latest_snapshot_.reset();
    agent_trails_.clear();
    inspector_selection_type_ = InspectorSelectionType::None;
    inspector_selected_primary_ = 0;
    inspector_selected_secondary_ = 0;
    inspector_highlight_node_.reset();
    inspector_follow_selection_ = false;
    scene_selected_node_ = 0;
    scene_cam_offset_x_ = 0.0f;
    scene_cam_offset_y_ = 0.0f;
    scene_cam_zoom_ = 1.5f;
    resetMapViewCamera();
    map_selected_node_.reset();
}

void AppHost::resetMapViewCamera()
{
    map_zoom_ = 1.0f;
    map_pan_x_ = 0.0f;
    map_pan_y_ = 0.0f;
}

void AppHost::refreshDefaultWorldgenConfig()
{
    std::fill(worldgen_config_buffer_.begin(), worldgen_config_buffer_.end(), '\0');
    std::fill(worldgen_output_buffer_.begin(), worldgen_output_buffer_.end(), '\0');
    std::fill(world_load_buffer_.begin(), world_load_buffer_.end(), '\0');
    std::fill(world_save_buffer_.begin(), world_save_buffer_.end(), '\0');
    std::fill(command_script_buffer_.begin(), command_script_buffer_.end(), '\0');
    world_load_status_.clear();
    world_save_status_.clear();
    world_command_status_.clear();
    command_script_status_.clear();
    worldgen_command_id_.reset();
    world_load_command_id_.reset();
    world_save_command_id_.reset();

    const std::filesystem::path defaultConfig{"data/worldgen/default.toml"};
    if (auto resolved = locateAsset(defaultConfig); !resolved.empty())
    {
        resolved.make_preferred();
        const auto text = resolved.string();
        std::snprintf(worldgen_config_buffer_.data(), worldgen_config_buffer_.size(), "%s", text.c_str());
    }
    else if (std::filesystem::exists(defaultConfig))
    {
        auto preferred = defaultConfig;
        preferred.make_preferred();
        const auto text = preferred.string();
        std::snprintf(worldgen_config_buffer_.data(), worldgen_config_buffer_.size(), "%s", text.c_str());
    }

    const std::filesystem::path defaultOutput{"data/world/generated/generated_world.json"};
    auto preferredOutput = defaultOutput;
    preferredOutput.make_preferred();
    const auto outputText = preferredOutput.string();
    std::snprintf(worldgen_output_buffer_.data(), worldgen_output_buffer_.size(), "%s", outputText.c_str());
    std::snprintf(world_load_buffer_.data(), world_load_buffer_.size(), "%s", outputText.c_str());
    std::snprintf(world_save_buffer_.data(), world_save_buffer_.size(), "%s", outputText.c_str());
}

} // namespace Genesis::Sandbox::Gui
