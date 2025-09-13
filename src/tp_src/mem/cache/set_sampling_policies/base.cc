#include "tp_src/mem/cache/set_sampling_policies/base.hh"

namespace gem5
{

namespace tp
{

namespace set_sampling_policy
{

SetSampler::SetSampler()
  : _isSample(false)
{
}

void
SetSampler::setSample()
{
    _isSample = true;
}

void
SetSampler::unsetSample()
{
    _isSample = false;
}

bool
SetSampler::isSample() const
{
    return _isSample;
}

}

}

}
