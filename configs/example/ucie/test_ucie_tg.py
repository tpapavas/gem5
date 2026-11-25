# task12/13: UCIeLink unit test.  PyTrafficGen -> UCIeLink -> SimpleMemory(0 lat)
import argparse, os
import m5
from m5.objects import *

base = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument("--config", default=os.path.join(base, "Config.yaml"))
ap.add_argument("--queue-size", type=int, default=16)
ap.add_argument("--header-bits", type=int, default=64)
ap.add_argument("--period", default="1000ns", help="TG inter-request period")
ap.add_argument("--rd-perc", type=int, default=100)
ap.add_argument("--block", type=int, default=64)
ap.add_argument("--duration", default="200us")
ap.add_argument("--max-outstanding", type=int, default=1000)
ap.add_argument("--random", action="store_true")
# task13 flit mode
ap.add_argument("--link-mode", default="packet")
ap.add_argument("--phy-model", default="leandros")
ap.add_argument("--ber", type=float, default=0.0)
ap.add_argument("--ber-deterministic", action="store_true")
ap.add_argument("--lanes", type=int, default=16)
ap.add_argument("--gtps", type=float, default=4.0)
ap.add_argument("--retry-buffer", type=int, default=64)
ap.add_argument("--payload-bytes", type=int, default=0)
ap.add_argument("--seed", type=int, default=1)
ap.add_argument("--min-period", default=None, help="random inter-request period lower bound (default = --period)")
ap.add_argument("--cache-line", type=int, default=64)
ap.add_argument("--no-resp-delay", action="store_true")
args = ap.parse_args()

system = System(cache_line_size=args.cache_line)
system.clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=VoltageDomain())
system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MiB")]
system.tgen = PyTrafficGen(max_outstanding_reqs=args.max_outstanding)
system.ucie = UCIeLink(config_file=os.path.abspath(args.config),
                       queue_size=args.queue_size,
                       header_bits=args.header_bits,
                       link_mode=args.link_mode, phy_model=args.phy_model,
                       ber=args.ber, ber_deterministic=args.ber_deterministic,
                       lanes=args.lanes, gtps=args.gtps,
                       retry_buffer_flits=args.retry_buffer,
                       flit_payload_bytes=args.payload_bytes,
                       rng_seed=args.seed,
                       apply_response_delay=not args.no_resp_delay)
system.mem = SimpleMemory(range=system.mem_ranges[0], latency="0ns",
                          latency_var="0ns", bandwidth="1000GiB/s")
system.tgen.port = system.ucie.cpu_side
system.ucie.mem_side = system.mem.port
root = Root(full_system=False, system=system)
m5.instantiate()

dur = m5.ticks.fromSeconds(m5.util.convert.anyToLatency(args.duration))
per = m5.ticks.fromSeconds(m5.util.convert.anyToLatency(args.period))
minper = m5.ticks.fromSeconds(m5.util.convert.anyToLatency(args.min_period)) if args.min_period else per
def gens():
    mk = system.tgen.createRandom if args.random else system.tgen.createLinear
    yield mk(dur, 0, 256 * 1024 * 1024, args.block, minper, per, args.rd_perc, 0)
    yield system.tgen.createExit(0)
system.tgen.start(gens())
ev = m5.simulate()
print("Exiting @ tick {} because {}".format(m5.curTick(), ev.getCause()))
