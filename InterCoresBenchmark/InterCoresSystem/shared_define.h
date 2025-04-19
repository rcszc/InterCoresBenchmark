// shared_define. 2025_04_11 RCSZ.
// window & benchmark shared.

#ifndef __SHARED_DEFINE_H
#define __SHARED_DEFINE_H
#include <vector>
#if defined(_MSC_VER)
#pragma warning(disable : 26813)
#endif
#include <stdint.h>

constexpr const char* GLOBAL_CONFIG_JSON  = "Configs/window_panel_config.json";
constexpr const char* GLOBAL_CONFIG_LOGO  = "Configs/inter_cores_benchmark_logo.png";
constexpr const char* GLOBAL_CONFIG_TITLE = "InterCores-Benchmark RCSZ(C)";

constexpr size_t TESTING_BLOCKS_GROUP = 1280;

struct BenchmarkExport {
	float TicksMax = 0.0f, TicksMin = 0.0f;
	float TimesMax = 0.0f, TimesMin = 0.0f;

	std::vector<std::vector<float>> CoresInterTicks = {};
	std::vector<std::vector<float>> CoresInterTime  = {};

	// copy speed: ns_block, ticks speed: mhz_ps.
	float AverageCopySpeed  = 0.0f;
	float AverageTicksSpeed = 0.0f; 
};

inline size_t FAST_SAFE_DIV(size_t a, size_t b) {
	size_t SAFE_V = b | (b == 0);
	return a / SAFE_V;
}
#endif