#pragma once
#include<memory>
#include<IVectorData.h>
#include<Vector.h>
class VectorFactoy{
public:
    static std::shared_ptr<IVectorData> create(uint8_t typeTag,uint32_t dim){
        switch (typeTag)
        {
        case 1:
            return std::make_shared<FloatVector>(dim);
            break;
        
        default:
            return nullptr;
        }
    }
};