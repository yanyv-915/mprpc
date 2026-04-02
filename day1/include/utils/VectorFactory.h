#pragma once
#include"../core/IVectorData.h"
#include"../network/Protocol.h"
#include"../cache/Vector.h"

#include<memory>
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