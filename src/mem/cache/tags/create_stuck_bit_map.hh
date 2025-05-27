#include <cstdint>
#include <cstdlib>

#ifndef CREATE_FAULTY_CACHES_MAPS_H
#define CREATE_FAULTY_CACHES_MAPS_H

void resetFaultyCacheMaps();
void updateFaultyCacheMaps(
    unsigned int cachesize, unsigned int assoc, unsigned int sblks_per_blk);

extern int sblkmap[64][8];

extern uint8_t stuckBitMaskOnesMap[64][8][64];
extern uint8_t stuckBitMaskZerosMap[64][8][64];

#endif /* CREATE_FAULTY_CACHES_MAPS_H */
