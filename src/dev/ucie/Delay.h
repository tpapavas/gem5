#include <string>
#include <yaml-cpp/yaml.h>
#include <math.h>
#include <cstdlib>

#ifndef CONFIG
#define CONFIG

class Delay {

public:

    Delay(const std::string& filename);
    float get_delay(int X);
    float calculateWireDelay();
    bool interpolate(float targetTech);

    //Models
    std::string latency_model_filename;

    //General parameters
    int fdiWidth;
    float digital_period;
    float word_period;
    float unit_interval;
    float divider_align;
    float d_serializer;
    float sampling_offset;
    float CDC_skew;
    float TX_driver_RC;
    float RX_FE;
    float d_channel;
    int CDC_flops;
    int bits_per_lane_per_word;


    //Wire parameters
    float wire_width;
    float wire_space;
    float wire_length;
    float wire_gnd_width;
    float wire_unshield_length;
    float wire_via_thickness;
    float wire_metal_thickness;
    float wire_vin;
    float wire_vrise;
    float wire_period;
    float wire_txres;

    //Technology parameters
    float tech_node;
    float voltage;
    float technologies[11] = {7, 10, 14, 16, 20, 32, 45, 65, 90, 130, 180};
    static constexpr float scaling_coeff_LUT[11][4] = {
        {-28.58,  76.6, -70.26,  24.96},
        {-34.95, 93.65, -85.99,   30.4},
        {-40.66, 109.2, -100.6,  35.92},
        {     0,  24.8, -47.52,  28.87},
        {     0, 34.63, -66.37,  41.15},
        { -1047,  2982,  -2797,  873.5},
        {-501.6,  1567,  -1619,  566.1},
        { -53.3, 230.4, -333.9,  178.6},
        {-60.34, 262.5, -384.2,  210.9},
        {-76.65, 334.9, -493.4,  275.8},
        {     0, 97.09, -356.7,  406.5},
    };
    float scaling_coeff_target[4] = {0, 0, 0, 0};
    float scaling_factor_source = 12.6036;
    float scaling_factor_target = 12.6036;
    float scaling_ratio = 1;
};

#endif