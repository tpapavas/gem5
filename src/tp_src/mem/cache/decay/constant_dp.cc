#include "tp_src/mem/cache/decay/constant_dp.hh"

#include "base/cprintf.hh"
#include "base/trace.hh"
#include "debug/TPDecayPolicies.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

Constant::Constant()
    : Base()
{
}

void
Constant::updateDecay()
{
    // DPRINTF(TPDecayPolicies, "%s: Constant DP\n", __func__);
    _decay--;
}

ConstantDecayData::ConstantDecayData()
    : GlobalDecayData()
{
}

} // namespace decay_policy

} // namespace tp

} // namespace gem5
