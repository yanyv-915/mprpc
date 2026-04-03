#pragma once
#include<netinet/in.h>
#include"../network/Protocol.h"


class IVectorData{
public:
    virtual ~IVectorData() = default;
    
    virtual size_t getSize() const = 0;
    virtual uint32_t dim() const = 0;
    virtual const void* getRawPtr() const = 0;
    virtual DataType getTypeTag() const = 0;
    virtual float compute_l2(const IVectorData* other) const = 0 ;
};