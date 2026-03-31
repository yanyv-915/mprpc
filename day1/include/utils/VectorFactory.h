#pragma once
#include<memory>
#include<IVectorData.h>
#include<Vector.h>
class VectorFactoy{
public:
    static std::shared_ptr<IVectorData> create(DataType typeTag,uint32_t dim){
        switch (typeTag)
        {
        case DataType::FLOAT32:
            return std::make_shared<FloatVector>(dim);
            break;
        
        default:
            return nullptr;
        }
    }
};