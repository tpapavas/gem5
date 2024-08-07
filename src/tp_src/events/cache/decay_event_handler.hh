#ifndef __TP_DECAY_EVENT_HANDLER_HH__
#define __TP_DECAY_EVENT_HANDLER_HH__

#include "mem/cache/base.hh"
#include "params/DecayEventHandler.hh"
#include "sim/sim_object.hh"
#include "tp_src/events/timing_event_handler.hh"

namespace gem5
{
namespace tp
{

class DecayEventHandler : public TimingEventHandler
{
    private:
        void processEvent() override;

        const int decayPeriod;

        int decayCounter;

        int localDecayCounter;

        BaseCache *cache;

        bool isOn = true;

        EventFunctionWrapper powerOffRemainingEvent;
        EventFunctionWrapper calcDecayEvent;
        bool onDecayEvent = false;
        int calcDecayPeriod;

        int powerOffRemainingPeriod;

        int timesRemainingFired;

        const int timesRemainingLimit;

        void processPowerOffRemainingEvent();
        void processCalcDecayEvent();
    public:
        DecayEventHandler(const DecayEventHandlerParams &p);

        void setCache(BaseCache *_cache);

        void enable();
};

} // namespace tp

} // namespace gem5

#endif // __TP_DECAY_EVENT_HANDLER_HH__
