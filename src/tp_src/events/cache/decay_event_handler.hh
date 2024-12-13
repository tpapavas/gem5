#ifndef __TP_DECAY_EVENT_HANDLER_HH__
#define __TP_DECAY_EVENT_HANDLER_HH__

#include "mem/cache/base.hh"
#include "params/DecayEventHandler.hh"
#include "params/IATACDecayEventHandler.hh"
#include "sim/sim_object.hh"
#include "tp_src/events/timing_event_handler.hh"

namespace gem5
{
namespace tp
{

class DecayEventHandler : public TimingEventHandler
{
    protected:
        virtual void processEvent() override;

        uint64_t decayPeriod;

        int decayCounter;

        int localDecayCounter;

        BaseCache *cache;

        bool isOn = true;

        EventFunctionWrapper powerOffRemainingEvent;
        int powerOffRemainingPeriod;

        EventFunctionWrapper calcDecayEvent;
        int calcDecayPeriod;

        int timesRemainingFired;

        const int timesRemainingLimit;

        virtual void processPowerOffRemainingEvent();
        void processCalcDecayEvent();
    public:
        DecayEventHandler(const DecayEventHandlerParams &p);

        virtual void setCache(BaseCache *_cache);

        void enable();

        bool isMechOn() { return isOn; }

        virtual void retreiveParams(int &, int &, float &, float &) {}
};

class IATACDecayEventHandler : public DecayEventHandler
{
    protected:
        void processEvent() override;

        //// extra code ////
        int globalCounter = 1;

        int initDecay = 8192;

        bool letOverflow = false;

        bool resetCounterOnHit = false;
        //// eof extra code ////

        void processPowerOffRemainingEvent() override;

    public:
        IATACDecayEventHandler(const IATACDecayEventHandlerParams &p);

        void setCache(BaseCache *_cache) override;
};

} // namespace tp

} // namespace gem5

#endif // __TP_DECAY_EVENT_HANDLER_HH__
