from m5.params import *
from m5.proxy import *
from m5.objects.SimObject import SimObject

# from m5.objects.Device import PioDevice


class PhotonicAccel(SimObject):
    type = "PhotonicAccel"
    cxx_header = "learning_gem5/part2/PhotonicAccel.hh"
    cxx_class = "gem5::PhotonicAccel"

    # Basic parameters
    matrix_size = Param.Int(32, "Size of square matrices for multiplication")
    clock_frequency = Param.Clock("1GHz", "Clock frequency of the accelerator")

    # PIO address range for control registers
    pio_addr = Param.Addr(0xF0000000, "Base address for PIO control registers")
    pio_size = Param.Addr(0x1000, "Size of PIO address range")

    ACCEL_COMMAND_ADDR = Param.Addr(0xF0000000, "Command register address")
    ACCEL_CONFIG_ADDR = Param.Addr(
        0xF0000004, "Configuration register address"
    )
    ACCEL_STATUS_ADDR = Param.Addr(0xF0000008, "Status register address")

    # Matrix memory base addresses
    MATRIX_A_BASE = Param.Addr(0xE0000000, "Matrix A base address")
    MATRIX_B_BASE = Param.Addr(0xE0001000, "Matrix B base address")
    MATRIX_RES_BASE = Param.Addr(0xE0002000, "Result matrix base address")
    # MATRIX_SIZE = Param.UInt32(32, "Matrix size (N for NxN matrix)")

    # pio_addr=Param.Addr('0xE0000000', "This is the base address for the accelerator")
    # pio_size=Param.Addr('0x1000' ,"Size of the mapped region")

    # DAC/ADC parameters
    dac_resolution = Param.Int(8, "Resolution of the DAC in bits")
    adc_resolution = Param.Int(8, "Resolution of the ADC in bits")
    dac_noise_level = Param.Float(
        0.005, "Noise level for the DAC (as a fraction)"
    )
    adc_noise_level = Param.Float(
        0.005, "Noise level for the ADC (as a fraction)"
    )

    # Photonic components parameters
    tia_gain = Param.Float(5.0, "Trans-impedance amplifier gain")
    mzi_phase_noise = Param.Float(
        0.01, "Phase noise in MZI array (as a fraction)"
    )
    photodetector_noise = Param.Float(
        0.02, "Photodetector noise (as a fraction)"
    )

    control_port = ResponsePort("Port for reading Matrix A")
    memory_port = RequestPort("Port for reading Matrix B")

    # Other photonic core parameters some of these i have not yet implememnted them in the C++ file

    cmd_trigger = Param.Bool(False, "Trigger bit for command")
    data_a = Param.Int(16, "16-bit input A")
    data_b = Param.Int(16, "16-bit input B")
    lsb_noise = Param.Float(0.5, "Value of noise in terms of LSB of data")
    noise_scale = Param.Float(1.0, "Experimental noise scaling factor")
    scaling_factor = Param.Int(1, "Scaling factor for inputs")
    offset1 = Param.Float(0.2, "Offset for Analog1")
    offset2 = Param.Float(0.2, "Offset for Analog2")
    offset_error = Param.Int(0, "Offset error for PhotonicCore")
    delay_prop = Param.Int(10, "Propagation delay in nanoseconds")
    amplification = Param.Float(1.0, "Amplification factor for Result")
    delay_response = Param.Int(100, "Response delay in nanoseconds")

    # Metric Parameters
    latency = Param.Latency("1ns", "Total Latancy incurred")
    throughput = Param.Int(0, "Expected PA throughput")
