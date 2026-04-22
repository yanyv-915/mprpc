#pragma once
#include"../core/IVectorData.h"
#include<vector>
#include"../network/Protocol.h"

#include<assert.h>

// --- 1. 先定义 Traits（类型映射），必须放在类外面 ---
template<typename T> struct TypeToTag;

template<> struct TypeToTag<float>   { static constexpr DataType value = DataType::FLOAT32; };
template<> struct TypeToTag<int16_t> { static constexpr DataType value = DataType::INT16;   };
template<> struct TypeToTag<uint8_t> { static constexpr DataType value = DataType::BINARY;   };
template<typename T>
class VectorDataImpl : public IVectorData{
private:
    std::vector<T> _data;
public:
    VectorDataImpl()=default;
    VectorDataImpl(std::vector<T>&& d):_data(std::move(d)){}
    VectorDataImpl(uint32_t dim){_data.resize(dim,0.0f);}
    float compute_l2(const IVectorData* other) const override {
        assert(other->getTypeTag() == this->getTypeTag());
        const T* p2 = static_cast<const T*>(other->getRawPtr());
        const T* p1 = _data.data();
        size_t n=_data.size();

        float sum=0.0f;
        for(size_t i=0;i<n;i++){
            float diff = static_cast<float> (p1[i]) - static_cast<float>(p2[i]);
            sum += diff*diff;
        }
        return sum;
    }
    const void* getRawPtr() const override {return _data.data();}
    size_t getSize() const override{return _data.size()*sizeof(T);}
    uint32_t dim() const override {return _data.size();}
    DataType getTypeTag() const override {
        return TypeToTag<T>::value;
    }
};