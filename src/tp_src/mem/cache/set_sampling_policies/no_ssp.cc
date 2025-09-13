#include "tp_src/mem/cache/set_sampling_policies/no_ssp.hh"

namespace gem5
{

namespace tp
{

namespace set_sampling_policy
{

void
NoSSP::initEntry(SetSampler* sampler){
    sampler->setSample();
}

} // namespace set_sampling_policy

} // namespace tp

} // namespace gem5
