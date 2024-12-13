#ifndef __TP_TOUR_DECAY_EVENT_HANDLER_HH__
#define __TP_TOUR_DECAY_EVENT_HANDLER_HH__

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
        virtual void processEvent() override;

        uint32_t dedicatedSets = 32;

        uint64_t tournamentWindow;

        float dThres = 0.01;
        float uThres = 0.02;
        float scaleFactor = 4;
        uint32_t duelingTypeId = 0;

        uint64_t TOUR_WINDOW_LIMIT = 36;
        Cycles TW_CYCLES; // the window size in cycles
    public:
        TourDecayEventHandler(const TourDecayEventHandlerParams &p);

        virtual void setCache(BaseCache *_cache) override;

        virtual void retreiveParams(int &, int &, float &, float &) override;

        void skipWindow() { tournamentWindow = TOUR_WINDOW_LIMIT - 1; }

        Cycles getWCycles() { return TW_CYCLES; }

        DecayDuelingMonitor *createTourMonitor(size_t total_sets,
            size_t constituency_size, size_t team_size, Tick clk_ticks);
};

} // namespace tp

} // namespace gem5

#endif // __TP_TOUR_DECAY_EVENT_HANDLER_HH__
