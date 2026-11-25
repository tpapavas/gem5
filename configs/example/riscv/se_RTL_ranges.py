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

    #binary = 'tests/test-progs/nvdla-se/nvdla-se'

    #binary = "<custom-path-to-runtime>/nvdla_runtime"

    #binary = "/data/tpapavasileiou/tools/GEM5-NVDLA/nvdla/gem5-plus/runtimes/nvdla_runtime_v2_0_SE_Verilated_nv_full"

    #binary = "/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/verilator_runtime_RISCV/nvdla_runtime_nv_full_SE_riscv"

    #binary = "/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/verilator_runtime_RISCV/riscv_verilator_modified_nvdla_runtime"

    binary = "/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/verilator_runtime_RISCV/riscv_verilator_modified_nvdla_runtime_v2"

    # binary = "/home/tpapavasileiou/nvdla_runtime_nv_small_SE"



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

    # Physical DRAM: 0x00000000 - 0x3fffffff (1 GiB)
    system.mem_ranges = [AddrRange("1GB")]

    # Keep the upper 512 MiB of DRAM as CPU/NVDLA shared memory.
    # CPU data accesses to this region are routed around L1D/L2 below.
    NVDLA_SHARED_BASE = 0x20000000
    NVDLA_SHARED_SIZE = 0x20000000
    NVDLA_PIO_BASE = 0x40000000
    NVDLA_PIO_STRIDE = 0x20040

    cacheable_dram_range = AddrRange(
        start=0x00000000, size=NVDLA_SHARED_BASE
    )
    nvdla_shared_range = AddrRange(
        start=NVDLA_SHARED_BASE, size=NVDLA_SHARED_SIZE
    )

    # Create CPU
    system.cpu = RiscvMinorCPU(numThreads=2)

    # Create L1 caches
    system.cpu.icache = L1Cache()
    system.cpu.dcache = L1Cache()

    # The normal CPU cache hierarchy only covers the lower 512 MiB.
    # The NVDLA shared region (0x20000000-0x3fffffff) bypasses L1D/L2.
    system.cpu.dcache.addr_ranges = [cacheable_dram_range]

    # Connect L1I directly to the CPU.
    system.cpu.icache.cpu_side = system.cpu.icache_port

    # Put a small routing xbar in front of L1D so CPU data requests can
    # either go through L1D/L2 (normal DRAM) or bypass them (NVDLA shared).
    system.cpu_dmem_xbar = L2XBar()
    system.cpu.dcache_port = system.cpu_dmem_xbar.cpu_side_ports
    system.cpu.dcache.cpu_side = system.cpu_dmem_xbar.mem_side_ports

    # Create L1 to L2 interconnect
    system.l2bus = L2XBar()

    # Link L1 with interconnect
    system.cpu.icache.mem_side = system.l2bus.cpu_side_ports
    system.cpu.dcache.mem_side = system.l2bus.cpu_side_ports



    # Create L2 cache
    system.l2cache = L2Cache()
    system.l2cache.addr_ranges = [cacheable_dram_range]
    system.l2cache.cpu_side = system.l2bus.mem_side_ports

    # Create memory bus
    system.membus = SystemXBar()

    # Link L2 with interconnect
    system.l2cache.mem_side = system.membus.cpu_side_ports

    # CPU accesses to the NVDLA shared DRAM bypass L1D and L2 completely.
    # This avoids creating CPU dirty/Modified cache lines for memory that
    # is also accessed by the NVDLA DMA engines.
    system.nvdla_shared_bridge = Bridge(
        delay="1ns", ranges=[nvdla_shared_range]
    )
    system.nvdla_shared_bridge.cpu_side_port = (
        system.cpu_dmem_xbar.mem_side_ports
    )
    system.nvdla_shared_bridge.mem_side_port = system.membus.cpu_side_ports


    system.iobus = IOXBar()
    system.iobridge = Bridge(delay="50ns")
    system.iobridge.ranges = [
        AddrRange(
            start=NVDLA_PIO_BASE,
            size=NVDLA_PIO_STRIDE * options.dlas,
        )
    ]

    system.iobridge.mem_side_port = system.iobus.cpu_side_ports
    system.iobridge.cpu_side_port = system.membus.mem_side_ports

    print("options.freq_ratio:", options.freq_ratio)

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
            base_addr_dram=NVDLA_SHARED_BASE,
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
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/caffe/lenet/lenet_nv_full_fp16.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/caffe/lenet/lenet_nv_small_int8.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/caffe/lenet/lenet_nv_full_int8.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/onnc/nn_suite/googlenet_cifar10_opset_6.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/onnc/out_nv_small_int8_NEW.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/onnc/lenet_gen_nv_small_int8.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/onnc/lenet_from_caffe_int8_nv_full.nvdla",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/onnc/lenet_from_caffe_int8_nv_small_recalculated.nvdla",
        options.loadable,
        "--image",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/eight_invert.pgm",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/images/cifar10/test_cat_0000.jpg",
        # "/data/tpapavasileiou/tools/GEM5-NVDLA/vp-bin/usr/local/nvdla/models/onnc/vp_in_000.jpg",
        options.image,
        "--normalize",
        str(options.normalize),
        "--rawdump",
        "--dlas",
        str(options.dlas),
    ]



    # Set the cpu to use the process as its workload and create thread contexts
    system.cpu.workload = [process, process]
    system.cpu.createThreads()


    # Set up the root SimObject and start the simulation
    root = Root(full_system=False, system=system)


    # Instantiate all of the objects we've created above
    m5.instantiate()



    # Map only the NVDLA shared DRAM window into the SE process.
    # It is exactly the upper 512 MiB of the configured 1 GiB DRAM.
    system.cpu.workload[0].map(
        NVDLA_SHARED_BASE,
        NVDLA_SHARED_BASE,
        NVDLA_SHARED_SIZE,
        cacheable=False,
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
