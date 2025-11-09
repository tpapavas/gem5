#include "learning_gem5/part2/PhotonicAccel.hh"

#include <cmath>
#include <random>

#include "base/statistics.hh"
#include "base/trace.hh"
#include "debug/PhotonicAccel.hh"
#include "dev/io_device.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/PhotonicAccel.hh"

//#include "dev/pio_device.hh"




namespace gem5
{

/////////////////////////////////////////////////////////////////////////////
// ControlPort Implementation (SlavePort/Response Port)
/////////////////////////////////////////////////////////////////////////////

PhotonicAccel::ControlPort::ControlPort(const std::string &name,
  PhotonicAccel *_owner)
    : SlavePort(name, _owner), owner(_owner)
{
}

Tick
PhotonicAccel::ControlPort::recvAtomic(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();

    // DEBUG: Log every packet received
    inform("DEBUG: Packet received - addr=0x%x, size=%d, %s",
           addr, pkt->getSize(), pkt->isRead() ? "READ" : "WRITE");

    // Handle the control register access
    if (pkt->isRead()) {
        // For reads, get data from the accelerator
        uint32_t data = owner->readMMIO(addr);

        inform("DEBUG: Packet read returning data=0x%x", data);

        // Set the data in the packet
        if (pkt->getSize() == 1) {
            pkt->setLE<uint8_t>(data);
        } else if (pkt->getSize() == 2) {
            pkt->setLE<uint16_t>(data);
        } else if (pkt->getSize() == 4) {
            pkt->setLE<uint32_t>(data);
        } else if (pkt->getSize() == 8) {
            pkt->setLE<uint64_t>(data);
        }
    } else if (pkt->isWrite()) {
        // For writes, extract data from the packet
        uint32_t data ;
        if (pkt->getSize() == 1) {
            data = pkt->getLE<uint8_t>();
        } else if (pkt->getSize() == 2) {
            data = pkt->getLE<uint16_t>();
        } else if (pkt->getSize() == 4) {
            data = pkt->getLE<uint32_t>();
        } else if (pkt->getSize() == 8) {
            data = pkt->getLE<uint64_t>();
        }

        inform("DEBUG: Packet write data=0x%x", data);

        // Write the data to the accelerator
        owner->writeMMIO(addr, data);
    }

    // Mark the packet as successful
    pkt->makeResponse();

    // Return a fixed latency
    return 1;
}

void
PhotonicAccel::ControlPort::recvFunctional(PacketPtr pkt)
{
    //  Handle  an atomic request
    recvAtomic(pkt);
}

bool
PhotonicAccel::ControlPort::recvTimingReq(PacketPtr pkt)
{
    // Handle timing requests immediately
    recvAtomic(pkt);

    // Schedule a response in the next cycle
    // In a more realistic implementation, you'd queue this and
    // respond after a delay
    return true;
}

void
PhotonicAccel::ControlPort::recvRespRetry()
{
    // Not needed for this simple implementation
}

AddrRangeList
PhotonicAccel::ControlPort::getAddrRanges() const
{
    AddrRangeList ranges;

    // Add the register address range
    ranges.push_back(AddrRange(owner->ACCEL_COMMAND_ADDR,
        owner->ACCEL_STATUS_ADDR + 4));

    // Add the matrix data ranges
    ranges.push_back(AddrRange(owner->MATRIX_A_BASE,
        owner->MATRIX_A_BASE + owner->MATRIX_SIZE * owner->MATRIX_SIZE));
    ranges.push_back(AddrRange(owner->MATRIX_B_BASE,
        owner->MATRIX_B_BASE + owner->MATRIX_SIZE * owner->MATRIX_SIZE));
    ranges.push_back(AddrRange(owner->MATRIX_RES_BASE,
        owner->MATRIX_RES_BASE + owner->MATRIX_SIZE * owner->MATRIX_SIZE));


}

/////////////////////////////////////////////////////////////////////////////
// MemoryPort Implementation (MasterPort)
/////////////////////////////////////////////////////////////////////////////

PhotonicAccel::MemoryPort::MemoryPort(const std::string &name,
    PhotonicAccel *_owner)
    : MasterPort(name, _owner), owner(_owner)
{
}

bool
PhotonicAccel::MemoryPort::recvTimingResp(PacketPtr pkt)
{
    inform("DEBUG: MemoryPort received timing response");
    // Handle timing response if needed
    delete pkt;
    return true;
}

void
PhotonicAccel::MemoryPort::recvReqRetry()
{
    inform("DEBUG: MemoryPort received request retry");
    // Handle request retry if needed
}

/////////////////////////////////////////////////////////////////////////////
// PhotonicAccel Implementation
/////////////////////////////////////////////////////////////////////////////

PhotonicAccel::PhotonicAccel(const PhotonicAccelParams &p)
    :SimObject(p),
          controlPort(name() + ".control_port", this),
      memoryPort(name() + ".memory_port", this),
      // Use parameter instead of constant
      matrixA(p.matrix_size * p.matrix_size),
      // Use parameter instead of constant
      matrixB(p.matrix_size * p.matrix_size),
      // Use parameter instead of constant
      matrixResult(p.matrix_size * p.matrix_size),
      // Use parameter instead of constant
      analogMatrixA(p.matrix_size * p.matrix_size),
      // Use parameter instead of constant
      analogMatrixB(p.matrix_size * p.matrix_size),
      // Use parameter instead of constant
      analogResult(p.matrix_size * p.matrix_size),
      // Set DAC/ADC parameters from Python params
      dacResolution(p.dac_resolution),
      adcResolution(p.adc_resolution),
      dacNoiseLevel(p.dac_noise_level),
      adcNoiseLevel(p.adc_noise_level),
      tiaGain(p.tia_gain),
      mziPhaseNoise(p.mzi_phase_noise),

          //Additional parmeters
          cmdTrigger(p.cmd_trigger),
      dataA(p.data_a),
      dataB(p.data_b),
      lsbNoise(p.lsb_noise),
      noiseScale(p.noise_scale),
      scalingFactor(p.scaling_factor),
      offset1(p.offset1),
      offset2(p.offset2),
      offsetError(p.offset_error),
      delayProp(p.delay_prop),
      amplificationFactor(p.amplification),
      delayResponse(p.delay_response)
{
    // Initialize registers
    commandReg = 0;
    configReg = 0;
    statusReg = STATUS_IDLE;


    DPRINTF(PhotonicAccel,
        "PhotonicAccel constructor called with photonic processing stages\n");
}

Port &
PhotonicAccel::getPort(const std::string &if_name, PortID idx)
{
    inform("DEBUG: getPort called for interface: %s", if_name.c_str());

    if (if_name == "control_port") {
        return controlPort;
    } else if (if_name == "memory_port") {
        return memoryPort;
    } else {
        return SimObject::getPort(if_name, idx);
    }
}

uint32_t
PhotonicAccel::readMMIO(Addr addr)
{
    DPRINTF(PhotonicAccel, "MMIO Read: addr=0x%x\n", addr);
    inform("DEBUG: MMIO Read addr=0x%x", addr);

    switch (addr) {
        case ACCEL_COMMAND_ADDR:
            inform("DEBUG: Reading command register: 0x%x", commandReg);
            return commandReg;
        case ACCEL_CONFIG_ADDR:
            inform("DEBUG: Reading config register: 0x%x", configReg);
            return configReg;
        case ACCEL_STATUS_ADDR:
            inform("DEBUG: Reading status register: 0x%x", statusReg);
            return statusReg;
        default:
            // Handle matrix read
            if (addr >= MATRIX_A_BASE && addr < MATRIX_B_BASE) {
                size_t offset = addr - MATRIX_A_BASE;
                if (offset < matrixA.size()) {
                    uint32_t value = matrixA[offset];
                    if (offset < 10) {
                        inform("DEBUG: Reading Matrix A[%lu] = %d "
                            "from addr 0x%x", offset, value, addr);
                    }
                    return value;
                }
            } else if (addr >= MATRIX_B_BASE && addr < MATRIX_RES_BASE) {
                size_t offset = addr - MATRIX_B_BASE;
                if (offset < matrixB.size()) {
                    uint32_t value = matrixB[offset];
                    if (offset < 10) {
                        inform("DEBUG: Reading Matrix B[%lu] = %d "
                            "from addr 0x%x", offset, value, addr);
                    }
                    return value;
                }
            } else if (addr >= MATRIX_RES_BASE) {
                size_t offset = addr - MATRIX_RES_BASE;
                if (offset < matrixResult.size()) {
                    uint32_t value = matrixResult[offset];
                    if (offset < 10) {
                        inform("DEBUG: Reading result[%lu] = %d "
                            "from addr 0x%x", offset, value, addr);
                    }
                    return value;
                }
            }
            break;
    }

    inform("DEBUG: Invalid MMIO read address: 0x%x", addr);
    DPRINTF(PhotonicAccel, "Invalid MMIO read address: 0x%x\n", addr);
    return 0;
}

void
PhotonicAccel::writeMMIO(Addr addr, uint32_t data)
{
    DPRINTF(PhotonicAccel, "MMIO Write: addr=0x%x, data=0x%x\n", addr, data);
    inform("DEBUG: MMIO Write addr=0x%x, data=0x%x", addr, data);

    switch (addr) {
        case ACCEL_COMMAND_ADDR:
            commandReg = data;
            inform("DEBUG: Command register write: 0x%x", data);

            // Add specific command logging
            switch (data) {
                case CMD_INIT:
                    inform("DEBUG: *** CMD_INIT RECEIVED ***");
                    break;
                case CMD_MATRIX_MUL:
                    inform("DEBUG: *** CMD_MATRIX_MUL RECEIVED ***");
                    break;
                case CMD_RESET:
                    inform("DEBUG: *** CMD_RESET RECEIVED ***");
                    break;
                case CMD_TEST_MODE:
                    inform("DEBUG: *** CMD_TEST_MODE RECEIVED ***");
                    break;
                case 0x42:
                    inform("DEBUG: *** CMD_DEBUG (0x42) RECEIVED ***");
                    break;
                default:
                    inform("DEBUG: Unknown command: 0x%x", data);
            }

            // Process command through the new handler
            handleCommand(data);
            break;

        case ACCEL_CONFIG_ADDR:
            configReg = data;
            inform("DEBUG: Config register write: 0x%x", data);
            break;

        case ACCEL_STATUS_ADDR:
            statusReg = data;
            inform("DEBUG: Status register write: 0x%x", data);
            break;

        default:
            // Handle matrix write
            if (addr >= MATRIX_A_BASE && addr < MATRIX_B_BASE) {
                size_t offset = addr - MATRIX_A_BASE;
                if (offset < matrixA.size()) {
                    matrixA[offset] = data & 0xFF;
                    if (offset < 10) {  // Log first 10 writes
                        inform("DEBUG: Writing Matrix A[%lu] = %d "
                            "to addr 0x%x", offset, data & 0xFF, addr);
                    }
                }
            } else if (addr >= MATRIX_B_BASE && addr < MATRIX_RES_BASE) {
                size_t offset = addr - MATRIX_B_BASE;
                if (offset < matrixB.size()) {
                    matrixB[offset] = data & 0xFF;
                    if (offset < 10) {  // Log first 10 writes
                        inform("DEBUG: Writing Matrix B[%lu] = %d "
                            "to addr 0x%x", offset, data & 0xFF, addr);
                    }
                }
            } else {
                inform("DEBUG: Write to unknown address: 0x%x", addr);
            }
            break;
    }
}

void
PhotonicAccel::handleCommand(uint32_t cmd)
{
    inform("DEBUG: handleCommand called with cmd=0x%x", cmd);

    switch (cmd) {
        case CMD_INIT:
            inform("DEBUG: Processing CMD_INIT");
            initializeAccelerator();
            break;
        case CMD_MATRIX_MUL:
            inform("DEBUG: Processing CMD_MATRIX_MUL - "
                "Starting matrix multiplication process");

            // Log matrix data before processing
            inform("DEBUG: Matrix A first 10 values: "
                   "%d %d %d %d %d %d %d %d %d %d",
                   matrixA[0], matrixA[1], matrixA[2], matrixA[3], matrixA[4],
                   matrixA[5], matrixA[6], matrixA[7], matrixA[8], matrixA[9]);
            inform("DEBUG: Matrix B first 10 values: "
                   "%d %d %d %d %d %d %d %d %d %d",
                   matrixB[0], matrixB[1], matrixB[2], matrixB[3], matrixB[4],
                   matrixB[5], matrixB[6], matrixB[7], matrixB[8], matrixB[9]);

            // Try direct write to result matrix to test memory access
            inform("DEBUG: Writing test pattern "
                "to result matrix before computation");
            if (!matrixResult.empty()) {
                matrixResult[0] = 0x12;  // Write test pattern
                matrixResult[1] = 0x34;
                matrixResult[2] = 0x56;
                inform("DEBUG: Test values written: %d %d %d",
                    matrixResult[0], matrixResult[1], matrixResult[2]);
            }

            // Perform the actual matrix multiplication
            performMatrixMultiplication();
            break;
        case CMD_RESET:
            inform("DEBUG: Processing CMD_RESET");
            resetAccelerator();
            break;
        case CMD_TEST_MODE:
            inform("DEBUG: Processing CMD_TEST_MODE");
            setTestMode();
            break;
        case 0x42:  // Debug command
            inform("DEBUG: Processing CMD_DEBUG (0x42)");
            // Write debug pattern to result matrix
            if (!matrixResult.empty()) {
                for (
                    int i = 0;i < std::min(10, (int)matrixResult.size());i++
                ) {
                    matrixResult[i] = 0xAA + i;
                    inform("DEBUG: Writing debug test value 0x%x "
                        "to result[%d]", 0xAA + i, i);
                }
                inform("DEBUG: Debug pattern written: %d %d %d %d %d",
                       matrixResult[0], matrixResult[1], matrixResult[2],
                       matrixResult[3], matrixResult[4]);
            }
            statusReg = STATUS_COMPLETE;
            break;
        default:
            inform("DEBUG: Unknown command: 0x%x", cmd);
            break;
    }

    inform("DEBUG: handleCommand completed for cmd=0x%x", cmd);
}

void
PhotonicAccel::digitalToAnalogConversion()
{
    inform("DEBUG: Starting Digital to Analog Conversion (DAC)");
    DPRINTF(PhotonicAccel, "Performing Digital to Analog Conversion (DAC)\n");

    // Calculate the max value based on DAC resolution
    float maxDigitalValue = (1 << dacResolution) - 1;
    inform("DEBUG: DAC max digital value: %f", maxDigitalValue);

    // Convert Matrix A from digital to analog
    for (size_t i = 0; i < matrixA.size(); i++) {
        // Normalize to 0.0-1.0 range and add noise
        float analogValue = matrixA[i] / maxDigitalValue;
        analogValue = addPhotonic_Noise(analogValue, dacNoiseLevel);
        analogMatrixA[i] = analogValue;
    }

    // Convert Matrix B from digital to analog
    for (size_t i = 0; i < matrixB.size(); i++) {
        // Normalize to 0.0-1.0 range and add noise
        float analogValue = matrixB[i] / maxDigitalValue;
        analogValue = addPhotonic_Noise(analogValue, dacNoiseLevel);
        analogMatrixB[i] = analogValue;
    }

    inform("DEBUG: DAC completed - Matrix A analog samples: %f %f %f",
           analogMatrixA[0], analogMatrixA[1], analogMatrixA[2]);
    inform("DEBUG: DAC completed - Matrix B analog samples: %f %f %f",
           analogMatrixB[0], analogMatrixB[1], analogMatrixB[2]);

    DPRINTF(PhotonicAccel, "DAC completed - "
            "Matrix A sample: %f, %f, Matrix B sample: %f, %f\n",
            analogMatrixA[0], analogMatrixA[1], analogMatrixB[0],
            analogMatrixB[1]);
}

float
PhotonicAccel::addPhotonic_Noise(float value, float noiseLevel)
{
    // Create a random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // Gaussian distribution with mean 0 and standard deviation of noiseLevel
    std::normal_distribution<float> d(0, noiseLevel);

    // Add noise to the value
    float noisy_value = value + d(gen);

    // Clamp to 0.0-1.0 range
    return std::max(0.0f, std::min(1.0f, noisy_value));
}

void
PhotonicAccel::performPhotonic_MMM()
{
    inform("DEBUG: Starting photonic matrix multiplication via MZI array");
    DPRINTF(PhotonicAccel,
        "Performing photonic matrix multiplication via MZI array\n");

    // Initialize result to zeros
    std::fill(analogResult.begin(), analogResult.end(), 0.0f);
    inform("DEBUG: Analog result matrix initialized to zeros");

    // Perform matrix multiplication in the "photonic domain"
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        for (int j = 0; j < MATRIX_SIZE; ++j) {
            float sum = 0.0f;

            for (int k = 0; k < MATRIX_SIZE; ++k) {
                // Simulate photonic multiplication (MZI operation)
                float product = analogMatrixA[i * MATRIX_SIZE + k] *
                  analogMatrixB[k * MATRIX_SIZE + j];

                // Add some non-linearity and
                // phase noise to simulate photonic effects
                product = addPhotonic_Noise(product, mziPhaseNoise);

                sum += product;
            }

            // Store the analog result
            analogResult[i * MATRIX_SIZE + j] = sum;

            // Log first few calculations
            if (i < 3 && j < 3) {
                inform("DEBUG: Photonic result[%d][%d] = %f", i, j, sum);
            }
        }
    }

    inform("DEBUG: Photonic MMM completed - First few results: %f %f %f",
           analogResult[0], analogResult[1], analogResult[2]);
    DPRINTF(PhotonicAccel, "Photonic MMM completed - Result sample: %f, %f\n",
            analogResult[0], analogResult[1]);
}

void
PhotonicAccel::photodetectAndAmplify()
{
    inform("DEBUG: Starting photodetection and TIA amplification");
    DPRINTF(PhotonicAccel,
        "Simulating photodetection and TIA amplification\n");

    // Apply photodetector response and TIA amplification to the result
    for (size_t i = 0; i < analogResult.size(); i++) {
        // Photodetection - typically square-law detection
        // for optical intensity
        float detected = analogResult[i] * analogResult[i];

        // Add photodetector noise
        detected = addPhotonic_Noise(detected, 0.02);  // 2% detector noise

        // Apply TIA gain and add electronic noise
        float amplified = detected * tiaGain;
        amplified = addPhotonic_Noise(amplified, 0.01);  // 1% amplifier noise

        // Store back the result
        analogResult[i] = amplified;
    }

    inform("DEBUG: Photodetection and amplification completed - "
           "First few values: %f %f %f",
           analogResult[0], analogResult[1], analogResult[2]);
    DPRINTF(PhotonicAccel, "Photodetection and amplification completed\n");
}

void
PhotonicAccel::analogToDigitalConversion()
{
    inform("DEBUG: Starting Analog to Digital Conversion (ADC)");
    DPRINTF(PhotonicAccel, "Performing Analog to Digital Conversion (ADC)\n");

    // Calculate the max value based on ADC resolution
    float maxDigitalValue = (1 << adcResolution) - 1;

    // Find the maximum analog value for normalization
    float maxAnalogValue = 0.0f;
    for (const auto& val : analogResult) {
        maxAnalogValue = std::max(maxAnalogValue, val);
    }

    // Avoid division by zero
    if (maxAnalogValue < 1e-6) {
        maxAnalogValue = 1.0f;
    }

    inform("DEBUG: ADC max analog value found: %f", maxAnalogValue);
    inform("DEBUG: ADC max digital value: %f", maxDigitalValue);

    // Convert analog result to digital
    for (size_t i = 0; i < analogResult.size(); i++) {
        // Normalize and scale to ADC range
        float normalized = analogResult[i] / maxAnalogValue;

        // Add ADC quantization noise
        normalized = addPhotonic_Noise(normalized, adcNoiseLevel);

        // Convert to digital value
        uint32_t digitalValue =
          static_cast<uint32_t>(normalized * maxDigitalValue + 0.5f);

        // Clamp to valid range and store result
        matrixResult[i] = std::min(digitalValue, static_cast<uint32_t>(255));

        // Log first few conversions
        if (i < 10) {
            inform("DEBUG: ADC[%lu]: analog=%f -> normalized=%f -> digital=%d",
                   i, analogResult[i], normalized, matrixResult[i]);
        }
    }

    inform("DEBUG: ADC completed - First 10 digital results: "
           "%d %d %d %d %d %d %d %d %d %d",
           matrixResult[0], matrixResult[1], matrixResult[2],
           matrixResult[3], matrixResult[4],
           matrixResult[5], matrixResult[6], matrixResult[7],
           matrixResult[8], matrixResult[9]);

    DPRINTF(PhotonicAccel, "ADC completed - Result sample: %d, %d\n",
            matrixResult[0], matrixResult[1]);
}

// TEMPORARY: Override with dummy values for testing
void
PhotonicAccel::performMatrixMultiplication()
{
    inform("DEBUG: *** ENTERING performMatrixMultiplication ***");
    statusReg = STATUS_BUSY;

    // UNCOMMENT THIS BLOCK FOR DUMMY VALUE TESTING
    /*
    inform("DEBUG: *** USING DUMMY VALUES FOR TESTING ***");

    // Skip all computation, just write dummy pattern
    for (int i = 0; i < MATRIX_SIZE * MATRIX_SIZE; i++) {
        matrixResult[i] = (i % 256);  // Simple pattern: 0,1,2,...,255,0,1,2...
    }

    inform("DEBUG: Dummy values written. First 10: "
           "%d %d %d %d %d %d %d %d %d %d",
           matrixResult[0], matrixResult[1], matrixResult[2],
           matrixResult[3], matrixResult[4],
           matrixResult[5], matrixResult[6], matrixResult[7],
           matrixResult[8], matrixResult[9]);

    statusReg = STATUS_COMPLETE;
    inform("DEBUG: *** Dummy matrix multiplication COMPLETE ***");
    return;
    */

    // NORMAL COMPUTATION PATH
    inform("DEBUG: Starting normal photonic processing pipeline");

    // Photonic Processing Pipeline
    inform("DEBUG: Step 1 - Starting DAC conversion");
    digitalToAnalogConversion();

    inform("DEBUG: Step 2 - Starting photonic MMM");
    performPhotonic_MMM();

    inform("DEBUG: Step 3 - Starting photodetection");
    photodetectAndAmplify();

    inform("DEBUG: Step 4 - Starting ADC conversion");
    analogToDigitalConversion();

    // Print sample outputs for debugging
    inform("DEBUG: Matrix A (digital) sample: %d, %d, %d, %d, %d",
            matrixA[0], matrixA[1], matrixA[2], matrixA[3], matrixA[4]);
    inform("DEBUG: Matrix B (digital) sample: %d, %d, %d, %d, %d",
            matrixB[0], matrixB[1], matrixB[2], matrixB[3], matrixB[4]);
    inform("DEBUG: Final result first 10 values: "
           "%d %d %d %d %d %d %d %d %d %d",
           matrixResult[0], matrixResult[1], matrixResult[2],
           matrixResult[3], matrixResult[4],
           matrixResult[5], matrixResult[6], matrixResult[7],
           matrixResult[8], matrixResult[9]);

    statusReg = STATUS_COMPLETE;
    inform("DEBUG: *** Matrix multiplication COMPLETE ***");

    DPRINTF(PhotonicAccel,
        "Photonic Matrix Multiplication Pipeline Complete\n");
}

void
PhotonicAccel::resetAccelerator()
{
    inform("DEBUG: Resetting Accelerator");
    DPRINTF(PhotonicAccel, "Resetting Accelerator\n");

    // Clear matrices
    std::fill(matrixA.begin(), matrixA.end(), 0);
    std::fill(matrixB.begin(), matrixB.end(), 0);
    std::fill(matrixResult.begin(), matrixResult.end(), 0);

    // Clear analog matrices
    std::fill(analogMatrixA.begin(), analogMatrixA.end(), 0.0f);
    std::fill(analogMatrixB.begin(), analogMatrixB.end(), 0.0f);
    std::fill(analogResult.begin(), analogResult.end(), 0.0f);

    // Reset registers
    commandReg = 0;
    configReg = 0;
    statusReg = STATUS_IDLE;

    inform("DEBUG: Accelerator reset complete");
}

void
PhotonicAccel::initializeAccelerator()
{
    inform("DEBUG: Initializing Accelerator");
    DPRINTF(PhotonicAccel, "Initializing Accelerator\n");
    statusReg = STATUS_IDLE;
    configReg = 0x0001;  // Basic configuration value
    inform("DEBUG: Accelerator initialization complete");
}

void
PhotonicAccel::setTestMode()
{
    inform("DEBUG: Entering Test Mode");
    DPRINTF(PhotonicAccel, "Entering Test Mode\n");
}

void
PhotonicAccel::startup()
{
    inform("DEBUG: Photonic Accelerator Startup");
    DPRINTF(PhotonicAccel, "Photonic Accelerator Startup\n");
}

void
PhotonicAccel::regStats()
{
    // Call the parent implementation
    SimObject::regStats();

    inform("DEBUG: Registering PhotonicAccel statistics");

    // Initialize the pktCount statistic
      pktCount
        .name(name() + ".pktCount")
        .desc("Total number of packets processed");

    // Additional statistics for photonic accelerator
    matrixMulCount
        .name(name() + ".matrixMulCount")
        .desc("Number of matrix multiplications performed");

    dacConversionCount
        .name(name() + ".dacConversionCount")
        .desc("Number of DAC conversions performed");

    adcConversionCount
        .name(name() + ".adcConversionCount")
        .desc("Number of ADC conversions performed");

    inform("DEBUG: PhotonicAccel statistics registered");
}



} // namespace gem5
