#ifndef CREATESUBBLKMAP_H
#define CREATESUBBLKMAP_H

#include <cstdlib>

void updateSubBlkMap(unsigned int cachesize, unsigned int assoc,
    unsigned int sblks);

extern int sblkmap[128][4];

#endif /* CREATESUBBLKMAP_H */
