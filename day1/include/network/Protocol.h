#pragma once
#include<netinet/in.h>
#include<vector>
#include<string>


#pragma pack(push,1)
struct MessageHeader{
    uint16_t magic; // 0x4647
    uint8_t op;     // 1:SET, 2:GET, 3:DEL, 4:SEARCH
    uint8_t  dataType;// <-- 新增：1:FLOAT32, 2:INT8, 3:BINARY...
    uint64_t key_id;
    uint32_t dim;
};
#pragma pack(pop)


struct Client{
    std::string readBuf;
    bool headerParsed=false;
    MessageHeader curHeader;
};