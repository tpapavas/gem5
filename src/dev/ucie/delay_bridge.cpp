#include "delay_bridge.h"
#include "Delay.h"

#include <exception>
#include <cstdio>

struct UcieDelayHandle {
    Delay model;

    explicit UcieDelayHandle(const char* path)
        : model(path)
    {}
};

extern "C" UcieDelayHandle*
ucie_delay_create(const char* config_path)
{
    try {
        return new UcieDelayHandle(config_path);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "UCIe Delay initialization failed: %s\n",
                     e.what());
        return nullptr;
    }
}

extern "C" double
ucie_delay_get(UcieDelayHandle* handle, int bytes)
{
    if (!handle)
        return -1.0;

    return handle->model.get_delay(bytes);
}

extern "C" void
ucie_delay_destroy(UcieDelayHandle* handle)
{
    delete handle;
}

extern "C" int
ucie_delay_fdi_width(UcieDelayHandle* handle)
{
    return handle ? handle->model.fdiWidth : -1;
}

extern "C" double
ucie_delay_word_period(UcieDelayHandle* handle)
{
    return handle ? handle->model.word_period : -1.0;
}

extern "C" double
ucie_delay_channel(UcieDelayHandle* handle)
{
    return handle ? handle->model.d_channel : -1.0;
}

extern "C" double
ucie_delay_digital_period(UcieDelayHandle* handle)
{
    return handle ? handle->model.digital_period : -1.0;
}
