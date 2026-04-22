#pragma once

#include <vector>
#include <cstddef>
#include <algorithm> // copy 需要
#include <cstring>   // memcpy 需要
#include<errno.h>
#include<sys/uio.h>

class Buffer {
private:
    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;

public:
    Buffer(size_t initial_size = 1024) 
        : buffer_(initial_size), read_index_(0), write_index_(0) {}

    // --- 状态查询 ---
    size_t readableBytes() const { return write_index_ - read_index_; }
    size_t writableBytes() const { return buffer_.size() - write_index_; }
    size_t prependableBytes() const { return read_index_; }

    // 指向可读数据的首地址（给解析器用）
    const char* peek() const { return &buffer_[read_index_]; }

    // --- 写入处理 (数据入口) ---
    void append(const char* data, size_t len) {
        if (writableBytes() < len) {
            makeSpace(len); // 空间不够，管家出来干活
        }
        // 拷贝数据到写指针位置
        std::copy(data, data + len, &buffer_[write_index_]);
        write_index_ += len;
    }

    // 给 recv 使用的直接写入地址
    char* beginWrite() { return &buffer_[write_index_]; }
    
    void hasWritten(size_t len) { write_index_ += len; }

    // --- 读取处理 (数据出口) ---
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            read_index_ += len; // 只是挪动指针，没删数据
        } else {
            // 全部读完了，直接归零，这是最高效的回收
            read_index_ = 0;
            write_index_ = 0;
        }
    }

    void retrieveAll() {
        read_index_ = 0;
        write_index_ = 0;
    }

    // --- 空间管理 (幕后管家) ---
    void makeSpace(size_t len) {
        if (writableBytes() + prependableBytes() < len) {
            // 真的装不下了，扩容。采用你建议的 2 倍策略更激进些：
            buffer_.resize(write_index_ + len); 
        } 
        else {
            // 空间够，只是不连续，把有效数据挪到最前面
            size_t readable = readableBytes();
            std::copy(buffer_.begin() + read_index_,
                      buffer_.begin() + write_index_,
                      buffer_.begin());
            read_index_ = 0;
            write_index_ = readable;
        }
    }

    ssize_t readFd(int fd,int* saveErrno){
        char extraBuf[65536];
        struct iovec vec[2];
        const size_t writable = writableBytes();

        vec[0].iov_base=beginWrite();
        vec[0].iov_len=writable;

        vec[1].iov_base=extraBuf;
        vec[1].iov_len=sizeof(extraBuf);

        const ssize_t n= readv(fd,vec,2);

        if(n<0){
            *saveErrno = errno;
        }
        else if(static_cast<size_t>(n) <= writable){
            write_index_ += n;
        }
        else{
            write_index_ = buffer_.size();
            append(extraBuf,n - writable);
        }
        return n;
    }
};