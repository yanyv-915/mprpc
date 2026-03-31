#pragma once
#include<IVectorData.h>
#include<vector>

class FloatVector : public IVectorData{
private:
    std::vector<float> data;
public:
    FloatVector(std::vector<float>&& d):data(std::move(d)) {};
    size_t getSize() const override {return data.size()* sizeof(float);}
    const void* getRawPtr() const override {return data.data();}
    uint8_t getTypeTag() const override {return 1;}
};