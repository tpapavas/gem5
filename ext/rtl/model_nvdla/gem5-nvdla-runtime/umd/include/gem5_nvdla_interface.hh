#ifndef __GEM5_NVDLA_INTERFACE_H_
#define __GEM5_NVDLA_INTERFACE_H_

#include <cstdint>

#include <nvdla_linux.h>

int32_t gem5_nvdla_submit(void *arg, struct nvdla_device *dev);

int32_t dla_execute_task(void *engine_context, void *task_data, void *config_data);


// methods from firmware/scheduler.c
int dla_process_events(void *engine_context, uint32_t *task_complete);
void dla_clear_task(void *engine_context);

// methods from firmware/cache.c
// void dla_get_refcount(struct dla_common_op_desc *op_desc);

#endif // __GEM5_NVDLA_INTERFACE_H_