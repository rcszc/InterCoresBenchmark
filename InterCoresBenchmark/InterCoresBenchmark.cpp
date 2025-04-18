// InterCoresBenchmark. 2025_04_10 By RCSZ.
// library: opengl, glfw, glew, imgui
// cpu inter cores speed benchmark.
// update: 2025_04_19, version: 0.1.1.0419 rcsz.
 
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "glew32.lib")

#include "InterCoresSystem/system_window/window_graphics.h"
#include "InterCoresSystem/system_benchmark/benchmark_inter_cores.h"

#include <random>

using namespace std;
using namespace PSAG_LOGGER;

inline ImVec4 GetRainbowColormapColor(float value) {
	struct ColorNode {
		float pos = 0.0f; ImVec4 color = {};
	};
	static const ColorNode ColorMap[] = {
		{ 0.0f,  ImVec4(0.0f, 0.0f, 1.0f, 1.0f) }, // BLUE
		{ 0.25f, ImVec4(0.0f, 1.0f, 1.0f, 1.0f) }, // CYAN
		{ 0.5f,  ImVec4(0.0f, 1.0f, 0.0f, 1.0f) }, // GREEN
		{ 0.75f, ImVec4(1.0f, 1.0f, 0.0f, 1.0f) }, // YELLOW
		{ 1.0f,  ImVec4(1.0f, 0.0f, 0.0f, 1.0f) }, // RED
	};
	value = clamp(value, 0.0f, 1.0f);
	for (int i = 0; i < 4; ++i) {
		if (value >= ColorMap[i].pos && value <= ColorMap[i + 1].pos) {
			float t = (value - ColorMap[i].pos) / (ColorMap[i + 1].pos - ColorMap[i].pos);
			ImVec4 c1 = ColorMap[i].color;
			ImVec4 c2 = ColorMap[i + 1].color;

			return ImVec4(c1.x + (c2.x - c1.x) * t, 
				c1.y + (c2.y - c1.y) * t, c1.z + (c2.z - c1.z) * t, 1.0f);
		}
	}
	return ColorMap[4].color;
}

inline float VALUE_LERP(float v, float t) {
	return (v - t) * 0.042f;
}

constexpr float OperWindowHeight = 68.0f;
constexpr float BarWindowHeight  = 18.0f;

enum SystemTestingState {
	TestingClose  = 1 << 0,
	TestingStart  = 1 << 1,
	TestingDelete = 1 << 2
};
class GuiPanelDraw :public SystemWindow::GuiRenderInterface {
private:
	vector<float> RainbowRuler = {};

	SystemTestingState TestingStatus = TestingClose;
	BenchmarkController* TestingController = nullptr;
	float TestingBenchmark = 0.0f;
protected:
	BenchmarkExport ExportData = {};
	vector<vector<float>>* DataReference = &ExportData.CoresInterTime;
	ImVec2 DataValueLimit = {};

	void DrawComponentColorBar(
		const ImVec2& size, const ImVec2& limit, float* data, size_t count,
		float draw_grid_space = 0.0f, float draw_front = 0.0f
	) {
		ImGui::BeginChild("##COLBAR", size);
		// calculate offset params.
		float PosOffset = 0.0f;
		float LenOffset = (size.x - draw_front) / (float)count;
		ImVec2 RectSize(LenOffset - draw_grid_space, size.y);
		float LimitRuler = limit.y - limit.x;
		for (size_t i = 0; i < count; ++i) {
			ListDrawRectangleFill(
				ImVec2(PosOffset + draw_front, 0.0f), RectSize,
				GetRainbowColormapColor((data[i] - limit.x) / LimitRuler)
			);
			PosOffset += LenOffset;
		}
		ImGui::EndChild();
	}

	void DrawComponentXRuler(
		const ImVec2& limit, float scale, size_t count, const ImVec4& color
	) {
		ImGui::BeginChild("RULER");
		float RulerOffset = 
			(ImGui::GetWindowWidth() - IMGUI_ITEM_SPAC * 2.0f) / (float)count;
		float RulerScale = (limit.y - limit.x) / (float)count;
		for (size_t i = 1; i < count; ++i) {
			float FCount = (float)i;
			ListDrawLine(
				ImVec2(RulerOffset * FCount, 0.0f),
				ImVec2(RulerOffset * FCount, scale),
				color, 3.2f);
			ListDrawCenterText(
				ImVec2(RulerOffset * (float)i, scale * 2.25f), 
				color, "%.0f", limit.x + RulerScale * FCount);
		}
		ImGui::EndChild();
	}

	void DrawOperateWindow() {
		ImGui::SetNextWindowPos(ImVec2(IMGUI_ITEM_SPAC, IMGUI_ITEM_SPAC));
		ImGui::SetNextWindowSize(ImVec2(
			ImGui::GetIO().DisplaySize.x - IMGUI_ITEM_SPAC * 2.0f,
			OperWindowHeight - IMGUI_ITEM_SPAC * 2.0f
		));
		ImGuiWindowFlags WindowFlags =
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
		ImGui::Begin("##OPER_WIN", (bool*)0, WindowFlags);

		ImVec2 ButtonSize(142.0f, OperWindowHeight - IMGUI_ITEM_SPAC * 4.0f);
		ImGui::ProgressBar(TestingBenchmark, ImVec2(256.0f, ButtonSize.y));

		switch (TestingStatus) {
		case(TestingClose):
			ImGui::SameLine();
			if (ImGui::Button("START", ButtonSize)) {
				TestingController = new BenchmarkController();
				TestingController->BenchmarkStatusCreate();
				TestingStatus = TestingStart;
			}
			ImGui::SameLine();
			if (ImGui::Button("Time", ButtonSize * ImVec2(0.5f, 1.0f))) {
				DataReference = &ExportData.CoresInterTime;
				DataValueLimit = ImVec2(ExportData.TimesMin, ExportData.TimesMax);
			}
			ImGui::SameLine();
			if (ImGui::Button("Tick", ButtonSize * ImVec2(0.5f, 1.0f))) {
				DataReference = &ExportData.CoresInterTicks;
				DataValueLimit = ImVec2(ExportData.TicksMin, ExportData.TicksMax);
			}
			break;
		case(TestingStart):
			TestingStatus = TestingController->BenchmarkRunStep() == true ?
				TestingDelete : TestingStart;
			TestingBenchmark += VALUE_LERP(
				TestingController->BenchmarkProgress, TestingBenchmark
			);
			break;
		case(TestingDelete):
			ExportData = TestingController->BenchmarkDataExport();
			TestingController->BenchmarkStatusDelete();
			delete TestingController;
			// reset system status.
			TestingBenchmark = 1.0f;
			TestingStatus = TestingClose;
			DataValueLimit = ImVec2(ExportData.TimesMin, ExportData.TimesMax);
			DataReference = &ExportData.CoresInterTime;
			break;
		}
		ImGui::SameLine();
		float Space = ButtonSize.x * 2.0f;
		float TextDraw = Space + ImGui::GetWindowWidth() * 0.5f;
		ListDrawCenterText(
			ImVec2(TextDraw, ImGui::GetWindowHeight() * 0.5f),
			ImVec4(0.38f, 0.0f, 0.78f, 0.72f),
			" Ticks Limit: (%.0f,%.0f) Times Limit: (%.2f,%.2f)",
			ExportData.TicksMin, ExportData.TicksMax, 
			ExportData.TimesMin, ExportData.TimesMax
		);
		ImGui::Button("?", ImVec2(32.0f, ButtonSize.y));
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::SetWindowFontScale(1.18f);
			ImGui::Text("Global Logic Cpu Cores: %u", thread::hardware_concurrency());
			ImGui::Text("Global: Ticks Min %.0f cycles, Max %.0f cycles", ExportData.TicksMin, ExportData.TicksMax);
			ImGui::Text("Global: Times Min %.3f ns, Max %.3f ns", ExportData.TimesMin, ExportData.TimesMax);
			ImGui::Text("Global: Avg Cache Speed %.3f ns/blocks", ExportData.AverageCopySpeed);
			ImGui::Text("Global: Avg RDTSC Freq %.3f mhz", ExportData.AverageTicksSpeed);
			ImGui::EndTooltip();
		}
		ImGui::End();
	}
public:
	bool RenderEventInit(void* config) override {
		for (size_t i = 0; i < 192; ++i)
			RainbowRuler.push_back((float)i / 192.0f);
		return true;
	}

	void RenderEventLoop() override {
		int ImGuiItemsAllocID = 0;

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    5.4f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   5.4f);

		ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.0f, 0.0f, 0.0f, 0.32f));
		ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.0f, 1.0f, 0.92f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.0f, 0.48f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.0f, 0.84f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.22f, 0.0f, 0.42f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.14f, 0.0f, 0.24f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.32f, 0.0f, 0.84f, 1.0f));

		DrawOperateWindow();

		ImGui::SetNextWindowPos(ImVec2(IMGUI_ITEM_SPAC, OperWindowHeight));
		ImGui::SetNextWindowSize(ImVec2(
			ImGui::GetIO().DisplaySize.x - IMGUI_ITEM_SPAC * 2.0f,
			ImGui::GetIO().DisplaySize.y - IMGUI_ITEM_SPAC - OperWindowHeight
		));
		ImGuiWindowFlags WindowFlags =
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
		ImGui::Begin("##VIEW_WIN", (bool*)0, WindowFlags);

		ImGui::PushID(++ImGuiItemsAllocID);
		DrawComponentColorBar(
			ImVec2(ImGui::GetWindowWidth() - IMGUI_ITEM_SPAC * 2.0f, BarWindowHeight),
			ImVec2(0.0f, 1.0f),
			RainbowRuler.data(), RainbowRuler.size()
		);
		ImGui::PopID();

		ImGui::SetCursorPosY(BarWindowHeight + IMGUI_ITEM_SPAC * 2.0f);
		DrawComponentXRuler(DataValueLimit,
			12.0f, 10, ImVec4(0.0f, 1.0f, 0.85f, 0.85f)
		);
		float GridSpace = (float)FAST_SAFE_DIV(128, DataReference->size() - 1);

		size_t CoreNumberCount = 0;
		float PosOffset = 78.0f;
		for (auto& DataItem : *DataReference) {
			ImGui::SetCursorPosY(PosOffset);
			ImGui::PushID(++ImGuiItemsAllocID);
			DrawComponentColorBar(
				ImVec2(ImGui::GetWindowWidth() - IMGUI_ITEM_SPAC * 2.0f, BarWindowHeight),
				DataValueLimit, DataItem.data(), DataItem.size(), GridSpace, 42.0f
			);
			ImGui::PopID();
			ListDrawText(
				ImVec2(IMGUI_ITEM_SPAC, PosOffset),
				ImVec4(0.0f, 1.0f, 0.85f, 0.85f),
				"%u", CoreNumberCount++
			);
			PosOffset += BarWindowHeight + GridSpace;
		}
		ImGui::End();
		ImGui::PopStyleColor(7);
		ImGui::PopStyleVar(3);
	}

	bool RenderEventFree() override {
		return true;
	}
};

int main() {
	PSAG_LOGGER::SET_PRINTLOG_STATE(true);
	PSAG_LOGGER::SET_PRINTLOG_COLOR(true);
	PSAG_LOGGER_PROCESS::StartLogProcessing("Logs/");

	SystemWindow::SystemWindowRenderer* SystemRenderer = nullptr;
	GuiPanelDraw* SystemGuiComponent = new GuiPanelDraw();
	// create init config components.
	SYSTEM_WINDOW_CREATE(
		GLOBAL_CONFIG_JSON, &SystemRenderer, SystemGuiComponent
	);
	SystemRenderer->RendererRun();

	PSAG_LOGGER_PROCESS::FreeLogProcessing();
}