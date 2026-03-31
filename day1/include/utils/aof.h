#pragma once
#include<string>
#include<fstream>
#include<IVectorData.h>
#include<Vector.h>
#include<Protocol.h>
#include<io.h>
#include<LRU.h>
#include<VectorFactory.h>
using std::string;

class AofManager{
private:
    string filename;
    std::ofstream aof_file;

public:
    AofManager(const string& path) : filename(path){
        aof_file.open(filename,std::ios::binary|std::ios::app);
    }

    void appendSet(uint64_t keyId,const IVectorData& vec){
        MessageHeader header;
        header.magic=0x4647;
        header.op=1;
        header.key_id=keyId;
        header.dim=vec.dim();

        if(IO::writeHeader(aof_file,header)){
            IO::writeVector(aof_file,vec);
            aof_file.flush();
        }
    }

    void appendDel(uint64_t keyId){
        MessageHeader header={0x4647,3,keyId,0};
        IO::writeHeader(aof_file,header);
        aof_file.flush();
    }

    void recover(VectorCache& cache){
        std::ifstream is("cache.bin",std::ios::binary);
        MessageHeader header;
        while(IO::readHeader(is,header)){
            if(header.magic!=0x4647) continue;
            if(header.op==1){
                auto vec=VectorFactoy::create(header.dataType,header.dim);
                if(vec&& IO::readRaw(is,(void*)vec->getRawPtr(),vec->getSize())){
                    cache.set(header.key_id,vec);
                }
            }
        }
    }
};