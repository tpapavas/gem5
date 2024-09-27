#ifndef __TP_TOUR_DECAY_EVENT_HANDLER_HH__
#define __TP_TOUR_DECAY_EVENT_HANDLER_HH__

#include "mem/cache/base.hh"
#include "params/TourDecayEventHandler.hh"
#include "sim/sim_object.hh"
#include "tp_src/events/cache/decay_event_handler.hh"

namespace gem5
{
namespace tp
{

class TourDecayEventHandler : public DecayEventHandler
{
    protected:
        uint32_t dedicatedSets = 32;

        float dThres = 0.01;
        float uThres = 0.02;
        float scaleFactor = 4;

    public:
        TourDecayEventHandler(const TourDecayEventHandlerParams &p);

        virtual void setCache(BaseCache *_cache) override;

        virtual void retreiveParams(int &, int &, float &, float &) override;
};

} // namespace tp

} // namespace gem5

#endif // __TP_TOUR_DECAY_EVENT_HANDLER_HH__
