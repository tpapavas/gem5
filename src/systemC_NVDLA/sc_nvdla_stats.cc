#include "sc_nvdla_stats.hh"

#include <unordered_map>

std::array<NvdlaStats, MAX_NVDLAS> gNvdlaStats{};

static std::unordered_map<
    const sc_core::sc_object *,
    unsigned
> nvdlaRootIds;

void
registerNvdlaStatsRoot(
    const sc_core::sc_object *root,
    unsigned id)
{
    std::cout << "[registerNvdlaStatsRoot] Registering NVDLA stats root: "
              << root->name() << " with id: " << id << std::endl;
    if (id >= MAX_NVDLAS) {
        SC_REPORT_FATAL(
            "NVDLA_STATS",
            "Invalid NVDLA id"
        );
    }

    nvdlaRootIds[root] = id;
}

NvdlaStats &
nvdlaStatsFor(const sc_core::sc_object *obj)
{
    const sc_core::sc_object *current = obj;

    while (current != nullptr) {
        auto it = nvdlaRootIds.find(current);

        if (it != nvdlaRootIds.end()) {
            std::cout
                << "[nvdlaStatsFor] object="
                << obj->name()
                << " -> DLA id="
                << it->second
                << " root="
                << current->name()
                << std::endl;

            return gNvdlaStats[it->second];
        }

        current = current->get_parent_object();
    }

    SC_REPORT_FATAL(
        "NVDLA_STATS",
        "Cannot determine NVDLA instance"
    );

    return gNvdlaStats[0];
}