#include "sandbox/gui/AppHost.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Genesis::Sandbox::Gui
{

class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit ImGuiLogSink(std::size_t maxEntries = 256)
        : max_entries_(std::max<std::size_t>(1, maxEntries))
    {
    }

    std::vector<std::string> snapshot()
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        return {entries_.begin(), entries_.end()};
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        std::string line(formatted.data(), formatted.size());
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        {
            line.pop_back();
        }

        entries_.push_back(std::move(line));
        if (entries_.size() > max_entries_)
        {
            entries_.pop_front();
        }
    }

    void flush_() override {}

private:
    std::size_t max_entries_;
    std::deque<std::string> entries_;
};

namespace
{

void FramebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
}
} // namespace

AppHost::AppHost(AppHostConfig config)
    : config_(std::move(config))
    , clear_color_{0.07f, 0.07f, 0.10f, 1.0f}
    , runtime_bridge_(std::make_unique<RuntimeBridge>())
    , speed_multiplier_ui_(1.0)
    , show_world_view_(true)
    , show_telemetry_(true)
    , show_logs_(true)
    , log_auto_scroll_(true)
    , show_agent_overlay_(true)
    , show_agent_trails_(false)
    , agent_trail_samples_(24)
    , scene_selected_node_(0)
    , scene_cam_offset_x_(0.0f)
    , scene_cam_offset_y_(0.0f)
    , scene_cam_zoom_(1.0f)
{
    if (auto logger = spdlog::default_logger())
    {
        log_sink_ = std::make_shared<ImGuiLogSink>(512);
        logger->sinks().push_back(log_sink_);
    }
}

AppHost::~AppHost()
{
    shutdown();
}

bool AppHost::initialize()
{
    if (initialized_)
    {
        return true;
    }

    if (!initializeGlfw())
    {
        shutdown();
        return false;
    }

    if (!initializeImGui())
    {
        shutdown();
        return false;
    }

    if (runtime_bridge_)
    {
        if (!runtime_bridge_->start())
        {
            spdlog::error("Failed to start RuntimeBridge background thread");
        }
        else
        {
            speed_multiplier_ui_ = runtime_bridge_->speedMultiplier();
        }
    }

    initialized_ = true;
    spdlog::info("Sandbox GUI initialization complete ({}x{}, vsync={})",
        config_.width,
        config_.height,
        config_.vsync ? "on" : "off");

    return true;
}

void AppHost::run()
{
    if (!initialized_)
    {
        spdlog::warn("AppHost::run() invoked before initialize()");
        return;
    }

    spdlog::info("Entering Sandbox GUI main loop");
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        beginFrame();
        renderGui();
        endFrame();
    }

    spdlog::info("Leaving Sandbox GUI main loop");
    shutdown();
}

bool AppHost::initializeGlfw()
{
    if (!glfw_initialized_)
    {
        if (!glfwInit())
        {
            spdlog::error("Failed to initialize GLFW");
            return false;
        }
        glfw_initialized_ = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(
        config_.width,
        config_.height,
        config_.title.c_str(),
        nullptr,
        nullptr);

    if (!window_)
    {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        glfw_initialized_ = false;
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(config_.vsync ? 1 : 0);

    glViewport(0, 0, config_.width, config_.height);
    glfwSetFramebufferSizeCallback(window_, FramebufferSizeCallback);

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    spdlog::info("OpenGL renderer: {} (version {})", renderer ? renderer : "unknown", version ? version : "unknown");

    return true;
}

bool AppHost::initializeImGui()
{
    if (imgui_initialized_)
    {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true))
    {
        spdlog::error("Failed to initialize ImGui GLFW backend");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        spdlog::error("Failed to initialize ImGui OpenGL3 backend");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    imgui_initialized_ = true;
    return true;
}

void AppHost::shutdown()
{
    if (log_sink_)
    {
        if (auto logger = spdlog::default_logger())
        {
            auto& sinks = logger->sinks();
            sinks.erase(std::remove(sinks.begin(), sinks.end(), log_sink_), sinks.end());
        }
        log_sink_.reset();
    }

    if (runtime_bridge_)
    {
        runtime_bridge_->stop();
        latest_snapshot_.reset();
    }

    if (imgui_initialized_)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized_ = false;
    }

    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    if (glfw_initialized_)
    {
        glfwTerminate();
        glfw_initialized_ = false;
    }

    initialized_ = false;
}

void AppHost::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void AppHost::renderGui()
{
    updateRuntimeSnapshot();
    drawDockspace();
    drawMainMenuBar();
    drawWelcomePanel();
    drawWorldViewPanel();
    drawSceneViewPanel();
    drawTelemetryPanel();
    drawLogPanel();
    drawStatusBar();
}

void AppHost::endFrame()
{
    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window_, &display_w, &display_h);

    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window_);
}

void AppHost::drawDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
}

void AppHost::drawMainMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Map View", nullptr, &show_world_view_);
            ImGui::MenuItem("Scene View", nullptr, &show_scene_view_);
            ImGui::MenuItem("Telemetry", nullptr, &show_telemetry_);
            ImGui::MenuItem("Log Console", nullptr, &show_logs_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void AppHost::drawWelcomePanel()
{
    ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Genesis Sandbox GUI · RuntimeBridge");
    ImGui::Separator();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Average %.2f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::ColorEdit4("Clear Color", clear_color_.data(), ImGuiColorEditFlags_NoInputs);
    if (ImGui::Checkbox("Enable VSync", &config_.vsync))
    {
        glfwSwapInterval(config_.vsync ? 1 : 0);
    }

    ImGui::Separator();
    if (runtime_bridge_)
    {
        const bool paused = runtime_bridge_->paused();
        if (paused)
        {
            if (ImGui::Button("Resume"))
            {
                runtime_bridge_->setPaused(false);
            }
        }
        else
        {
            if (ImGui::Button("Pause"))
            {
                runtime_bridge_->setPaused(true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Step"))
        {
            runtime_bridge_->requestStep(1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step x10"))
        {
            runtime_bridge_->requestStep(10);
        }

        ImGui::Separator();
        float speed = static_cast<float>(speed_multiplier_ui_);
        if (ImGui::SliderFloat("Speed Multiplier", &speed, 0.25f, 8.0f, "%.2fx"))
        {
            speed_multiplier_ui_ = speed;
            runtime_bridge_->setSpeedMultiplier(speed_multiplier_ui_);
        }

        if (latest_snapshot_)
        {
            const auto& tick = latest_snapshot_->telemetry;
            ImGui::Text("Playback: %s", paused ? "Paused" : "Running");
            ImGui::Text("Step: %llu", static_cast<unsigned long long>(tick.step));
            ImGui::Text("Agents: %zu", tick.agents.size());
            ImGui::Text("Resources: %zu", tick.resources.size());
        }
        else
        {
            ImGui::Text("Playback: %s", paused ? "Paused" : "Running");
            ImGui::TextUnformatted("Waiting for first snapshot…");
        }
    }

    ImGui::Separator();
    ImGui::TextWrapped(
        "Focus: RuntimeBridge advances the simulation in a background thread, exposes pause/step/speed controls, and "
        "feeds the world/telemetry panels with the latest snapshot. Next milestones will add live metrics, inspector "
        "tools, and interactive world regeneration.");

    ImGui::End();
}

void AppHost::drawWorldViewPanel()
{
    if (!show_world_view_)
    {
        return;
    }

    if (!ImGui::Begin("Map View", &show_world_view_))
    {
        ImGui::End();
        return;
    }

    if (!runtime_bridge_)
    {
        ImGui::TextUnformatted("RuntimeBridge unavailable.");
        ImGui::End();
        return;
    }

    const auto& atlas = runtime_bridge_->atlas();
    const auto* agentPositionsPtr = latest_snapshot_ ? &latest_snapshot_->agentPositions : nullptr;

    ImGui::Checkbox("Agents", &show_agent_overlay_);
    ImGui::SameLine();
    ImGui::Checkbox("Trails", &show_agent_trails_);
    ImGui::SameLine();
    ImGui::Checkbox("Interpolate", &map_interpolate_);
    if (show_agent_trails_)
    {
        ImGui::SameLine();
        int trailSamples = static_cast<int>(agent_trail_samples_);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("Trail Length", &trailSamples, 4, 64))
        {
            agent_trail_samples_ = static_cast<std::size_t>(trailSamples);
            for (auto& [id, trail] : agent_trails_)
            {
                while (trail.size() > agent_trail_samples_)
                {
                    trail.pop_front();
                }
            }
        }
    }

    const ImVec4 colorMove{0.30f, 0.63f, 0.96f, 1.0f};
    const ImVec4 colorConsume{0.97f, 0.62f, 0.24f, 1.0f};
    const ImVec4 colorIdle{0.66f, 0.66f, 0.66f, 1.0f};
    const ImVec4 colorUnknown{0.82f, 0.52f, 0.90f, 1.0f};

    if (show_agent_overlay_)
    {
        ImGui::Spacing();
        auto legendEntry = [](const char* id, const char* text, const ImVec4& color) {
            ImGui::ColorButton(id, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(12.0f, 12.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted(text);
        };
        legendEntry("##legend_move", "MoveTo", colorMove);
        ImGui::SameLine();
        legendEntry("##legend_consume", "Consume", colorConsume);
        ImGui::SameLine();
        legendEntry("##legend_idle", "Idle", colorIdle);
        ImGui::SameLine();
        legendEntry("##legend_other", "Other", colorUnknown);
        ImGui::Separator();
    }
    else
    {
        ImGui::Spacing();
        ImGui::Separator();
    }

        std::unordered_map<std::uint32_t, std::vector<const genesis::telemetry::ResourceSnapshot*>> resourcesByLocation;
        if (latest_snapshot_)
        {
            for (const auto& resource : latest_snapshot_->telemetry.resources)
            {
                resourcesByLocation[resource.location.value].push_back(&resource);
            }
        }

        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax{canvasPos.x + std::max(120.0f, canvasSize.x), canvasPos.y + std::max(120.0f, canvasSize.y)};

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
    drawList->AddRectFilled(canvasPos, canvasMax, bgColor);
    drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));

    if (!atlas.nodes.empty())
    {
        const float padding = 28.0f;
        const float width = std::max(atlas.extent.x, 1.0f);
        const float height = std::max(atlas.extent.y, 1.0f);
        const float scaleX = (canvasMax.x - canvasPos.x - padding * 2.0f) / width;
        const float scaleY = (canvasMax.y - canvasPos.y - padding * 2.0f) / height;

        auto toScreen = [&](const RuntimeBridge::Vector2& pos) {
            return ImVec2(
                canvasPos.x + padding + pos.x * scaleX,
                canvasPos.y + padding + pos.y * scaleY);
        };

        std::unordered_map<std::uint32_t, ImVec2> nodePositions;
        nodePositions.reserve(atlas.nodes.size());
        for (const auto& node : atlas.nodes)
        {
            nodePositions.emplace(node.id.value, toScreen(node.position));
        }

        const ImU32 edgeColor = ImGui::GetColorU32(ImVec4(0.45f, 0.47f, 0.58f, 1.0f));
        for (const auto& edge : atlas.edges)
        {
            auto fromIt = nodePositions.find(edge.from.value);
            auto toIt = nodePositions.find(edge.to.value);
            if (fromIt == nodePositions.end() || toIt == nodePositions.end())
            {
                continue;
            }
            drawList->AddLine(fromIt->second, toIt->second, edgeColor, edge.bidirectional ? 1.6f : 1.2f);
        }

        for (const auto& node : atlas.nodes)
        {
            auto it = nodePositions.find(node.id.value);
            if (it == nodePositions.end())
            {
                continue;
            }

            const float radius = 12.0f;
            ImU32 fillColor = ImGui::GetColorU32(ImVec4(0.56f, 0.63f, 0.87f, 0.90f));
            switch (node.kind)
            {
            case genesis::world::LocationKind::Region:
                fillColor = ImGui::GetColorU32(ImVec4(0.36f, 0.62f, 0.51f, 0.90f));
                break;
            case genesis::world::LocationKind::Building:
                fillColor = ImGui::GetColorU32(ImVec4(0.70f, 0.54f, 0.34f, 0.90f));
                break;
            case genesis::world::LocationKind::Room:
                fillColor = ImGui::GetColorU32(ImVec4(0.83f, 0.68f, 0.43f, 0.90f));
                break;
            case genesis::world::LocationKind::Point:
                fillColor = ImGui::GetColorU32(ImVec4(0.56f, 0.63f, 0.87f, 0.90f));
                break;
            }

            drawList->AddCircleFilled(it->second, radius, fillColor, 20);
            drawList->AddCircle(it->second, radius, ImGui::GetColorU32(ImGuiCol_Border), 20, 1.5f);

            const ImVec2 labelPos{it->second.x + radius + 6.0f, it->second.y - ImGui::GetTextLineHeight() * 0.5f};
            drawList->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), node.name.c_str());
        }

        for (const auto& spawn : atlas.spawns)
        {
            auto nodePosIt = nodePositions.find(spawn.resource.location.value);
            ImVec2 markerBase = nodePosIt != nodePositions.end() ? nodePosIt->second : toScreen(spawn.position);
            markerBase = ImVec2(std::floor(markerBase.x) + 0.5f, std::floor(markerBase.y) + 0.5f);
            markerBase.y += 22.0f;
            const ImVec2 a{markerBase.x - 6.0f, markerBase.y};
            const ImVec2 b{markerBase.x + 6.0f, markerBase.y};
            const ImVec2 c{markerBase.x, markerBase.y + 10.0f};
            const ImU32 markerColor = ImGui::GetColorU32(ImVec4(0.92f, 0.66f, 0.27f, 1.0f));
            drawList->AddTriangleFilled(a, b, c, markerColor);
            drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.2f);
        }

        if (latest_snapshot_)
        {
            auto colorForResource = [](genesis::world::ResourceType type) -> ImU32 {
                switch (type)
                {
                case genesis::world::ResourceType::Food:
                    return ImGui::GetColorU32(ImVec4(0.95f, 0.61f, 0.27f, 1.0f));
                case genesis::world::ResourceType::Drink:
                    return ImGui::GetColorU32(ImVec4(0.27f, 0.61f, 0.95f, 1.0f));
                case genesis::world::ResourceType::Social:
                    return ImGui::GetColorU32(ImVec4(0.48f, 0.76f, 0.47f, 1.0f));
                default:
                    return ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                }
            };

            const ImU32 barBackground = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.18f, 0.9f));
            const ImU32 barBorder = ImGui::GetColorU32(ImGuiCol_Border);

            for (const auto& [locationId, resources] : resourcesByLocation)
            {
                auto posIt = nodePositions.find(locationId);
                if (posIt == nodePositions.end())
                {
                    continue;
                }

                const ImVec2 basePos = ImVec2(std::floor(posIt->second.x) + 0.5f, std::floor(posIt->second.y) + 0.5f);
                float offsetY = 20.0f;

                for (const auto* resource : resources)
                {
                    const float capacity = static_cast<float>(resource->capacity);
                    const float current = static_cast<float>(resource->current);
                    const float ratio = capacity > 0.0f ? std::clamp(current / capacity, 0.0f, 1.0f) : 0.0f;

                    const ImVec2 barMin{std::floor(basePos.x - 28.0f) + 0.5f, std::floor(basePos.y + offsetY) + 0.5f};
                    const ImVec2 barMax{std::floor(basePos.x + 28.0f) + 0.5f, std::floor(barMin.y + 7.5f) + 0.5f};
                    drawList->AddRectFilled(barMin, barMax, barBackground, 3.0f);
                    const ImVec2 fillMax{barMin.x + (barMax.x - barMin.x) * ratio, barMax.y};
                    drawList->AddRectFilled(barMin, fillMax, colorForResource(resource->type), 3.0f);
                    drawList->AddRect(barMin, barMax, barBorder, 3.0f);

                    char buffer[48];
                    std::snprintf(buffer, sizeof(buffer), "%-12s %2u/%2u",
                        resource->name.c_str(), resource->current, resource->capacity);
                    const ImVec2 textPos{std::floor(barMin.x) + 0.5f, std::floor(barMax.y + 1.0f) + 0.5f};
                    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, ImGui::GetColorU32(ImGuiCol_Text), buffer);

                    offsetY += 22.0f;
                }
            }
        }

        if (show_agent_overlay_ && latest_snapshot_)
        {
            // Build movement progress map for interpolation (optional)
            std::unordered_map<std::uint32_t, std::tuple<RuntimeBridge::Vector2, RuntimeBridge::Vector2, float>> progress;
            if (map_interpolate_ && !latest_snapshot_->telemetry.movementProgress.empty())
            {
                for (const auto& mp : latest_snapshot_->telemetry.movementProgress)
                {
                    auto fromPos = atlas.nodePosition(mp.from).value_or(RuntimeBridge::Vector2{});
                    auto toPos = atlas.nodePosition(mp.to).value_or(fromPos);
                    progress.emplace(mp.entityId, std::make_tuple(fromPos, toPos, std::clamp(mp.t01, 0.0f, 1.0f)));
                }
            }
            enum class AgentState
            {
                Idle,
                Move,
                Consume,
                Other
            };

            std::unordered_map<std::uint32_t, AgentState> agentStates;
            agentStates.reserve(latest_snapshot_->telemetry.actions.size());
            for (const auto& action : latest_snapshot_->telemetry.actions)
            {
                AgentState state = AgentState::Other;
                if (action.currentAction == "MoveTo")
                {
                    state = AgentState::Move;
                }
                else if (action.currentAction == "ConsumeResource")
                {
                    state = AgentState::Consume;
                }
                else if (action.currentAction == "Idle")
                {
                    state = AgentState::Idle;
                }
                agentStates[action.entityId] = state;
            }

            const ImU32 moveColor = ImGui::GetColorU32(colorMove);
            const ImU32 consumeColor = ImGui::GetColorU32(colorConsume);
            const ImU32 idleColor = ImGui::GetColorU32(colorIdle);
            const ImU32 otherColor = ImGui::GetColorU32(colorUnknown);
            const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Text);

            auto colorForState = [&](AgentState state) -> ImU32 {
                switch (state)
                {
                case AgentState::Move:
                    return moveColor;
                case AgentState::Consume:
                    return consumeColor;
                case AgentState::Idle:
                    return idleColor;
                case AgentState::Other:
                default:
                    return otherColor;
                }
            };

            for (std::size_t i = 0; i < latest_snapshot_->telemetry.agents.size(); ++i)
            {
                const auto& agent = latest_snapshot_->telemetry.agents[i];
                RuntimeBridge::Vector2 agentPos = atlas.nodePosition(agent.location).value_or(RuntimeBridge::Vector2{});
                if (agentPositionsPtr && i < agentPositionsPtr->size())
                {
                    agentPos = (*agentPositionsPtr)[i];
                }
                if (map_interpolate_)
                {
                    if (auto itp = progress.find(agent.entityId); itp != progress.end())
                    {
                        const auto& [fromP, toP, t] = itp->second;
                        agentPos.x = fromP.x + (toP.x - fromP.x) * t;
                        agentPos.y = fromP.y + (toP.y - fromP.y) * t;
                    }
                }

                ImVec2 screenPos = toScreen(agentPos);
                screenPos = ImVec2(std::floor(screenPos.x) + 0.5f, std::floor(screenPos.y) + 0.5f);

                auto posIt = nodePositions.find(agent.location.value);
                const auto state = agentStates.contains(agent.entityId) ? agentStates[agent.entityId] : AgentState::Idle;
                const ImU32 fillColor = colorForState(state);
                drawList->AddCircleFilled(screenPos, 7.0f, fillColor, 16);
                drawList->AddCircle(screenPos, 7.0f, borderColor, 16, 1.4f);

                // Agent name label
                if (!agent.name.empty())
                {
                    const ImVec2 namePos{screenPos.x + 9.0f, screenPos.y - ImGui::GetTextLineHeight() * 0.5f};
                    drawList->AddText(namePos, ImGui::GetColorU32(ImGuiCol_Text), agent.name.c_str());
                }

                if (show_agent_trails_)
                {
                    auto trailIt = agent_trails_.find(agent.entityId);
                    if (trailIt != agent_trails_.end() && trailIt->second.size() > 1)
                    {
                        const auto& trail = trailIt->second;
                        ImVec2 previous = toScreen(trail.front());
                        previous = ImVec2(std::floor(previous.x) + 0.5f, std::floor(previous.y) + 0.5f);
                        for (std::size_t i = 1; i < trail.size(); ++i)
                        {
                            ImVec2 current = toScreen(trail[i]);
                            current = ImVec2(std::floor(current.x) + 0.5f, std::floor(current.y) + 0.5f);
                            drawList->AddLine(previous, current, fillColor, 2.0f);
                            previous = current;
                        }
                    }
                }
            }
        }
    }
    else
    {
        ImGui::TextUnformatted("World data not available.");
    }

    ImGui::Dummy(ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y));
    ImGui::End();
}

void AppHost::drawSceneViewPanel()
{
    if (!show_scene_view_)
    {
        return;
    }

    if (!ImGui::Begin("Scene View", &show_scene_view_))
    {
        ImGui::End();
        return;
    }

    if (!runtime_bridge_)
    {
        ImGui::TextUnformatted("RuntimeBridge unavailable.");
        ImGui::End();
        return;
    }

    const auto& atlas = runtime_bridge_->atlas();

    // Node selector
    if (scene_selected_node_ == 0 && !atlas.nodes.empty())
    {
        scene_selected_node_ = atlas.nodes.front().id.value;
    }

    if (ImGui::BeginCombo("Node", [this, &atlas]() {
            for (const auto& n : atlas.nodes)
            {
                if (n.id.value == scene_selected_node_)
                    return n.name.c_str();
            }
            return "(none)";
        }()))
    {
        for (const auto& n : atlas.nodes)
        {
            const bool selected = (n.id.value == scene_selected_node_);
            if (ImGui::Selectable(n.name.c_str(), selected))
            {
                scene_selected_node_ = n.id.value;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Canvas setup
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax{canvasPos.x + std::max(120.0f, canvasSize.x), canvasPos.y + std::max(120.0f, canvasSize.y)};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
    drawList->AddRectFilled(canvasPos, canvasMax, bgColor);
    drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));

    // Camera controls (mouse drag/scroll)
    ImGui::InvisibleButton("SceneCanvas", ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y), ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && io.MouseWheel != 0.0f)
    {
        const float zoomStep = 1.0f + (io.MouseWheel > 0.0f ? 0.1f : -0.1f);
        scene_cam_zoom_ = std::clamp(scene_cam_zoom_ * zoomStep, 0.5f, 3.0f);
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        scene_cam_offset_x_ += delta.x;
        scene_cam_offset_y_ += delta.y;
    }

    // Gather local points: resource spawns and anchors for selected node
    std::vector<ImVec2> resourcePts;
    for (const auto& sp : atlas.spawns)
    {
        if (sp.resource.location.value != scene_selected_node_)
            continue;
        if (sp.resource.local_coord.has_value())
        {
            resourcePts.emplace_back(static_cast<float>(sp.resource.local_coord->first), static_cast<float>(sp.resource.local_coord->second));
        }
    }
    std::vector<std::pair<ImVec2, std::string>> anchorPts; // pos, label
    for (const auto& e : atlas.edges)
    {
        if (e.from.value == scene_selected_node_ && e.anchorFrom.has_value())
        {
            anchorPts.emplace_back(ImVec2(e.anchorFrom->x, e.anchorFrom->y), std::string("to ") + std::to_string(e.to.value));
        }
        if (e.to.value == scene_selected_node_ && e.anchorTo.has_value())
        {
            anchorPts.emplace_back(ImVec2(e.anchorTo->x, e.anchorTo->y), std::string("to ") + std::to_string(e.from.value));
        }
    }

    // Determine grid bounds (prefer tilemap meta)
    int minX = 0, minY = 0, maxX = 9, maxY = 9; // default 10x10
    for (const auto& tm : atlas.tilemaps)
    {
        if (tm.nodeId == scene_selected_node_ && tm.width > 0 && tm.height > 0)
        {
            minX = 0; minY = 0; maxX = tm.width - 1; maxY = tm.height - 1;
            break;
        }
    }
    auto incorporate = [&](const ImVec2& p) {
        minX = std::min(minX, static_cast<int>(std::floor(p.x)));
        minY = std::min(minY, static_cast<int>(std::floor(p.y)));
        maxX = std::max(maxX, static_cast<int>(std::ceil(p.x)));
        maxY = std::max(maxY, static_cast<int>(std::ceil(p.y)));
    };
    // If no tilemap meta, extend from points
    bool hasMeta = false;
    for (const auto& tm : atlas.tilemaps)
    {
        if (tm.nodeId == scene_selected_node_ && tm.width > 0 && tm.height > 0) { hasMeta = true; break; }
    }
    if (!hasMeta)
    {
        for (const auto& p : resourcePts) incorporate(p);
        for (const auto& ap : anchorPts) incorporate(ap.first);
    }

    const float cellPx = 24.0f * scene_cam_zoom_;
    auto toScreen = [&](float gx, float gy) {
        const float sx = canvasPos.x + scene_cam_offset_x_ + (gx - minX) * cellPx + 8.0f;
        const float sy = canvasPos.y + scene_cam_offset_y_ + (gy - minY) * cellPx + 8.0f;
        return ImVec2(std::floor(sx) + 0.5f, std::floor(sy) + 0.5f);
    };

    // Draw grid
    const int cols = (maxX - minX + 1);
    const int rows = (maxY - minY + 1);
    const ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
    for (int x = 0; x <= cols; ++x)
    {
        const ImVec2 a = toScreen(static_cast<float>(minX + x), static_cast<float>(minY));
        const ImVec2 b = toScreen(static_cast<float>(minX + x), static_cast<float>(maxY + 1));
        drawList->AddLine(a, b, gridColor, 1.0f);
    }
    for (int y = 0; y <= rows; ++y)
    {
        const ImVec2 a = toScreen(static_cast<float>(minX), static_cast<float>(minY + y));
        const ImVec2 b = toScreen(static_cast<float>(maxX + 1), static_cast<float>(minY + y));
        drawList->AddLine(a, b, gridColor, 1.0f);
    }

    // Draw resources
    const ImU32 foodColor = ImGui::GetColorU32(ImVec4(0.93f, 0.67f, 0.27f, 1.0f));
    for (const auto& p : resourcePts)
    {
        const ImVec2 center = toScreen(p.x + 0.5f, p.y + 0.5f);
        const float r = std::max(3.0f, cellPx * 0.25f);
        drawList->AddCircleFilled(center, r, foodColor, 12);
        drawList->AddCircle(center, r, ImGui::GetColorU32(ImGuiCol_Border), 12, 1.2f);
    }

    // Draw anchors
    const ImU32 anchorColor = ImGui::GetColorU32(ImVec4(0.38f, 0.74f, 0.88f, 1.0f));
    for (const auto& ap : anchorPts)
    {
        const ImVec2 base = toScreen(ap.first.x + 0.5f, ap.first.y + 0.5f);
        const float w = std::max(4.0f, cellPx * 0.2f);
        const ImVec2 a{base.x - w, base.y};
        const ImVec2 b{base.x + w, base.y};
        const ImVec2 c{base.x, base.y + w * 1.6f};
        drawList->AddTriangleFilled(a, b, c, anchorColor);
        drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
        const ImVec2 labelPos{base.x + w + 4.0f, base.y - ImGui::GetTextLineHeight() * 0.5f};
        drawList->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), ap.second.c_str());
    }

    ImGui::Dummy(ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y));
    ImGui::End();
}

void AppHost::drawTelemetryPanel()
{
    if (!show_telemetry_)
    {
        return;
    }

    if (!ImGui::Begin("Telemetry", &show_telemetry_))
    {
        ImGui::End();
        return;
    }

    if (!latest_snapshot_)
    {
        ImGui::TextUnformatted("Waiting for telemetry…");
        ImGui::End();
        return;
    }

    const auto& tick = latest_snapshot_->telemetry;
    ImGui::Text("Step: %llu", static_cast<unsigned long long>(tick.step));
    ImGui::Text("Agents: %zu", tick.agents.size());
    ImGui::Text("Actions: %zu", tick.actions.size());
    ImGui::Text("Needs: %zu", tick.needs.size());

    if (!tick.needs.empty())
    {
        float hungerSum = 0.0f;
        std::uint32_t critical = 0;
        for (const auto& need : tick.needs)
        {
            hungerSum += need.value;
            if (need.critical)
            {
                ++critical;
            }
        }
        const float average = hungerSum / static_cast<float>(tick.needs.size());
        ImGui::Separator();
        ImGui::Text("Avg Need: %.2f", average);
        ImGui::Text("Critical Needs: %u", critical);
    }

    if (!tick.resources.empty())
    {
        ImGui::Separator();
        for (const auto& resource : tick.resources)
        {
            ImGui::Text("#%u %s (%u/%u)",
                resource.location.value,
                resource.name.c_str(),
                resource.current,
                resource.capacity);
        }
    }

    ImGui::End();
}

void AppHost::drawLogPanel()
{
    if (!show_logs_)
    {
        return;
    }

    if (!ImGui::Begin("Log Console", &show_logs_))
    {
        ImGui::End();
        return;
    }

    if (!log_sink_)
    {
        ImGui::TextUnformatted("Log sink unavailable.");
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Auto-scroll", &log_auto_scroll_);
    ImGui::Separator();

    const auto lines = log_sink_->snapshot();

    ImGui::BeginChild("LogConsole.ScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    const bool stickToBottom = log_auto_scroll_ &&
        (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f || log_last_line_count_ == 0);

    for (const auto& line : lines)
    {
        ImGui::TextUnformatted(line.c_str());
    }

    if (stickToBottom && !lines.empty())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    log_last_line_count_ = lines.size();

    ImGui::EndChild();
    ImGui::End();
}

void AppHost::drawStatusBar()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Status Bar", nullptr, flags))
    {
        if (latest_snapshot_)
        {
            const auto& tick = latest_snapshot_->telemetry;
            const double totalSeconds = static_cast<double>(tick.step) * static_cast<double>(tick.stepSeconds);
            const std::uint64_t secs = static_cast<std::uint64_t>(totalSeconds);
            const std::uint64_t h = secs / 3600ULL;
            const std::uint64_t m = (secs % 3600ULL) / 60ULL;
            const std::uint64_t s = secs % 60ULL;
            ImGui::Text("Time %02llu:%02llu:%02llu | Step %llu | Agents %zu | Resources %zu | Actions %zu",
                static_cast<unsigned long long>(h),
                static_cast<unsigned long long>(m),
                static_cast<unsigned long long>(s),
                static_cast<unsigned long long>(tick.step),
                tick.agents.size(),
                tick.resources.size(),
                tick.actions.size());
        }
        else
        {
            ImGui::TextUnformatted("Simulation warming up…");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void AppHost::updateRuntimeSnapshot()
{
    if (!runtime_bridge_)
    {
        latest_snapshot_.reset();
        agent_trails_.clear();
        return;
    }

    if (auto snapshot = runtime_bridge_->latestSnapshot())
    {
        latest_snapshot_ = std::move(snapshot);
        if (latest_snapshot_)
        {
            updateAgentTrails(*latest_snapshot_);
        }
    }
}

void AppHost::updateAgentTrails(const RuntimeBridge::Snapshot& snapshot)
{
    if (!runtime_bridge_)
    {
        agent_trails_.clear();
        return;
    }

    const auto& atlas = runtime_bridge_->atlas();
    const auto* agentPositionsPtr = &snapshot.agentPositions;
    std::unordered_set<std::uint32_t> observed;
    observed.reserve(snapshot.telemetry.agents.size());

    for (std::size_t i = 0; i < snapshot.telemetry.agents.size(); ++i)
    {
        const auto& agent = snapshot.telemetry.agents[i];
        observed.insert(agent.entityId);
        RuntimeBridge::Vector2 position = atlas.nodePosition(agent.location).value_or(RuntimeBridge::Vector2{});
        if (agentPositionsPtr && i < agentPositionsPtr->size())
        {
            position = (*agentPositionsPtr)[i];
        }

        auto& trail = agent_trails_[agent.entityId];
        if (trail.empty() || std::fabs(trail.back().x - position.x) > std::numeric_limits<float>::epsilon() ||
            std::fabs(trail.back().y - position.y) > std::numeric_limits<float>::epsilon())
        {
            trail.push_back(position);
            while (trail.size() > agent_trail_samples_)
            {
                trail.pop_front();
            }
        }
    }

    for (auto it = agent_trails_.begin(); it != agent_trails_.end();)
    {
        if (!observed.contains(it->first))
        {
            it = agent_trails_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace Genesis::Sandbox::Gui
