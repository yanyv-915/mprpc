#pragma once
#include"../core/IVectorData.h"
#include"../network/Protocol.h"

#include<iostream>
#include<vector>
#include<string>


class IO{
public:

    static bool writeHeader(std::ostream& os,const MessageHeader& header){
        os.write(reinterpret_cast<const char*>(&header),sizeof(MessageHeader));
        return os.good();
    }

    static bool writeVector(std::ostream& os,const IVectorData& vec){
        os.write(reinterpret_cast<const char*>(vec.getRawPtr()),vec.getSize());
        return os.good();
    }

    static bool readHeader(std::istream& is,MessageHeader& header){
        is.read(reinterpret_cast<char*>(&header),sizeof(MessageHeader));
        return is.gcount() == sizeof(MessageHeader);
    }

    static bool readRaw(std::istream& is,void* dest,size_t size){
        is.read(reinterpret_cast<char*>(dest),size);
        return (size_t)is.gcount() == size;
    }
};