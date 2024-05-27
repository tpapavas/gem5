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
