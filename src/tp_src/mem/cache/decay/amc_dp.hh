#ifndef __TP_CACHE_DECAY_MECHANISM_AMC_HH__
#define __TP_CACHE_DECAY_MECHANISM_AMC_HH__

#include <memory>

#include "base/cprintf.hh"
#include "base/trace.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "tp_src/mem/cache/decay/constant_dp.hh"
#include "tp_src/mem/cache/decay/dueling.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class AMCDecayData;

class AMC : public gem5::tp::decay_policy::Constant
{
  public:
    AMC();

    DecayDueler* getDecayDueler() override {
      DPRINTF(TPCacheDecayDebug, "AMCDP: getDecayDueler\n");
      return duelerData;
    }
  protected:
    DecayDueler* duelerData;
};

class AMCDecayData : public ConstantDecayData
{
  public:
    AMCDecayData();

    virtual AMC* instantiateDecay() override { return new AMC(); };

  friend AMC;
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_AMC_HH__
