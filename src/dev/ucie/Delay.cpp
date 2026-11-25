#include "Delay.h"
#include <torch/script.h>
#include <torch/torch.h>
#include <stdexcept>
#include <algorithm>

Delay::Delay(const std::string& filename) {
    YAML::Node config = YAML::LoadFile(filename);

    // Technology parameters
    if (YAML::Node parameter = config["Technology"]["voltage"]) {
        voltage = config["Technology"]["voltage"].as<float>();    
    }
    else {
        voltage = 0.9;
    }
    if (YAML::Node parameter = config["Technology"]["node"]) {
        tech_node = config["Technology"]["node"].as<float>();    
    }
    else {
        tech_node = 45.0;
    }

    if (!interpolate(tech_node))
        throw std::runtime_error("UCIe Delay: technology node requires extrapolation");

    // Models (relative paths are resolved against the YAML's directory)
    latency_model_filename = config["Models"]["latency_model_filename"].as<std::string>();
    if (!latency_model_filename.empty() && latency_model_filename[0] != '/') {
        std::string::size_type slash = filename.find_last_of('/');
        std::string dir = (slash == std::string::npos) ? std::string(".")
                                                       : filename.substr(0, slash);
        latency_model_filename = dir + "/" + latency_model_filename;
    }

    // General parameters
    if (YAML::Node parameter = config["General"]["fdiWidth"]) {
        fdiWidth = config["General"]["fdiWidth"].as<int>();    
    }
    else {
        fdiWidth = 64;
    }
    if (YAML::Node parameter = config["General"]["digital_period"]) {
        digital_period = config["General"]["digital_period"].as<float>();    
    }
    else {
        digital_period = scaling_ratio * 5000;
    }
    if (YAML::Node parameter = config["General"]["unit_interval"]) {
        unit_interval = config["General"]["unit_interval"].as<float>();    
    }
    else {
        unit_interval = 62.5;
    }
    if (YAML::Node parameter = config["General"]["divider_align"]) {
        divider_align = config["General"]["divider_align"].as<float>();    
    }
    else {
        divider_align = 125;
    }
    if (YAML::Node parameter = config["General"]["d_serializer"]) {
        d_serializer = config["General"]["d_serializer"].as<float>();    
    }
    else {
        d_serializer = 1860;
    }
    if (YAML::Node parameter = config["General"]["sampling_offset"]) {
        sampling_offset = config["General"]["sampling_offset"].as<float>();    
    }
    else {
        sampling_offset = 31.25;
    }
    if (YAML::Node parameter = config["General"]["CDC_skew"]) {
        CDC_skew = config["General"]["CDC_skew"].as<float>();    
    }
    else {
        CDC_skew = 6.25;
    }
    if (YAML::Node parameter = config["General"]["TX_driver_RC"]) {
        TX_driver_RC = config["General"]["TX_driver_RC"].as<float>();    
    }
    else {
        TX_driver_RC = 5.2;
    }
    if (YAML::Node parameter = config["General"]["RX_FE"]) {
        RX_FE = config["General"]["RX_FE"].as<float>();    
    }
    else {
        RX_FE = 5.7;
    }
    if (YAML::Node parameter = config["General"]["CDC_flops"]) {
        CDC_flops = config["General"]["CDC_flops"].as<int>();    
    }
    else {
        CDC_flops = 3;
    }
    if (YAML::Node parameter = config["General"]["bits_per_lane_per_word"]) {
        bits_per_lane_per_word = config["General"]["bits_per_lane_per_word"].as<int>();    
    }
    else {
        bits_per_lane_per_word = 32;
    }
    word_period = bits_per_lane_per_word * unit_interval;

    // Wire parameters
    if (YAML::Node parameter = config["Wire"]["wire_width"]) {
        wire_width = config["Wire"]["wire_width"].as<float>();    
    }
    else {
        wire_width = 2.5;
    }
    if (YAML::Node parameter = config["Wire"]["wire_space"]) {
        wire_space = config["Wire"]["wire_space"].as<float>();   
    }
    else {
        wire_space = 2.64;
    }
    if (YAML::Node parameter = config["Wire"]["wire_length"]) {
        wire_length = config["Wire"]["wire_length"].as<float>();   
    }
    else {
        wire_length = 2961;
    }
    if (YAML::Node parameter = config["Wire"]["wire_gnd_width"]) {
        wire_gnd_width = config["Wire"]["wire_gnd_width"].as<float>();   
    }
    else {
        wire_gnd_width = 3.7;
    }
    if (YAML::Node parameter = config["Wire"]["wire_unshield_length"]) {
        wire_unshield_length = config["Wire"]["wire_unshield_length"].as<float>();   
    }
    else {
        wire_unshield_length = 577;
    }
    if (YAML::Node parameter = config["Wire"]["wire_via_thickness"]) {
        wire_via_thickness = config["Wire"]["wire_via_thickness"].as<float>();   
    }
    else {
        wire_via_thickness = 1.02;
    }
    if (YAML::Node parameter = config["Wire"]["wire_metal_thickness"]) {
        wire_metal_thickness = config["Wire"]["wire_metal_thickness"].as<float>();   
    }
    else {
        wire_metal_thickness = 1.4;
    }
    if (YAML::Node parameter = config["Wire"]["wire_vin"]) {
        wire_vin = config["Wire"]["wire_vin"].as<float>();   
    }
    else {
        wire_vin = 0.95;
    }
    if (YAML::Node parameter = config["Wire"]["wire_vrise"]) {
        wire_vrise = config["Wire"]["wire_vrise"].as<float>();   
    }
    else {
        wire_vrise = 61.75;
    }
    if (YAML::Node parameter = config["Wire"]["wire_period"]) {
        wire_period = config["Wire"]["wire_period"].as<float>();   
    }
    else {
        wire_period = 527.25;
    }
    if (YAML::Node parameter = config["Wire"]["wire_txres"]) {
        wire_txres = config["Wire"]["wire_txres"].as<float>();   
    }
    else {
        wire_txres = 47;
    }

    d_channel = calculateWireDelay();
}

float Delay::calculateWireDelay(){

    c10::InferenceMode guard;

    static torch::jit::script::Module model;
    static bool loaded = false;
    if (!loaded) {
        try {
            model = torch::jit::load(this->latency_model_filename, torch::kCPU);
        } catch (const c10::Error& e) {
            std::cerr << "Error loading model '" << this->latency_model_filename
                      << "':\n" << e.what() << "\n";
            throw;
        }
        model.eval();
        loaded = true;
    }

    std::vector<float> in = {
        this->wire_width, this->wire_space, this->wire_length, this->wire_unshield_length, this->wire_via_thickness,
        this->wire_metal_thickness, this->wire_vin, this->wire_vrise, this->wire_period, this->wire_txres
    };

    torch::Tensor X = torch::from_blob(in.data(), {1, 10}, torch::kFloat32).clone();

    torch::Tensor output = model.forward({X}).toTensor().squeeze();
    TORCH_CHECK(output.numel() == 1, "unexpected model output size: ", output.numel());

    return output.item<float>();
}

float Delay::get_delay(int X) {
    // Number of extra word periods; ceil must operate on real numbers
    // (the original int/int division truncated, giving E=0 for X<fdiWidth).
    long E = (long)std::ceil((double)fdiWidth / 64.0) *
             (long)std::ceil((double)X / (double)fdiWidth);
    if (E < 1) E = 1;
    float delay;
   
    delay = digital_period + (CDC_flops+1) * word_period + word_period/2 + divider_align + d_serializer + (bits_per_lane_per_word-1) * unit_interval + TX_driver_RC + d_channel + RX_FE + (word_period - sampling_offset + CDC_skew) + (CDC_flops+2) * digital_period + (E-1) * word_period;
    return delay;
}

bool Delay::interpolate(float targetTech ) {
    // Find closest technologies

    int lessTech = -1;
    int moreTech = -1;
    for (int i = 0; i < 11; i++) {
        if(technologies[i] <= targetTech) {
            lessTech = i;
        }
        if(technologies[i] >= targetTech) {
            moreTech = i;
            break;
        }
    }
    if(lessTech < 0 || moreTech < 0) {
        std::cerr << "Error: adjusting to that tech would require extrapolation." << std::endl;
        return false;
    }
    
    float dx = technologies[moreTech] - technologies[lessTech];
    float dx_small = targetTech - technologies[lessTech];
    
    if (lessTech == moreTech) {
        for (int i = 0; i < 4; i++) {
            scaling_coeff_target[i] = scaling_coeff_LUT[lessTech][i];
        }
    }
    else {
        for (int i = 0; i < 4; i++) {
            float dy = scaling_coeff_LUT[moreTech][i] - scaling_coeff_LUT[lessTech][i];
            scaling_coeff_target[i] = scaling_coeff_LUT[lessTech][i] + dy/dx * dx_small;
        }
    }

    float v_pow = 1;
    scaling_factor_target = 0;
    for (int i = 0; i < 4; i++) {
        scaling_factor_target += scaling_coeff_target[3 - i] * v_pow;
        v_pow *= voltage;
    }

    scaling_ratio = scaling_factor_target / scaling_factor_source;
    return true;
}
