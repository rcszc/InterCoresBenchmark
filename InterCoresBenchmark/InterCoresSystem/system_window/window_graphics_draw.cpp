// window_graphics_draw.
#include <fstream>
#include "window_graphics.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;
using namespace PSAG_LOGGER;

#if defined(_WIN32) && defined(SYSTEM_ENABLE_WINAPI)
StaticStrLABEL SYSTEM_LOG_TAG_WIN32 = "WIN32_DWM";

enum WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED     = 0,
    WCA_ACCENT_POLICY = 19,
    WCA_BLURBEHIND    = 20,
};
// define(wapi) 'ACCENT_STATE'.
enum ACCENT_STATE {
    ACCENT_DISABLED                   = 0,
    ACCENT_ENABLE_GRADIENT            = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND          = 3, // 毛玻璃效果
    ACCENT_ENABLE_ACRYLICBLURBEHIND   = 4, // 亚克力效果
    ACCENT_ENABLE_HOSTBACKDROP        = 5,
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB DataAttribute;
    PVOID  DataPtrVoid;
    SIZE_T DataSize;
};

// 手动加载 SetWindowCompositionAttribute 函数.
using pSetWindowCompositionAttribute = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
#define CVT_COMP_ATTRIB reinterpret_cast<pSetWindowCompositionAttribute>
// 开启窗口背景模糊效果.
bool EnableWindowsBlur(HWND hwnd) {
    // 动态加载 "user32.dll" 中的 SetWindowCompositionAttribute 函数.
    HMODULE ModuleH = LoadLibrary(TEXT("user32.dll"));
    if (!ModuleH) {
        PushLogger(LogError, SYSTEM_LOG_TAG_WIN32, 
            "win,dwm: failed load 'user32.dll' library.");
        return false;
    }
    pSetWindowCompositionAttribute SetWindowCompositionAttribute 
        = CVT_COMP_ATTRIB(GetProcAddress(ModuleH, "SetWindowCompositionAttribute"));
    // check function pointer.
    if (SetWindowCompositionAttribute == nullptr) {
        PushLogger(LogError, SYSTEM_LOG_TAG_WIN32, 
            "win,dwm: failed get composition_attribute function.");
        FreeLibrary(ModuleH);
        return false;
    }
    // 配置 ACCENT_POLICY 为毛玻璃效果.
    ACCENT_POLICY Accent = {};
    Accent.AccentState   = ACCENT_ENABLE_BLURBEHIND;
    Accent.AccentFlags   = 1;
    Accent.GradientColor = 0xAA00FF00;

    // ACCENT_POLICY => WINDOW.
    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.DataAttribute = WCA_ACCENT_POLICY;
    data.DataPtrVoid   = &Accent;
    data.DataSize      = sizeof(Accent);
    // CALL PROC FUNCTION. 
    SetWindowCompositionAttribute(hwnd, &data);
    FreeLibrary(ModuleH);
    return true;
}

void EnableWindowsTransparency(HWND hwnd) {
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    PushLogger(LogInfo, SYSTEM_LOG_TAG_WIN32, "win,dwm: set layered window attributes.");
}

// windows dwm api, win7? win10, win11.
void SetSystemWindowStyleDark(GLFWwindow* window, COLORREF color) {
    HWND Hwnd = glfwGetWin32Window(window);
    BOOL DarkMode = TRUE;
    // setting window attributes.
    DwmSetWindowAttribute(Hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &DarkMode, sizeof(DarkMode));
    DwmSetWindowAttribute(Hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
    DwmSetWindowAttribute(Hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));

    PushLogger(LogInfo, SYSTEM_LOG_TAG_WIN32, "win,dwm: set window attributes.");
}
#endif

inline void WindowClearBuffer(GLFWwindow* window, const ImVec4& color) {
    // clear window buffer.
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwMakeContextCurrent(window);
    glfwSwapBuffers(window);
}

namespace SystemWindow {
    static void GLFW_ERROR_CALLBACK(int error, const char* description) {
        // glfw error_output callback print.
        PushLogger(LogError, "GLFW_SYSTEM", "glfw error code: %d message: %s", 
            error, description);
    }
    StaticStrLABEL SYSTEM_LOG_TAG_DRAW = "WINDOW_DRAW";

    bool SystemWindowRenderer::InitCreateOpenGL(
        ImVec2 size, bool vsync, uint32_t msaa, const string& name
    ) {
        // glfw init create.
        glfwSetErrorCallback(GLFW_ERROR_CALLBACK);
        if (!glfwInit()) {
            PushLogger(LogError, SYSTEM_LOG_TAG_DRAW, "glfw: global init failed.");
            return false;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // glfw.version 3.2+
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);         // glfw.version 3.0+

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // fixed window size.
        glfwWindowHint(GLFW_SAMPLES, msaa);         // 4x Samples MSAA.
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

        // create window monitor.
        GLFWmonitor* CreateMonitor = {};

        size.x = size.x < 256.0f ? 256.0f : size.x;
        size.y = size.y < 256.0f ? 256.0f : size.y;
        WindowObject = glfwCreateWindow((int)size.x, (int)size.y, name.c_str(), CreateMonitor, nullptr);

        // create glfw context.
        glfwMakeContextCurrent(WindowObject);
        // setting async. (设置垂直同步)
        glfwSwapInterval((int)vsync);

        // check glew library status.
        if (glewInit() != GL_NO_ERROR) {
            PushLogger(LogError, SYSTEM_LOG_TAG_DRAW, "glew: global init failed.");
            return false;
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }

    bool SystemWindowRenderer::InitCreateImGui(const string& shader, const string& f, float fsz) {
        IMGUI_CHECKVERSION();
        // check font file path valid.
        if (!filesystem::exists(f)) {
            PushLogger(LogError, SYSTEM_LOG_TAG_DRAW,
                "imgui: fonts file path invalid: %s", f.c_str());
            return false;
        }
        float FontsSize = fsz < 1.0f ? 1.0f : fsz;

        // setup imgui context.
        ImGui::CreateContext();
        ImGuiIO& GuiIO = ImGui::GetIO(); (void)GuiIO;

        // enable keyboard & gamepad controls.
        GuiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        GuiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // imgui load font.
        auto ImGuiFontLoad = ImGui::GetIO().Fonts;
        auto FontLoad = ImGuiFontLoad->GetGlyphRangesChineseFull();
        ImGuiFontLoad->AddFontFromFileTTF(f.c_str(), FontsSize, NULL, FontLoad);
        // imgui set global font scale.
        ImGui::GetIO().FontGlobalScale = 0.78f;

        // setup platform & renderer backends.
        ImGui_ImplGlfw_InitForOpenGL(WindowObject, true);
        ImGui_ImplOpenGL3_Init(shader.c_str());
        return true;
    }

    void SystemWindowRenderer::FreeRendererContext() {
        // free graph: imgui context.
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // free graph: window context.
        glfwDestroyWindow(WindowObject);
        glfwTerminate();
    }

    SystemWindowRenderer::SystemWindowRenderer(
        const string& name, const ImVec2& size, bool vsync, uint32_t msaa,
        const string& shader, const string& font, float font_size
    ) {
        bool InitErrorFlags = false;

        InitErrorFlags |= !InitCreateOpenGL(size, vsync, 4, name);
        InitErrorFlags |= !InitCreateImGui(shader, font, font_size);

        InitErrorFlags != true ? 
            PushLogger(LogInfo,  SYSTEM_LOG_TAG_DRAW, "render context init.") :
            PushLogger(LogError, SYSTEM_LOG_TAG_DRAW, "render context params err.");
    }

    SystemWindowRenderer::~SystemWindowRenderer() {
        FreeRendererContext();
        PushLogger(LogInfo, SYSTEM_LOG_TAG_DRAW, "render context destroy.");
    }

    void SystemWindowRenderer::SettingColorBackground(const ImVec4& color) {
        WindowClearBuffer(WindowObject, color);
    }
    void SystemWindowRenderer::SettingColorBorder(const ImVec4& color) {
#if defined(_WIN32) && defined(SYSTEM_ENABLE_WINAPI)
        auto WIN_COLOR = RGB(float(255.0 * color.x), float(255.0 * color.y), float(255.0 * color.z));
        SetSystemWindowStyleDark(WindowObject, WIN_COLOR);
#endif
    }
    void SystemWindowRenderer::SettingTransparencyBlur() {
#if defined(_WIN32) && defined(SYSTEM_ENABLE_WINAPI)
        // win api enable transparency & blur.
        EnableWindowsTransparency(glfwGetWin32Window(WindowObject));
        EnableWindowsBlur(glfwGetWin32Window(WindowObject));
#endif
    }

    void SystemWindowRenderer::SettingGuiDrawObject(GuiRenderInterface* object, void* exten) {
        if (object == nullptr) {
            PushLogger(LogError, SYSTEM_LOG_TAG_DRAW, "render object ptr invalid.");
            return;
        }
        GuiDrawObject = object;
        // call => render event init func.
        if (!object->RenderEventInit(exten))
            ErrorCode = STARTER_GRAPH_STATE_O_INIT;
    }

    void SystemWindowRenderer::SettingGuiIconFiles(const string& image) {
        GLFWimage ImageFormatInfo = {};
        int ImageChannels = NULL;
        // check icon image file path valid.
        if (filesystem::exists(image)) {
            ImageFormatInfo.pixels = stbi_load(
                image.c_str(), &ImageFormatInfo.width, &ImageFormatInfo.height, &ImageChannels, NULL
            );
            // load image => program icon => free image data.
            glfwSetWindowIcon(WindowObject, 1, &ImageFormatInfo);
            stbi_image_free(ImageFormatInfo.pixels);
            return;
        }
        ErrorCode = STARTER_GRAPH_STATE_ICON;
    }

    int32_t SystemWindowRenderer::RendererRun() {
        // E: run event draw_object is nullptr
        ErrorCode = GuiDrawObject == nullptr ? STARTER_GRAPH_STATE_NULLPTR : ErrorCode;
        // E: init error status invalid
        if (ErrorCode != STARTER_GRAPH_STATE_NOERR) return ErrorCode;

        int32_t BufferSize[2] = {};
        // opengl & imgui render_loop.
        while (!(bool)glfwWindowShouldClose(WindowObject)) {
            // rendering event_loop.
            glfwPollEvents();
            glfwGetFramebufferSize(WindowObject, &BufferSize[0], &BufferSize[1]);

            glViewport(0, 0, BufferSize[0], BufferSize[1]);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            {
                GuiDrawObject->RenderEventLoop();
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwMakeContextCurrent(WindowObject);
            glfwSwapBuffers(WindowObject);
        }
        GuiDrawObject->RenderEventFree();
        size_t HashThreadID = (size_t)hash<thread::id>{}(this_thread::get_id());
        PushLogger(LogInfo, SYSTEM_LOG_TAG_DRAW,
            "render context event loop exit, hash: %u", HashThreadID);
        return STARTER_GRAPH_STATE_NOERR;
    }
}

inline string FileStringContent(const string& path) {
    ifstream FileRead(path);
    // read file(open) status.
    if (!FileRead.is_open()) {
        PushLogger(LogError, "FILE_LOAD", "failed open(read) file path.");
        return string();
    }
    // read source string data.
    string Content((istreambuf_iterator<char>(FileRead)), istreambuf_iterator<char>());
    FileRead.close();
    PushLogger(LogInfo, "FILE_LOAD", "read file path: %s", path.c_str());
    PushLogger(LogInfo, "FILE_LOAD", "read content size: %u", Content.size());
    return Content;
}

inline ImVec4 ArrayToImVecColor(const array<float, 4>& color) {
    return ImVec4(color[0], color[1], color[2], color[3]);
}

void SYSTEM_WINDOW_CREATE(
    const string& file, 
    SystemWindow::SystemWindowRenderer** renderer,
    SystemWindow::GuiRenderInterface* comp
) {
    SystemWindow::SystemWindowConifg 
        ConfigParams = nlohmann::json::parse(FileStringContent(file));
    ImVec2 WindowContextSize = ImVec2(
        (float)ConfigParams.WindowSizeWidth,
        (float)ConfigParams.WindowSizeHeight
    );
    // create window component.
    *renderer = new SystemWindow::SystemWindowRenderer(
        GLOBAL_CONFIG_TITLE,
        WindowContextSize,
        ConfigParams.WindowVsync, ConfigParams.WindowMSAA,
        ConfigParams.ImGuiShaderVersion,
        ConfigParams.ImGuiFontPath, ConfigParams.ImGuiFontSize
    );
    (*renderer)->SettingGuiIconFiles(GLOBAL_CONFIG_LOGO);
    // exten window style config.
    (*renderer)->SettingColorBackground(ArrayToImVecColor(ConfigParams.ExtWinStyleColor));
    (*renderer)->SettingColorBorder(ArrayToImVecColor(ConfigParams.ExtWinStyleBorder));
    // exten stle: win,dwm enable transparency-blur.
    if (ConfigParams.ExtWinStyleEnableBlur) (*renderer)->SettingTransparencyBlur();

    // create draw gui component.
    (*renderer)->SettingGuiDrawObject(comp);
}