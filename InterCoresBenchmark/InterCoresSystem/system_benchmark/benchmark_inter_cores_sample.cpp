// benchmark_inter_cores_sample.
#include "benchmark_inter_cores.h"

using namespace std;
using namespace PSAG_LOGGER;

namespace SystemInterCores {
	StaticStrLABEL SYSTEM_LOG_TAG_SAMPLE = "INTER_SAMPER";

	void InterTaskSampleData::SampleDataADD(const SampleDataGroup& data) {
		lock_guard<mutex> Lock(SamplesDatasetMtx);
		SamplesDataset.push_back(data);
	}
	vector<SampleDataGroup> InterTaskSampleData::SampleDatasetREAD() {
		lock_guard<mutex> Lock(SamplesDatasetMtx);
		return SamplesDataset;
	}
	void InterTaskSampleData::SampleDatasetCLEAR() {
		lock_guard<mutex> Lock(SamplesDatasetMtx);
		SamplesDataset.clear();
	}

	void InterTaskSampler::SamplerContextBegin() {
		// begin: cache context & TIMER now.
		SamplerTIMER = chrono::steady_clock::now();
		SamplerRDTSC = READ_CPU_RDTSC();
	}
	void InterTaskSampler::SamplerContextEnd() {
		SampleDataTemp.SampleCountRDTSC.push_back(READ_CPU_RDTSC() - SamplerRDTSC);
		uint64_t ContextTime 
			= (uint64_t)chrono::duration_cast<chrono::nanoseconds>(
				chrono::steady_clock::now() - SamplerTIMER
			).count();
		SampleDataTemp.SampleCountTIMER.push_back(ContextTime);
	}
	void InterTaskSampler::SamplerDataCheckFlag(bool flag) {
		SampleDataTemp.SampleCheckFlag.push_back(flag);
	}
}