# Standalone minimal gem5 memory-path smoke test (no NVDLA needed).
# This is a configuration example; the NVDLA integration is in the README.
import os
import m5
from m5.objects import *

base = os.path.dirname(os.path.abspath(__file__))
system = System()
system.clk_domain = SrcClockDomain(clock='1GHz', voltage_domain=VoltageDomain())
system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('512MiB')]
system.membus = SystemXBar()
system.ucie = UCIeLink(config_file=os.path.join(base, 'Config.yaml'))
system.ucie.cpu_side = system.membus.mem_side_ports
system.mem = SimpleMemory(range=system.mem_ranges[0])
system.ucie.mem_side = system.mem.port
system.system_port = system.membus.cpu_side_ports
root = Root(full_system=False, system=system)
m5.instantiate()
print('UCIe SimObject instantiated. Connect a CPU/traffic generator for transactions.')
