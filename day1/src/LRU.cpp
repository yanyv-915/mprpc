#include"../include/cache/LRU.h"
#include"../include/utils/aof.h"
#include"../include/core/IVectorData.h"
#include"../include/network/Protocol.h"

#include<algorithm>

VectorCache::VectorCache(size_t cap):cap_single(cap),segments(SEGMENT_CNT){
    aof=std::make_shared<AofManager>();
    vector<thread> threads;
    threads.reserve(SEGMENT_CNT);
    for(size_t i=0;i<SEGMENT_CNT;i++){
        threads.emplace_back([this,i](){
            segments[i].storage.reserve(cap_single);
        });
    }
    for(auto& t:threads){
        if(t.joinable()) t.join();
    }
    global_dim = aof->getDim();
    std::cout<<"文件初始化为"<<global_dim<<"维\n";
    aof->recover(*this);
}

bool VectorCache::checkDim(const shared_ptr<IVectorData>& vec){
    if(global_dim == -1){
        global_dim = vec->dim();
        return true;
    }
    else if(global_dim != vec->dim()){
        std::cerr<<"维度匹配错误！期望维度 "<<global_dim<<" 实际维度 "<<vec->dim()<<"\n";
        return false;
    }
    return true;
}

void VectorCache::set(const uint64_t& key,const std::shared_ptr<IVectorData>& vec){
    size_t seg_idx=key % MASK;
    std::unique_lock<std::shared_mutex> lk(segments[seg_idx].mtx);
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
    std::shared_lock<std::shared_mutex> lk(segments[seg_idx].mtx);
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
    std::shared_lock<std::shared_mutex> lk(segments[seg_idx].mtx);
    auto& cur_seg=segments[seg_idx];
    auto it=cur_seg.storage.find(key);
    if(it==cur_seg.storage.end()) return;
    auto listIt=it->second;
    cur_seg.cache_list.erase(listIt.it);
    cur_seg.storage.erase(key);
}

vector<shared_ptr<IVectorData>> VectorCache::search(const shared_ptr<IVectorData>& query,size_t topK){
    size_t total_in_db = 0;
    for(size_t i=0; i<SEGMENT_CNT; i++) total_in_db += segments[i].storage.size();
    if(total_in_db % 10000 == 0)    std::cout << "DEBUG: Current total storage size: " << total_in_db << std::endl;
    vector<std::future<vector<SearchRes>>> futures;
    for(size_t i=0;i<SEGMENT_CNT;i++){
        futures.push_back(std::async(std::launch::async,[this,i,&query](){
            vector<SearchRes> local_res;
            std::shared_lock<std::shared_mutex> lk(segments[i].mtx);
            for(auto& pair:segments[i].storage){
                float dist=query->compute_l2(pair.second.data.get());
                local_res.push_back({pair.second.data,dist});
            }
            return local_res;
        }));
    }
    std::vector<SearchRes> all_candidates;
    for(auto& f:futures){
        auto res=f.get();
        all_candidates.insert(all_candidates.end(),res.begin(),res.end());
    }
    std::sort(all_candidates.begin(),all_candidates.end(),[](const SearchRes& a,const SearchRes& b){
        return a.distance<b.distance;
    });
    vector<shared_ptr<IVectorData>> ans;
    for(size_t i=0;i<std::min((size_t)all_candidates.size(),topK);i++){
        ans.push_back(all_candidates[i].vec);
    }
    // // ... 之前的排序代码 ...
    // printf("Debug: Found total %zu candidates, requested TopK %zu\n", all_candidates.size(), topK);
    // for(size_t i=0; i < std::min(all_candidates.size(), (size_t)5); ++i) {
    //     printf("  Rank %zu: dist = %f\n", i+1, all_candidates[i].distance);
    // }
    return ans;
}

Response VectorCache::handleRequest(const MessageHeader& header,shared_ptr<IVectorData>& vec){
    Response res{};
    res.op=header.op;
    res.dataType=header.dataType;
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
    case OpCode::SEARCH:{
        res.data=search(vec,header.key_id);
        break;
    }
    default:
        break;
    }
    res.success=true;
    return res;
}


