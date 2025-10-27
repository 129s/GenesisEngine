#include "sandbox/gui/controller/WorldCommandController.hpp"

#include <algorithm>
#include <cstdio>

#include "../CommandUiHelpers.hpp"

namespace genesis::sandbox::gui
{

WorldCommandController::WorldCommandController(UiState& state,
                                               RuntimeProviderFn runtimeProvider,
                                               ToastFn toastFn,
                                               ResetSceneFn resetSceneFn)
    : state_(state)
    , runtime_provider_(std::move(runtimeProvider))
    , toast_fn_(std::move(toastFn))
    , reset_scene_fn_(std::move(resetSceneFn))
{
}

void WorldCommandController::refreshStatuses(const std::vector<RuntimeBridge::CommandProgress>& commands)
{
    auto findCommand = [&](std::uint64_t id) -> const RuntimeBridge::CommandProgress* {
        auto it = std::find_if(commands.begin(), commands.end(), [id](const RuntimeBridge::CommandProgress& command) {
            return command.id == id;
        });
        return it != commands.end() ? &(*it) : nullptr;
    };

    if (state_.worldgen_command_id)
    {
        if (const auto* command = findCommand(*state_.worldgen_command_id))
        {
            state_.world_command_status = commandStateSummary(*command);
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                break;
            case RuntimeBridge::CommandState::Succeeded:
            {
                if (auto* runtime = runtime_provider_())
                {
                    if (auto latestOpt = runtime->lastGeneration(); latestOpt && latestOpt->success)
                    {
                        const auto& latest = *latestOpt;
                        if (latest.outputPath)
                        {
                            const auto text = latest.outputPath->string();
                            std::snprintf(state_.world_load_buffer.data(), state_.world_load_buffer.size(), "%s", text.c_str());
                        }
                        if (state_.worldgen_use_random_seed && latest.seed.value != 0)
                        {
                            state_.worldgen_seed = latest.seed.value;
                        }
                    }
                }
                toast_fn_("World generated", ImVec4(0.62f, 0.84f, 0.58f, 1.0f), 3.0);
                state_.worldgen_command_id.reset();
                break;
            }
            case RuntimeBridge::CommandState::Failed:
                toast_fn_("World generation failed", ImVec4(0.95f, 0.45f, 0.45f, 1.0f), 3.0);
                state_.worldgen_command_id.reset();
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
            statusText = commandStateSummary(*command);
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                break;
            case RuntimeBridge::CommandState::Succeeded:
                onSuccess();
                idHolder.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                idHolder.reset();
                break;
            }
        }
    };

    updateStatus(state_.world_load_command_id, state_.world_load_status, [&]() {
        reset_scene_fn_();
        toast_fn_("World loaded", ImVec4(0.62f, 0.84f, 0.58f, 1.0f), 3.0);
    });

    updateStatus(state_.world_save_command_id, state_.world_save_status, [&]() {
        toast_fn_("World saved", ImVec4(0.62f, 0.84f, 0.58f, 1.0f), 3.0);
    });
}

} // namespace genesis::sandbox::gui
