/**
 * @file file_utils.h
 * @brief RAII 文件封装 - 自动管理文件资源，带错误检查
 */

#pragma once

#include <cstdio>
#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstring>

/**
 * @class SafeFile
 * @brief RAII 文件封装类
 * 
 * 自动管理文件打开和关闭，提供带错误检查的读写方法
 */
class SafeFile {
public:
    /**
     * @brief 构造函数：打开文件
     * @param[in] path 文件路径
     * @param[in] mode 打开模式（如 "r", "w", "rb", "wb"）
     * @throws std::runtime_error 如果打开失败
     */
    SafeFile(const std::string& path, const char* mode) 
        : fp_(std::fopen(path.c_str(), mode)), path_(path) 
    {
        if (!fp_) {
            throw std::runtime_error("Cannot open '" + path + "': " + std::strerror(errno));
        }
    }
    
    /**
     * @brief 析构函数：自动关闭文件
     */
    ~SafeFile() {
        if (fp_) {
            if (std::fclose(fp_) != 0) {
                std::fprintf(stderr, "Warning: fclose failed for '%s': %s\n", 
                            path_.c_str(), std::strerror(errno));
            }
        }
    }
    
    /// @brief 禁止拷贝构造
    SafeFile(const SafeFile&) = delete;
    /// @brief 禁止拷贝赋值
    SafeFile& operator=(const SafeFile&) = delete;
    
    /**
     * @brief 移动构造函数
     * @param[in,out] other 源对象
     */
    SafeFile(SafeFile&& other) noexcept 
        : fp_(other.fp_), path_(std::move(other.path_)) 
    {
        other.fp_ = nullptr;
    }
    
    /**
     * @brief 获取原始文件指针
     * @return FILE* 指针
     */
    FILE* get() const { return fp_; }
    
    /**
     * @brief 输出纯字符串（无格式化）
     * @param[in] str 要输出的字符串
     * @throws std::runtime_error 如果写入失败
     */
    void puts(const char* str) {
        if (std::fputs(str, fp_) == EOF) {
            throw std::runtime_error("Write failed for '" + path_ + "': " + std::strerror(errno));
        }
    }
    
    /**
     * @brief 格式化写入（至少需要一个参数）
     * @tparam T 第一个参数类型
     * @tparam Args 其余参数类型
     * @param[in] fmt 格式字符串
     * @param[in] first 第一个参数
     * @param[in] rest 其余参数
     * @throws std::runtime_error 如果写入失败
     */
    template<typename T, typename... Args>
    void printf(const char* fmt, T first, Args... rest) {
        if (std::fprintf(fp_, fmt, first, rest...) < 0) {
            throw std::runtime_error("Write failed for '" + path_ + "': " + std::strerror(errno));
        }
    }
    
    /**
     * @brief 二进制写入
     * @param[in] data  数据指针
     * @param[in] size  单个元素大小
     * @param[in] count 元素数量
     * @throws std::runtime_error 如果写入失败
     */
    void write(const void* data, size_t size, size_t count) {
        if (std::fwrite(data, size, count, fp_) != count) {
            throw std::runtime_error("Write failed for '" + path_ + "': " + std::strerror(errno));
        }
    }
    
    /**
     * @brief 二进制读取
     * @param[out] data  数据指针
     * @param[in]  size  单个元素大小
     * @param[in]  count 元素数量
     * @throws std::runtime_error 如果读取失败
     */
    void read(void* data, size_t size, size_t count) {
        if (std::fread(data, size, count, fp_) != count) {
            throw std::runtime_error("Read failed for '" + path_ + "': " + std::strerror(errno));
        }
    }

private:
    FILE* fp_;       ///< 文件指针
    std::string path_;  ///< 文件路径（用于错误信息）
};