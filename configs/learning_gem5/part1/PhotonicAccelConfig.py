import m5
from m5.objects import *
from m5.util import addToPath
from m5.objects.PhotonicAccel import PhotonicAccel
import os
from caches import *
import m5.debug


# Get the path to the current script
thispath = os.path.dirname(os.path.realpath(__file__))

# Define the binary path relative to the script location
binary = os.path.join(
    thispath, "../../../tests/test-progs/mat-mul/bin/mat-mul"
)

# Check if the binary exists and is executable
if os.path.exists(binary):
    if os.access(binary, os.X_OK):
        print(f"Using binary: {binary}")
    else:
        print(
            f"Warning: {binary} exists but is not executable. Try 'chmod +x {binary}'"
        )
else:
    print(f"Warning: Binary {binary} does not exist.")
    binary = ""  # Set to empty if not found

# Set up the system
system = System()

# Set up the clock domain
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

# Set the memory mode
system.mem_mode = "timing"

system.mem_ranges = [
    AddrRange("3GB"),  # Regular RAM
    AddrRange(0xE0000000, size="4kB"),  # Matrix A
    AddrRange(0xE0001000, size="4kB"),  # Matrix B
    AddrRange(0xE0002000, size="4kB"),  # Matrix Result
    AddrRange(0xF0000000, size="4kB"),  # Accelerator registers
]

# Define address ranges for the accelerator
# Range for accelerator control registers
accel_control_start = 0xF0000000
accel_control_size = 0x1000
accel_control_end = accel_control_start + accel_control_size
print(
    f"Accelerator control range: 0x{accel_control_start:08x} - 0x{accel_control_end:08x}"
)


# Create the CPU
system.cpu = TimingSimpleCPU()

# Create the main memory bus (connects CPU to memory)
system.membus = SystemXBar()

# Connect the CPU caches directly to the memory bus
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

# Create a memory controller and connect it to the membus for main memory
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports


# Matrix data mem is covering only Matrix A,leaving out Matrix B and the Results
# Create memory for matrix data
system.matrix_data_mem = SimpleMemory()
# system.matrix_data_mem.range = AddrRange(matrix_data_start, matrix_data_end)
system.matrix_data_mem.range = AddrRange(0xE0000000, 0xE0003000)

# system.matrix_data_mem.port = system.membus.mem_side_ports

print("Created memory for matrix data")


# Create the IO bus (connects accelerator to memory)
system.iobus = IOXBar()

# Create and connect the PhotonicAccel
system.photonic_accel = PhotonicAccel()

# Configure photonic parameters
system.photonic_accel.dac_resolution = 8
system.photonic_accel.adc_resolution = 8
system.photonic_accel.dac_noise_level = 0.005
system.photonic_accel.adc_noise_level = 0.005
system.photonic_accel.tia_gain = 5.0
system.photonic_accel.mzi_phase_noise = 0.01
system.photonic_accel.photodetector_noise = 0.02
system.photonic_accel.pio_addr = 0xF0000000
system.photonic_accel.pio_size = 0x1000

# Set additional photonic core parameters i set all these to 0 for now
system.photonic_accel.cmd_trigger = True
system.photonic_accel.data_a = 16
system.photonic_accel.data_b = 16
system.photonic_accel.lsb_noise = 0.005
system.photonic_accel.noise_scale = 0.005
system.photonic_accel.scaling_factor = 0.2
system.photonic_accel.offset1 = 0.2
system.photonic_accel.offset2 = 0.2
system.photonic_accel.offset_error = 0.5
system.photonic_accel.delay_prop = 10
system.photonic_accel.amplification = 1.0
system.photonic_accel.delay_response = 100
system.photonic_accel.latency = "1ns"
system.photonic_accel.throughput = 0.5

print("Successfully created PhotonicAccel with photonic parameters")

# Connect the accelerator to the IO bus
system.photonic_accel.control_port = system.iobus.mem_side_ports
print("Connected PhotonicAccel control port to memory bus")

# Connect the memory port directly to memory bus
system.photonic_accel.memory_port = system.matrix_data_mem.port
print("Connected PhotonicAccel memory port to memory bus")


system.photonic_accel.pio_addr = 0xF0000000
system.photonic_accel.pio_size = 0x1000

system.iobridge = Bridge()
system.iobridge.mem_side_port = system.iobus.cpu_side_ports
system.iobridge.cpu_side_port = system.membus.mem_side_ports
system.iobridge.ranges = [AddrRange(accel_control_start, accel_control_end)]
system.iobridge.ranges = [
    # AddrRange(system.photonic_accel.pio_addr,
    #  system.photonic_accel.pio_addr + system.photonic_accel.pio_size),
    AddrRange(0xF0000000, 0xF0001000),
    AddrRange(0xE0000000, 0xE0003000),  # Matrix data range
]

system.system_port = system.membus.cpu_side_ports


# Initialize workload
if binary and os.path.exists(binary) and os.access(binary, os.X_OK):
    # Only proceed if binary exists and is executable
    try:
        system.workload = SEWorkload.init_compatible(binary)

        # Create a proper process
        process = Process()
        process.executable = binary
        process.cmd = [binary]
        print(f"Created process with binary: {binary}")
    except Exception as e:
        print(f"Error creating workload: {e}")
        process = Process()
        process.cmd = [""]


# Set the process as the CPU workload
system.cpu.workload = process

# Create threads
system.cpu.createThreads()

# Create interrupt controller (required for X86)
if m5.defines.buildEnv["USE_X86_ISA"]:
    system.cpu.createInterruptController()
    system.cpu.interrupts[0].pio = system.membus.mem_side_ports
    system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
    system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# Set up the root SimObject
root = Root(full_system=False, system=system)

# Instantiate the simulation
try:
    m5.instantiate()
    print("Simulation instantiated successfully")

    # Run the simulation
    print("Beginning simulation!")
    print(
        "CPU will execute the binary with photonic accelerator performing matrix multiplication"
    )

    exit_event = m5.simulate()
    print(
        "Exiting @ tick {} because {}".format(
            m5.curTick(), exit_event.getCause()
        )
    )
except Exception as e:
    print(f"Error during simulation: {e}")
