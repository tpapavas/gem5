//Subblkmap of Faulty Cache (F-Cache) - pfails=0.000010
#include <cstdio>

#include "create_stuck_bit_map.hh"

int sblkmap[128][4] = {0};

u_int8_t stuckBitMaskOnesMap[128][4][64] = {0};
u_int8_t stuckBitMaskZerosMap[128][4][64] = {0};

void resetFaultyCacheMaps(
  unsigned int sets, unsigned int sblks_per_set, unsigned int sblk_bytes)
{
  for (int i = 0; i < sets; i++) {
    for (int j = 0; j < sblks_per_set; j++) {
      for (int k = 0; k < sblk_bytes; k++) {
        stuckBitMaskZerosMap[i][j][k] = 255;
      }
    }
  }
}

void updateFaultyCacheMaps(
  unsigned int cachesize, unsigned int assoc, unsigned int sblks_per_blk)
{
  // unsigned int sets = (cachesize*1024) / (sblk_bytes*sblks_per_blk) / assoc;
  // resetFaultyCacheMaps(sets, assoc*sblks_per_blk, sblk_bytes);
  //cachesize-assoc-subblocks
  if (cachesize == 32){
    if (assoc == 4){  //cachesize:32
      if (sblks_per_blk == 1){  //cachesize:32 - assoc:4
        sblkmap[35][1] = 1;
        sblkmap[108][2] = 1;
        sblkmap[115][3] = 1;

        stuckBitMaskOnesMap[35][1][4] = 64;
        stuckBitMaskZerosMap[108][2][20] = 191;
        stuckBitMaskZerosMap[115][3][19] = 251;
      }
        // cachesize=32(KByte), assoc=4, sets=128, blocks=512,
        //   blk_size=512(bits), data_set_bits=2048, tag_set_bits=76
        // data_fbits=3, tag_fbits=0, merged_fbits=3, merged_fsblks=3,
        //   merged_fblks=3, total_off=192(Bytes), nonmatched_faulty_tag_data=0
        // bad_sets=128, total_ffsets=0, new_ffsets=0, common_ffsets=0
        // fsets_1_ffblks=3, fsets_2_ffblks=0,
        //   fsets_3_ffblks=0, fsets_4_ffblks=0
        // fsets_1_fblks=3, fsets_2_fblks=0, fsets_3_fblks=0, fsets_4_fblks=0
        // way-1_fblks=0, way-2_fblks=1, way-3_fblks=1, way-4_fblks=1
        // fblks_1_fsblks=3
      else {  //incorrect number of subblocks
        printf("ERROR - Cache Fault Map: Incorrect number of subblocks\n");
        exit(1);
      }
    }
    else { //incorrect associativity
      printf("ERROR - Cache Fault Map: Incorrect associativity\n");
      exit(1);
    }
  }
  else {  //incorrect cache size
    printf("ERROR - Cache Fault Map: Incorrect cache size\n");
    exit(1);
  }
}
