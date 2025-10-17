#include "sandbox/gui/AppHost.hpp"

#include <spdlog/spdlog.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <utility>

namespace Genesis::Sandbox::Gui
{
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
{
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
    drawDockspace();
    drawMainMenuBar();
    drawWelcomePanel();

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
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void AppHost::drawWelcomePanel()
{
    ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Genesis Sandbox GUI · Smoke");
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
    ImGui::TextWrapped(
        "当前阶段目标：验证 GLFW/OpenGL/ImGui 集成、窗口生命周期与 Docking 工作流。"
        "接下来将接入 Runtime 快照与世界渲染。");

    ImGui::End();
}

} // namespace Genesis::Sandbox::Gui
