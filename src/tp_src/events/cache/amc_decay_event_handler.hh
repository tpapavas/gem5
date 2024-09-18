#ifndef __TP_AMC_DECAY_EVENT_HANDLER_HH__
#define __TP_AMC_DECAY_EVENT_HANDLER_HH__

#include "mem/cache/base.hh"
#include "params/AMCDecayEventHandler.hh"
#include "sim/sim_object.hh"
#include "tp_src/events/cache/decay_event_handler.hh"

namespace gem5
{
namespace tp
{

class AMCDecayEventHandler : public DecayEventHandler
{
    public:
        AMCDecayEventHandler(const AMCDecayEventHandlerParams &p);
};

} // namespace tp

} // namespace gem5

#endif // __TP_AMC_DECAY_EVENT_HANDLER_HH__
