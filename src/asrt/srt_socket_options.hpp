// srt_socket_options_v2.hpp - 基于 SRT 官方实现的增强选项管理
#pragma once

#include <srt/srt.h>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include "srt_log.hpp"

namespace asrt {

// 选项值的变体类型
struct OptionValue {
    std::string s;
    union {
        int i;
        int64_t l;
        bool b;
    };
    
    const void* value = nullptr;
    size_t size = 0;
};

// 单个 socket 选项定义
struct SocketOption {
    enum Type { STRING = 0, INT, INT64, BOOL, ENUM };
    enum Binding { PRE = 0, POST };  // PRE = pre-bind/pre-connect, POST = any time
    
    std::string name;
    SRT_SOCKOPT symbol;
    Binding binding;
    Type type;
    const std::map<std::string, int>* valmap;  // 枚举值映射
    
    // 应用选项到 socket
    bool apply(SRTSOCKET socket, const std::string& value) const;
    
    // 提取不同类型的值
    template<Type T>
    bool extract(const std::string& value, OptionValue& val) const;
    
private:
    bool applyt(SRTSOCKET socket, Type t, const std::string& value) const;
};

// 选项管理器
class SrtSocketOptions {
public:
    // 从官方选项中识别的 PRE 选项（相当于 pre-bind + pre）
    static const std::vector<SocketOption>& get_pre_options();
    
    // POST 选项
    static const std::vector<SocketOption>& get_post_options();
    
    // 所有选项的映射（用于快速查找）
    static const std::map<std::string, const SocketOption*>& get_all_options();
    
public:
    SrtSocketOptions() = default;
    explicit SrtSocketOptions(const std::map<std::string, std::string>& options);
    
    // 设置选项
    bool set_option(const std::string& option_str);
    bool set_options(const std::map<std::string, std::string>& options);
    
    // 应用选项（返回失败的选项列表）
    std::vector<std::string> apply_pre(SRTSOCKET sock) const;
    std::vector<std::string> apply_post(SRTSOCKET sock) const;
    
    // 获取选项值（用于调试）
    const std::map<std::string, std::string>& get_options() const { return options_; }
    
    // 特殊处理的选项
    bool has_linger() const { return linger_set_; }
    int get_linger() const { return linger_val_; }
    
private:
    static void init_option_registry();
    
private:
    std::map<std::string, std::string> options_;
    bool linger_set_ = false;
    int linger_val_ = 0;
    
    // 静态选项注册表
    static std::vector<SocketOption> pre_options_;
    static std::vector<SocketOption> post_options_;
    static std::map<std::string, const SocketOption*> all_options_;
    static bool initialized_;
};

// 辅助函数
extern const std::set<std::string> true_names;
extern const std::set<std::string> false_names;

// 枚举映射
extern const std::map<std::string, int> enummap_transtype;

} // namespace asrt
