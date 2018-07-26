#ifndef _FSDefine_H_
#define _FSDefine_H_

#include <stdio.h>

class FSDefine{
    public:
    //if _cacheWrites[index] has achieved CACHE_TRACK_THRESHOLD, allocate a cacheTrack to track more
    enum{ CACHE_TRACK_THRESHOLD = 100};

}