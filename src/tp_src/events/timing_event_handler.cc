#include "tp_src/events/timing_event_handler.hh"
#include "debug/TPHello.hh"

#include <iostream>

namespace gem5 
{
namespace tp 
{

TimingEventHandler::TimingEventHandler(const TimingEventHandlerParams &params) :
    ClockedObject(params), 
	event([this]{processEvent();}, name()),
	period(this->cyclesToTicks(params.period)),
	numOfFires(params.number_of_fires),
	timesFired(0),
	tillSimEnd(params.run_till_sim_end),
	startDelay(params.start_delay)
{
    DPRINTF(TPHello, "Created the TimingEventHandler object with the name %s\n", name());
}

void 
TimingEventHandler::startup() 
{
	DPRINTF(TPHello, "Decay period: %d #ticks\n", period);
    
    schedule(event, curTick() + startDelay + period);
}

void 
TimingEventHandler::processEvent() 
{
	timesFired++;
	DPRINTF(TPHello, "Hello world! Processing the event! #%d fired\n", timesFired);
	if (tillSimEnd || timesFired < numOfFires) {
		schedule(event, curTick() + period);	
	} else {
		DPRINTF(TPHello, "Done firing!\n");
	}
}

} // namespace tp
} // namespace gem5
