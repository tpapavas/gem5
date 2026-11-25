# Copyright (c) 2016-2017, 2019-2021 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# This is an example configuration script for full system simulation of
# a generic ARM bigLITTLE system.
import argparse
import os
import sys
import m5
import m5.util
from m5.objects import *
m5.util.addToPath("../../")
from caches import *
from common import FSConfig
from common import SysPaths
from common import ObjectList
from common import Options
default_mem_size = "1GB"
def addOptions(parser):
    parser.add_argument(
        "--restore-from",
        type=str,
        default=None,
        help="Restore from checkpoint",
    )
    parser.add_argument(
        "--dtb", type=str, default=None, help="DTB file to load"
    )
    # required=True,
    parser.add_argument("--kernel", type=str, default="", help="Linux kernel")
    parser.add_argument(
        "--root",
        type=str,
        default="/dev/vda1",
        help="Specify the kernel CLI root= argument",
    )
    parser.add_argument(
        "--machine-type",
        type=str,
        choices=ObjectList.platform_list.get_names(),
        default="VExpress_GEM5",
        help="Hardware platform class",
    )
    parser.add_argument(
        "--disk",
        action="append",
        type=str,
        default=[],
        help="Disks to instantiate",
    )
    parser.add_argument(
        "--bootscript", type=str, default="", help="Linux bootscript"
    )
    parser.add_argument(
        "--cpu-type",
        type=str,
        # choices=list(cpu_types.keys()),
        default="timing",
        help="CPU simulation mode. Default: %(default)s",
    )
    parser.add_argument(
        "--kernel-init", type=str, default="/sbin/init", help="Override init"
    )
    parser.add_argument(
        "--dlas", type=int, default=1, help="Number of available DLAs"
    )
    parser.add_argument(
        "--big-cpus",
        type=int,
        default=1,
        help="Number of big CPUs to instantiate",
    )
    parser.add_argument(
        "--little-cpus",
        type=int,
        default=1,
        help="Number of little CPUs to instantiate",
    )
    parser.add_argument(
        "--caches",
        action="store_true",
        default=False,
        help="Instantiate caches",
    )
    parser.add_argument(
        "--last-cache-level",
        type=int,
        default=2,
        help="Last level of caches (e.g. 3 for L3)",
    )
    parser.add_argument(
        "--big-cpu-clock",
        type=str,
        default="2GHz",
        help="Big CPU clock frequency",
    )
    parser.add_argument(
        "--little-cpu-clock",
        type=str,
        default="1GHz",
        help="Little CPU clock frequency",
    )
    parser.add_argument(
        "--sim-quantum",
        type=str,
        default="1ms",
        help="Simulation quantum for parallel simulation. "
        "Default: %(default)s",
    )
    parser.add_argument(
        "--mem-size",
        type=str,
        default=default_mem_size,
        help="System memory size",
    )
    parser.add_argument(
        "--kernel-cmd",
        type=str,
        default=None,
        help="Custom Linux kernel command",
    )
    parser.add_argument(
        "--bootloader",
        action="append",
        help="executable file that runs before the --kernel",
    )
    parser.add_argument(
        "--kvm-userspace-gic",
        action="store_true",
        default=False,
        help="Use the gem5 GIC in a KVM simulation",
    )
    parser.add_argument(
        "--accelerators",
        action="store_true",
        default=False,
        help="Instantiate Accelerator",
    )
    # options.maxReqNVDLA
    parser.add_argument(
        "--maxReqNVDLA",
        type=int,
        default=4,
        help="max requests Inflight in NVDLA",
    )
    # options.enableWaveform
    parser.add_argument(
        "--enableWaveform",
        action="store_true",
        default=False,
        help="Enable tracing waveform of NVDLA",
    )
    # options.enableTiming
    parser.add_argument(
        "--enableTimingAXI",
        action="store_true",
        default=False,
        help="Enable Timing memory requests NVDLA",
    )
    # options.ddr_type
    parser.add_argument(
        "--ddr-type",
        type=str,
        default="DDR3_1600_8x8",
        help="specify system dram type",
    )
    # available: DDR4_2400_8x8
    # options.numNVDLA
    parser.add_argument(
        "--numNVDLA", type=int, default=1, help="number of NVDLAs"
    )
    # options.freq_ratio
    parser.add_argument(
        "--freq-ratio",
        type=float,
        default=1,
        help="=(frequency of LITTLE CPU) / (frequency of NVDLA)",
    )
    # options.buffer_mode
    parser.add_argument(
        "--buffer-mode",
        type=str,
        default="all",
        help="How to use pr/sh cache/embedded-SPM. "
        "all: cache all; pft: prefetch-buffer-only; "
        "pft-cut: prefetch buffer but does not prefetch overflowed tensors",
    )
    # options.dma_enable
    parser.add_argument(
        "--dma-enable",
        action="store_true",
        default=False,
        help="Use the buffer embedded in NVDLA wrapper, aided with DMA",
    )
    # options.shared_spm
    parser.add_argument(
        "--shared-spm",
        action="store_true",
        default=False,
        help="change embedded buffer to shared",
    )
    # options.embed_spm_size
    parser.add_argument(
        "--embed-spm-size",
        type=str,
        default="64kB",
        help="specify embedded buffer size",
    )
    # options.embed_spm_assoc
    parser.add_argument(
        "--embed-spm-assoc",
        type=str,
        default="full",
        help="embedded buffer associativity: "
        "use string, full: fully-associative",
    )
    # options.embed_spm_lat
    parser.add_argument(
        "--embed-spm-lat",
        type=int,
        default=12,
        help="specify embedded SPM latency",
    )
    # options.cvsram_enable
    parser.add_argument(
        "--cvsram-enable",
        action="store_true",
        default=False,
        help="Use NVDLA CVSRAM",
    )
    # options.cvsram_size
    parser.add_argument(
        "--cvsram-size",
        type=str,
        default="1MB",
        help="specify NVDLA CVSRAM size",
    )
    # options.cvsram_bandwidth
    parser.add_argument(
        "--cvsram-bandwidth",
        type=str,
        default="128GB/s",
        help="Bandwidth of CVSRAM",
    )
    # options.remapper
    parser.add_argument(
        "--remapper",
        help="Prefix of the name of remapper class",
        default="Identity",
    )
    # options.add_accel_private_cache
    parser.add_argument(
        "--add-accel-private-cache",
        action="store_true",
        default=False,
        help="Add private cache for NVDLA",
    )
    # options.accel_pr_cache_size
    parser.add_argument(
        "--accel-pr-cache-size",
        type=str,
        default="1MB",
        help="specify private cache size for accelerators",
    )
    # options.accel_pr_cache_assoc
    parser.add_argument(
        "--accel-pr-cache-assoc",
        type=int,
        default=16,
        help="specify private cache associativity for accelerators",
    )
    # options.accel_pr_cache_tag_lat
    parser.add_argument(
        "--accel-pr-cache-tag-lat",
        type=int,
        default=12,
        help="specify private cache tag latency for accelerators",
    )
    # options.accel_pr_cache_dat_lat
    parser.add_argument(
        "--accel-pr-cache-dat-lat",
        type=int,
        default=12,
        help="specify private cache data latency for accelerators",
    )
    # options.accel_pr_cache_resp_lat
    parser.add_argument(
        "--accel-pr-cache-resp-lat",
        type=int,
        default=5,
        help="specify private cache response latency for accelerators",
    )
    # options.accel_pr_cache_mshr
    parser.add_argument(
        "--accel-pr-cache-mshr",
        type=int,
        default=32,
        help="specify number of private cache mshrs",
    )
    # options.accel_pr_cache_tgts_per_mshr
    parser.add_argument(
        "--accel-pr-cache-tgts-per-mshr",
        type=int,
        default=8,
        help="specify number of targets per private cache mshr",
    )
    # options.accel_pr_cache_wr_buf
    parser.add_argument(
        "--accel-pr-cache-wr-buf",
        type=int,
        default=8,
        help="specify number of private cache write buffers for accelerators",
    )
    # options.accel_pr_cache_clus
    parser.add_argument(
        "--accel-pr-cache-clus",
        type=str,
        default="mostly_incl",
        help="specify private cache size cusivity for accelerators",
    )
    # options.add_accel_shared_cache
    parser.add_argument(
        "--add-accel-shared-cache",
        action="store_true",
        default=False,
        help="Add shared cache for numNVDLA * NVDLA",
    )
    # options.accel_sh_cache_size
    parser.add_argument(
        "--accel-sh-cache-size",
        type=str,
        default="4MB",
        help="specify shared cache size for accelerators",
    )
    # options.accel_sh_cache_assoc
    parser.add_argument(
        "--accel-sh-cache-assoc",
        type=int,
        default=16,
        help="specify shared cache associativity for accelerators",
    )
    # options.accel_sh_cache_tag_lat
    parser.add_argument(
        "--accel-sh-cache-tag-lat",
        type=int,
        default=12,
        help="specify shared cache tag latency for accelerators",
    )
    # options.accel_sh_cache_dat_lat
    parser.add_argument(
        "--accel-sh-cache-dat-lat",
        type=int,
        default=12,
        help="specify shared cache data latency for accelerators",
    )
    # options.accel_sh_cache_resp_lat
    parser.add_argument(
        "--accel-sh-cache-resp-lat",
        type=int,
        default=5,
        help="specify shared cache response latency for accelerators",
    )
    # options.accel_sh_cache_mshr
    parser.add_argument(
        "--accel-sh-cache-mshr",
        type=int,
        default=128,
        help="specify number of shared cache mshrs",
    )
    # options.accel_sh_cache_tgts_per_mshr
    parser.add_argument(
        "--accel-sh-cache-tgts-per-mshr",
        type=int,
        default=8,
        help="specify number of targets per shared cache mshr",
    )
    # options.accel_sh_cache_wr_buf
    parser.add_argument(
        "--accel-sh-cache-wr-buf",
        type=int,
        default=32,
        help="specify number of shared cache write buffers for accelerators",
    )
    # options.accel_sh_cache_clus
    parser.add_argument(
        "--accel-sh-cache-clus",
        type=str,
        default="mostly_excl",
        help="specify shared cache size cusivity for accelerators",
    )
    # options.pft_enable
    parser.add_argument(
        "--pft-enable",
        action="store_true",
        default=False,
        help="issue hardware prefetching when inflight request queue is underrun",
    )
    # options.pft_threshold
    parser.add_argument(
        "--pft-threshold",
        type=int,
        default=16,
        help="the threshold of current inflight memory requests to launch software prefetch",
    )
    # options.use_fake_mem
    parser.add_argument(
        "--use-fake-mem",
        action="store_true",
        default=False,
        help="whether to use fake memory to simulate",
    )
    parser.add_argument(
        "-P",
        "--param",
        action="append",
        default=[],
        help="Set a SimObject parameter relative to the root node. "
        "An extended Python multi range slicing syntax can be used "
        "for arrays. For example: "
        "'system.cpu[0,1,3:8:2].max_insts_all_threads = 42' "
        "sets max_insts_all_threads for cpus 0, 1, 3, 5 and 7 "
        "Direct parameters of the root object are not accessible, "
        "only parameters of its children.",
    )
    parser.add_argument(
        "--vio-9p", action="store_true", help=Options.vio_9p_help
    )
    parser.add_argument(
        "--dtb-gen",
        action="store_true",
        help="Doesn't run simulation, it generates a DTB only",
    )
    parser.add_argument(
        "--loadable",
        type=str,
        required=True,
        help="Path to model loadable",
    )
    parser.add_argument(
        "--image", "--input-image",
        dest="image",
        type=str,
        required=True,
        help="Path to model input image",
    )
    parser.add_argument("--normalize", type=float, default=255.0,
                        help="Normalization value passed to the runtime (default: 255)")
    # task23 2-DLA (from task27): batches and runtime binary
    parser.add_argument("--batches", type=int, default=1,
                        help="Number of batches passed to the runtime (--batches); "
                             "only added to the runtime command when > 1")
    parser.add_argument("--runtime", type=str,
                        default="/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/verilator_runtime_RISCV/nv_full_nvdla_runtime_riscv",
                        help="RISC-V nvdla_runtime binary (default: the 1-DLA runtime used so far)")
    # ---------------- task12: UCIe chiplet topology ----------------
    parser.add_argument('--ucie-dla', action='store_true',
                        help='DLA chiplet: NVDLA dram/dma -> IOXBar -> UCIe dla_link -> ucie_fabric')
    parser.add_argument('--ucie-io', action='store_true',
                        help='I/O chiplet: ucie_fabric -> UCIe io_link -> io_xbar -> MemCtrl/iobridge')
    parser.add_argument('--ucie-cpu', action='store_true',
                        help='CPU chiplet: L2 (+uncached DLA bridge) -> IOXBar -> UCIe cpu_link -> fabric/L3')
    # task20: L2 size override (default: caches.py L2Cache = 256kB)
    parser.add_argument('--l2-size', type=str, default=None,
                        help='CPU L2 cache size, e.g. 4MB (default: 256kB from caches.py)')
    parser.add_argument('--l2-assoc', type=int, default=None,
                        help='CPU L2 associativity (default: 8 from caches.py)')
    parser.add_argument('--l1i-size', type=str, default=None,
                        help='CPU L1 instruction cache size (default: 32kB from caches.py)')
    parser.add_argument('--l1d-size', type=str, default=None,
                        help='CPU L1 data cache size (default: 32kB from caches.py)')
    parser.add_argument('--l1i-assoc', type=int, default=None,
                        help='CPU L1I associativity (default: 8)')
    parser.add_argument('--l1d-assoc', type=int, default=None,
                        help='CPU L1D associativity (default: 8)')
    parser.add_argument('--l3', action='store_true',
                        help='Shared L3 chiplet between CPU chiplet and fabric (DLA traffic bypasses it)')
    parser.add_argument('--l3-size', type=str, default='2MB')
    parser.add_argument('--l3-assoc', type=int, default=16)
    parser.add_argument('--l3-lat', type=int, default=20,
                        help='L3 tag/data/response latency (cycles)')
    parser.add_argument('--dla-xbar-only', action='store_true',
                        help='Control: insert the DLA IOXBar but no UCIe link')
    parser.add_argument('--ucie-config', type=str, default=None,
                        help='UCIe Delay YAML used for all links (default configs/example/ucie/Config.yaml)')
    parser.add_argument('--ucie-dla-config', type=str, default=None)
    parser.add_argument('--ucie-io-config', type=str, default=None)
    parser.add_argument('--ucie-cpu-config', type=str, default=None)
    parser.add_argument('--no-sf', type=str, default='',
                        help='comma list of {l3bus,fabric}: disable snoop filter there')
    parser.add_argument('--ucie-queue-size', type=int, default=16)
    parser.add_argument('--ucie-header-bits', type=int, default=64)
    # ---------------- task13: flit-accurate UCIe mode ----------------
    parser.add_argument('--ucie-link-mode', default='packet',
                        choices=['packet', 'flit256_std', 'flit68', 'flit256_leandros'])
    parser.add_argument('--ucie-phy-model', default='leandros',
                        choices=['leandros', 'spec', 'none'])
    parser.add_argument('--ucie-ber', type=float, default=0.0)
    parser.add_argument('--ucie-ber-deterministic', action='store_true')
    parser.add_argument('--ucie-lanes', type=int, default=16)
    parser.add_argument('--ucie-gtps', type=float, default=4.0)
    parser.add_argument('--ucie-retry-buffer', type=int, default=64)
    parser.add_argument('--ucie-seed', type=int, default=1)
    parser.add_argument('--ucie-csb', action='store_true',
                        help='CSB register path CPU->NVDLA crosses a UCIe link (csb_sync: CPU stalls for the round trip)')
    parser.add_argument('--ucie-csb-config', type=str, default=None)
    # ---------------- task15: L3 chiplet + DRAM chiplet ----------------
    parser.add_argument('--l3-chiplet', action='store_true',
                        help='L3 as a separate chiplet: [cpu_link ->] ucie_fabric_cpu -> ucie_l3_link -> L3 -> ucie_l3_mem_link -> ucie_fabric (DLA shared range bypasses it)')
    parser.add_argument('--ucie-l3-config', type=str, default=None,
                        help='YAML for ucie_l3_link and ucie_l3_mem_link')
    parser.add_argument('--ucie-dram', action='store_true',
                        help='DRAM chiplet (MemCtrl+DRAM): io_xbar (or fabric) -> ucie_dram_link -> MemCtrl')
    parser.add_argument('--ucie-dram-config', type=str, default=None)
    return parser
def generateDtb(root):
    root.system.generateDtb(os.path.join(m5.options.outdir, "system.dtb"))
def main():
    parser = argparse.ArgumentParser(
        description="Generic ARM big.LITTLE configuration"
    )
    addOptions(parser)
    options = parser.parse_args()
    for name in ("loadable", "image"):
        path = getattr(options, name)
        if not os.path.isfile(path):
            parser.error("--{}: file not found: {}".format(name, path))
    # Program to execute
    #binary = "/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/verilator_runtime_RISCV/riscv_verilator_modified_nvdla_runtime_v2"
    # binary= "/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/verilator_runtime_RISCV/nv_full_nvdla_runtime_riscv"
    binary = options.runtime
    if not os.path.isfile(binary):
        parser.error("--runtime: file not found: {}".format(binary))
    if options.dlas < 1 or options.dlas > 2:
        parser.error("--dlas must be 1 or 2 (CPU has nvdla_port_plus_0/1 only)")

    # Simulation system
    system = System(multi_thread=True)
    # Clock configuration
    system.clk_domain = SrcClockDomain()
    # system.clk_domain.clock = "3GHz"

    print("Little CPU clock:", options.little_cpu_clock)
    system.clk_domain.clock = options.little_cpu_clock
    system.clk_domain.voltage_domain = VoltageDomain()

    # Memory configuration
    system.mem_mode = "timing"
    system.mem_ranges = [AddrRange("1GB")]
    NVDLA_SHARED_BASE = 0x20000000
    NVDLA_SHARED_SIZE = 0x20000000
    NVDLA_PIO_BASE = 0x40000000
    NVDLA_PIO_STRIDE = 0x20040
    cpu_cacheable_range = AddrRange(start=0x00000000, size=NVDLA_SHARED_BASE)
    nvdla_shared_range = AddrRange(start=NVDLA_SHARED_BASE, size=NVDLA_SHARED_SIZE)

    # Create CPU
    # system.cpu = X86MinorCPU()
    system.cpu = RiscvMinorCPU(numThreads=2)

    # Create L1 caches
    # task20: L1 size/assoc overridable (defaults unchanged: 32kB, 8-way)
    system.cpu.icache = L1Cache(assoc=options.l1i_assoc or 8)
    system.cpu.dcache = L1Cache(assoc=options.l1d_assoc or 8)
    if options.l1i_size:
        system.cpu.icache.size = options.l1i_size
    if options.l1d_size:
        system.cpu.dcache.size = options.l1d_size
    system.cpu.dcache.addr_ranges = [cpu_cacheable_range]
    system.cpu.icache.cpu_side = system.cpu.icache_port
    system.cpu.dcache.cpu_side = system.cpu.dcache_port

    # Create L1 to L2 interconnect
    system.l2bus = L2XBar()
    # Link L1 with interconnect
    system.cpu.icache.mem_side = system.l2bus.cpu_side_ports
    system.cpu.dcache.mem_side = system.l2bus.cpu_side_ports
    # Create L2 cache
    system.l2cache = L2Cache()
    if options.l2_size:
        system.l2cache.size = options.l2_size  # task20
    if options.l2_assoc:
        system.l2cache.assoc = options.l2_assoc  # task20
    system.l2cache.addr_ranges = [cpu_cacheable_range]
    # Link L2 cache with L1 to L2 interconnect
    system.l2cache.cpu_side = system.l2bus.mem_side_ports
    # ------------------------------------------------------------------
    # task12 chiplet topology. With no --ucie-*/--l3 flags the wiring is
    # identical to se_RTL_ucie.py without --ucie (baseline).
    # ------------------------------------------------------------------
    here = os.path.dirname(os.path.abspath(__file__))
    default_yaml = options.ucie_config or os.path.join(here, "..", "ucie", "Config.yaml")
    def link_yaml(specific):
        y = os.path.abspath(specific or default_yaml)
        if not os.path.isfile(y):
            parser.error("UCIe YAML not found: " + y)
        return y
    def mk_link(yaml):
        return UCIeLink(config_file=link_yaml(yaml),
                        queue_size=options.ucie_queue_size,
                        header_bits=options.ucie_header_bits,
                        link_mode=options.ucie_link_mode,
                        phy_model=options.ucie_phy_model,
                        ber=options.ucie_ber,
                        ber_deterministic=options.ucie_ber_deterministic,
                        lanes=options.ucie_lanes,
                        gtps=options.ucie_gtps,
                        retry_buffer_flits=options.ucie_retry_buffer,
                        rng_seed=options.ucie_seed)

    # UCIe die-to-die fabric (former membus)
    system.ucie_fabric = SystemXBar()
    fabric = system.ucie_fabric

    # Shared NVDLA region bypasses L2; the Bridge isolates the coherent
    # l2bus. It advertises only the DLA shared range.
    system.nvdla_shared_bridge = Bridge(delay="1ns", ranges=[nvdla_shared_range])
    system.nvdla_shared_bridge.cpu_side_port = system.l2bus.mem_side_ports

    # ---- CPU chiplet egress (+ optional L3 chiplet) ----
    if options.l3 and options.l3_chiplet:
        parser.error('--l3 (inline) and --l3-chiplet are exclusive')
    if options.l3_chiplet:
        # task15: L3 on its own chiplet. The fabric is split into two gem5
        # xbars (one physical switch): ucie_fabric_cpu (CPU side, above L3)
        # and ucie_fabric (below L3: DLA, I/O, DRAM). The L3 chiplet's UCIe
        # port is modelled by two links: fabric_cpu -> L3 (l3_link) and
        # L3 miss/writeback -> fabric (l3_mem_link).
        system.ucie_fabric_cpu = L2XBar()
        system.ucie_l3_link = mk_link(options.ucie_l3_config)
        system.ucie_l3_mem_link = mk_link(options.ucie_l3_config)
        system.l3cache = Cache(size=options.l3_size, assoc=options.l3_assoc,
                               tag_latency=options.l3_lat,
                               data_latency=options.l3_lat,
                               response_latency=options.l3_lat,
                               mshrs=32, tgts_per_mshr=12,
                               write_buffers=16,
                               addr_ranges=[cpu_cacheable_range])
        system.ucie_fabric_cpu.mem_side_ports = system.ucie_l3_link.cpu_side
        system.ucie_l3_link.mem_side = system.l3cache.cpu_side
        system.l3cache.mem_side = system.ucie_l3_mem_link.cpu_side
        system.ucie_l3_mem_link.mem_side = fabric.cpu_side_ports
        # DLA shared (uncacheable) range bypasses the L3 inside the fabric
        system.l3_dla_bypass = Bridge(delay="1ns", ranges=[nvdla_shared_range])
        system.l3_dla_bypass.cpu_side_port = system.ucie_fabric_cpu.mem_side_ports
        system.l3_dla_bypass.mem_side_port = fabric.cpu_side_ports
        cpu_dest = system.ucie_fabric_cpu.cpu_side_ports
    elif options.l3:
        system.l3bus = L2XBar()
        system.l3cache = Cache(size=options.l3_size, assoc=options.l3_assoc,
                               tag_latency=options.l3_lat,
                               data_latency=options.l3_lat,
                               response_latency=options.l3_lat,
                               mshrs=32, tgts_per_mshr=12,
                               write_buffers=16,
                               addr_ranges=[cpu_cacheable_range])
        system.l3cache.cpu_side = system.l3bus.mem_side_ports
        system.l3cache.mem_side = fabric.cpu_side_ports
        # DLA shared (uncacheable) range bypasses the L3
        system.l3_dla_bypass = Bridge(delay="1ns", ranges=[nvdla_shared_range])
        system.l3_dla_bypass.cpu_side_port = system.l3bus.mem_side_ports
        system.l3_dla_bypass.mem_side_port = fabric.cpu_side_ports
        cpu_dest = system.l3bus.cpu_side_ports
    else:
        cpu_dest = fabric.cpu_side_ports
    if options.ucie_cpu:
        system.cpu_xbar = IOXBar()
        system.l2cache.mem_side = system.cpu_xbar.cpu_side_ports
        system.nvdla_shared_bridge.mem_side_port = system.cpu_xbar.cpu_side_ports
        system.ucie_cpu_link = mk_link(options.ucie_cpu_config)
        system.cpu_xbar.mem_side_ports = system.ucie_cpu_link.cpu_side
        system.ucie_cpu_link.mem_side = cpu_dest
    else:
        system.l2cache.mem_side = cpu_dest
        if options.l3 or options.l3_chiplet:
            system.nvdla_shared_bridge.mem_side_port = fabric.cpu_side_ports
        else:
            system.nvdla_shared_bridge.mem_side_port = cpu_dest

    no_sf = [t for t in options.no_sf.split(',') if t]
    if options.l3 and options.ucie_cpu and 'fabric' not in no_sf:
        # The non-snooping cpu_link hides L2 from the L3, so L3 evictions
        # never carry BLOCK_CACHED; the fabric snoop filter would then
        # panic ("requestor is not a holder") on a later re-eviction.
        print("task12: --l3 --ucie-cpu => ucie_fabric.snoop_filter = NULL")
        no_sf.append('fabric')
    if options.l3_chiplet and 'fabric' not in no_sf:
        # task15: L3 sits behind the non-snooping l3_mem_link; same
        # workaround as task12 (L3 evictions without BLOCK_CACHED).
        print("task15: --l3-chiplet => ucie_fabric.snoop_filter = NULL")
        no_sf.append('fabric')
    for x in no_sf:
        if x == 'l3bus' and options.l3:
            system.l3bus.snoop_filter = NULL
        elif x == 'l3bus' and options.l3_chiplet:
            system.ucie_fabric_cpu.snoop_filter = NULL
        elif x == 'fabric':
            fabric.snoop_filter = NULL
        else:
            parser.error('--no-sf: unknown/invalid ' + x)

    # ---- I/O chiplet: memory controller + PIO bridge ----
    if options.ucie_io:
        system.ucie_io_link = mk_link(options.ucie_io_config)
        system.io_xbar = IOXBar()
        system.ucie_io_link.cpu_side = fabric.mem_side_ports
        system.io_xbar.cpu_side_ports = system.ucie_io_link.mem_side
        io_dest = system.io_xbar.mem_side_ports
    else:
        io_dest = fabric.mem_side_ports

    system.iobus = IOXBar()
    system.iobridge = Bridge(delay="50ns")
    system.iobridge.ranges = [AddrRange(start=NVDLA_PIO_BASE, size=NVDLA_PIO_STRIDE * options.dlas)]
    system.iobridge.mem_side_port = system.iobus.cpu_side_ports
    system.iobridge.cpu_side_port = io_dest
    print("options.freq_ratio:", options.freq_ratio)
    # Create NVDLA Device
    system.nvdla = [
        NvDlaDeviceSE(
            id_nvdla=i,
            pio_addr=NVDLA_PIO_BASE + NVDLA_PIO_STRIDE * i,
            pio_size=NVDLA_PIO_STRIDE,
            dma_enable=True,  # options.dma_enable,
            spm_latency=options.embed_spm_lat,
            spm_line_size=1024,
            spm_size=options.embed_spm_size,
            use_shared_spm=options.shared_spm,
            freq_ratio=options.freq_ratio,
            assoc=options.embed_spm_assoc.lower(),
            base_addr_dram=0x40000000,
            base_addr_sram=0x0,
            trace_mode=False,
            maxReq=options.maxReqNVDLA,
        )
        for i in range(options.dlas)
    ]
    for i in range(options.dlas):
        system.nvdla[i].pio = system.iobus.mem_side_ports

    # ---- DLA chiplet ----
    if options.ucie_dla or options.dla_xbar_only:
        system.dla_xbars = [IOXBar() for _ in range(options.dlas)]
    if options.ucie_dla:
        system.ucie_dla_links = [mk_link(options.ucie_dla_config)
                                 for _ in range(options.dlas)]
    for i in range(options.dlas):
        if options.ucie_dla or options.dla_xbar_only:
            system.nvdla[i].dram_port = system.dla_xbars[i].cpu_side_ports
            system.nvdla[i].dma_port = system.dla_xbars[i].cpu_side_ports
            if options.ucie_dla:
                system.dla_xbars[i].mem_side_ports = system.ucie_dla_links[i].cpu_side
                system.ucie_dla_links[i].mem_side = fabric.cpu_side_ports
            else:
                system.dla_xbars[i].mem_side_ports = fabric.cpu_side_ports
        else:
            system.nvdla[i].dram_port = fabric.cpu_side_ports
            system.nvdla[i].dma_port = fabric.cpu_side_ports
    # CSB register path: direct CPU<->NVDLA (task12), or through a
    # synchronous UCIe link (task13 --ucie-csb, no xbar in between)
    if options.ucie_csb:
        csb_links = []
        for i in range(options.dlas):
            l = mk_link(options.ucie_csb_config)
            l.csb_sync = True
            csb_links.append(l)
        system.ucie_csb_links = csb_links
        for i in range(options.dlas):
            exec(f"system.cpu.nvdla_port_plus_{i} = system.ucie_csb_links[{i}].cpu_side")
            system.ucie_csb_links[i].mem_side = system.nvdla[i].cmd_cpu_side
    else:
        for i in range(options.dlas):
            exec(
                f"system.nvdla[{i}].cmd_cpu_side = system.cpu.nvdla_port_plus_{i}"
            )

    system.cpu.createInterruptController()
    system.system_port = fabric.cpu_side_ports
    # task15 --ucie-dram: DRAM chiplet holds MemCtrl + DRAM (HBM-base-die
    # style); the I/O chiplet keeps iobridge/PIO.
    if options.ucie_dram:
        system.ucie_dram_links = [mk_link(options.ucie_dram_config)
                                  for _ in system.mem_ranges]
        for l in system.ucie_dram_links:
            l.cpu_side = io_dest
        mem_dest = [l.mem_side for l in system.ucie_dram_links]
    else:
        mem_dest = [io_dest for _ in system.mem_ranges]
    system.mem_ctrls = [
        MemCtrl(
            dram=eval(options.ddr_type + "(range=r)"),
            port=mem_dest[k],
        )
        for k, r in enumerate(system.mem_ranges)
    ]
    system.workload = SEWorkload.init_compatible(binary)
    # Create a process for a the application
    process = Process()
    # Command is a list which begins with the executable (like argv)
    process.cmd = [
        binary,
        "--loadable",
        options.loadable,
        "--image",
        options.image,
    ]
    if options.normalize is not None:
        process.cmd += ["--normalize", str(options.normalize)]
    process.cmd += [
        "--dlas",
        str(options.dlas),
    ]
    if options.batches > 1:
        process.cmd += ["--batches", str(options.batches)]
    print("Runtime cmd:", " ".join(process.cmd))
    # Set the cpu to use the process as its workload and create thread contexts
    system.cpu.workload = [process, process]
    system.cpu.createThreads()
    # Set up the root SimObject and start the simulation
    root = Root(full_system=False, system=system)
    # Instantiate all of the objects we've created above
    m5.instantiate()
    # Dedicate upper 1GB to Gemmini device
    system.cpu.workload[0].map(
        0x2000_0000, 0x2000_0000, 0x2000_0000, cacheable=False
    )
    print("========== Beginning simulation ==========")
    exit_event = m5.simulate()
    print(
        "Exiting @ tick {} because {}".format(
            m5.curTick(), exit_event.getCause()
        )
    )
if __name__ == "__m5_main__":
    main()
