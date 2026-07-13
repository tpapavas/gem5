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
from common.cores.arm import ex5_LITTLE

import devices
from devices import AtomicCluster, KvmCluster, FastmodelCluster

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
    return parser


def generateDtb(root):
    root.system.generateDtb(os.path.join(m5.options.outdir, "system.dtb"))


def main():
    parser = argparse.ArgumentParser(
        description="Generic ARM big.LITTLE configuration"
    )
    addOptions(parser)
    options = parser.parse_args()

    # Program to execute
    # binary = 'tests/test-progs/nvdla-se/nvdla-se'
    binary = "<custom-path-to-runtime>/nvdla_runtime"
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
    # system.mem_ranges.append(AddrRange(start=0xC0000000, size="1GB"))

    # Create CPU
    # system.cpu = X86MinorCPU()
    system.cpu = ArmMinorCPU(numThreads=2)

    # Create Gemmini device
    # system.gemmini_dev = GemminiDevA(
    #     ndp_ctrl=("0x40000000", "0x40001000"),
    #     ndp_data=("0x40001000", "0x80000000"),
    #     max_rsze=0x40,
    #     max_reqs=64,
    # )

    # Create L1 caches
    system.cpu.icache = L1Cache()
    system.cpu.dcache = L1Cache()
    system.cpu.dcache.addr_ranges = system.mem_ranges

    # Connect L1I cache to the CPU
    system.cpu.icache.cpu_side = system.cpu.icache_port
    system.cpu.dcache.cpu_side = system.cpu.dcache_port

    # # Connect Gemmini device to the CPU and L1D to Gemmini device
    # # system.gemmini_dev.cpu_side = system.cpu.dcache_port
    # # system.cpu.dcache.cpu_side = system.gemmini_dev.mem_side

    # Create L1 to L2 interconnect
    system.l2bus = L2XBar()

    # Link L1 with interconnect
    system.cpu.icache.mem_side = system.l2bus.cpu_side_ports
    system.cpu.dcache.mem_side = system.l2bus.cpu_side_ports

    # Create L2 cache
    system.l2cache = L2Cache()

    # Link L2 cache with L1 to L2 interconnect
    system.l2cache.cpu_side = system.l2bus.mem_side_ports

    # Create memory bus
    system.membus = SystemXBar()

    # Link L2 with interconnect
    system.l2cache.mem_side = system.membus.cpu_side_ports

    # # For NO CACHE system
    # system.cpu.icache_port = system.membus.cpu_side_ports
    # system.cpu.dcache_port = system.membus.cpu_side_ports

    # Connect Gemmini device to L2
    # system.gemmini_dev.dma_port = system.l2bus.cpu_side_ports
    system.iobus = IOXBar()
    system.iobridge = Bridge(delay="50ns")
    system.iobridge.ranges = [AddrRange(start=0x40000000, size="1GB")]

    system.iobridge.mem_side_port = system.iobus.cpu_side_ports
    system.iobridge.cpu_side_port = system.membus.mem_side_ports

    print("options.freq_ratio:", options.freq_ratio)
    # Create NVDLA Device
    system.nvdla = [
        NvDlaDeviceSE(
            id_nvdla=i,
            pio_addr=0x40000000 + 0x20040 * i,
            pio_size=0x20040,
            dma_enable=True,  # options.dma_enable,
            spm_latency=options.embed_spm_lat,
            spm_line_size=1024,
            spm_size=options.embed_spm_size,
            use_shared_spm=options.shared_spm,
            freq_ratio=options.freq_ratio,
            assoc=options.embed_spm_assoc.lower(),
            base_addr_dram=0x40000000,
            base_addr_sram=0x0,
            maxReq=options.maxReqNVDLA,
        )
        for i in range(options.dlas)
    ]
    for i in range(options.dlas):
        system.nvdla[i].pio = system.iobus.mem_side_ports

    # for DMA
    for i in range(options.dlas):
        system.nvdla[i].dram_port = system.membus.cpu_side_ports
        system.nvdla[i].dma_port = system.membus.cpu_side_ports

    for i in range(options.dlas):
        exec(
            f"system.nvdla[{i}].cmd_cpu_side = system.cpu.nvdla_port_plus_{i}"
        )
        # system.nvdla[0].cmd_cpu_side = system.cpu.nvdla_port_plus_0
        # system.nvdla[1].cmd_cpu_side = system.cpu.nvdla_port_plus_1

    # for caches
    # system.nvdla_pr_cache = Cache(
    #     tag_latency=options.accel_pr_cache_tag_lat,
    #     data_latency=options.accel_pr_cache_dat_lat,
    #     response_latency=options.accel_pr_cache_resp_lat,
    #     mshrs=options.accel_pr_cache_mshr,
    #     tgts_per_mshr=options.accel_pr_cache_tgts_per_mshr,
    #     size=options.accel_pr_cache_size,
    #     assoc=options.accel_pr_cache_assoc,
    #     write_buffers=options.accel_pr_cache_wr_buf,
    #     clusivity=options.accel_pr_cache_clus
    # )
    # system.nvdla.dram_port = system.nvdla_pr_cache.cpu_side
    # system.nvdla_pr_cache.mem_side = system.membus.cpu_side_ports

    # Create interrupt controller
    system.cpu.createInterruptController()

    # Connect interruptions and IO with memory bus (required by X86)
    if m5.defines.buildEnv["USE_X86_ISA"]:
        system.cpu.interrupts[0].pio = system.membus.mem_side_ports
        system.cpu.interrupts[0].int_master = system.membus.cpu_side_ports
        system.cpu.interrupts[0].int_slave = system.membus.mem_side_ports

    # Connect special port to allow read/write memory
    system.system_port = system.membus.cpu_side_ports

    # Create a DDR3 memory controller
    # system.mem_ctrls = MemCtrl()
    # system.mem_ctrls.dram = DDR3_1600_8x8()
    # system.mem_ctrls.dram.range = system.mem_ranges[0]
    # system.mem_ctrls.port = system.membus.mem_side_ports

    system.mem_ctrls = [
        MemCtrl(
            dram=eval(options.ddr_type + "(range=r)"),
            port=system.membus.mem_side_ports,
        )
        for r in system.mem_ranges
    ]

    system.workload = SEWorkload.init_compatible(binary)

    # Create a process for a the application
    process = Process()

    # Command is a list which begins with the executable (like argv)
    process.cmd = [
        binary,
        "--loadable",
        "/data/tpapavasileiou/tools/GEM5-NVDLA/nvdla/gem5-plus/nonet.nvdla",
        "--image",
        "/data/tpapavasileiou/tools/GEM5-NVDLA/nvdla/gem5-plus/random_2x2_bin.pgm",
        # "--normalize",
        # "255",
        "--dlas",
        options.dlas,
    ]

    # Set the cpu to use the process as its workload and create thread contexts
    system.cpu.workload = [process, process]
    system.cpu.createThreads()

    # Set up the root SimObject and start the simulation
    root = Root(full_system=False, system=system)

    # Instantiate all of the objects we've created above
    m5.instantiate()

    # Dedicate upper 1GB to Gemmini device
    system.cpu.workload[0].map(
        0x2000_0000, 0x2000_0000, 0x6000_0000, cacheable=False
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
