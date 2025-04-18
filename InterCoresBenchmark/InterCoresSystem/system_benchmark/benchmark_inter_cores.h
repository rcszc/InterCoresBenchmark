// benchmark_inter_cores. 2025_04_11 RCSZ.
// inter(interconnection)

#ifndef __BENCHMARK_INTER_CORES_H
#define __BENCHMARK_INTER_CORES_H
#if defined(_WIN32)
#include <windows.h>
#endif
#include <array>
#include "../shared_define.h"
#include "toolkits_thread_pool.hpp"

namespace SystemInterCores {
#if defined(_WIN32)
    // this thread =bind=> phy processors.
    inline bool ThisThreadPhyBind(uint32_t core) {
        SYSTEM_INFO SystemInfo = {};
        GetSystemInfo(&SystemInfo);

        // check system phy cpu(s).
        if (core >= SystemInfo.dwNumberOfProcessors)
            return false;
        DWORD_PTR CORE_MASK = (1ULL << core);
        HANDLE THREAD = GetCurrentThread();
        DWORD_PTR RESULT = SetThreadAffinityMask(THREAD, CORE_MASK);

        return RESULT != 0;
    }
	inline uint64_t READ_CPU_RDTSC() {
		return (uint64_t)__rdtsc();
	}
	// BENCHMARK 设置线程为最高优先级.
	inline void T_BENCHMARK() { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); }
	inline void T_NORMAL() { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL); }
#endif
	template<typename T>
	// thread-safe no mutex buffers.
	struct DataDoubleBuffer {
	protected:
		std::atomic<bool> DataBuffersIdx = false;
		std::atomic<bool> DataBcakStatus = true;
		T DataBuffers[2] = {};
	public:
		T* GetFrontBuffer() { return &DataBuffers[(size_t)DataBuffersIdx]; }
		T* GetBackBuffer()  { return &DataBuffers[(size_t)!DataBuffersIdx]; }

		// front buf thread control.
		bool SwapBuffers() {
			// check read status: true: [read_ok].
			if (!DataBcakStatus) return false;
			DataBuffersIdx = !DataBuffersIdx;
			DataBcakStatus = false;
			return true;
		}
		// read begin -> context -> end, swap buffers.
		bool BackBufferReadBegin() { return !DataBcakStatus; }
		void BackBufferReadEnd()   { DataBcakStatus = true; }
	};

#define BENCHMARK_BLOCK_BYTES 0x4000
#define BENCHMARK_BLOCK_LINES 0x20
	// cache line stack memory.
	struct alignas(BENCHMARK_BLOCK_LINES) BenchmarkDataBlock {
		uint8_t DataBlockBytes[BENCHMARK_BLOCK_BYTES] = {};
	};

	struct alignas(32) SampleDataGroup {
		std::string SmpGroupMessage = {};
		std::vector<bool> SampleCheckFlag = {};

		std::vector<uint64_t> SampleCountTIMER = {};
		std::vector<uint64_t> SampleCountRDTSC = {};
		// thread bind cpu.
		size_t BindLogicCoreCount = 0;

		size_t CacheOperBlock  = 0;
		size_t CacheOperNumber = 0;
	};

	// inter point to point double channels.
	// channel: double buffers.
	class InterTaskDChannels {
	protected:
		static DataDoubleBuffer<BenchmarkDataBlock> RxDataChannel;
		static DataDoubleBuffer<BenchmarkDataBlock> TxDataChannel;
	};

	class InterTaskSampleData {
	private:
		static std::vector<SampleDataGroup> SamplesDataset;
		static std::mutex SamplesDatasetMtx;
	protected:
		void SampleDataADD(const SampleDataGroup& data);
		std::vector<SampleDataGroup> SampleDatasetREAD();
		void SampleDatasetCLEAR();
	};

	enum StatusFlagCode {
		STATE_FLAG_CODE_EXIT   = 1 << 0,
		STATE_FLAG_CODE_WAIT   = 1 << 1,
		STATE_FLAG_CODE_RXTXFB = 1 << 2,
		STATE_FLAG_CODE_RXTXBF = 1 << 3
	};

	struct ControlHandle {
		std::atomic<StatusFlagCode>* WorkerStatus = nullptr;
		std::condition_variable* WorkerNotify = nullptr;

		constexpr ControlHandle() : 
			WorkerStatus(nullptr), WorkerNotify(nullptr) {}
		constexpr ControlHandle(
			std::atomic<StatusFlagCode>* ws, std::condition_variable* wn
		) : WorkerStatus(ws), WorkerNotify(wn) {}
	};

	class InterWorkerThread :
		public InterTaskDChannels, public InterTaskSampleData {
	protected:
		std::mutex WorkerMutex = {};
		std::atomic<StatusFlagCode> WorkerStatus = {};
		std::condition_variable WorkerNotify = {};
	public:
		ControlHandle GetControlHandle() {
			return ControlHandle(&WorkerStatus, &WorkerNotify);
		}
		size_t DataBlocksNumber = 0;
		bool InterWorkerRun(uint32_t logiccore);
	};

	class InterTaskSampler {
	protected:
		SampleDataGroup SampleDataTemp = {};

		std::chrono::steady_clock::time_point SamplerTIMER = 
			std::chrono::steady_clock::now();
		uint64_t SamplerRDTSC = 0;
	public:
		InterTaskSampler(const std::string& msg, size_t params[3]) {
			SampleDataTemp.SmpGroupMessage = msg;
			SampleDataTemp.BindLogicCoreCount = params[0];
			// config cache block bytes & number.
			SampleDataTemp.CacheOperBlock  = params[1];
			SampleDataTemp.CacheOperNumber = params[2];
		}
		SampleDataGroup GetSampleData() const {
			return SampleDataTemp;
		}
		void SamplerContextBegin();
		void SamplerContextEnd();
		void SamplerDataCheckFlag(bool flag);
	};

	struct InterBenchmarkPoint {
		size_t TxLogicCore = 0;

		double AverageTransmCycles = 0.0f;
		double AverageTransmNs     = 0.0f;
	};
	struct InterBenchmarkReport {
		size_t BenchmarkLogicCore = 0;
		std::vector<InterBenchmarkPoint> LogicCoresSpeed = {};
	};

	using WorkersPair = std::pair<size_t, size_t>;
	// inter tasks (virtual cores).
	class InterTasksGlobal :public InterTaskSampleData {
	private:
		std::vector<ControlHandle> WorkerHandles = {};
		std::vector<std::thread>   WorkerThreads = {};
	protected:
		// current workers, inter pair index.
		WorkersPair CurrentWorkers = {};

		InterBenchmarkReport BenchmarkReport = {};
		bool DataProcess(size_t bench_cpu);

		void CreateWorkerThread(size_t blocks, size_t logiccore);
		void DeleteWorkerThreadAll();
		// true: worker is runing.
		bool WorkersTaskStatus();
		bool StartWorkersTaskPair(const WorkersPair& indexes);
	public:
		InterTasksGlobal();
	};
}

class BenchmarkController :public SystemInterCores::InterTasksGlobal {
protected:
	std::vector<SystemInterCores::InterBenchmarkReport>
		ReportsDataset = {};
	size_t SystemCpuLogicCores = 0;
	size_t CoreCountRx = 0;
	size_t CoreCountTx = 0;
	// cores inter oper_count.
	size_t SystemOperations = 0;
public:
	bool BenchmarkStatusCreate();
	float BenchmarkProgress = 0.0f;
	// return true: run benchmark setp end.
	bool BenchmarkRunStep();
	void BenchmarkStatusDelete();

	// data process, calculate, format.
	BenchmarkExport BenchmarkDataExport();
};


#endif