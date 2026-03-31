#pragma once
#include<netinet/in.h>

enum class DataType : uint8_t {
    FLOAT32 = 1,
    FLOAT64 = 2,
    INT8    = 3,
    BINARY  = 4
};

class IVectorData{
public:
    virtual ~IVectorData() = default;
    virtual size_t getSize() const = 0;
    virtual uint32_t dim() const = 0;
    virtual const void* getRawPtr() const = 0;
    virtual uint8_t getTypeTag() const = 0;
};