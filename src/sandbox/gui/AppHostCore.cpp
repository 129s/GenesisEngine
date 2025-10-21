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
    , ui_state_()
    , world_command_controller_(ui_state_,
                                [this]() -> RuntimeBridge* { return runtime_bridge_.get(); },
                                [this](const std::string& text, const ImVec4& color, double lifetime) { pushToast(text, color, lifetime); },
                                [this]() { resetSceneForNewWorld(); })
    , ui_context_{*this, ui_state_, runtime_bridge_.get(), latest_snapshot_, speed_multiplier_ui_, config_, world_command_controller_}
{
    refreshDefaultWorldgenConfig();
    ui_state_.worldgen_seed = static_cast<std::uint64_t>(std::random_device{}());
    std::fill(ui_state_.inspector_search_buffer.begin(), ui_state_.inspector_search_buffer.end(), '\0');

    if (auto logger = spdlog::default_logger())
    {
        ui_state_.log_sink = std::make_shared<ImGuiLogSink>(512);
        logger->sinks().push_back(ui_state_.log_sink);
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

    ImFontConfig defaultCfg{};
    defaultCfg.OversampleH = 1;
    defaultCfg.OversampleV = 1;
    defaultCfg.PixelSnapH = true;
    if (!io.Fonts->AddFontDefault(&defaultCfg))
    {
        io.Fonts->AddFontDefault();
    }
    const float baseFontSize = 14.0f;

    bool hasCjkFont = false;
    if (auto cjkFont = locateCjkFont(); !cjkFont.empty())
    {
        ImFontConfig config;
        config.MergeMode = true;
        config.FontDataOwnedByAtlas = false;
        config.OversampleH = 1;
        config.OversampleV = 1;
        config.PixelSnapH = true;
#ifdef IMGUI_ENABLE_FREETYPE
        config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_ForceAutoHint;
#endif
        const auto u8Path = cjkFont.u8string();
        const std::string pathUtf8(u8Path.begin(), u8Path.end());
        if (ImFont* cjkFont = io.Fonts->AddFontFromFileTTF(pathUtf8.c_str(), baseFontSize, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon()))
        {
            hasCjkFont = true;
            io.FontDefault = cjkFont;
            spdlog::info("Loaded CJK UI font: {}", pathUtf8);
        }
    }

    if (!hasCjkFont)
    {
        spdlog::warn("CJK font not found, Chinese text rendering may be degraded");
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.GrabRounding = 0.0f;

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
    browser_view_.render(ui_context_);
    main_view_.render(ui_context_);
    inspector_view_.render(ui_context_);
    status_bar_view_.render(ui_context_);
    control_bar_view_.render(ui_context_);
    drawToasts();
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
        ui_state_.agent_trails.clear();
        return;
    }

    if (auto snapshot = runtime_bridge_->latestSnapshot())
    {
        latest_snapshot_ = std::move(snapshot);
        if (latest_snapshot_)
        {
            updateAgentTrails(*latest_snapshot_);
            if (ui_state_.inspector_selection_type == UiState::InspectorSelectionType::Agent)
            {
                const auto trackedId = ui_state_.inspector_selected_primary;
                bool foundAgent = false;
                for (const auto& agent : latest_snapshot_->telemetry.agents)
                {
                    if (agent.entityId == trackedId)
                    {
                        ui_state_.inspector_highlight_node = agent.location.value;
                        ui_state_.map_selected_node = agent.location.value;
                        if (ui_state_.inspector_follow_selection)
                        {
                            ui_state_.scene_selected_node = agent.location.value;
                        }
                        foundAgent = true;
                        break;
                    }
                }
                if (!foundAgent)
                {
                    ui_state_.inspector_highlight_node.reset();
                }
            }
            else if (ui_state_.inspector_selection_type == UiState::InspectorSelectionType::Resource)
            {
                if (ui_state_.inspector_selected_primary < latest_snapshot_->telemetry.resources.size())
                {
                    ui_state_.inspector_highlight_node =
                        latest_snapshot_->telemetry.resources[ui_state_.inspector_selected_primary].location.value;
                    ui_state_.map_selected_node = ui_state_.inspector_highlight_node;
                }
                else
                {
                    ui_state_.inspector_highlight_node.reset();
                }
            }
            else if (ui_state_.inspector_selection_type == UiState::InspectorSelectionType::Node)
            {
                ui_state_.inspector_highlight_node = ui_state_.inspector_selected_primary;
                ui_state_.map_selected_node = ui_state_.inspector_selected_primary;
            }
        }
    }
}

void AppHost::updateAgentTrails(const RuntimeBridge::Snapshot& snapshot)
{
    if (!runtime_bridge_)
    {
        ui_state_.agent_trails.clear();
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

        auto& trail = ui_state_.agent_trails[agent.entityId];
        if (trail.empty() || std::fabs(trail.back().x - position.x) > std::numeric_limits<float>::epsilon() ||
            std::fabs(trail.back().y - position.y) > std::numeric_limits<float>::epsilon())
        {
            trail.push_back(position);
            while (trail.size() > ui_state_.agent_trail_samples)
            {
                trail.pop_front();
            }
        }
    }

    for (auto it = ui_state_.agent_trails.begin(); it != ui_state_.agent_trails.end();)
    {
        if (!observed.contains(it->first))
        {
            it = ui_state_.agent_trails.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void AppHost::drawToasts()
{
    if (ui_state_.toasts.empty())
    {
        return;
    }
    const double now = ImGui::GetTime();
    ui_state_.pruneExpiredToasts(now);
    if (ui_state_.toasts.empty())
    {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = 12.0f;
    ImVec2 cursor{viewport->Pos.x + viewport->Size.x - margin, viewport->Pos.y + margin};

    int index = 0;
    for (auto& t : ui_state_.toasts)
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
    ui_state_.pushToast(text, color, lifetimeSec);
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
            ui_state_.main_view_active_tab = MainViewTab::Monitor;
            ui_state_.browser_active_section = BrowserSection::Monitor;
            pushToast("主视图 → Monitor", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F9))
        {
            ui_state_.main_view_active_tab = MainViewTab::World;
            ui_state_.browser_active_section = BrowserSection::World;
            pushToast("主视图 → World", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
        }
    }
}

} // namespace Genesis::Sandbox::Gui
