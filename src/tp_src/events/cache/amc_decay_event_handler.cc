#include "tp_src/events/cache/amc_decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "debug/TPDecayPolicies.hh"
#include "mem/cache/base.hh"

namespace gem5
{
namespace tp
{

AMCDecayEventHandler::AMCDecayEventHandler(
        const AMCDecayEventHandlerParams &params) :
    DecayEventHandler(params)
{
    DPRINTF(TPCacheDecay,
        "Created the DecayEventHandler object with the name %s\n"
        "TOUR_WINDOW_LIMIT: %" PRIu64"",
        name(), TOUR_WINDOW_LIMIT);

    eventType = gem5::tp::EventType::DECAY_AMC;
}

} // namespace tp
} // namespace gem5
