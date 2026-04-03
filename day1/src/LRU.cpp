#include"../include/cache/LRU.h"
#include"../include/utils/aof.h"
VectorCache::VectorCache(size_t cap):cap_single(cap),segments(SEGMENT_CNT){
    aof=std::make_shared<AofManager>();
    vector<thread> threads;
    threads.reserve(SEGMENT_CNT);
    for(size_t i=0;i<SEGMENT_CNT;i++){
        threads.emplace_back([this,i](){
            segments[i].storage.reserve(cap_single);
        });
    }
    aof->recover(*this);
    for(auto& t:threads){
        if(t.joinable()) t.join();
    }
}


void VectorCache::set(const uint64_t& key,const std::shared_ptr<IVectorData>& vec){
    if(!vec){
        throw std::invalid_argument("向量数据不能为nullptr");
    }
    size_t seg_idx=key % MASK;
    std::lock_guard<mutex> lk(segments[seg_idx].mtx);
    auto it=segments[seg_idx].storage.find(key);
    auto& cur_seg=segments[seg_idx];
    if(it!=cur_seg.storage.end()){
        auto& listIt=it->second;
        listIt.data=vec;
        cur_seg.cache_list.splice(cur_seg.cache_list.begin(),cur_seg.cache_list,listIt.it);
        return;
    }

    if(cur_seg.cache_list.size()>=cap_single){
        auto it=cur_seg.cache_list.back();
        cur_seg.storage.erase(it);
        cur_seg.cache_list.pop_back();
    }
    auto new_list=cur_seg.cache_list.insert(cur_seg.cache_list.begin(),key);
    cur_seg.storage[key]={vec,new_list};
}

bool VectorCache::get(const uint64_t& key,std::shared_ptr<IVectorData>& vec){
    if(!vec){
        throw std::invalid_argument("向量数据不能为nullptr");
        return false;
    }
    size_t seg_idx=key % MASK;
    std::lock_guard<mutex> lk(segments[seg_idx].mtx);
    auto& cur_seg=segments[seg_idx];
    auto it=cur_seg.storage.find(key);
    if(it!=cur_seg.storage.end()){
        auto listIt=it->second;
        cur_seg.cache_list.splice(cur_seg.cache_list.begin(),cur_seg.cache_list,listIt.it);
        vec=listIt.data;
        return true;
    }
    return false;
}

void VectorCache::del(const uint64_t& key){
    size_t seg_idx=key % MASK;
    std::lock_guard<mutex> lk(segments[seg_idx].mtx);
    auto& cur_seg=segments[seg_idx];
    auto it=cur_seg.storage.find(key);
    if(it==cur_seg.storage.end()) return;
    auto listIt=it->second;
    cur_seg.cache_list.erase(listIt.it);
    cur_seg.storage.erase(key);
}

void VectorCache::handleRequest(const MessageHeader& header,shared_ptr<IVectorData>& vec,const size_t& fd){
    switch (header.op)
    {
    case OpCode::SET:
        set(header.key_id,vec);
        aof->appendSet(header.key_id,vec);
        break;
    case OpCode::GET:
        get(header.key_id,vec);
        break;
    case OpCode::DEL:
        del(header.key_id);
        break;
    case OpCode::SEARCH:
        
        break;
    default:
        break;
    }
}


