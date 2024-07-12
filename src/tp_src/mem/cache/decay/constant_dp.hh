#ifndef __TP_CACHE_DECAY_MECHANISM_CONSTANT_HH__
#define __TP_CACHE_DECAY_MECHANISM_CONSTANT_HH__

#include "tp_src/mem/cache/decay/base.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class Constant : public Base
{
  public:
    Constant();

    virtual void handleHit(std::shared_ptr<GlobalDecayData>&) override {};
    virtual void handleMiss(std::shared_ptr<GlobalDecayData>&) override {};

    virtual void updateDecay() override;

  protected:
};

class ConstantDecayData : public GlobalDecayData
{
  protected:
    int _globalCounter;

  public:
    ConstantDecayData();

    bool isOn() { return _on; }
    void setOn(bool onoff) { _on = onoff;}

    virtual void updateMaxGlobals(int) override {};

    virtual void checkAcumOverflow(int) override {};

    virtual void setGlobal(int val) override { _globalCounter = val; }
    virtual int getGlobal() { return _globalCounter; }
    // void setGlobal(int i, int val) { _globalDecay[i] = val; }

    virtual void printGlobals() override {};

    virtual Constant* instantiateDecay() override { return new Constant(); }

  friend Constant;
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_CONSTANT_HH__
