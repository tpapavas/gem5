#include "tp_src/learning_gem5/part2/tp_goodbye_object.hh"

#include "base/trace.hh"
#include "debug/TPHello.hh"
#include "sim/sim_exit.hh"

namespace gem5 {

TPGoodbyeObject::TPGoodbyeObject(const TPGoodbyeObjectParams &params) :
	SimObject(params), event(*this), bandwidth(params.write_bandwidth),
	bufferSize(params.buffer_size),	buffer(nullptr), bufferUsed(0) {
	buffer = new char[bufferSize];
	DPRINTF(TPHello, "Created the goodbye object.\n");
}

TPGoodbyeObject::~TPGoodbyeObject() {
	delete[] buffer;
}

void TPGoodbyeObject::processEvent() {
	DPRINTF(TPHello, "Processing the event!\n");
	fillBuffer();
}

void TPGoodbyeObject::sayGoodbye(std::string other_name) {
	DPRINTF(TPHello, "Saying goodbye to %s\n", other_name);

	message = "Goodbye " + other_name + "!! ";

	fillBuffer();
}

void TPGoodbyeObject::fillBuffer() {
	// There better be a message
	assert(message.length() > 0);

	// Copy from the message to the buffer per byte
	int bytes_copied = 0;
	for (auto it = message.begin(); it < message.end() && bufferUsed < bufferSize - 1; it++, bufferUsed++, bytes_copied++) {
		// Copy the character into the buffer
		buffer[bufferUsed] = *it;
	}
	
	if (bufferUsed < bufferSize - 1) {
		// Wait for the next copy for as long as it would have taken
		DPRINTF(TPHello, "Scheduling another fillBuffer in %d ticks\n",
			bandwidth * bytes_copied);
		schedule(event, curTick() + bandwidth * bytes_copied);
	} else {
		DPRINTF(TPHello, "Goodbye done copying!\n");
		// Be sure to take into account the time for the last bytes
		exitSimLoop(buffer, 0, curTick() + bandwidth * bytes_copied);
	}
}

} // namespace gem5
