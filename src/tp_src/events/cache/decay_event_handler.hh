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
    protected:
        void processEvent() override;

        uint64_t decayPeriod;

        int decayCounter;

        int localDecayCounter;

        BaseCache *cache;

        bool isOn = true;

        EventFunctionWrapper powerOffRemainingEvent;
        EventFunctionWrapper calcDecayEvent;
        int calcDecayPeriod;

        int powerOffRemainingPeriod;

        int timesRemainingFired;

        const int timesRemainingLimit;

        void processPowerOffRemainingEvent();
        void processCalcDecayEvent();

        uint64_t tournamentWindow;

        uint64_t TOUR_WINDOW_LIMIT = 36;
    public:
        DecayEventHandler(const DecayEventHandlerParams &p);

        void setCache(BaseCache *_cache);

        void enable();
};

} // namespace tp

} // namespace gem5

#endif // __TP_DECAY_EVENT_HANDLER_HH__
