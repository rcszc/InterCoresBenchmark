// window_graphics. 2025_04_10 RCSZ.

#ifndef __WINDOW_GRAPHICS_H
#define __WINDOW_GRAPHICS_H
#include <functional>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")
#endif
#include "nlohmann/json.hpp"

#include "../psag_system_logger.hpp"
#include "../shared_define.h"
#include "window_graphics_imgui.hpp"

#define SYSTEM_ENABLE_WINAPI
namespace SystemWindow {
	// render system error code flags.
#define STARTER_GRAPH_STATE_NOERR    0
#define STARTER_GRAPH_STATE_NULLPTR -2
#define STARTER_GRAPH_STATE_ICON    -3
#define STARTER_GRAPH_STATE_O_INIT  -4
#define STARTER_GRAPH_STATE_O_FREE  -5

	// renderer draw gui interface.
	class GuiRenderInterface {
	public:
		virtual bool RenderEventInit(void* config) = 0;
		virtual void RenderEventLoop() = 0;
		virtual bool RenderEventFree() = 0;
	};

	struct SystemWindowConifg {
		uint32_t    WindowSizeWidth;
		uint32_t    WindowSizeHeight;
		bool        WindowVsync;
		uint32_t    WindowMSAA;

		std::string ImGuiShaderVersion;
		std::string ImGuiFontPath;
		float       ImGuiFontSize;

		std::array<float, 4> ExtWinStyleBorder;
		std::array<float, 4> ExtWinStyleColor;
		bool                 ExtWinStyleEnableBlur;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(
			SystemWindowConifg,
			WindowSizeWidth, WindowSizeHeight,
			WindowVsync, WindowMSAA,
			ImGuiShaderVersion, ImGuiFontPath, ImGuiFontSize,
			ExtWinStyleBorder, ExtWinStyleColor,
			ExtWinStyleEnableBlur
		)
		// REGISTER NLOHMANN JSON.
	};

	class SystemWindowRenderer {
	protected:
		GuiRenderInterface* GuiDrawObject = nullptr;
		GLFWwindow* WindowObject = nullptr;

		int32_t ErrorCode = STARTER_GRAPH_STATE_NOERR;

		bool InitCreateOpenGL(ImVec2 size, bool vsync, uint32_t msaa, const std::string& name);
		bool InitCreateImGui(const std::string& shader, const std::string& f, float fsz);

		void FreeRendererContext();
	public:
		SystemWindowRenderer(
			const std::string& name, const ImVec2& size, bool vsync, uint32_t msaa,
			const std::string& shader, const std::string& font, float font_size
		);
		~SystemWindowRenderer();

		void SettingColorBackground (const ImVec4& color);
		void SettingColorBorder     (const ImVec4& color);
		void SettingTransparencyBlur();

		void SettingGuiDrawObject(
			GuiRenderInterface* object, void* exten = nullptr
		);
		void SettingGuiIconFiles(const std::string& image);

		int32_t RendererRun();
	};
}
void SYSTEM_WINDOW_CREATE(
	const std::string& file, 
	SystemWindow::SystemWindowRenderer** renderer, 
	SystemWindow::GuiRenderInterface* comp
);

#endif