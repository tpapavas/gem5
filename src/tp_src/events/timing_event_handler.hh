#ifndef __LEARNING_GEM5_TIMING_EVENT_HANDLER_HH__
#define __LEARNING_GEM5_TIMING_EVENT_HANDLER_HH__

#include "params/TimingEventHandler.hh"
#include "sim/clocked_object.hh"

namespace gem5
{
namespace tp
{

enum EventType
{
    PLAIN_TIMING,
    DECAY_CONST,
    DECAY_IATAC,
    DECAY_TOUR,
    DECAY_AMC
};

class TimingEventHandler : public ClockedObject
{
    protected:
        EventType eventType;

        EventFunctionWrapper event;

        const Tick period;

        int numOfFires, timesFired;

        bool tillSimEnd;

        int startDelay;

        virtual void processEvent();
    public:
        TimingEventHandler(const TimingEventHandlerParams &p);

        void startup() override;

        EventType getEventType() { return eventType; }
};

} // namespace tp

} // namespace gem5

#endif // __LEARNING_GEM5_TIMING_EVENT_HANDLER_HH__
