from m5.params import *
from m5.SimObject import SimObject

class TPSimpleMemobj(SimObject):
	type = 'TPSimpleMemobj'
	cxx_header = "tp_src/learning_gem5/part2/tp_simple_memobj.hh"
	cxx_class = "gem5::TPSimpleMemobj"

	inst_port = ResponsePort("CPU side port, receives requests")
	data_port = ResponsePort("CPU side port, receives requests")
	mem_side = RequestPort("Memory side port, sends requests")
