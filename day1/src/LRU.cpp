#include<../include/cache/LRU.h>
VectorCache::VectorCache(size_t cap):capacity(cap),segments(SEGMENT_CNT){}

void VectorCache::set(const uint64_t& key,const std::shared_ptr<IVectorData>& vec){
    size_t seg_idx=key & MASK;
    std::lock_guard<mutex> lk(segments[seg_idx].mtx);
    segments[seg_idx].storage[key]={vec};
}

bool VectorCache::get(const uint64_t& key,std::shared_ptr<IVectorData>& vec){
    size_t seg_id=key & MASK;
    std::lock_guard<mutex> lk(segments[seg_id].mtx);
    auto it=segments[seg_id].storage.find(key);
    if(it!=segments[seg_id].storage.end()){
        vec=it->second;
        return true;
    }
    return false;
}


