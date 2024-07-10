#ifndef __TP_CACHE_DECAY_MECHANISM_BASE_HH__
#define __TP_CACHE_DECAY_MECHANISM_BASE_HH__

#include "params/BaseDecayPolicy.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class Base;

class GlobalDecayData
{
  public:
    GlobalDecayData();

    bool isOn() { return _on; }
    void setOn(bool onoff) { _on = onoff;}

    virtual void updateMaxGlobals(int) = 0;

    virtual void checkAcumOverflow(int) = 0;

    virtual void setGlobal(int val) = 0;
    // void setGlobal(int i, int val) { _globalDecay[i] = val; }

    virtual void printGlobals();

  protected:
    // static const int _MAX_ACCESS = 32;

    // static const int _MIN_COUNT = 8;
    // static const int _MAX_COUNT = 1024;

    // int _acumcounter[_MAX_ACCESS];
    // int _globalDecay[_MAX_ACCESS];
    // int _maxGlobalDecay[_MAX_ACCESS];

    bool _on;

  friend Base;
};

class Base : ClockedObject
{
  public:
    Base(const BaseDecayPolicyParams &p);

    virtual void handleHit(std::shared_ptr<GlobalDecayData>&) = 0;
    virtual void handleMiss(std::shared_ptr<GlobalDecayData>&) = 0;

    // bool decayElapsed() { return (_elapsed >= _decay) && _onoff; }
    // static void printGlobals();

    virtual void updateDecay() = 0;

    int getDecay() { return _decay; }

    // int getElapsed() { return _elapsed; }

    int getCounter() { return _counter; }

    // bool getWrong() { return _wrongBit; }

    void setPower(bool onoff) { _onoff = onoff; }
    bool isPoweredOff() const { return !_onoff; }

    virtual std::string print() const;

  protected:
    // static const int _MAX_ACCESS = 32;

    /** If the block was prematurely turned off. */
    // bool _wrongBit = false;

    /** if the block is on or off */
    bool _onoff = true;

    /** decay interval that has to elapse. */
    int _decay = 8192;

    /** max interaccess interval. */
    // int _thits = 0;

    /** ticks elapsed since last access. */
    // int _elapsed = 0;

    /** access counter */
    int _counter = 1;
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_BASE_HH__
