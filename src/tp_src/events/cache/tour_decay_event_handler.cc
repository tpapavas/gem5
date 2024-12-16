#include "tp_src/events/cache/tour_decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "debug/TPDecayPolicies.hh"
#include "mem/cache/base.hh"
#include "tour_decay_event_handler.hh"
#include "tp_src/mem/cache/decay/dueling.hh"

namespace gem5
{
namespace tp
{

TourDecayEventHandler::TourDecayEventHandler(
        const TourDecayEventHandlerParams &params) :
    DecayEventHandler(params),
    dedicatedSets(params.dedicated_sets),
    tournamentWindow(0),
    dThres(params.d_threshold),
    uThres(params.u_threshold),
    scaleFactor(params.s_factor),
    duelingTypeId(params.dueling_type),
    dimLimit(params.dim_limit),
    leastDIMs(params.least_dims),
    dimToImRatio(params.dim_to_im_ratio)
{
    TW_CYCLES = Cycles(params.window_size * 9 * 128000);
    TOUR_WINDOW_LIMIT = TW_CYCLES / ticksToCycles(decayPeriod);
    DPRINTF(TPCacheDecay,
        "Created the DecayEventHandler object with the name %s\n"
        "TOUR_WINDOW_LIMIT: %" PRIu64"",
        name(), TOUR_WINDOW_LIMIT);

    eventType = gem5::tp::EventType::DECAY_TOUR;
}

void TourDecayEventHandler::setCache(BaseCache *_cache)
{
    DecayEventHandler::setCache(_cache);
    // cache->getDecayDuelingMonitor()->setDuelingType(
    //     static_cast<DecayDuelingMonitor::DuelingType>(duelingTypeId));
}

DecayDuelingMonitor *
TourDecayEventHandler::createTourMonitor(size_t total_sets,
    size_t constituency_size, size_t team_size, Tick clk_ticks)
{
    DecayDuelingMonitor* decayDuelingMonitor = nullptr;
    DecayDuelingMonitor::DuelingType duelingType =
        static_cast<DecayDuelingMonitor::DuelingType>(duelingTypeId);

    switch (duelingType)
    {
        case DecayDuelingMonitor::DuelingType::PLAIN:
        {
            decayDuelingMonitor = new TourPlain(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES);
            break;
        }

        case DecayDuelingMonitor::DuelingType::JUMP:
        {
            decayDuelingMonitor = new TourJump(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES);
            break;
        }

        case DecayDuelingMonitor::DuelingType::E_JUMP:
        {
            decayDuelingMonitor = new TourEJump(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES);
            break;
        }

        case DecayDuelingMonitor::DuelingType::E_JUMP_C:
        {
            decayDuelingMonitor = new TourEJumpC(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES);
            break;
        }

        case DecayDuelingMonitor::DuelingType::UD_S:
        {
            decayDuelingMonitor = new TourUD_S(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES, dimLimit, leastDIMs, dimToImRatio);
            break;
        }

        case DecayDuelingMonitor::DuelingType::UD_S_SIMPLE:
        {
            decayDuelingMonitor = new TourUD_S_Simple(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES, dimLimit, leastDIMs, dimToImRatio);
            break;
        }

        case DecayDuelingMonitor::DuelingType::EN_AWARE:
        {
            decayDuelingMonitor = new TourEnAware(
                total_sets, dedicatedSets, constituency_size,
                team_size, dThres, uThres, scaleFactor, clk_ticks,
                TW_CYCLES);
            break;
        }

        default:
            break;
    }
    // decayDuelingMonitor->setTourParams(dimLimit, leastDIMs, dimToImRatio);

    return decayDuelingMonitor;
}


void
TourDecayEventHandler::processEvent()
{
    if (!isOn) {
        schedule(event, curTick() + decayPeriod);
            return;
    }

    tournamentWindow++;

    timesFired++;
    timesRemainingFired = 0;

    DPRINTF(TPCacheDecayDebug, "Processing the decay event! #%d fired\n",
        timesFired);
    if (!cache->updateDecayAndPowerOff(decayPeriod,
            tournamentWindow, TOUR_WINDOW_LIMIT)) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    } else if (tillSimEnd || timesFired < numOfFires) {
        schedule(event, curTick() + decayPeriod);
    } else {
        DPRINTF(TPCacheDecay, "Done firing!\n");
        return;
    }

    if (!calcDecayEvent.scheduled()) {
       schedule(calcDecayEvent, curTick() + calcDecayPeriod);
    }

    if (tournamentWindow % TOUR_WINDOW_LIMIT == 0) {
        tournamentWindow = 0;

        TOUR_WINDOW_LIMIT = TW_CYCLES / ticksToCycles(decayPeriod);
        DPRINTF(TPDecayPolicies, "TOUR_WINDOW_LIMIT: %" PRIu64"\n",
            TOUR_WINDOW_LIMIT);
    }
}

void TourDecayEventHandler::retreiveParams(
        int &param1, int &param2, float &param3, float &param4)
{
    param1 = dedicatedSets;
    param2 = scaleFactor;
    param3 = dThres;
    param4 = uThres;
}

} // namespace tp
} // namespace gem5
