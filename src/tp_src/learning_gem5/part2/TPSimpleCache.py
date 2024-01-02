from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class TPSimpleCache(ClockedObject):
	type = 'TPSimpleCache'
	cxx_header = 'tp_src/learning_gem5/part2/tp_simple_cache.hh'
	cxx_class = 'gem5::TPSimpleCache'

	cpu_side = VectorResponsePort("CPU side port, receives requests")
	mem_side = RequestPort("Memory side port, sends requests")

	latency = Param.Cycles(1, "Cycles taken on a hit or to resolve a miss")

	size = Param.MemorySize("16KiB", "The size of the cache")

	system = Param.System(Parent.any, "The system this cache is part of")
