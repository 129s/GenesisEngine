#include "sandbox/gui/AppHost.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
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

        std::lock_guard<std::mutex> lock(this->mutex_);
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
    drawTelemetryPanel();
    drawLogPanel();
    drawStatusBar();

    if (show_demo_window_)
    {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }
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
            ImGui::MenuItem("Dear ImGui Demo", nullptr, &show_demo_window_);
            ImGui::MenuItem("World View", nullptr, &show_world_view_);
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
    ImGui::Checkbox("Show Dear ImGui Demo", &show_demo_window_);
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

    if (!ImGui::Begin("World View", &show_world_view_))
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
            markerBase.y += 22.0f;
            const ImVec2 a{markerBase.x - 6.0f, markerBase.y};
            const ImVec2 b{markerBase.x + 6.0f, markerBase.y};
            const ImVec2 c{markerBase.x, markerBase.y + 10.0f};
            const ImU32 markerColor = ImGui::GetColorU32(ImVec4(0.92f, 0.66f, 0.27f, 1.0f));
            drawList->AddTriangleFilled(a, b, c, markerColor);
            drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.2f);
        }
    }
    else
    {
        ImGui::TextUnformatted("World data not available.");
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
            ImGui::Text("Step %llu | Agents %zu | Resources %zu | Actions %zu",
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
        return;
    }

    if (auto snapshot = runtime_bridge_->latestSnapshot())
    {
        latest_snapshot_ = std::move(snapshot);
    }
}

} // namespace Genesis::Sandbox::Gui
