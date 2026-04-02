#include "../include/utils/aof.h"
AofManager::AofManager(const string &path) : filename(path)
{
    aof_file.open(filename, std::ios::binary | std::ios::app);
    if(!aof_file.is_open()){
        std::cerr<<"文件打开失败!\n";
    }
    worker=std::thread(&AofManager::work_loop,this);
    
}
AofManager::AofManager()
{
    aof_file.open("../cache.bin", std::ios::binary | std::ios::app);
    if(!aof_file.is_open()){
        std::cerr<<"文件打开失败!\n";
    }
    
    worker=std::thread(&AofManager::work_loop,this);
}
AofManager::~AofManager(){
    {
        std::unique_lock<mutex> lk(aof_mtx);
        stop=true;
        persist_buffer.swap(activa_buffer);
    }
    cv.notify_all();
    if(worker.joinable()) worker.join();
    if(aof_file.is_open()) aof_file.close();
}
void AofManager::work_loop(){
    while(true){
        vector<AofTask> temp_buffer;
        {
            std::unique_lock<mutex> lk(aof_mtx);
            cv.wait(lk,[this]{
                return stop || !persist_buffer.empty();
            });
            if(stop && persist_buffer.empty()) break;
            temp_buffer.swap(persist_buffer);
        }
        if(!temp_buffer.empty()){
            for(const auto& rec:temp_buffer){
                IO::writeHeader(aof_file,rec.header);
                IO::writeVector(aof_file,*(rec.data));
            }
            aof_file.flush();
        }
    }
}

void AofManager::asyncPush(const MessageHeader& header,const std::shared_ptr<IVectorData>& vec){
    std::lock_guard<mutex> lk(aof_mtx);
    activa_buffer.push_back({header,vec});
    if(activa_buffer.size()>=buffer_threshold){
        persist_buffer.insert(persist_buffer.end(),
                                std::make_move_iterator(activa_buffer.begin()),
                                std::make_move_iterator(activa_buffer.end()));
        activa_buffer.clear();
        cv.notify_one();
    }
}

void AofManager::appendSet(const uint64_t& keyId, const std::shared_ptr<IVectorData>& vec)
{
    MessageHeader header;
    header.magic = 0x4647;
    header.op = OpCode::SET;
    header.dataType = vec->getTypeTag();
    header.key_id = keyId;
    header.dim = vec->dim();
    asyncPush(header,vec);
}

void AofManager::appendDel(const uint64_t& keyId)
{
    std::lock_guard<mutex> lk(aof_mtx);
    MessageHeader header = {0x4647, OpCode::DEL, DataType::BINARY, keyId, 0};
    IO::writeHeader(aof_file, header);
    aof_file.flush();
}

void AofManager::recover(VectorCache &cache)
{
    std::ifstream is("cache.bin", std::ios::binary);
    MessageHeader header;
    while (IO::readHeader(is, header))
    {
        if (header.magic != 0x4647)
            continue;
        if (header.op == OpCode::SET)
        {
            auto vec = VectorFactoy::create(header.dataType, header.dim);
            if (vec && IO::readRaw(is, (void *)vec->getRawPtr(), vec->getSize()))
            {
                cache.set(header.key_id, vec);
            }
        }
    }
}