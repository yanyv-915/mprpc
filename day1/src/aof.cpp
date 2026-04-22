#include "../include/utils/aof.h"
#include <set>
#include<filesystem>
#include<unistd.h>
namespace fs = std::filesystem;

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
    filename = "../cache.bin";
    aof_file.open(filename, std::ios::binary | std::ios::app);
    if(!aof_file.is_open()){
        std::cerr<<"文件打开失败!\n";
    }
    worker=std::thread(&AofManager::work_loop,this);
}
AofManager::~AofManager(){
    {
        std::unique_lock<mutex> lk(aof_mtx);
        persist_buffer.swap(activa_buffer);
        stop=true;
    }
    cv.notify_all();
    if(worker.joinable()) worker.join();
    if(aof_file.is_open()) aof_file.close();
}

long AofManager::getDim(){
    std::ifstream ifs("../cache.bin",std::ios::binary);
    if(!ifs.is_open()){
        std::cerr<<"文件不存在！\n";
        return -1;
    }
    ifs.seekg(0,std::ios::end);
    auto size=ifs.tellg();
    if(static_cast<size_t>(size)<sizeof(MessageHeader)){
        std::cerr<<"文件为空或损坏！\n";
        return -1;
    }
    ifs.seekg(0);
    MessageHeader header{};
    IO::readHeader(ifs,header);
    return static_cast<long>(header.dim);
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
        std::error_code ec;
        uintmax_t fileSize = fs::file_size(filename,ec);

        if(fileSize > file_threshold&&!ec){
            if(_trigger){
                std::thread([this](){
                    _trigger();
                }).detach();
                file_threshold=2*fileSize;
            }
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
    if(is_rewriting){
        std::lock_guard<std::mutex> lk_re(rewrite_mtx);
        rewrite_buf.push_back({header,vec});
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
    asyncPush(header,nullptr);
}

void AofManager::recover(VectorCache &cache) {
    std::ifstream is(filename, std::ios::binary);
    MessageHeader header;
    std::streampos last_good_pos = 0;
    std::set<uint64_t> unique_keys;
    while (IO::readHeader(is, header)) {
        std::streampos current_pos = is.tellg();
        if (header.magic != 0x4647) {
            // 打印偏移量，看看是不是在读乱码
            std::cout << "[Critical] AOF 损坏，偏移量: " << is.tellg() << ". 停止加载。" << std::endl;
            if (truncate(filename.c_str(), current_pos) == -1) {
                perror("truncate failed"); // 打印错误
            }
            break;
        }

        if (header.op == OpCode::SET) {
            unique_keys.insert(header.key_id);
            auto vec = VectorFactoy::create(header.dataType, header.dim);
            if (vec && IO::readRaw(is, (void *)vec->getRawPtr(), vec->getSize())) {
                cache.set(header.key_id, vec);
            }
        } 
        else if (header.op == OpCode::DEL) {
            cache.del(header.key_id);
        }
    }
    
    std::cout << "AOF 恢复报告: \n"
              << "唯一的 Key ID 数量: " << unique_keys.size() << std::endl;
}

void AofManager::rewrite(VectorCache& cache){
    is_rewriting=true;
    std::cout<<"文件过大，发起重写\n";
    string tmp_file = filename+".temp";
    std::ofstream ofs(tmp_file,std::ios::binary);
    size_t cnt=0;
    auto snap=cache.getVectorSnap();
    for(auto& [id,data]: snap){
        MessageHeader h{};
        h.magic=0x4647;
        h.op=OpCode::SET;
        h.key_id=id;
        h.dim = data->dim();
        IO::writeHeader(ofs,h);
        IO::writeVector(ofs,*data);
        cnt++;
        if(cnt % 5000 == 0){
              std::cout<<"已重写1000条数据进入缓冲区\n";
        }
    }
    {
        // 使用 std::scoped_lock 一次性锁住两个互斥量，防止死锁
        std::scoped_lock lock(rewrite_mtx, aof_mtx); 

        // A. 补齐重写期间产生的增量
        for(auto& task : rewrite_buf) {
            IO::writeHeader(ofs, task.header);
            IO::writeVector(ofs, *task.data);
        }
        rewrite_buf.clear();
        ofs.flush();
        ofs.close();

        // B. 物理更名与句柄切换
        if(aof_file.is_open()) aof_file.close();
        std::rename(tmp_file.c_str(), filename.c_str());

        // C. 【关键】在释放锁之前关闭重写开关
        is_rewriting = false; 

        // D. 重新打开新文件供主线程继续追加
        aof_file.open(filename, std::ios::app | std::ios::binary);
    }
    std::cout<<"AOF重写完成,文件已替换,当前文件大小阈值为"<<file_threshold/(1024*1024)<<"MB"<<'\n';
}