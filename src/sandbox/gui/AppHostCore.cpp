#include "sandbox/gui/AppHost.hpp"

#include <spdlog/spdlog.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_freetype.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "FilesystemHelpers.hpp"
#include "ImGuiLogSink.hpp"

namespace Genesis::Sandbox::Gui
{
using json = nlohmann::json;

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
    , show_inspector_(true)
    , show_world_view_(true)
    , show_scene_view_(true)
    , show_telemetry_(true)
    , show_logs_(true)
    , show_worldgen_panel_(true)
    , log_auto_scroll_(true)
    , show_agent_overlay_(true)
    , show_agent_trails_(false)
    , agent_trail_samples_(24)
    , inspector_selection_type_(InspectorSelectionType::None)
    , inspector_selected_primary_(0)
    , inspector_selected_secondary_(0)
    , inspector_follow_selection_(false)
    , scene_selected_node_(0)
    , scene_cam_offset_x_(0.0f)
    , scene_cam_offset_y_(0.0f)
    , scene_cam_zoom_(1.5f)
    , scene_show_grid_(true)
    , scene_show_anchors_(true)
    , scene_show_resources_(true)
{
    refreshDefaultWorldgenConfig();
    worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
    std::fill(inspector_search_buffer_.begin(), inspector_search_buffer_.end(), '\0');

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
        handleShortcuts();
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigDockingWithShift = false;

    io.Fonts->AddFontDefault();

    bool hasCjkFont = false;
    if (auto cjkFont = locateCjkFont(); !cjkFont.empty())
    {
        ImFontConfig config;
        config.MergeMode = true;
        config.FontDataOwnedByAtlas = false;
        config.OversampleH = 2;
        config.OversampleV = 2;
        ImWchar ranges[] = {0x4e00, 0x9fff, 0};
        if (io.Fonts->AddFontFromFileTTF(cjkFont.string().c_str(), 17.0f, &config, ranges))
        {
            hasCjkFont = true;
        }
    }

    if (!hasCjkFont)
    {
        spdlog::warn("CJK font not found, Chinese text rendering may be degraded");
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.21f, 0.29f, 0.46f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.36f, 0.55f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.40f, 0.63f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.32f, 0.40f, 0.56f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.46f, 0.66f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.52f, 0.76f, 1.0f);

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true))
    {
        spdlog::error("ImGui_ImplGlfw_InitForOpenGL failed");
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 150"))
    {
        spdlog::error("ImGui_ImplOpenGL3_Init failed");
        return false;
    }

    imgui_initialized_ = true;
    return true;
}

void AppHost::shutdown()
{
    if (runtime_bridge_)
    {
        runtime_bridge_->stop();
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
    drawWorldGenerationPanel();
    drawInspectorPanel();
    drawWorldViewPanel();
    drawSceneViewPanel();
    drawTelemetryPanel();
    drawLogPanel();
    drawControlToolbar();
    drawToasts();
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
            if (inspector_selection_type_ == InspectorSelectionType::Agent)
            {
                const auto trackedId = inspector_selected_primary_;
                bool foundAgent = false;
                for (const auto& agent : latest_snapshot_->telemetry.agents)
                {
                    if (agent.entityId == trackedId)
                    {
                        inspector_highlight_node_ = agent.location.value;
                        map_selected_node_ = agent.location.value;
                        if (inspector_follow_selection_)
                        {
                            scene_selected_node_ = agent.location.value;
                        }
                        foundAgent = true;
                        break;
                    }
                }
                if (!foundAgent)
                {
                    inspector_highlight_node_.reset();
                }
            }
            else if (inspector_selection_type_ == InspectorSelectionType::Resource)
            {
                if (inspector_selected_primary_ < latest_snapshot_->telemetry.resources.size())
                {
                    inspector_highlight_node_ =
                        latest_snapshot_->telemetry.resources[inspector_selected_primary_].location.value;
                    map_selected_node_ = inspector_highlight_node_;
                }
                else
                {
                    inspector_highlight_node_.reset();
                }
            }
            else if (inspector_selection_type_ == InspectorSelectionType::Node)
            {
                inspector_highlight_node_ = inspector_selected_primary_;
                map_selected_node_ = inspector_selected_primary_;
            }
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

void AppHost::drawToasts()
{
    if (toasts_.empty())
    {
        return;
    }
    const double now = ImGui::GetTime();
    while (!toasts_.empty() && toasts_.front().expiresAt <= now)
    {
        toasts_.pop_front();
    }
    if (toasts_.empty())
    {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = 12.0f;
    ImVec2 cursor{viewport->Pos.x + viewport->Size.x - margin, viewport->Pos.y + margin};

    int index = 0;
    for (auto& t : toasts_)
    {
        const ImVec2 textSize = ImGui::CalcTextSize(t.text.c_str());
        const ImVec2 winSize{textSize.x + 24.0f, textSize.y + 16.0f};
        cursor.x -= winSize.x;

        ImGui::SetNextWindowPos(cursor);
        ImGui::SetNextWindowSize(winSize);
        ImGui::SetNextWindowBgAlpha(0.92f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                                       ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoMove;
        std::string winId = "##toast_" + std::to_string(index++);
        if (ImGui::Begin(winId.c_str(), nullptr, flags))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, t.color);
            ImGui::SetCursorPos(ImVec2(12.0f, 8.0f));
            ImGui::TextUnformatted(t.text.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::End();

        cursor.y += winSize.y + 8.0f;
        cursor.x = viewport->Pos.x + viewport->Size.x - margin;
    }
}

void AppHost::pushToast(const std::string& text, const ImVec4& color, double lifetimeSec)
{
    Toast t;
    t.text = text;
    t.color = color;
    t.expiresAt = ImGui::GetTime() + lifetimeSec;
    toasts_.push_back(std::move(t));
    if (toasts_.size() > 8)
    {
        toasts_.pop_front();
    }
}

void AppHost::handleShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && runtime_bridge_)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
        {
            const bool paused = runtime_bridge_->paused();
            runtime_bridge_->setPaused(!paused);
            pushToast(paused ? "Resume" : "Pause", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F6))
        {
            runtime_bridge_->requestStep(1);
            pushToast("Step x1", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F7))
        {
            runtime_bridge_->requestStep(10);
            pushToast("Step x10", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F8))
        {
            show_logs_ = !show_logs_;
            pushToast(show_logs_ ? "Log: On" : "Log: Off", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F9))
        {
            show_telemetry_ = !show_telemetry_;
            pushToast(show_telemetry_ ? "Telemetry: On" : "Telemetry: Off", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        }
    }
}

} // namespace Genesis::Sandbox::Gui
