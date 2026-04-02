#pragma once
#include<netinet/in.h>
#include<vector>
#include<string>

enum class OpCode:uint8_t{
    SET = 1,
    GET = 2,
    DEL = 3,
    SEARCH = 4,
    UNKNOWN = 0,
};

enum class DataType:uint8_t{
    FLOAT32 = 1,
    INT16 = 2,
    BINARY = 3
};

#pragma pack(push,1)
struct MessageHeader{
    uint16_t magic; // 0x4647
    OpCode op;     // 1:SET, 2:GET, 3:DEL, 4:SEARCH
    DataType  dataType;// <-- 新增：1:FLOAT32, 2:INT8, 3:BINARY...
    uint64_t key_id;
    uint32_t dim;
};
#pragma pack(pop)


struct Client{
    std::string readBuf;
    bool headerParsed=false;
    MessageHeader curHeader;
};