#include<cstdio>
#include<iostream>
#include<thread>
#include<pthread>
#include<map>
#include<mutex>

#define CACHE_LINE_SIZE 64
#define CACHE_LINE_SIZE_SHIFT 6
#define MAX_ENTRIES 2

using namespace std;

//define a global mutex
mutex _mutex;
//define a global map
//Map<unsigned long, cacheLineWriteCounter*> _cacheWritesMap;
Map<unsigned long, cacheLineHistory*> _cacheTrackingsMap;//key-value is cacheLineIndex-a pointer


//transform between addr and cacheLineIndex
unsigned long addr2CacheLineIndex(unsigned long addr);
unsigned long cacheLineIndex2startAddr(unsigned long index);
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


void store_1_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 1, true);
}

void store_2_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 2, true);
}
void store_4_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 4, true);
}

void store_8_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 8, true);
}

void store_16_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 16, true);
}

void load_1_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 1, false);
}

void load_2_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 2, false);
}

void load_4_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 4, false);
}

void load_8_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 8, false);
}

void load_16_bytes(unsigned long addr){
    unsigned long tID = pthread_self();
    handleAccess(tID, addr, 16, false);
}


class  cacheLineHistory{
private:
    struct accessEntry{
        unsigned long tID;
        unsigned long addr;
        size_t size;
        bool isWrite;
    };
    int n_entries;
    struct accessEntry _entries[MAX_ENTRIES];
    sem_t semHistory;
public:
    cacheLineHistory(){
        sem_init(&semHistory, 0, 1);
        n_entries = 0;
    }
    virtual ~cacheLineHistory(){
        sem_destroy(&semHistory);
    }

    inline bool isFull(){
        return n_entries == MAX_ENTRIES;
    }

    inline void flushHistoryOnWrite(unsigned long tID, unsigned long addr, size_t size){
        _entries[0].tID = tID;
        _entries[0].addr = addr;
        _entries[0].size = size;
        _entries[0].isWrite = true;
        n_entries = 1;
    }

    inline bool checkIfFirstAccess(unsigned long tID, unsigned long addr, size_t size, bool isWrite){
        if(n_entries == 0){
            _entries[0].tID = tID;
            _entries[0].addr = addr;
            _entries[0].size = size;
            _entries[0].isWrite = isWrite;
            n_entries = 1;
            return true;
        }
        return false;
    }

    inline bool checkTorFSharing(unsigned long tID, unsigned long addr, size_t size, bool isWrite){
        if(_entries[0].addr+_entries[0].size < addr || addr+size < _entries[0].addr){
            //report false sharing
            cout << "Detect a false sharing!\n";
            cout << _entries[0].tID << "\t" << _entries[0].addr << "\t" << _entries[0].size << _entries[0].isWrite <<"\n";
            cout << tID << "\t" << addr << "\t" << size << "\t" << isWrite << "\n";
            return true
        }
        else{
            if(isFull() && (_entries[1].addr+_entries[1].size < addr || addr+size < _entries[1].addr)){
                //report false sharing
                cout << "Detect a false sharing!\n";
                cout << _entries[1].tID << "\t" << _entries[1].addr << "\t" << _entries[1].size << _entries[1].isWrite <<"\n";
                cout << tID << "\t" << addr << "\t" << size << "\t" << isWrite << "\n";
                return true;
            }
        }
        return false;   
    }

    bool updateHistory(unsigned long tID, unsigned long addr, size_t size, bool isWrite){
        sem_wait(&semHistory);
        //if current access is first access;
        if (checkIfFirstAccess(tID, addr, size, isWrite) == false){
            bool hasInvalidation = false;
            bool hasFalseSharing = false;
            if(isWrite == false){
                if(!isFull()){
                    if(_entries[0].tID != tID){
                        _entries[1].tID = tID;
                        _entries[1].addr = addr;
                        _entries[1].size = size;
                        _entries[1].isWrite = false;
                        n_entries = 2;
                    }
                }
            }
            else{//just write can cause false sharing
                if(isFull()){
                    //report invalidation first
                    cout << "Detect an invalidation, need to judge true sharing/false sharing\n";
                    //judge true sharing or false sharing 
                    hasInvalidation = true;
                }
                else{
                    if(_entries[0].tID != tID){
                         //report invalidation first
                        cout << "Detect an invalidation, need to judge true sharing/false sharing\n";
                        hasInvalidation = true;
                    }
                }
                flushHistoryOnWrite(tID, addr, size);
                if(hasInvalidation == true){
                    hasFalseSharing = checkTorFSharing(tID, addr, size, isWrite);
                }
            }
        }
        sem_post(&semHistory);
        return hasFalseSharing;
    }
    
};       
 

inline unsigned long addr2CacheLineIndex(unsigned long addr){
    return addr >> CACHE_LINE_SIZE_SHIFT;
}

inline unsigned long cacheLineIndex2startAddr(unsigned long index){
    return index << CACHE_LINE_SIZE_SHIFT;
} 

inline size_t calSpanCacheLines(unsigned long addr, size_t size){
    //ins_size max is 16 Bytes,cache_line_size is 64 Bytes, so the max span is 2
    size_t cacheLineSpan = 1;
    unsigned long index = addr2CacheLineIndex(addr);
    nextCacheLineStartAddr = ( index +1 ) * CACHE_LINE_SIZE;
    if (addr + size >= nextCacheLineStartAddr){
        cacheLineSpan = 2;
    }
    return cacheLineSpan;
}

void handleAccess(unsigned long tID, unsigned long addr, size_t size, bool isWrite){
    //get index of addr in _cacheWrites
    unsigned long cacheLineIndex = addr2CacheLineIndex(addr);
    size_t cacheLineSpan = calSpanCacheLines(addr, size);
    int i;
    for( i = 0; i < cacheLineSpan; i++){
        _mutex.lock();
        map<unsigned long, cacheLineHistory*>::iterator it = _cacheTrackingsMap.find(cacheLineIndex + i);
        if(_cacheTrackingsMap.end() == it){
            //this key(cacheLineIndex) doesnt exist.
            *cacheLineHistory cacheHistory = new cacheLineHistory();
            _cacheTrackingsMap[cacheLineIndex+i] = cacheHistory;
        }
        _mutex.unlock();

        if(cacheLineSpan == 1){
            _cacheTrackingsMap[cacheLineIndex+i].updateHistory(tID, addr, size, isWrite);
        }
        else{
            nextStartAddr = cacheLineIndex2startAddr(cacheLineIndex+1);
            if(i==0){
                _cacheTrackingsMap[cacheLineIndex+i].updateHistory(tID, addr, nextStartAddr-addr, isWrite);
            }
            else{
                _cacheTrackingsMap[cacheLineIndex+i].updateHistory(tID, nextStartAddr, size-nextStartAddr+addr, isWrite);
            }
        }    
    }
}

//test 
int main(){
    return 0;
}