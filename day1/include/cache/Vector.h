#pragma once
#include"../core/IVectorData.h"
#include<vector>
#include"../network/Protocol.h"
class FloatVector : public IVectorData{
private:
    std::vector<float> data;
public:
    FloatVector(std::vector<float>&& d):data(std::move(d)) {};
    FloatVector(uint32_t dim){
        data.resize(dim,0);
    }
    size_t getSize() const override {return data.size()* sizeof(float);}
    const void* getRawPtr() const override {return data.data();}
    DataType getTypeTag() const override {return DataType::FLOAT32;}
    uint32_t dim() const override {return data.size();}
};