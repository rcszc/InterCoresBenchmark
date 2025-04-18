// benchmark_inter_cores_system.
#include <random>
#include "benchmark_inter_cores.h"

using namespace std;
using namespace PSAG_LOGGER;

#include <immintrin.h>

inline size_t AutoWindowLen(size_t N) {
	return clamp((size_t)sqrt(N), (size_t)3, (size_t)100);
}

inline bool CheckMemoryBlock(
	SystemInterCores::BenchmarkDataBlock* data
) {
	mt19937_64 RandomGen(chrono::duration_cast<chrono::nanoseconds>(
		chrono::steady_clock::now().time_since_epoch()
	).count());
	uniform_int_distribution<int> Distribution(0, BENCHMARK_BLOCK_BYTES - 1);
	return data->DataBlockBytes[0] == 
		data->DataBlockBytes[(size_t)Distribution(RandomGen)];
}

struct DataPoint {
	double DenScore = 0.0; // density score
	double Value    = 0.0; // point value
	size_t OriIndex = 0;   // original index
	constexpr DataPoint() : OriIndex(0), DenScore(0.0), Value(0.0) {}
	constexpr DataPoint(double ds, double v, size_t o) :
		DenScore(ds), Value(v), OriIndex(o)
	{}
};
vector<double> DataFilterOutliers(
	const vector<double>& data, double keep_ratio, int window = 10
) {
	size_t WinLength = AutoWindowLen(data.size());
	size_t LEN = data.size();
	if (LEN == 0) return vector<double>();

	vector<DataPoint> DataPoints = {};
	for (size_t i = 0; i < LEN; ++i) 
		DataPoints.push_back(DataPoint(0.0, data[i], i));
	// data points value sorting.
	sort(DataPoints.begin(), DataPoints.end(), 
		[](const DataPoint& a, const DataPoint& b) { 
			return a.Value < b.Value; 
		});
	for (size_t i = 0; i < LEN; ++i) {
		size_t RIGHT = min(LEN - 1, i + WinLength);
		size_t LEFT  = max(0, i - WinLength);
		size_t COUNT = 0;
		double DistSum = 0.0;
		for (size_t j = LEFT; j <= RIGHT; ++j) {
			if (i == j) continue;
			DistSum += abs(DataPoints[i].Value - DataPoints[j].Value);
			COUNT++;
		}
		if (COUNT > 0) {
			DataPoints[i].DenScore = 1.0 / (DistSum / COUNT);
			continue;
		}
		DataPoints[i].DenScore = 0.0;
	}
	// data points density_score sorting.
	sort(DataPoints.begin(), DataPoints.end(), 
		[](const DataPoint& a, const DataPoint& b) { 
			return a.DenScore > b.DenScore; 
		});
	size_t KeepCount = (size_t)round(keep_ratio * LEN);
	vector<double> Filtered = {};
	for (size_t i = 0; i < KeepCount; ++i)
		Filtered.push_back(DataPoints[i].Value);
	return Filtered;
}

inline vector<double> VectorDataConvert(const vector<uint64_t>& data) {
	vector<double> DoubleVector(data.size());
	transform(data.begin(), data.end(), DoubleVector.begin(),
		[](uint64_t value) { return (double)value; }
	);
	return DoubleVector;
}

inline double DataCalculateAverage(const vector<double>& data) {
	double ValuesSum = 0.0;
	for (auto Value : data) ValuesSum += Value;
	return ValuesSum / (double)data.size();
}

namespace SystemInterCores {
	StaticStrLABEL SYSTEM_LOG_TAG_WORKER = "INTER_WORKER";

	DataDoubleBuffer<BenchmarkDataBlock> InterTaskDChannels::RxDataChannel = {};
	DataDoubleBuffer<BenchmarkDataBlock> InterTaskDChannels::TxDataChannel = {};

	vector<SampleDataGroup> InterTaskSampleData::SamplesDataset = {};
	mutex InterTaskSampleData::SamplesDatasetMtx = {};

	bool InterWorkerThread::InterWorkerRun(uint32_t phy_core) {
		size_t THIS_THASH = SystemThreads::ThisThreadID();

		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"inter worker event start, thread hash: %u", THIS_THASH);
#if defined(_WIN32)
		if (!ThisThreadPhyBind(phy_core)) {
			PSAG_LOGGER::PushLogger(LogError, SYSTEM_LOG_TAG_WORKER,
				"failed bind system phy core.");
			return false;
		}
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"system bind physical core: %u", phy_core);
#else
		PSAG_LOGGER::PushLogger(LogWarning, SYSTEM_LOG_TAG_WORKER,
			"not system api bind physical core.");
#endif
		// inter worker proc_loop. 
		while (WorkerStatus != STATE_FLAG_CODE_EXIT) {
			WorkerStatus = STATE_FLAG_CODE_WAIT;

			unique_lock<mutex> ULOCK(WorkerMutex);
			WorkerNotify.wait(ULOCK, [&] { 
				return WorkerStatus != STATE_FLAG_CODE_WAIT || 
					WorkerStatus == STATE_FLAG_CODE_EXIT;
			});
			ULOCK.unlock();

			auto RX_DATA_PROC_FUNC = [&]() {
				size_t BlocksCount = 0;
				while (BlocksCount < DataBlocksNumber) {
					if (RxDataChannel.SwapBuffers()) {
						auto BufferRef = RxDataChannel.GetFrontBuffer();
						// generate randon bytes fill code.
						mt19937_64 RandomGen(chrono::duration_cast<chrono::nanoseconds>(
							chrono::steady_clock::now().time_since_epoch()
						).count());
						uniform_int_distribution<int> Distribution(0, 255);
						// write cpu core cache blocks.
						memset(BufferRef->DataBlockBytes, (int)Distribution(RandomGen),
							BENCHMARK_BLOCK_BYTES);
						++BlocksCount;
					}
				}
				PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
					"thread: %u, rx data operation end.", THIS_THASH);
			};

			auto TX_DATA_PROC_FUNC = [&]() {
				size_t InitParams[3] = { phy_core, BENCHMARK_BLOCK_BYTES, DataBlocksNumber };
				InterTaskSampler DataSampler(
					"RXTX-BACK-FRONT THREAD " + to_string(THIS_THASH),
					InitParams
				);
				BenchmarkDataBlock PrivateCache = {};

				size_t BlocksCount = 0;
				while (BlocksCount < DataBlocksNumber) {
					if (RxDataChannel.BackBufferReadBegin()) {
						DataSampler.SamplerContextBegin();
						// channel data copy to private.
						memcpy(
							PrivateCache.DataBlockBytes,
							RxDataChannel.GetBackBuffer()->DataBlockBytes,
							BENCHMARK_BLOCK_BYTES
						);
						DataSampler.SamplerContextEnd();
						RxDataChannel.BackBufferReadEnd();
						// add check flag: avoid optimize cache.
						DataSampler.SamplerDataCheckFlag(CheckMemoryBlock(&PrivateCache));
						++BlocksCount;
					}
				}
				PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
					"thread: %u, tx data operation end.", THIS_THASH);
				SampleDataADD(DataSampler.GetSampleData());
			};

			T_BENCHMARK();
			switch (WorkerStatus) {
			// buffer: rx-ch -> front, tx-ch -> back.
			case(STATE_FLAG_CODE_RXTXFB): {
				PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
					"thread: %u, [rx->front, tx->back] rx-tx.", THIS_THASH);
				RX_DATA_PROC_FUNC();
				break;
			}
			// buffer: rx-ch -> back, tx-ch -> front.
			case(STATE_FLAG_CODE_RXTXBF): {
				PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
					"thread: %u, [rx->back, tx->front] tx-rx.", THIS_THASH);
				TX_DATA_PROC_FUNC();
				break;
			}}
			T_NORMAL();
		}
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"inter worker event exit, thread hash : % u", THIS_THASH);
		return true;
	}
	
	bool InterTasksGlobal::DataProcess(size_t bench_cpu) {
		// read sampler & check data size.
		vector<SampleDataGroup> SourceData = SampleDatasetREAD();
		if (SourceData.size() < 4) return false;

		InterBenchmarkPoint InterPointTemp = {};
		for (auto& Data : SourceData) {
			// calculate cycles(RDTSC) avg & time avg.
			InterPointTemp.AverageTransmCycles = DataCalculateAverage(
				DataFilterOutliers(VectorDataConvert(Data.SampleCountRDTSC), 0.85)
			);
			InterPointTemp.AverageTransmNs = DataCalculateAverage(
				DataFilterOutliers(VectorDataConvert(Data.SampleCountTIMER), 0.85)
			);
			InterPointTemp.TxLogicCore = Data.BindLogicCoreCount;
			BenchmarkReport.LogicCoresSpeed.push_back(InterPointTemp);
		}
		BenchmarkReport.BenchmarkLogicCore = bench_cpu;
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"data process: size: %u, core: %u", SourceData.size(), bench_cpu);
		// clear sample data cache.
		SampleDatasetCLEAR();
		return true;
	}

	InterTasksGlobal::InterTasksGlobal() {
		// print const global params.
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"params: block size: %u bytes", BENCHMARK_BLOCK_BYTES);
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"params: block cache alignas: %u bits", BENCHMARK_BLOCK_LINES);
	}

	void InterTasksGlobal::CreateWorkerThread(size_t blocks, size_t logiccore) {
		promise<ControlHandle> HandleDataPromise;
		auto Future = HandleDataPromise.get_future();
		// create worker thread.
		WorkerThreads.emplace_back([blocks, logiccore, &HandleDataPromise]() {
			InterWorkerThread WorkerObject = {};
			HandleDataPromise.set_value(WorkerObject.GetControlHandle());
			WorkerObject.DataBlocksNumber = blocks;
			WorkerObject.InterWorkerRun((uint32_t)logiccore);
		});
		WorkerHandles.push_back(Future.get());
	}

	void InterTasksGlobal::DeleteWorkerThreadAll() {
		// check workers params.
		if (WorkerThreads.size() != WorkerHandles.size()) {
			PSAG_LOGGER::PushLogger(LogError, SYSTEM_LOG_TAG_WORKER,
				"failed delete, threads != handles.");
			return;
		}
		for (size_t i = 0; i < WorkerThreads.size(); ++i) {
			// setting flag & notify thread => join thread.
			WorkerHandles[i].WorkerStatus->store(STATE_FLAG_CODE_EXIT);
			WorkerHandles[i].WorkerNotify->notify_all();
			if (WorkerThreads[i].joinable()) WorkerThreads[i].join();
		}
		WorkerThreads.clear();
		WorkerHandles.clear();
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_WORKER,
			"inter workers delete all.");
	}

	bool InterTasksGlobal::WorkersTaskStatus() {
		if (WorkerThreads.empty()) return false;
		return !(WorkerHandles[
			CurrentWorkers.first
		].WorkerStatus->load() == STATE_FLAG_CODE_WAIT &&
				WorkerHandles[
					CurrentWorkers.second
				].WorkerStatus->load() == STATE_FLAG_CODE_WAIT);
	}

	bool InterTasksGlobal::StartWorkersTaskPair(const WorkersPair& indexes) {
		if (indexes.second == indexes.first) {
			PSAG_LOGGER::PushLogger(LogError, SYSTEM_LOG_TAG_WORKER,
				"strat workers pair: sec = fst.");
			return false;
		}
		if (indexes.second >= WorkerHandles.size() || indexes.first >= WorkerHandles.size()) {
			PSAG_LOGGER::PushLogger(LogError, SYSTEM_LOG_TAG_WORKER,
				"strat workers pair: sec > workers_max | fst > workers_max.");
			return false;
		}
		WorkerHandles[indexes.first ].WorkerStatus->store(STATE_FLAG_CODE_RXTXFB);
		WorkerHandles[indexes.second].WorkerStatus->store(STATE_FLAG_CODE_RXTXBF);
		WorkerHandles[indexes.first ].WorkerNotify->notify_all();
		WorkerHandles[indexes.second].WorkerNotify->notify_all();

		CurrentWorkers = indexes;
		return true;
	}
}

StaticStrLABEL SYSTEM_LOG_TAG_TEST = "BENCHMARK";

bool BenchmarkController::BenchmarkStatusCreate() {
	SystemCpuLogicCores = thread::hardware_concurrency();
	if (SystemCpuLogicCores < 2) {
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_TEST,
			"invalid cpu cores on platform, logic: %u", SystemCpuLogicCores
		);
		return false;
	}
	SystemOperations = 0;
	CoreCountRx = 0;
	CoreCountTx = 0;
	// create workers number = system threads.
	for (size_t i = 0; i < SystemCpuLogicCores; ++i)
		CreateWorkerThread(TESTING_BLOCKS_GROUP, i);
	return true;
}

bool BenchmarkController::BenchmarkRunStep() {
	if (CoreCountRx >= SystemCpuLogicCores) return true;
	if (!WorkersTaskStatus()) {
		if (CoreCountRx == CoreCountTx) ++CoreCountTx;
		if (CoreCountTx >= SystemCpuLogicCores) {

			DataProcess(CoreCountRx);
			ReportsDataset.push_back(BenchmarkReport);

			CoreCountTx = 0; ++CoreCountRx;
			// clear report temp.
			BenchmarkReport.LogicCoresSpeed.clear();
			BenchmarkReport.BenchmarkLogicCore = 0;
		}
		if (CoreCountRx >= SystemCpuLogicCores) 
			return true;
		BenchmarkProgress = (float)CoreCountRx / (float)SystemCpuLogicCores;
		PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_TEST,
			"benchmark logic pair: %u,%u", CoreCountRx, CoreCountTx
		);
		// run benchmark workers(cores) pair.
		StartWorkersTaskPair(pair(CoreCountRx, CoreCountTx));
		++CoreCountTx;
		++SystemOperations;
	}
	return false;
}

void BenchmarkController::BenchmarkStatusDelete() {
	DeleteWorkerThreadAll();
}

BenchmarkExport BenchmarkController::BenchmarkDataExport() {
	BenchmarkExport ExportData = {};
	ExportData.TicksMin = 3.40282347E+38F;

	float InterTimesSum = 0.0f, InterTicksSum = 0.0f;
	
	size_t CoreLast = 0;
	for (auto& DataItem : ReportsDataset) {
		// check data sequence.
		if (DataItem.BenchmarkLogicCore != CoreLast) {
			PSAG_LOGGER::PushLogger(LogError, SYSTEM_LOG_TAG_TEST,
				"data sequence err: %u : %u", DataItem.BenchmarkLogicCore, 
				CoreLast);
			return BenchmarkExport();
		}
		++CoreLast;
		vector<float> InterTicks = {}, InterTime = {};
		// data package => data array.
		for (auto& Point : DataItem.LogicCoresSpeed) {
			float Ticks = (float)Point.AverageTransmCycles;
			float Times = (float)Point.AverageTransmNs;
			InterTicks.push_back(Ticks);
			InterTime.push_back(Times);
			if (Ticks < ExportData.TicksMin) ExportData.TicksMin = Ticks;
			if (Ticks > ExportData.TicksMax) ExportData.TicksMax = Ticks;
			if (Times < ExportData.TimesMin) ExportData.TimesMin = Times;
			if (Times > ExportData.TimesMax) ExportData.TimesMax = Times;

			InterTicksSum += Ticks;
			InterTimesSum += Times;
		}
		ExportData.CoresInterTicks.push_back(InterTicks);
		ExportData.CoresInterTime.push_back(InterTime);
	}
	InterTicksSum /= 1000000.0f;
	ExportData.AverageCopySpeed = InterTimesSum / TESTING_BLOCKS_GROUP;
	InterTimesSum /= 1000000000.0f; // time ns convert sec.
	ExportData.AverageTicksSpeed = InterTicksSum / InterTimesSum;
	return ExportData;
}