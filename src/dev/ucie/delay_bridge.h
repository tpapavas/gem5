#ifndef GEM5_UCIE_DELAY_BRIDGE_H
#define GEM5_UCIE_DELAY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UcieDelayHandle UcieDelayHandle;

UcieDelayHandle* ucie_delay_create(const char* config_path);
/* X = transfer size in BYTES (Leandros model, fdiWidth = 64 B). */
double ucie_delay_get(UcieDelayHandle* handle, int bytes);
void ucie_delay_destroy(UcieDelayHandle* handle);
/* Model constants (ps / bits) exposed for serialization modelling. */
int ucie_delay_fdi_width(UcieDelayHandle* handle);
double ucie_delay_word_period(UcieDelayHandle* handle);
double ucie_delay_channel(UcieDelayHandle* handle);
double ucie_delay_digital_period(UcieDelayHandle* handle);

#ifdef __cplusplus
}
#endif

#endif
