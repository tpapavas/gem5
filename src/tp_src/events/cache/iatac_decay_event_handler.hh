#ifndef __TP_IATAC_DECAY_EVENT_HANDLER_HH__
#define __TP_IATAC_DECAY_EVENT_HANDLER_HH__

#include "mem/cache/base.hh"
#include "params/IATACDecayEventHandler.hh"
#include "sim/sim_object.hh"
#include "tp_src/events/timing_event_handler.hh"

namespace gem5
{
namespace tp
{

class IATACDecayEventHandler : public TimingEventHandler
{
    private:
        void processEvent() override;

        const int decayPeriod;

        BaseCache *cache;

        bool isOn = true;

        EventFunctionWrapper powerOffRemainingEvent;

        int powerOffRemainingPeriod;

        int timesRemainingFired;

        const int timesRemainingLimit;

        //// extra code ////
        int globalCounter = 1;

        int initDecay = 8192;

        bool letOverflow = false;

        bool resetCounterOnHit = false;
        //// eof extra code ////

        void processPowerOffRemainingEvent();

    public:
        IATACDecayEventHandler(const IATACDecayEventHandlerParams &p);

        void setCache(BaseCache *_cache);

        void enable();

        bool isMechOn() { return isOn; }
};

} // namespace tp

} // namespace gem5

#endif // __TP_IATAC_DECAY_EVENT_HANDLER_HH__
