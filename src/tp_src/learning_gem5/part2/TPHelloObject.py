from m5.params import *
from m5.SimObject import SimObject

class TPHelloObject(SimObject):
	type = 'TPHelloObject'
	cxx_header = "tp_src/learning_gem5/part2/tp_hello_object.hh"
	cxx_class = "gem5::TPHelloObject"

	time_to_wait = Param.Latency("Time before firing event")
	number_of_fires = Param.Int(1, "Number of times to fire the event before goodbye")
	goodbye_object = Param.TPGoodbyeObject("A tp_goodbye object")

class TPGoodbyeObject(SimObject):
	type = 'TPGoodbyeObject'
	cxx_header = "tp_src/learning_gem5/part2/tp_goodbye_object.hh"
	cxx_class = "gem5::TPGoodbyeObject"

	buffer_size = Param.MemorySize(
		'1kB',
		"Size of buffer to fill with goodbye")
	#tp_write_bandwith = Param.MemoryBandwith(
	#	'10MB/s',
	#	"Bandwith to fill the buffer")

	write_bandwidth = Param.MemoryBandwidth('100MB/s', "Bandwidth to fill "
                                            "the buffer")
