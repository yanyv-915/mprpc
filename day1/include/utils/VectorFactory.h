#pragma once
#include"../core/IVectorData.h"
#include"../network/Protocol.h"
#include"../cache/Vector.h"

#include<memory>

using FloatVectorData = VectorDataImpl<float>;
using Int16VectorData = VectorDataImpl<int16_t>;

class VectorFactoy{
public:
    static std::shared_ptr<IVectorData> create(DataType typeTag,uint32_t dim){
        switch (typeTag)
        {
        case DataType::FLOAT32:
            return std::make_shared<FloatVectorData>(dim);
            break;
        case DataType::INT16:
            return std::make_shared<Int16VectorData>(dim);
            break;
        default:
            return nullptr;
        }
    }
};