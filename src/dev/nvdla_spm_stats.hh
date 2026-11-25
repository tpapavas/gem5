#ifndef __DEV_NVDLA_SPM_STATS_HH__
#define __DEV_NVDLA_SPM_STATS_HH__

#include "base/statistics.hh"
#include "base/stats/group.hh"
#include "spmStats.hh"

namespace gem5
{

/**
 * [SPM_STATS] gem5 stats of the NVDLA embedded buffer (SPM), printed in stats.txt as
 * <nvdla>.spm.<name>. The wrapper (ext/rtl/model_nvdla) counts in plain integers (SpmStats);
 * sync() adds the increase since the previous call, so resetstats/dumpstats behave as for
 * any other stat. Call sync() after every NVDLA iteration.
 */
struct NvdlaSpmStats : public statistics::Group
{
#define NVDLA_SPM_STATS_SCALAR(name, desc) statistics::Scalar name;
    NVDLA_SPM_STATS(NVDLA_SPM_STATS_SCALAR)
#undef NVDLA_SPM_STATS_SCALAR
    statistics::Formula readHitRate;
    statistics::Formula writeHitRate;

    SpmStats copied;

    explicit NvdlaSpmStats(statistics::Group *parent)
        : statistics::Group(parent, "spm"),
#define NVDLA_SPM_STATS_ADD(name, desc) \
          ADD_STAT(name, statistics::units::Count::get(), desc),
          NVDLA_SPM_STATS(NVDLA_SPM_STATS_ADD)
#undef NVDLA_SPM_STATS_ADD
          ADD_STAT(readHitRate, statistics::units::Ratio::get(),
                   "Read hit rate (AXI read beats)",
                   read_hits / (read_hits + read_misses)),
          ADD_STAT(writeHitRate, statistics::units::Ratio::get(),
                   "Write hit rate (AXI write beats, all-in-one buffer mode)",
                   write_hits / (write_hits + write_misses))
    {
        readHitRate.precision(6);
        writeHitRate.precision(6);
    }

    void
    sync(const SpmStats &now)
    {
#define NVDLA_SPM_STATS_SYNC(name, desc) \
        if (now.name != copied.name) { \
            name += now.name - copied.name; \
            copied.name = now.name; \
        }
        NVDLA_SPM_STATS(NVDLA_SPM_STATS_SYNC)
#undef NVDLA_SPM_STATS_SYNC
    }
};

} // namespace gem5

#endif // __DEV_NVDLA_SPM_STATS_HH__
