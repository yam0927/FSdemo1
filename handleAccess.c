#include <stdio.h>
#include <stdlib.h>
#include "cacheTrack.h"
#include "FSDefine.h"

// general handle access(read/write) API
void handleAccess(unsigned long addr, size_t size, int isWrite);

//transform between addr and cacheLineIndex
unsigned long addr2CacheLineIndex(unsigned long addr);
unsigned long CacheLineIndex2addr(unsigned long index);
//concrete handle access function
void store_1_bytes(unsigned long addr);
void store_2_bytes(unsigned long addr);
void store_4_bytes(unsigned long addr);
void store_8_bytes(unsigned long addr);
void store_16_bytes(unsigned long addr);
void load_1_bytes(unsigned long addr);
void load_2_bytes(unsigned long addr);
void load_4_bytes(unsigned long addr);
void load_8_bytes(unsigned long addr);
void load_16_bytes(unsigned long addr);


unsigned long *_cacheWrites;
cacheTrack **_cacheTrackings;

void initCacheStatus(){
    size_t n_cacheLines;//the number of cache lines
    size_t cacheStatusSize;//memory of total cache line status

    n_cacheLines = MAX_USER_SPACE / CACHE_LINE_SIZE;
    cacheStatusSize = n_cacheLines * sizeof(unsigned long)

    //_cacheWrites[i] store a counter(each integer size:unsigned long)
    //_cacheTrackings[i] store a pointer to an cacheTrack object(each pointer size:unsigned long)
    _cacheWrites = (unsigned long *)malloc(cacheStatusSize);//need to limit the base???
    _cacheTrackings = (cacheTrack **)malloc(cacheStatusSize);//need to limit the base???




}


void handleAccess(unsigned long addr, size_t size, int isWrite){
    //get index of addr in _cacheWrites
    unsigned long index = addr2CacheLineIndex(addr);
    unsigned long *totalWritesPtr = &_cacheWrites[index];
    unsigned long totalWrites = *totalWritesPtr;
    cacheTrack *track = NULL;
    //no need to track more info.
    if (totalWrites < CACHE_TRACK_THRESHOLD ){
        if(isWrite == 1){
            if (atomic_increment_and_get(totalWritesPtr) == CACHE_TRACK_THRESHOLD){
                cacheStart = getCacheStart(addr);
                track = allocateTrack(addr);
                //compare _cacheTracking[index](type:cacheTrack) with 0, 
                //if _cacheTracking[index]==0, update it with track(type:cacheTrack *)
                atomic_compare_and_swap(_cacheTracking[index], 0, track);
            }
        }
    }
    else{
        ;
    }
}



inline unsigned long addr2CacheLineIndex(unsigned long addr){
    return addr >> CACHE_LINE_SIZE_SHIFT;
}

inline unsigned long CacheLineIndex(unsigned long index){
    return index << CACHE_LINE_SIZE_SHIFT;
} 