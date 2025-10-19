#include "sandbox/gui/AppHost.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

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
#include <deque>
#include <filesystem>
#include <limits>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace Genesis::Sandbox::Gui
{
using json = nlohmann::json;

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
        void sink_it_(const spdlog::details::log_msg &msg) override
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
        const char* commandStateLabel(RuntimeBridge::CommandState state)
        {
            switch (state)
            {
            case RuntimeBridge::CommandState::Pending:
                return "Pending";
            case RuntimeBridge::CommandState::Succeeded:
                return "Succeeded";
            case RuntimeBridge::CommandState::Failed:
                return "Failed";
            default:
                return "Unknown";
            }
        }

        ImVec4 commandStateColor(RuntimeBridge::CommandState state)
        {
            switch (state)
            {
            case RuntimeBridge::CommandState::Pending:
                return ImVec4(0.95f, 0.78f, 0.35f, 1.0f);
            case RuntimeBridge::CommandState::Succeeded:
                return ImVec4(0.45f, 0.85f, 0.45f, 1.0f);
            case RuntimeBridge::CommandState::Failed:
                return ImVec4(0.95f, 0.4f, 0.35f, 1.0f);
            default:
                return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            }
        }

        std::string commandStateSummary(const RuntimeBridge::CommandProgress& command)
        {
            std::string summary = std::string(commandStateLabel(command.state)) + " (#" + std::to_string(command.id) + ")";
            if (!command.message.empty())
            {
                summary += " · " + command.message;
            }
            return summary;
        }

        void FramebufferSizeCallback(GLFWwindow * /*window*/, int width, int height)
        {
            glViewport(0, 0, width, height);
        }

        std::filesystem::path locateAsset(const std::filesystem::path &relative)
        {
            constexpr int searchDepth = 5;
            auto current = std::filesystem::current_path();
            for (int i = 0; i <= searchDepth; ++i)
            {
                const auto candidate = current / relative;
                if (std::filesystem::exists(candidate))
                {
                    return candidate;
                }
                if (!current.has_parent_path())
                {
                    break;
                }
                current = current.parent_path();
            }
            return {};
        }

        std::filesystem::path locateCjkFont()
        {
            if (auto font = locateAsset(std::filesystem::path("data/fonts/NotoSansSC-Regular.ttf")); !font.empty())
            {
                return font;
            }

#if defined(_WIN32)
            if (const char *winDir = std::getenv("WINDIR"); winDir && winDir[0] != '\0')
            {
                const std::filesystem::path base{winDir};
                const std::array<const char *, 3> candidates = {"simsun.ttc", "simhei.ttf", "msyh.ttc"};
                for (const auto *name : candidates)
                {
                    const auto systemFont = base / "Fonts" / name;
                    if (std::filesystem::exists(systemFont))
                    {
                        return systemFont;
                    }
                }
            }
#endif

            return {};
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

        const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
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

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        io.Fonts->TexGlyphPadding = 1;
        io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
        io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Bitmap | ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;

        ImFontConfig defaultCfg{};
        defaultCfg.OversampleH = 1;
        defaultCfg.OversampleV = 1;
        defaultCfg.PixelSnapH = true;
        io.Fonts->AddFontDefault(&defaultCfg);

        if (const auto fontPath = locateCjkFont(); !fontPath.empty())
        {
            ImFontConfig fontCfg{};
            fontCfg.OversampleH = 1;
            fontCfg.OversampleV = 1;
            fontCfg.PixelSnapH = true;
            const float fontSize = 14.0f;
            const ImWchar *ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
            if (auto *cjkFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), fontSize, &fontCfg, ranges))
            {
                io.FontDefault = cjkFont;
                spdlog::info("Loaded UI font: {}", fontPath.string());
            }
            else
            {
                spdlog::warn("Failed to load CJK font at {}", fontPath.string());
            }
        }
        else
        {
            spdlog::warn("CJK font asset not found; Chinese glyphs may display as '?'");
        }

        ImGuiStyle &style = ImGui::GetStyle();
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
                auto &sinks = logger->sinks();
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

        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window_);
    }

    void AppHost::drawDockspace()
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
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
                ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
                ImGui::MenuItem("Map View", nullptr, &show_world_view_);
                ImGui::MenuItem("Scene View", nullptr, &show_scene_view_);
                ImGui::MenuItem("Telemetry", nullptr, &show_telemetry_);
                ImGui::MenuItem("World Generation", nullptr, &show_worldgen_panel_);
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

        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("Average %.2f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::ColorEdit4("Clear Color", clear_color_.data(), ImGuiColorEditFlags_NoInputs);

        ImGui::Separator();
        if (runtime_bridge_)
        {
            const bool paused = runtime_bridge_->paused();
            ImGui::Text("Playback: %s", paused ? "Paused" : "Running");
            if (latest_snapshot_)
            {
                const auto &tick = latest_snapshot_->telemetry;
                ImGui::Text("Step: %llu", static_cast<unsigned long long>(tick.step));
                ImGui::Text("Agents: %zu", tick.agents.size());
                ImGui::Text("Resources: %zu", tick.resources.size());
            }
            else
            {
                ImGui::TextUnformatted("Waiting for first snapshot…");
            }
        }
        else
        {
            ImGui::TextUnformatted("RuntimeBridge unavailable.");
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Focus: RuntimeBridge advances the simulation in a background thread, exposes pause/step/speed controls, and "
            "feeds the world/telemetry panels with the latest snapshot. Use the toolbar above (or F5/F6/F7 hotkeys) for "
            "playback control; VSync, logging and telemetry toggles are also available via toolbar shortcuts.");

        ImGui::End();
    }

void AppHost::drawWorldGenerationPanel()
{
    if (!show_worldgen_panel_)
    {
        return;
    }

    if (!ImGui::Begin("World Generation", &show_worldgen_panel_))
    {
        ImGui::End();
        return;
    }

    const bool bridgeReady = runtime_bridge_ != nullptr;
    std::vector<RuntimeBridge::CommandProgress> commandStatuses;
    if (bridgeReady)
    {
        commandStatuses = runtime_bridge_->commandStatusSnapshot();
        refreshCommandStatusTexts(commandStatuses);
        std::unordered_set<std::uint64_t> currentIds;
        currentIds.reserve(commandStatuses.size());
        for (const auto &cmd : commandStatuses)
        {
            currentIds.insert(cmd.id);
        }
        for (auto it = world_queue_hidden_completed_.begin(); it != world_queue_hidden_completed_.end();)
        {
            if (!currentIds.contains(*it))
            {
                it = world_queue_hidden_completed_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }



    ImGui::TextUnformatted("World Generation → command-queue driven generate/load/save.");
    ImGui::Separator();

    ImGui::InputText("Config Path", worldgen_config_buffer_.data(), worldgen_config_buffer_.size());
    ImGui::InputText("Output Path", worldgen_output_buffer_.data(), worldgen_output_buffer_.size());

    if (ImGui::Checkbox("Random Seed", &worldgen_use_random_seed_))
    {
        if (worldgen_use_random_seed_)
        {
            worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
        }
    }


    if (worldgen_use_random_seed_)
    {
        ImGui::SameLine();
        if (ImGui::Button("Refresh Seed"))
        {
            worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
        }
        ImGui::SameLine();
        ImGui::Text("Current: %llu", static_cast<unsigned long long>(worldgen_seed_));
    }
    else
    {
        ImGui::InputScalar("Seed", ImGuiDataType_U64, &worldgen_seed_);
    }

    const std::string configInput(worldgen_config_buffer_.data());
    const std::string outputInput(worldgen_output_buffer_.data());
    const bool hasConfig = !configInput.empty();
    if (!hasConfig)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide config path");
    }

    if (!bridgeReady || !hasConfig)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Generate World"))
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
                world_command_status_ = "Command enqueued (#" + std::to_string(*id) + ")";
            }
            else
            {
                world_command_status_ = "Submit failed: " + error;
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
                ImGui::Text("Last generation succeeded");
                ImGui::BulletText("Config: %s", result.configPath.string().c_str());
                ImGui::BulletText("Seed: %llu", static_cast<unsigned long long>(result.seed.value));
                ImGui::BulletText("Locations: %zu · Edges: %zu", result.locationCount, result.edgeCount);
                ImGui::BulletText("Duration: %.2f ms", result.durationMs);
                if (result.outputPath)
                {
                    ImGui::BulletText("Output file: %s", result.outputPath->string().c_str());
                }
                else
                {
                    ImGui::BulletText("Output file: not specified (kept in memory)");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Generation failed: %s", result.error.c_str());
            }

            if (!result.logs.empty())
            {
                if (ImGui::BeginChild("WorldGenLogs", ImVec2(0.0f, 180.0f), true))
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
    ImGui::InputText("Load Path", world_load_buffer_.data(), world_load_buffer_.size());
    const std::string loadInput(world_load_buffer_.data());
    if (loadInput.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide load path");
    }

    if (!bridgeReady || loadInput.empty())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Load World"))
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
                world_load_status_ = "Command enqueued (#" + std::to_string(*id) + ")";
            }
            else
            {
                world_load_status_ = "Submit failed: " + error;
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

    ImGui::Separator();
    ImGui::InputText("Save Path", world_save_buffer_.data(), world_save_buffer_.size());
    const std::string saveInput(world_save_buffer_.data());
    if (saveInput.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide save path");
    }

    if (!bridgeReady || saveInput.empty())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save Current World"))
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
                world_save_status_ = "Command enqueued (#" + std::to_string(*id) + ")";
            }
            else
            {
                world_save_status_ = "Submit failed: " + error;
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
    ImGui::TextUnformatted("Command Script");
    ImGui::InputText("Script Path", command_script_buffer_.data(), command_script_buffer_.size());
    if (!bridgeReady)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Run Script"))
    {
        const std::string scriptPath(command_script_buffer_.data());
        if (scriptPath.empty())
        {
            command_script_status_ = "Please provide script path";
        }
        else if (bridgeReady)
        {
            std::string error;
            if (runtime_bridge_->enqueueCommandScript(std::filesystem::path(scriptPath), "script", error))
            {
                command_script_status_ = "Script enqueued";
            }
            else
            {
                command_script_status_ = "Script execution failed: " + error;
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

    ImGui::Separator();
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Command Queue"))
    {
        ImGui::Checkbox("Show Pending", &world_queue_show_pending_);
        ImGui::SameLine();
        ImGui::Checkbox("Show Succeeded", &world_queue_show_succeeded_);
        ImGui::SameLine();
        ImGui::Checkbox("Show Failed", &world_queue_show_failed_);

        bool hasVisibleCompleted = false;
        for (const auto &cmd : commandStatuses)
        {
            if (cmd.state == RuntimeBridge::CommandState::Succeeded && !world_queue_hidden_completed_.contains(cmd.id))
            {
                hasVisibleCompleted = true;
                break;
            }
        }

        ImGui::SameLine(0.0f, 18.0f);
        if (!hasVisibleCompleted)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Clear Completed"))
        {
            for (const auto &cmd : commandStatuses)
            {
                if (cmd.state == RuntimeBridge::CommandState::Succeeded)
                {
                    world_queue_hidden_completed_.insert(cmd.id);
                }
            }
        }
        if (!hasVisibleCompleted)
        {
            ImGui::EndDisabled();
        }

        std::vector<RuntimeBridge::CommandProgress> filtered;
        filtered.reserve(commandStatuses.size());
        for (const auto &cmd : commandStatuses)
        {
            if (cmd.state == RuntimeBridge::CommandState::Pending && !world_queue_show_pending_)
            {
                continue;
            }
            if (cmd.state == RuntimeBridge::CommandState::Succeeded)
            {
                if (!world_queue_show_succeeded_)
                {
                    continue;
                }
                if (world_queue_hidden_completed_.contains(cmd.id))
                {
                    continue;
                }
            }
            if (cmd.state == RuntimeBridge::CommandState::Failed && !world_queue_show_failed_)
            {
                continue;
            }
            filtered.push_back(cmd);
        }

        if (filtered.empty())
        {
            ImGui::TextUnformatted("No command entries");
        }
        else if (ImGui::BeginChild("CommandQueueView", ImVec2(0.0f, 220.0f), true))
        {
            if (ImGui::BeginTable("CommandQueueTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Notes");
                ImGui::TableHeadersRow();

                for (const auto& cmd : filtered)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("#%llu", static_cast<unsigned long long>(cmd.id));

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(cmd.label.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(cmd.source.c_str());

                    ImGui::TableSetColumnIndex(3);
                    const ImVec4 color = commandStateColor(cmd.state);
                    ImGui::TextColored(color, "%s", commandStateLabel(cmd.state));

                    ImGui::TableSetColumnIndex(4);
                    if (!cmd.message.empty())
                    {
                        ImGui::TextWrapped("%s", cmd.message.c_str());
                    }
                    else if (cmd.payloadJson.has_value())
                    {
                        ImGui::TextDisabled("%s", cmd.payloadJson->c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("-");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

    void AppHost::drawInspectorPanel()
    {
        if (!show_inspector_)
        {
            return;
        }

        if (!ImGui::Begin("Inspector", &show_inspector_))
        {
            ImGui::End();
            return;
        }

        if (!latest_snapshot_)
        {
            ImGui::TextUnformatted("Waiting for snapshot...");
            ImGui::End();
            return;
        }

        const auto &snapshot = *latest_snapshot_;
        const auto &tick = snapshot.telemetry;
        const RuntimeBridge::WorldAtlas *atlasPtr = runtime_bridge_ ? &runtime_bridge_->atlas() : nullptr;

        ImGui::InputTextWithHint("##InspectorSearch", "Search name/id/type", inspector_search_buffer_.data(), inspector_search_buffer_.size());
        std::string filterRaw(inspector_search_buffer_.data());
        std::string filterLower = filterRaw;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const bool filterEmpty = filterLower.empty();

        auto toLowerString = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        };

        auto matchesFilter = [&](const std::string &text, std::uint32_t id, const std::string &extra) -> bool {
            if (filterEmpty)
            {
                return true;
            }
            if (!text.empty())
            {
                auto lowered = toLowerString(text);
                if (lowered.find(filterLower) != std::string::npos)
                {
                    return true;
                }
            }
            if (!extra.empty())
            {
                auto lowered = toLowerString(extra);
                if (lowered.find(filterLower) != std::string::npos)
                {
                    return true;
                }
            }
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%u", id);
            auto lowered = toLowerString(std::string{buffer});
            return lowered.find(filterLower) != std::string::npos;
        };

        const auto resourceTypeName = [](genesis::world::ResourceType type) -> const char * {
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

        const float listWidth = 260.0f;
        const ImVec2 listSize{listWidth, ImGui::GetContentRegionAvail().y};
        ImGui::BeginChild("InspectorList", listSize, true);

        if (ImGui::CollapsingHeader("Agents", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::vector<std::size_t> indices(tick.agents.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
                const auto &a = tick.agents[lhs];
                const auto &b = tick.agents[rhs];
                if (a.name == b.name)
                {
                    return a.entityId < b.entityId;
                }
                if (a.name.empty())
                {
                    return false;
                }
                if (b.name.empty())
                {
                    return true;
                }
                return a.name < b.name;
            });

            for (std::size_t idx : indices)
            {
                const auto &agent = tick.agents[idx];
                std::string nodeName;
                if (atlasPtr)
                {
                    for (const auto &node : atlasPtr->nodes)
                    {
                        if (node.id.value == agent.location.value)
                        {
                            nodeName = node.name;
                            break;
                        }
                    }
                }

                if (!matchesFilter(agent.name, agent.entityId, nodeName))
                {
                    continue;
                }

                char label[128];
                if (!agent.name.empty())
                {
                    std::snprintf(label, sizeof(label), "%s [#%u]", agent.name.c_str(), agent.entityId);
                }
                else
                {
                    std::snprintf(label, sizeof(label), "Agent [#%u]", agent.entityId);
                }

                const bool selected = inspector_selection_type_ == InspectorSelectionType::Agent && inspector_selected_primary_ == agent.entityId;
                ImGui::PushID(static_cast<int>(agent.entityId));
                if (ImGui::Selectable(label, selected))
                {
                    inspector_selection_type_ = InspectorSelectionType::Agent;
                    inspector_selected_primary_ = agent.entityId;
                    inspector_selected_secondary_ = static_cast<std::uint32_t>(idx);
                    inspector_highlight_node_ = agent.location.value;
                    map_selected_node_ = agent.location.value;
                    if (inspector_follow_selection_)
                    {
                        scene_selected_node_ = agent.location.value;
                    }
                }
                ImGui::PopID();
                if (!nodeName.empty() && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Node: %s", nodeName.c_str());
                }
            }
        }

        if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (std::size_t i = 0; i < tick.resources.size(); ++i)
            {
                const auto &resource = tick.resources[i];
                std::string nodeName;
                if (atlasPtr)
                {
                    for (const auto &node : atlasPtr->nodes)
                    {
                        if (node.id.value == resource.location.value)
                        {
                            nodeName = node.name;
                            break;
                        }
                    }
                }

                const std::string extra = std::string(resourceTypeName(resource.type)) + " " + nodeName;
                if (!matchesFilter(resource.name, resource.location.value, extra))
                {
                    continue;
                }

            char label[160];
            std::snprintf(label, sizeof(label), "%s (%s) [Node #%u]", resource.name.c_str(), resourceTypeName(resource.type), resource.location.value);

                const bool selected = inspector_selection_type_ == InspectorSelectionType::Resource && inspector_selected_primary_ == static_cast<std::uint32_t>(i);
                ImGui::PushID(static_cast<int>(resource.location.value * 4096 + static_cast<std::uint32_t>(i)));
                if (ImGui::Selectable(label, selected))
                {
                    inspector_selection_type_ = InspectorSelectionType::Resource;
                    inspector_selected_primary_ = static_cast<std::uint32_t>(i);
                    inspector_selected_secondary_ = resource.location.value;
                    inspector_highlight_node_ = resource.location.value;
                    map_selected_node_ = resource.location.value;
                }
                ImGui::PopID();
            }
        }

        if (atlasPtr && ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto &node : atlasPtr->nodes)
            {
                if (!matchesFilter(node.name, node.id.value, ""))
                {
                    continue;
                }

                char label[128];
                std::snprintf(label, sizeof(label), "%s [#%u]", node.name.c_str(), node.id.value);

                const bool selected = inspector_selection_type_ == InspectorSelectionType::Node && inspector_selected_primary_ == node.id.value;
                ImGui::PushID(static_cast<int>(node.id.value));
                if (ImGui::Selectable(label, selected))
                {
                    inspector_selection_type_ = InspectorSelectionType::Node;
                    inspector_selected_primary_ = node.id.value;
                    inspector_selected_secondary_ = 0;
                    inspector_highlight_node_ = node.id.value;
                    map_selected_node_ = node.id.value;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("InspectorDetails", ImVec2(0.0f, listSize.y), false);

        switch (inspector_selection_type_)
        {
        case InspectorSelectionType::None:
            ImGui::TextUnformatted("Select an entry on the left to view details.");
            break;
        case InspectorSelectionType::Agent:
        {
            const genesis::telemetry::AgentSnapshot *agent = nullptr;
            for (const auto &candidate : tick.agents)
            {
                if (candidate.entityId == inspector_selected_primary_)
                {
                    agent = &candidate;
                    break;
                }
            }

            if (!agent)
            {
                ImGui::Text("Agent #%u is not present in the current snapshot.", inspector_selected_primary_);
                break;
            }

            std::string nodeName = "(Unknown)";
            if (atlasPtr)
            {
                for (const auto &node : atlasPtr->nodes)
                {
                    if (node.id.value == agent->location.value)
                    {
                        nodeName = node.name;
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
            ImGui::SameLine(0.0f, 12.0f);
            if (ImGui::Button("Focus on Map##agentFocus"))
            {
                show_world_view_ = true;
                inspector_highlight_node_ = agent->location.value;
                map_selected_node_ = agent->location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("Open Scene##agentScene"))
            {
                show_scene_view_ = true;
                scene_selected_node_ = agent->location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            bool followChanged = ImGui::Checkbox("Follow##agentFollow", &inspector_follow_selection_);
            if (followChanged && inspector_follow_selection_)
            {
                scene_selected_node_ = agent->location.value;
                map_selected_node_ = agent->location.value;
            }
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("ID: %u", agent->entityId);
            ImGui::SameLine(0.0f, 12.0f);
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
        case InspectorSelectionType::Resource:
        {
            if (inspector_selected_primary_ >= tick.resources.size())
            {
                ImGui::TextUnformatted("Selected resource index is stale.");
                break;
            }

            const auto &resource = tick.resources[inspector_selected_primary_];
            ImGui::Text("%s", resource.name.c_str());
            ImGui::SameLine(0.0f, 12.0f);
            if (ImGui::Button("Focus on Map##resourceFocus"))
            {
                show_world_view_ = true;
                inspector_highlight_node_ = resource.location.value;
                map_selected_node_ = resource.location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("Open Scene##resourceScene"))
            {
                show_scene_view_ = true;
                scene_selected_node_ = resource.location.value;
            }
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Node: #%u", resource.location.value);

            ImGui::Text("Type: %s", resourceTypeName(resource.type));
            ImGui::Text("Inventory: %u / %u", resource.current, resource.capacity);
            break;
        }
        case InspectorSelectionType::Node:
        {
            if (!atlasPtr)
            {
                ImGui::TextUnformatted("No node information available.");
                break;
            }

            const RuntimeBridge::WorldAtlas::Node *selectedNode = nullptr;
            for (const auto &node : atlasPtr->nodes)
            {
                if (node.id.value == inspector_selected_primary_)
                {
                    selectedNode = &node;
                    break;
                }
            }

            if (!selectedNode)
            {
                ImGui::Text("Node #%u does not exist.", inspector_selected_primary_);
                break;
            }

            ImGui::Text("%s", selectedNode->name.c_str());
            ImGui::SameLine(0.0f, 12.0f);
            if (ImGui::Button("Focus on Map##nodeFocus"))
            {
                show_world_view_ = true;
                inspector_highlight_node_ = selectedNode->id.value;
                map_selected_node_ = selectedNode->id.value;
            }
            ImGui::SameLine(0.0f, 12.0f);
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
                    ImGui::TextColored(evt.success ? ImVec4(0.62f, 0.84f, 0.58f, 1.0f) : ImVec4(0.95f, 0.45f, 0.45f, 1.0f),
                                       "[#%llu] %s", static_cast<unsigned long long>(evt.id), evt.label.c_str());
                    if (!evt.message.empty())
                    {
                        ImGui::BulletText("%s", evt.message.c_str());
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::End();
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
        map_selected_node_.reset();
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

        const auto &atlas = runtime_bridge_->atlas();
        const auto *agentPositionsPtr = latest_snapshot_ ? &latest_snapshot_->agentPositions : nullptr;

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
                for (auto &[id, trail] : agent_trails_)
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
        const ImU32 highlightColor = ImGui::GetColorU32(ImVec4(0.98f, 0.83f, 0.37f, 1.0f));

        if (show_agent_overlay_)
        {
            ImGui::Spacing();
            auto legendEntry = [](const char *id, const char *text, const ImVec4 &color)
            {
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

        std::unordered_map<std::uint32_t, std::vector<const genesis::telemetry::ResourceSnapshot *>> resourcesByLocation;
        if (latest_snapshot_)
        {
            for (const auto &resource : latest_snapshot_->telemetry.resources)
            {
                resourcesByLocation[resource.location.value].push_back(&resource);
            }
        }

        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax{canvasPos.x + std::max(120.0f, canvasSize.x), canvasPos.y + std::max(120.0f, canvasSize.y)};

        ImDrawList *drawList = ImGui::GetWindowDrawList();
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

            auto toScreen = [&](const RuntimeBridge::Vector2 &pos)
            {
                return ImVec2(
                    canvasPos.x + padding + pos.x * scaleX,
                    canvasPos.y + padding + pos.y * scaleY);
            };

            std::unordered_map<std::uint32_t, ImVec2> nodePositions;
            nodePositions.reserve(atlas.nodes.size());
            for (const auto &node : atlas.nodes)
            {
                nodePositions.emplace(node.id.value, toScreen(node.position));
            }

            // Selected node edges (parent/children only)
            std::optional<std::uint32_t> selectedEdgeNode = map_selected_node_ ? map_selected_node_ : inspector_highlight_node_;
            const RuntimeBridge::WorldAtlas::Node *selectedNodeInfo = nullptr;
            if (selectedEdgeNode)
            {
                for (const auto &node : atlas.nodes)
                {
                    if (node.id.value == *selectedEdgeNode)
                    {
                        selectedNodeInfo = &node;
                        break;
                    }
                }
            }

            if (selectedNodeInfo)
            {
                auto selectedPosIt = nodePositions.find(selectedNodeInfo->id.value);
                if (selectedPosIt != nodePositions.end())
                {
                    const ImU32 parentEdgeColor = ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
                    const ImU32 childEdgeColor = ImGui::GetColorU32(ImVec4(0.38f, 0.72f, 0.96f, 1.0f));

                    if (selectedNodeInfo->parent.value != 0 && selectedNodeInfo->parent.value != selectedNodeInfo->id.value)
                    {
                        auto parentPosIt = nodePositions.find(selectedNodeInfo->parent.value);
                        if (parentPosIt != nodePositions.end())
                        {
                            drawList->AddLine(selectedPosIt->second, parentPosIt->second, parentEdgeColor, 2.4f);
                        }
                    }

                    for (const auto &node : atlas.nodes)
                    {
                        if (node.parent.value != selectedNodeInfo->id.value)
                        {
                            continue;
                        }
                        auto childPosIt = nodePositions.find(node.id.value);
                        if (childPosIt != nodePositions.end())
                        {
                            drawList->AddLine(selectedPosIt->second, childPosIt->second, childEdgeColor, 2.4f);
                        }
                    }
                }
            }

            // Prepare for label overlap avoidance
            std::vector<ImRect> placedLabels;
            placedLabels.reserve(atlas.nodes.size());

            auto nodeKindIcon = [](genesis::world::LocationKind kind) -> const char*
            {
                switch (kind)
                {
                case genesis::world::LocationKind::Region: return "R";
                case genesis::world::LocationKind::Building: return "B";
                case genesis::world::LocationKind::Room: return "r";
                case genesis::world::LocationKind::Point: return "."; // simple dot placeholder
                }
                return "?";
            };

            const float nodeRadius = 12.0f;

            for (const auto &node : atlas.nodes)
            {
                auto it = nodePositions.find(node.id.value);
                if (it == nodePositions.end())
                {
                    continue;
                }

                const float radius = nodeRadius;
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

                const bool isSelected = map_selected_node_ && node.id.value == *map_selected_node_;
                const bool highlighted = (inspector_highlight_node_ && node.id.value == *inspector_highlight_node_) || isSelected;
                if (highlighted)
                {
                    drawList->AddCircle(it->second, radius + 4.0f, highlightColor, 24, 2.5f);
                }

                // Icon inside the node
                const char* icon = nodeKindIcon(node.kind);
                const ImVec2 iconSize = ImGui::CalcTextSize(icon);
                const ImVec2 iconPos{it->second.x - iconSize.x * 0.5f, it->second.y - iconSize.y * 0.5f};
                drawList->AddText(iconPos, ImGui::GetColorU32(ImGuiCol_Text), icon);

                // Label to the right, avoid overlap; force when hovered/selected
                bool hovered = false;
                if (ImGui::IsWindowHovered())
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    const float dx = mouse.x - it->second.x;
                    const float dy = mouse.y - it->second.y;
                    hovered = (dx * dx + dy * dy) <= (radius * radius);
                }
                const ImVec2 nameSize = ImGui::CalcTextSize(node.name.c_str());
                const ImVec2 labelPos{it->second.x + radius + 6.0f, it->second.y - nameSize.y * 0.5f};
                ImRect labelRect{labelPos, ImVec2(labelPos.x + nameSize.x, labelPos.y + nameSize.y)};
                bool overlaps = false;
                for (const auto& r : placedLabels)
                {
                    if (r.Overlaps(labelRect)) { overlaps = true; break; }
                }
                if (!overlaps || hovered || highlighted)
                {
                    drawList->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), node.name.c_str());
                    placedLabels.push_back(labelRect);
                }

                // Click to open Scene View on this node
                if (ImGui::IsWindowHovered())
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    const float dx = mouse.x - it->second.x;
                    const float dy = mouse.y - it->second.y;
                    if ((dx * dx + dy * dy) <= (radius * radius))
                    {
                        if (hovered)
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", node.name.c_str());
                            ImGui::Separator();
                            ImGui::Text("ID: %u", node.id.value);
                            ImGui::Text("Kind: %u", static_cast<unsigned int>(node.kind));
                            ImGui::EndTooltip();
                        }
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            map_selected_node_ = node.id.value;
                            scene_selected_node_ = node.id.value;
                            show_scene_view_ = true;
                        }
                    }
                }
            }

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                map_selected_node_.reset();
            }

            for (const auto &spawn : atlas.spawns)
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
                auto colorForResource = [](genesis::world::ResourceType type) -> ImU32
                {
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

                for (const auto &[locationId, resources] : resourcesByLocation)
                {
                    auto posIt = nodePositions.find(locationId);
                    if (posIt == nodePositions.end())
                    {
                        continue;
                    }

                    const ImVec2 basePos = ImVec2(std::floor(posIt->second.x) + 0.5f, std::floor(posIt->second.y) + 0.5f);
                    float offsetY = 20.0f;

                    for (const auto *resource : resources)
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
                    for (const auto &mp : latest_snapshot_->telemetry.movementProgress)
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
                for (const auto &action : latest_snapshot_->telemetry.actions)
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

                auto colorForState = [&](AgentState state) -> ImU32
                {
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
                    const auto &agent = latest_snapshot_->telemetry.agents[i];
                    RuntimeBridge::Vector2 agentPos = atlas.nodePosition(agent.location).value_or(RuntimeBridge::Vector2{});
                    if (agentPositionsPtr && i < agentPositionsPtr->size())
                    {
                        agentPos = (*agentPositionsPtr)[i];
                    }
                    if (map_interpolate_)
                    {
                        if (auto itp = progress.find(agent.entityId); itp != progress.end())
                        {
                            const auto &[fromP, toP, t] = itp->second;
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

                    if (inspector_selection_type_ == InspectorSelectionType::Agent && inspector_selected_primary_ == agent.entityId)
                    {
                        drawList->AddCircle(screenPos, 11.0f, highlightColor, 24, 2.5f);
                    }

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
                            const auto &trail = trailIt->second;
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

        constexpr ImGuiWindowFlags sceneViewFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Scene View", &show_scene_view_, sceneViewFlags))
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

        const auto &atlas = runtime_bridge_->atlas();

        // Node selector
        if (scene_selected_node_ == 0 && !atlas.nodes.empty())
        {
            scene_selected_node_ = atlas.nodes.front().id.value;
        }

        if (ImGui::BeginCombo("Node", [this, &atlas]()
                              {
            for (const auto& n : atlas.nodes)
            {
                if (n.id.value == scene_selected_node_)
                    return n.name.c_str();
            }
            return "(none)"; }()))
        {
            for (const auto &n : atlas.nodes)
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

        // Toggles
        ImGui::Checkbox("Grid", &scene_show_grid_);
        ImGui::SameLine();
        ImGui::Checkbox("Anchors", &scene_show_anchors_);
        ImGui::SameLine();
        ImGui::Checkbox("Resources", &scene_show_resources_);

        // Canvas setup
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax{canvasPos.x + std::max(120.0f, canvasSize.x), canvasPos.y + std::max(120.0f, canvasSize.y)};
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
        drawList->AddRectFilled(canvasPos, canvasMax, bgColor);
        drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));

        // Camera controls (mouse drag/scroll)
        ImGui::InvisibleButton("SceneCanvas", ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y), ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        ImGuiIO &io = ImGui::GetIO();
        if (hovered && io.MouseWheel != 0.0f)
        {
            const float zoomStep = 1.0f + (io.MouseWheel > 0.0f ? 0.1f : -0.1f);
            scene_cam_zoom_ = std::max(scene_cam_zoom_ * zoomStep, 0.05f);
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            scene_cam_offset_x_ += delta.x;
            scene_cam_offset_y_ += delta.y;
        }

        // Gather local points: resource spawns and anchors for selected node
        std::vector<ImVec2> resourcePts;
        for (const auto &sp : atlas.spawns)
        {
            if (sp.resource.location.value != scene_selected_node_)
                continue;
            if (sp.resource.local_coord.has_value())
            {
                resourcePts.emplace_back(static_cast<float>(sp.resource.local_coord->first), static_cast<float>(sp.resource.local_coord->second));
            }
        }
        std::vector<std::pair<ImVec2, std::string>> anchorPts; // pos, label
        for (const auto &e : atlas.edges)
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
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId == scene_selected_node_ && tm.width > 0 && tm.height > 0)
            {
                minX = 0;
                minY = 0;
                maxX = tm.width - 1;
                maxY = tm.height - 1;
                break;
            }
        }
        auto incorporate = [&](const ImVec2 &p)
        {
            minX = std::min(minX, static_cast<int>(std::floor(p.x)));
            minY = std::min(minY, static_cast<int>(std::floor(p.y)));
            maxX = std::max(maxX, static_cast<int>(std::ceil(p.x)));
            maxY = std::max(maxY, static_cast<int>(std::ceil(p.y)));
        };
        // If no tilemap meta, extend from points
        bool hasMeta = false;
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId == scene_selected_node_ && tm.width > 0 && tm.height > 0)
            {
                hasMeta = true;
                break;
            }
        }
        if (!hasMeta)
        {
            for (const auto &p : resourcePts)
                incorporate(p);
            for (const auto &ap : anchorPts)
                incorporate(ap.first);
        }

        float basePx = 24.0f;
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId == scene_selected_node_ && tm.tileW > 0)
            {
                basePx = std::max(basePx, static_cast<float>(tm.tileW));
                break;
            }
        }
        const float cellPx = basePx * scene_cam_zoom_;
        auto toScreen = [&](float gx, float gy)
        {
            const float sx = canvasPos.x + scene_cam_offset_x_ + (gx - minX) * cellPx + 8.0f;
            const float sy = canvasPos.y + scene_cam_offset_y_ + (gy - minY) * cellPx + 8.0f;
            return ImVec2(std::floor(sx) + 0.5f, std::floor(sy) + 0.5f);
        };

        // Draw grid
        const int cols = (maxX - minX + 1);
        const int rows = (maxY - minY + 1);
        const ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.28f, 1.0f));

        // Draw tile coverage using solid colors as placeholder for actual textures
        const ImU32 tileColorA = ImGui::GetColorU32(ImVec4(0.18f, 0.25f, 0.32f, 0.65f));
        const ImU32 tileColorB = ImGui::GetColorU32(ImVec4(0.15f, 0.21f, 0.28f, 0.65f));
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId != scene_selected_node_ || tm.width <= 0 || tm.height <= 0)
            {
                continue;
            }

            for (int y = 0; y < tm.height; ++y)
            {
                for (int x = 0; x < tm.width; ++x)
                {
                    const float gx0 = static_cast<float>(minX + x);
                    const float gy0 = static_cast<float>(minY + y);
                    const float gx1 = gx0 + 1.0f;
                    const float gy1 = gy0 + 1.0f;
                    const ImVec2 cellMin = toScreen(gx0, gy0);
                    const ImVec2 cellMax = toScreen(gx1, gy1);
                    const ImU32 fill = ((x + y) % 2 == 0) ? tileColorA : tileColorB;
                    drawList->AddRectFilled(cellMin, cellMax, fill);
                }
            }
            break; // use the first matching tilemap per node
        }

        if (scene_show_grid_)
        {
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
        }

        // Draw resources
        const ImU32 foodColor = ImGui::GetColorU32(ImVec4(0.93f, 0.67f, 0.27f, 1.0f));
        if (scene_show_resources_)
        {
            for (const auto &p : resourcePts)
            {
                const ImVec2 center = toScreen(p.x + 0.5f, p.y + 0.5f);
                const float r = std::max(3.0f, cellPx * 0.25f);
                drawList->AddCircleFilled(center, r, foodColor, 12);
                drawList->AddCircle(center, r, ImGui::GetColorU32(ImGuiCol_Border), 12, 1.2f);
            }
        }

        // Draw anchors
        const ImU32 anchorColor = ImGui::GetColorU32(ImVec4(0.38f, 0.74f, 0.88f, 1.0f));
        if (scene_show_anchors_)
        {
            for (const auto &ap : anchorPts)
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

        const auto &tick = latest_snapshot_->telemetry;
        ImGui::Text("Step: %llu", static_cast<unsigned long long>(tick.step));
        ImGui::Text("Agents: %zu", tick.agents.size());
        ImGui::Text("Actions: %zu", tick.actions.size());
        ImGui::Text("Needs: %zu", tick.needs.size());

        if (!tick.needs.empty())
        {
            float hungerSum = 0.0f;
            std::uint32_t critical = 0;
            for (const auto &need : tick.needs)
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
            for (const auto &resource : tick.resources)
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

        for (const auto &line : lines)
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

    void AppHost::drawControlToolbar()
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        const float menuHeight = ImGui::GetFrameHeight();
        const float paddingY = ImGui::GetStyle().FramePadding.y;
        const float height = menuHeight + paddingY * 2.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, paddingY));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("Control Toolbar", nullptr, flags))
        {
            if (runtime_bridge_)
            {
                bool paused = runtime_bridge_->paused();
                if (ImGui::Button(paused ? "Resume" : "Pause"))
                {
                    runtime_bridge_->setPaused(!paused);
                    pushToast(paused ? "Resume" : "Pause", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                }

                ImGui::SameLine();
                if (ImGui::Button("Step"))
                {
                    runtime_bridge_->requestStep(1);
                    pushToast("Step x1", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
                }

                ImGui::SameLine();
                if (ImGui::Button("Step x10"))
                {
                    runtime_bridge_->requestStep(10);
                    pushToast("Step x10", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
                }

                ImGui::SameLine(0.0f, 12.0f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.0f, 12.0f);

                float speed = static_cast<float>(speed_multiplier_ui_);
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::SliderFloat("Speed", &speed, 0.25f, 8.0f, "%.2fx"))
                {
                    speed_multiplier_ui_ = speed;
                    runtime_bridge_->setSpeedMultiplier(speed_multiplier_ui_);
                }

                ImGui::SameLine(0.0f, 18.0f);
                if (ImGui::Checkbox("VSync", &config_.vsync))
                {
                    glfwSwapInterval(config_.vsync ? 1 : 0);
                }

                if (latest_snapshot_)
                {
                    ImGui::SameLine(0.0f, 18.0f);
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine(0.0f, 12.0f);
                    const auto &tick = latest_snapshot_->telemetry;
                    ImGui::Text("Step %llu | Agents %zu | Resources %zu | Actions %zu",
                                static_cast<unsigned long long>(tick.step),
                                tick.agents.size(),
                                tick.resources.size(),
                                tick.actions.size());
                }
                else
                {
                    ImGui::SameLine(0.0f, 18.0f);
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine(0.0f, 12.0f);
                    ImGui::TextUnformatted("Waiting for snapshot...");
                }
            }
            else
            {
                ImGui::TextUnformatted("RuntimeBridge unavailable.");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    void AppHost::drawStatusBar()
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
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
                const auto &tick = latest_snapshot_->telemetry;
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
                if (inspector_selection_type_ == InspectorSelectionType::Agent)
                {
                    const auto trackedId = inspector_selected_primary_;
                    bool foundAgent = false;
                    for (const auto &agent : latest_snapshot_->telemetry.agents)
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
                        inspector_highlight_node_ = latest_snapshot_->telemetry.resources[inspector_selected_primary_].location.value;
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

    void AppHost::updateAgentTrails(const RuntimeBridge::Snapshot &snapshot)
    {
        if (!runtime_bridge_)
        {
            agent_trails_.clear();
            return;
        }

        const auto &atlas = runtime_bridge_->atlas();
        const auto *agentPositionsPtr = &snapshot.agentPositions;
        std::unordered_set<std::uint32_t> observed;
        observed.reserve(snapshot.telemetry.agents.size());

        for (std::size_t i = 0; i < snapshot.telemetry.agents.size(); ++i)
        {
            const auto &agent = snapshot.telemetry.agents[i];
            observed.insert(agent.entityId);
            RuntimeBridge::Vector2 position = atlas.nodePosition(agent.location).value_or(RuntimeBridge::Vector2{});
            if (agentPositionsPtr && i < agentPositionsPtr->size())
            {
                position = (*agentPositionsPtr)[i];
            }

            auto &trail = agent_trails_[agent.entityId];
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

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        const float margin = 12.0f;
        ImVec2 cursor{viewport->Pos.x + viewport->Size.x - margin, viewport->Pos.y + margin};

        int index = 0;
        for (auto &t : toasts_)
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

    void AppHost::pushToast(const std::string &text, const ImVec4 &color, double lifetimeSec)
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
        ImGuiIO &io = ImGui::GetIO();
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


