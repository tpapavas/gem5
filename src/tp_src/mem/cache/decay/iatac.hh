#ifndef __TP_DECAY_MECHANISM_IATAC_HH__
#define __TP_DECAY_MECHANISM_IATAC_HH__

//// decay refactor code ////
#include "tp_src/mem/cache/decay/base.hh"

//// eof decay refactor code ////

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class IATAC : public Base
{
  public:
    IATAC();

    void handleHit(std::shared_ptr<GlobalDecayData>&) override;
    void handleMiss(std::shared_ptr<GlobalDecayData>&) override;

    bool decayElapsed() { return (_elapsed >= _decay) && _onoff; }
    // static void printGlobals();

    void updateDecay();
    // int getDecay() { return _decay; }

    int getElapsed() { return _elapsed; }

    // int getCounter() { return _counter; }

    bool getWrong() { return _wrongBit; }

    //// extra code ////
    void setDecay(int decay_interval) { _decay = decay_interval; }

    void setLetOverflow(bool let_overflow) { _letOverflow =let_overflow; }

    bool doResetCounterOnDecayedHit() { return _resetCounterOnHit; }
    void setResetCounterOnHit(bool reset_counter_on_hit)
    {
      _resetCounterOnHit = reset_counter_on_hit;
    }

    void resetDecayCounter() { _counter = 1; }
    //// eof extra code ////

    std::string print() const;

  protected:
    static const int _MAX_ACCESS = 32;

    // static int _acumcounter[];
    // static int _globalDecay[];
    // static int _maxGlobalDecay[];

    // static bool _first_iatac_obj;

    /** If the block was prematurely turned off. */
    bool _wrongBit = false;

    /** if the block is on or off */
    // bool _onoff = true;

    /** decay interval that has to elapse. */
    // int _decay = 8192;

    /** max interaccess interval. */
    int _thits = 0;

    /** ticks elapsed since last access. */
    int _elapsed = 0;

    /** access counter */
    // int _counter = 1;

    bool _accessOverflow = false;

    //// extra code ////
    bool _letOverflow = false;

    bool _resetCounterOnHit = false;
    //// eof extra code ////
    // void _setupGlobalStructs();
};

class IATACdata : public GlobalDecayData
{
  public:
    IATACdata();

    void updateMaxGlobals(int);

    void checkAcumOverflow(int);

    virtual void setGlobal(int val);
    void setGlobal(int i, int val) { _globalDecay[i] = val; }

    // TODO
    virtual int getGlobal() override { return -1; };

    //// extra code ////
    int getInitLocalDecay() { return _initLocalDecay; }
    void setInitLocalDecay(int init_local_decay)
    {
      _initLocalDecay = init_local_decay;
    }

    bool doLetOverflow() { return _letOverflow; }
    void setLetOverflow(bool letOverflow)
    {
      _letOverflow = letOverflow;
    }

    bool doResetCounterOnHit() { return _resetCounterOnHit; }
    void setResetCounterOnHit(bool reset_counter_on_hit)
    {
      _resetCounterOnHit = reset_counter_on_hit;
    }
    //// eof extra code ////

    void printGlobals();

    virtual IATAC* instantiateDecay() override { return new IATAC(); }

  protected:
    static const int _MAX_ACCESS = 32;

    static const int _MIN_COUNT = 8;
    static const int _MAX_COUNT = 1024;

    int _acumcounter[_MAX_ACCESS];
    int _globalDecay[_MAX_ACCESS];
    int _maxGlobalDecay[_MAX_ACCESS];

    bool _isMax[_MAX_ACCESS];

    //// extra code ////
    int _initLocalDecay = 8192;

    bool _letOverflow = false;

    bool _resetCounterOnHit = false;
    //// eof extra code ////

  friend IATAC;
};

} // decay_policy

} // namespace tp

} // namespace gem5

#endif // __TP_DECAY_MECHANISM_IATAC_HH__
