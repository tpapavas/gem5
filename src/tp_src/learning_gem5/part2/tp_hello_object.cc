#include "tp_src/learning_gem5/part2/tp_hello_object.hh"
#include "debug/TPHello.hh"

#include <iostream>

namespace gem5 {

TPHelloObject::TPHelloObject(const TPHelloObjectParams &params) :
    SimObject(params), 
	event([this]{processEvent();}, name()),
	goodbye(params.goodbye_object),
	myName(params.name),
	latency(params.time_to_wait),
	timesLeft(params.number_of_fires) {
    DPRINTF(TPHello, "Created the hello object with the name %s\n", myName);
	panic_if(!goodbye, "TPHelloObject must have a non-null TPGoodbyeObject");
}

void TPHelloObject::startup() {
	schedule(event, latency);
}

void TPHelloObject::processEvent() {
	timesLeft--;
	DPRINTF(TPHello, "Hello world! Processing the event! %d left\n", timesLeft);
	
	if (timesLeft <= 0) {
		DPRINTF(TPHello, "Done firing!\n");
		goodbye->sayGoodbye(myName);
	} else {
		schedule(event, curTick() + latency);
	}
}

} // namespace gem5
