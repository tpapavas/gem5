
/*
 *  Header file for photonic accelerators
 */

#ifndef __LEARNING_GEM5_PART2_PHOTONIC_ACCEL_HH__
#define __LEARNING_GEM5_PART2_PHOTONIC_ACCEL_HH__

#include <cstdint>
#include <vector>

#include "base/statistics.hh"
#include "dev/io_device.hh"
#include "mem/port.hh"
#include "params/PhotonicAccel.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class PhotonicAccel : public SimObject
{
  private:
    // Memory-mapped register addresses (matching software driver)
    static const uint32_t ACCEL_COMMAND_ADDR   = 0xF0000000;
    static const uint32_t ACCEL_CONFIG_ADDR    = 0xF0000004;
    static const uint32_t ACCEL_STATUS_ADDR    = 0xF0000008;

    static const uint32_t MATRIX_A_BASE        = 0xE0000000;
    static const uint32_t MATRIX_B_BASE        = 0xE0001000;
    static const uint32_t MATRIX_RES_BASE      = 0xE0002000;

    // Command definitions
    static const uint8_t CMD_INIT       = 0x01;
    static const uint8_t CMD_MATRIX_MUL = 0x02;
    static const uint8_t CMD_RESET      = 0x03;
    static const uint8_t CMD_TEST_MODE  = 0x04;

    // Status register bit flags
    static const uint8_t STATUS_IDLE     = 0x0;
    static const uint8_t STATUS_COMPLETE = 0x1;
    static const uint8_t STATUS_ERROR    = 0x2;
    static const uint8_t STATUS_BUSY     = 0x4;

    // Matrix size configuration
        int MATRIX_SIZE;
    // Registers and state


    uint32_t commandReg;
    uint32_t configReg;
    uint32_t statusReg;

    // Digital Matrix storage (These are accessibleto the CPU )
    std::vector<uint8_t> matrixA;
    std::vector<uint8_t> matrixB;
    std::vector<uint8_t> matrixResult;

    // Analog matrix storage (internal to accelerator)
    std::vector<float> analogMatrixA;
    std::vector<float> analogMatrixB;
    std::vector<float> analogResult;

        bool cmdTrigger;
    int16_t dataA;
    int16_t dataB;
    double lsbNoise;
    double noiseScale;
    int16_t scalingFactor;
    int16_t offset1;
    int16_t offset2;
    int16_t offsetError;
    int delayProp;
    double amplificationFactor;
    int delayResponse;

        statistics::Scalar pktCount;
    statistics::Scalar matrixMulCount;
    statistics::Scalar dacConversionCount;
    statistics::Scalar adcConversionCount;

        void handleCommand(uint32_t cmd);


    // Control port (slave) for receiving commands from CPU
    class ControlPort : public SlavePort
    {
      private:
        PhotonicAccel *owner;

      public:
        ControlPort(const std::string &name, PhotonicAccel *owner);

      protected:
        Tick recvAtomic(PacketPtr pkt) override;
        void recvFunctional(PacketPtr pkt) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;
    };


    // Memory access port (master) for accessing matrices in memory
    class MemoryPort : public MasterPort
    {
      private:
        PhotonicAccel *owner;

      public:
        MemoryPort(const std::string &name, PhotonicAccel *owner);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
    };

    // Port instances
    ControlPort controlPort;
    MemoryPort memoryPort;

    // Photonic processing stages
    // DAC - Convert digital matrices to analog
    void digitalToAnalogConversion();
    // Photonic matrix multiplication using MZI
    void performPhotonic_MMM();
    // Photodetection and TIA amplification
    void photodetectAndAmplify();
    // ADC - Convert analog result to digital
    void analogToDigitalConversion();

    // Internal methods
    void performMatrixMultiplication();
    void resetAccelerator();
    void initializeAccelerator();
    void setTestMode();

    // Simulation of photonic noise and imperfections

    float addPhotonic_Noise(float value, float noiseLevel);

    // DAC and ADC parameters
    int dacResolution;    // Resolution in bits
    int adcResolution;    // Resolution in bits
    float dacNoiseLevel;  // Noise level for DAC conversion
    float adcNoiseLevel;  // Noise level for ADC conversion
    float tiaGain;        // Trans-impedance amplifier gain
    float mziPhaseNoise;  // Phase noise in MZI array


  public:
    /**
     * Constructor for the PhotonicAccel
     */
    PhotonicAccel(const PhotonicAccelParams &p);

    /**
     * Called when simulation starts
     */
    void startup() override;

            /**
     * Register statistics for the PhotonicAccel
     */
    void regStats() override;

    /**
     * Handle MMIO read from a specific address
     */
    uint32_t readMMIO(Addr addr);

    /**
     * Handle MMIO write to a specific address
     */
    void writeMMIO(Addr addr, uint32_t data);

    /**
     * Get a port with the given name
     */
   Port &getPort(const std::string &if_name,
    PortID idx = InvalidPortID) override;
};

} // namespace gem5

#endif // __LEARNING_GEM5_PART2_PHOTONIC_ACCEL_HH_
