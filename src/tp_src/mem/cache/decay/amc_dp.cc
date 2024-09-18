#include "tp_src/mem/cache/decay/amc_dp.hh"

#include "base/cprintf.hh"
#include "base/trace.hh"
#include "debug/TPDecayPolicies.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

AMC::AMC()
    : Constant()
{
    duelerData = new DecayDueler();
    DPRINTF(TPCacheDecayDebug, "AMCDP: constructor\n");
}

AMCDecayData::AMCDecayData()
    : ConstantDecayData()
{
}

} // namespace decay_policy

} // namespace tp

} // namespace gem5
