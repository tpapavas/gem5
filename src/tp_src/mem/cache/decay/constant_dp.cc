#include "tp_src/mem/cache/decay/constant_dp.hh"

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
    _decay--;
}


} // namespace decay_policy

} // namespace tp

} // namespace gem5
