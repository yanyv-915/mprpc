#pragma once
#include<iostream>
#include<vector>
#include<string>
using VectorData = std::vector<double>;
class IO{
public:
    template<typename T>
    static void write(std::ostream& os,const T& data){
        os.write(reinterpret_cast<const char*>(&data),sizeof(T));
    }

    static void writeString(std::ostream& os,const std::string& str){
        uint32_t len=static_cast<uint32_t>(str.size());
        write(os,len);
        os.write(str.data(),len);
    }

    static void writeVec(std::ostream& os,const VectorData& vec){
        uint32_t dim=static_cast<uint32_t>(vec.size());
        write(os,dim);
        os.write(reinterpret_cast<const char*>(vec.data()),dim*sizeof(VectorData::value_type));
    }

    template<typename T>
    static bool read(std::istream& is,T& data){
        is.read(reinterpret_cast<char*>(&data),sizeof(T));
        return is.gcount()==sizeof(T);
    }

    static std::string readString(std::istream& is){
        std::string str;
        if(!readString(is, str)) return "";
        return str;
    }

    static bool readString(std::istream& is, std::string& out){
        uint32_t len=0;
        if(!read(is,len)) return false;

        std::string str(len,'\0');
        if(len > 0){
            is.read(&str[0],len);
            if(!is) return false;
        }
        out = std::move(str);
        return true;
    }

    static VectorData readVec(std::istream& is){
        VectorData vec;
        if(!readVec(is, vec)) return {};
        return vec;
    }

    static bool readVec(std::istream& is, VectorData& out){
        uint32_t dim=0;
        if(!read(is,dim)) return false;

        VectorData vec(dim);
        is.read(reinterpret_cast<char*>(vec.data()),dim*sizeof(VectorData::value_type));
        if(!is) return false;
        out = std::move(vec);
        return true;
    }
};