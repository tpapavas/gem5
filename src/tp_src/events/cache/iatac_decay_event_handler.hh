#ifndef __TP_IATAC_DECAY_EVENT_HANDLER_HH__
#define __TP_IATAC_DECAY_EVENT_HANDLER_HH__

#include "mem/cache/base.hh"
#include "params/IATACDecayEventHandler.hh"
#include "sim/sim_object.hh"
#include "tp_src/events/cache/decay_event_handler.hh"

namespace gem5
{
namespace tp
{

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

        void enable();

        bool isMechOn() { return isOn; }
};

} // namespace tp

} // namespace gem5

#endif // __TP_IATAC_DECAY_EVENT_HANDLER_HH__
