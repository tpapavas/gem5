#include <cstdlib>

#ifndef CREATE_FAULTY_CACHES_MAPS_H
#define CREATE_FAULTY_CACHES_MAPS_H

void resetFaultyCacheMaps(
    unsigned int sets, unsigned int sblks_per_set, unsigned int sblk_bytes);
void updateFaultyCacheMaps(
    unsigned int cachesize, unsigned int assoc, unsigned int sblks_per_blk);

extern int sblkmap[128][4];

extern u_int8_t stuckBitMaskOnesMap[128][4][64];
extern u_int8_t stuckBitMaskZerosMap[128][4][64];

#endif /* CREATE_FAULTY_CACHES_MAPS_H */
