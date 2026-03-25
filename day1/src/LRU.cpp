#include"../include/LRU.h"

std::filesystem::path VectorCache::defaultAofPath() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    fs::path exeDir = (!ec && !exe.empty()) ? exe.parent_path() : fs::current_path();

    // Prefer writing beside the executable; if not feasible, fall back one level up.
    fs::path p1 = (exeDir / "cache.aof").lexically_normal();
    fs::path p2 = (exeDir / ".." / "cache.aof").lexically_normal();
    return fs::exists(p1) ? p1 : p2;
}

VectorCache::VectorCache(size_t cap):capacity(cap){
    aof_path = defaultAofPath();
    aof_file.open(aof_path, std::ios::out | std::ios::app | std::ios::binary);
    if(!aof_file.is_open()){
        std::cerr<<"文件打开失败: "<<aof_path<<endl;
        return;
    }
    cacheMap.reserve(cap+10);
    cout<<"容器加载成功!"<<endl;
}

VectorCache::~VectorCache(){
    if(aof_file.is_open()){
        aof_file.close();
    }
}

bool VectorCache::checkDim(const VectorData& vec){
    int current_dim = vec.size();
    if (global_dim == -1)
    {
        // 第一次存入数据，由第一条数据决定整个系统的维度
        global_dim = current_dim;
        cout << "系统维度已确定为: " << global_dim << endl;
    }
    else if (current_dim != global_dim)
    {
        // 如果后续数据维度不对，拒绝写入
        std::cerr << "错误：维度不匹配！期望 " << global_dim << " 维，实际收到 " << current_dim << " 维。" << endl;
        return false; // 不执行后续的 write 操作
    }
    return true;
}

void VectorCache::loadBin(){
    std::ifstream ifs(aof_path,std::ios::binary);
    if(!ifs.is_open()){
        std::cout << "未发现历史数据文件或打开失败。" << std::endl;
        return;
    }

    if (ifs.peek() != EOF) {
        // 尝试偷看第一条数据的维度，来初始化 global_dim
        long current_pos = ifs.tellg();
        uint32_t first_key_len = 0;
        IO::read(ifs, first_key_len); // 跳过 Key 长度
        ifs.seekg(first_key_len, std::ios::cur); // 跳过 Key 内容
        
        uint32_t saved_dim = 0;
        IO::read(ifs, saved_dim); // 读取维度
        this->global_dim = saved_dim; 
        
        cout << "[INIT] 从磁盘恢复，系统维度锁定为: " << global_dim << endl;
        ifs.seekg(current_pos); // 把指针指回去，开始正常的循环读取
    }

    int cnt=0;
    while(ifs.peek()!=EOF){
        std::string key=IO::readString(ifs);
        if(key.empty() && ifs.eof()) break;

        VectorData vec=IO::readVec(ifs);
        this->put(key,vec);
        cnt++;
    }
    std::cout << ">>> 二进制数据热启动成功，恢复了 " << cnt << " 条数据。" << std::endl;
    is_loading=false;
    ifs.close();
}

VectorData VectorCache::parseVector(string vecStr){
    VectorData res;
    size_t pos=0;
    string token;
    while((pos=vecStr.find(','))!=string::npos){
        token=vecStr.substr(0,pos);
        res.push_back(std::stod(token));
        vecStr.erase(0,pos+1);
    }
    if(!vecStr.empty()){
        res.push_back(std::stod(vecStr));
    }
    return res;
}

float VectorCache::calL2(const VectorData& v1,const VectorData& v2){
    float dist=0.0f;
    for(size_t i=0;i<v1.size();i++){
        float diff=v1[i]-v2[i];
        dist+=diff*diff;
    }
    return dist;
}

void VectorCache::exe_set(const string& rawRequest){
    size_t firstSpace=rawRequest.find(' ');
        size_t secondSpace=rawRequest.find(' ',firstSpace+1);
        
        if(firstSpace!=string::npos && secondSpace !=string::npos){
            string key=rawRequest.substr(firstSpace+1,secondSpace-firstSpace-1);
            string vecPart=rawRequest.substr(secondSpace+1);
            VectorData vec=parseVector(vecPart);
            if(checkDim(vec)){
                this->put(key,vec);
            }
            //cout<<"Successfully SET key: "<<key<<" with "<<vecPart<<endl;
        }
}

void VectorCache::exe_get(const string& rawRequest,const int& client_fd){
    string key=rawRequest.substr(4);
        VectorData out;
        string reply;
        if(!this->get(key,out)){
            reply="Not Found!\n";
        }
        else{
            reply="";
            for(int i=0;i<out.size();i++){
                reply+=std::to_string(out[i])+" ";
            }
        }
        if(client_fd==-1){
            return;
        }
        send(client_fd,reply.c_str(),reply.size(),MSG_NOSIGNAL);
}

string VectorCache::exe_search(const string& q){
    if(q.size()<7) return "ERROR";
    size_t pos=q.find(' ');
    if(pos==string::npos) return "ERROR";
    while(pos<q.size()&&q[pos]==' ') { pos++; }
    if(pos>=q.size()) return "ERROR";
    string real=q.substr(pos);
    VectorData queryVec=parseVector(real);
    if(global_dim!=-1&&queryVec.size()!=(size_t)global_dim){
        return "ERROR: Query dimension mismatch\n";
    }
    string bestKey="None";
    float minDist=std::numeric_limits<float>::max();

    std::lock_guard<mutex> lk(l_mtx);
    for(auto const& [key,node]:cacheMap){
        float dist=calL2(queryVec,node->data);
        if(dist<minDist){
            minDist=dist;
            bestKey=key;
        }
    }
    return "Result: " + bestKey + " (Distance: " + std::to_string(minDist) + ")\n";
}

void VectorCache::saveToBin(const string& request){
    std::lock_guard<mutex> lk(aof_mtx);
    //cout << "[DEBUG] Entering saveToBin, request: " << request << endl;
    if(!aof_file.is_open()){
        
        std::cerr<<"文件打开失败: "<<aof_path<<endl;
        return;
    }
    if(request.substr(0,3) == "SET"){
        if (!aof_file)
        {
            std::cerr << "文件写入流状态异常！错误代码: " << strerror(errno) << std::endl;
        }
        if(request.size()<4) return;
        int pos1=3;
        while(pos1<request.size()&&request[pos1]==' ') { pos1++; }
        if(pos1>=request.size()) return;
        int pos2=request.find_first_of(' ',pos1);
        if (pos2 == string::npos) return;
        string key=request.substr(pos1,pos2-pos1);
        //cout << "[DEBUG] Binary write successful for key: " << key << endl;


        while(pos2<request.size()&&request[pos2]==' ') { pos2++; }
        if(pos2>=request.size()) return;
        string sVec=request.substr(pos2);
        VectorData vec=parseVector(sVec);
        if(checkDim(vec)){
            //cout << "[DEBUG] Writing Key: '" << key << "' Length: " << key.size() << endl;
            IO::writeString(aof_file,key);
            IO::writeVec(aof_file,vec);
            if (!aof_file.good()) {
                std::cerr << "AOF 写入失败，流状态异常: " << aof_path << std::endl;
                return;
            }
            aof_file.flush();
        }
    }
}

bool VectorCache::get(const std::string& key,VectorData& outData){
    std::unique_lock<mutex> lk(l_mtx);
    auto it=cacheMap.find(key);
    if(it==cacheMap.end()) return false;
    auto listIt=it->second;
    cacheList.splice(cacheList.begin(),cacheList,listIt);
    lk.unlock();
    outData=listIt->data;
    return true;
}

void VectorCache::put(const std::string& key,const VectorData& data){
    std::unique_lock<mutex> lk(l_mtx);
    //std::cout << "[DEBUG] Putting Key: " << key << " | Dim: " << data.size() << std::endl;
    auto it=cacheMap.find(key);
    if(it!=cacheMap.end()){
        auto listIt=it->second;
        listIt->data=data;
        cacheList.splice(cacheList.begin(),cacheList,listIt);
    }
    else{
        if(cacheList.size()>=capacity){
            auto lastNode=cacheList.back();
            cacheMap.erase(lastNode.key);
            cacheList.pop_back();
        }
        cacheList.push_front({key,data});
        cacheMap[key]=cacheList.begin();
    }
}

void VectorCache::executeCommand(const string& rawRequest,const int& client_fd){
    if(rawRequest.substr(0,3)=="SET"){
        exe_set(rawRequest);
    }
    else if(rawRequest.substr(0,3)=="GET"){
        exe_get(rawRequest,client_fd);
    }
    else if(rawRequest.substr(0,6)=="SEARCH"){
        send(client_fd,exe_search(rawRequest).c_str(),exe_search(rawRequest).size(),MSG_NOSIGNAL);
    }
    else{
        string reply="未知指令!请重新检查!\n";
        if(client_fd != -1){
            send(client_fd,reply.c_str(),reply.size(),MSG_NOSIGNAL);
        }
    }
}

void VectorCache::handleRequest(const string& rawRequest,const int& client_fd){
    if(is_loading){
        cout<<client_fd<<endl;
        string reply="内存处于加载阶段，请耐心等待！\n";
        if(client_fd != -1){
            send(client_fd,reply.c_str(),reply.size(),MSG_NOSIGNAL);
        }
        return;
    }
    // 关键：把 client_fd 透传给 executeCommand，才能在 GET/异常等场景回包给客户端
    //cout<<rawRequest<<endl;
    executeCommand(rawRequest,client_fd);
    saveToBin(rawRequest);
}

void VectorCache::print(){
    for(auto& node:cacheList){
        cout<<node.key<<":";
        for(size_t i=0;i<node.data.size();i++){
            cout<<node.data[i]<<" ";
        }
        cout<<endl;
    }
}
