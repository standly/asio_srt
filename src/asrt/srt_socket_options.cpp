// srt_socket_options_v2.cpp - 基于 SRT 官方实现的增强选项管理
#include "srt_socket_options.hpp"
#include <algorithm>
#include <cstring>

namespace asrt {

// 全局常量
const std::set<std::string> true_names = { "1", "yes", "on", "true" };
const std::set<std::string> false_names = { "0", "no", "off", "false" };

const std::map<std::string, int> enummap_transtype = {
    { "live", SRTT_LIVE },
    { "file", SRTT_FILE }
};

// 静态成员初始化
std::vector<SocketOption> SrtSocketOptions::pre_options_;
std::vector<SocketOption> SrtSocketOptions::post_options_;
std::map<std::string, const SocketOption*> SrtSocketOptions::all_options_;
bool SrtSocketOptions::initialized_ = false;

// SocketOption 成员函数实现
template<>
bool SocketOption::extract<SocketOption::STRING>(const std::string& value, OptionValue& o) const {
    o.s = value;
    o.value = o.s.data();
    o.size = o.s.size();
    return true;
}

template<>
bool SocketOption::extract<SocketOption::INT>(const std::string& value, OptionValue& o) const {
    try {
        o.i = std::stoi(value, 0, 0);
        o.value = &o.i;
        o.size = sizeof(o.i);
        return true;
    } catch (...) {
        return false;
    }
}

template<>
bool SocketOption::extract<SocketOption::INT64>(const std::string& value, OptionValue& o) const {
    try {
        o.l = std::stoll(value);
        o.value = &o.l;
        o.size = sizeof(o.l);
        return true;
    } catch (...) {
        return false;
    }
}

template<>
bool SocketOption::extract<SocketOption::BOOL>(const std::string& value, OptionValue& o) const {
    bool val;
    if (false_names.count(value))
        val = false;
    else if (true_names.count(value))
        val = true;
    else
        return false;
    
    o.b = val;
    o.value = &o.b;
    o.size = sizeof(o.b);
    return true;
}

template<>
bool SocketOption::extract<SocketOption::ENUM>(const std::string& value, OptionValue& o) const {
    if (valmap) {
        auto p = valmap->find(value);
        if (p != valmap->end()) {
            o.i = p->second;
            o.value = &o.i;
            o.size = sizeof(o.i);
            return true;
        }
    }
    
    // 尝试解析为整数
    try {
        o.i = std::stoi(value, 0, 0);
        o.value = &o.i;
        o.size = sizeof(o.i);
        return true;
    } catch (...) {
        return false;
    }
}

bool SocketOption::applyt(SRTSOCKET socket, Type t, const std::string& value) const {
    OptionValue o;
    int result = -1;
    
    switch (t) {
        case STRING:
            if (extract<STRING>(value, o))
                result = srt_setsockopt(socket, 0, symbol, o.value, o.size);
            break;
        case INT:
            if (extract<INT>(value, o))
                result = srt_setsockopt(socket, 0, symbol, o.value, o.size);
            break;
        case INT64:
            if (extract<INT64>(value, o))
                result = srt_setsockopt(socket, 0, symbol, o.value, o.size);
            break;
        case BOOL:
            if (extract<BOOL>(value, o)) {
                int bool_val = o.b ? 1 : 0;
                result = srt_setsockopt(socket, 0, symbol, &bool_val, sizeof(bool_val));
            }
            break;
        case ENUM:
            if (extract<ENUM>(value, o))
                result = srt_setsockopt(socket, 0, symbol, o.value, o.size);
            break;
    }
    
    return result != -1;
}

bool SocketOption::apply(SRTSOCKET socket, const std::string& value) const {
    return applyt(socket, type, value);
}

// SrtSocketOptions 实现
void SrtSocketOptions::init_option_registry() {
    if (initialized_) return;
    
    // PRE 选项（包括 pre-bind 和 pre）
    pre_options_ = {
        // 传输类型和模式
        { "transtype", SRTO_TRANSTYPE, SocketOption::PRE, SocketOption::ENUM, &enummap_transtype },
        { "messageapi", SRTO_MESSAGEAPI, SocketOption::PRE, SocketOption::BOOL, nullptr },
        { "tsbpdmode", SRTO_TSBPDMODE, SocketOption::PRE, SocketOption::BOOL, nullptr },
        { "tlpktdrop", SRTO_TLPKTDROP, SocketOption::PRE, SocketOption::BOOL, nullptr },
        { "nakreport", SRTO_NAKREPORT, SocketOption::PRE, SocketOption::BOOL, nullptr },
        
        // 缓冲区和窗口
        { "mss", SRTO_MSS, SocketOption::PRE, SocketOption::INT, nullptr },
        { "fc", SRTO_FC, SocketOption::PRE, SocketOption::INT, nullptr },
        { "sndbuf", SRTO_SNDBUF, SocketOption::PRE, SocketOption::INT, nullptr },
        { "rcvbuf", SRTO_RCVBUF, SocketOption::PRE, SocketOption::INT, nullptr },
        
        // 延迟和超时
        { "latency", SRTO_LATENCY, SocketOption::PRE, SocketOption::INT, nullptr },
        { "rcvlatency", SRTO_RCVLATENCY, SocketOption::PRE, SocketOption::INT, nullptr },
        { "peerlatency", SRTO_PEERLATENCY, SocketOption::PRE, SocketOption::INT, nullptr },
        { "conntimeo", SRTO_CONNTIMEO, SocketOption::PRE, SocketOption::INT, nullptr },
        { "peeridletimeo", SRTO_PEERIDLETIMEO, SocketOption::PRE, SocketOption::INT, nullptr },
        
        // 加密
        { "pbkeylen", SRTO_PBKEYLEN, SocketOption::PRE, SocketOption::INT, nullptr },
        { "passphrase", SRTO_PASSPHRASE, SocketOption::PRE, SocketOption::STRING, nullptr },
        { "kmrefreshrate", SRTO_KMREFRESHRATE, SocketOption::PRE, SocketOption::INT, nullptr },
        { "kmpreannounce", SRTO_KMPREANNOUNCE, SocketOption::PRE, SocketOption::INT, nullptr },
        { "enforcedencryption", SRTO_ENFORCEDENCRYPTION, SocketOption::PRE, SocketOption::BOOL, nullptr },
        
        // 网络选项
        { "ipttl", SRTO_IPTTL, SocketOption::PRE, SocketOption::INT, nullptr },
        { "iptos", SRTO_IPTOS, SocketOption::PRE, SocketOption::INT, nullptr },
        { "ipv6only", SRTO_IPV6ONLY, SocketOption::PRE, SocketOption::INT, nullptr },
        
        // 其他 PRE 选项
        { "minversion", SRTO_MINVERSION, SocketOption::PRE, SocketOption::INT, nullptr },
        { "streamid", SRTO_STREAMID, SocketOption::PRE, SocketOption::STRING, nullptr },
        { "congestion", SRTO_CONGESTION, SocketOption::PRE, SocketOption::STRING, nullptr },
        { "payloadsize", SRTO_PAYLOADSIZE, SocketOption::PRE, SocketOption::INT, nullptr },
        { "packetfilter", SRTO_PACKETFILTER, SocketOption::PRE, SocketOption::STRING, nullptr },
        { "retransmitalgo", SRTO_RETRANSMITALGO, SocketOption::PRE, SocketOption::INT, nullptr },
        
#ifdef SRT_ENABLE_BINDTODEVICE
        { "bindtodevice", SRTO_BINDTODEVICE, SocketOption::PRE, SocketOption::STRING, nullptr },
#endif
#if ENABLE_BONDING
        { "groupconnect", SRTO_GROUPCONNECT, SocketOption::PRE, SocketOption::INT, nullptr },
        { "groupminstabletimeo", SRTO_GROUPMINSTABLETIMEO, SocketOption::PRE, SocketOption::INT, nullptr },
#endif
#ifdef ENABLE_AEAD_API_PREVIEW
        { "cryptomode", SRTO_CRYPTOMODE, SocketOption::PRE, SocketOption::INT, nullptr },
#endif
    };
    
    // POST 选项
    post_options_ = {
        // 带宽控制
        { "maxbw", SRTO_MAXBW, SocketOption::POST, SocketOption::INT64, nullptr },
        { "inputbw", SRTO_INPUTBW, SocketOption::POST, SocketOption::INT64, nullptr },
        { "mininputbw", SRTO_MININPUTBW, SocketOption::POST, SocketOption::INT64, nullptr },
        { "oheadbw", SRTO_OHEADBW, SocketOption::POST, SocketOption::INT, nullptr },
        
        // 其他 POST 选项
        { "snddropdelay", SRTO_SNDDROPDELAY, SocketOption::POST, SocketOption::INT, nullptr },
        { "drifttracer", SRTO_DRIFTTRACER, SocketOption::POST, SocketOption::BOOL, nullptr },
        { "lossmaxttl", SRTO_LOSSMAXTTL, SocketOption::POST, SocketOption::INT, nullptr },
        
#ifdef ENABLE_MAXREXMITBW
        { "maxrexmitbw", SRTO_MAXREXMITBW, SocketOption::POST, SocketOption::INT64, nullptr },
#endif
    };
    
    // 构建所有选项的映射
    all_options_.clear();
    for (const auto& opt : pre_options_) {
        all_options_[opt.name] = &opt;
    }
    for (const auto& opt : post_options_) {
        all_options_[opt.name] = &opt;
    }
    
    initialized_ = true;
}

const std::vector<SocketOption>& SrtSocketOptions::get_pre_options() {
    init_option_registry();
    return pre_options_;
}

const std::vector<SocketOption>& SrtSocketOptions::get_post_options() {
    init_option_registry();
    return post_options_;
}

const std::map<std::string, const SocketOption*>& SrtSocketOptions::get_all_options() {
    init_option_registry();
    return all_options_;
}

SrtSocketOptions::SrtSocketOptions(const std::map<std::string, std::string>& options) {
    set_options(options);
}

bool SrtSocketOptions::set_option(const std::string& option_str) {
    size_t pos = option_str.find('=');
    if (pos == std::string::npos) {
        ASRT_LOG_ERROR("Invalid option format (expected key=value): {}", option_str);
        return false;
    }
    
    std::string key = option_str.substr(0, pos);
    std::string value = option_str.substr(pos + 1);
    
    // 去除空格
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // 特殊处理 linger 选项
    if (key == "linger") {
        try {
            linger_val_ = std::stoi(value);
            linger_set_ = true;
            options_[key] = value;
            ASRT_LOG_DEBUG("Set linger option: {}", value);
            return true;
        } catch (...) {
            ASRT_LOG_ERROR("Invalid linger value: {}", value);
            return false;
        }
    }
    
    // 检查选项是否有效
    const auto& all_opts = get_all_options();
    if (all_opts.find(key) == all_opts.end()) {
        ASRT_LOG_WARNING("Unknown SRT option: {}", key);
        // 仍然保存，可能是新版本的选项
    }
    
    options_[key] = value;
    
    ASRT_LOG_DEBUG("Set option: {} = {}", key, value);
    
    return true;
}

bool SrtSocketOptions::set_options(const std::map<std::string, std::string>& options) {
    bool all_success = true;
    for (const auto& [key, value] : options) {
        std::string option_str = key + "=" + value;
        if (!set_option(option_str)) {
            all_success = false;
        }
    }
    return all_success;
}

std::vector<std::string> SrtSocketOptions::apply_pre(SRTSOCKET sock) const {
    std::vector<std::string> failures;
    
    // 特殊处理 linger
    if (linger_set_) {
        linger lin;
        lin.l_linger = linger_val_;
        lin.l_onoff = lin.l_linger > 0 ? 1 : 0;
        if (srt_setsockopt(sock, 0, SRTO_LINGER, &lin, sizeof(linger)) == -1) {
            failures.push_back("linger");
            ASRT_LOG_ERROR("Failed to set linger option");
        }
    }
    
    // 应用 PRE 选项
    const auto& pre_opts = get_pre_options();
    for (const auto& opt : pre_opts) {
        auto it = options_.find(opt.name);
        if (it != options_.end()) {
            if (!opt.apply(sock, it->second)) {
                failures.push_back(opt.name);
                ASRT_LOG_ERROR("Failed to set option {} = {}", opt.name, it->second);
            } else {
                ASRT_LOG_DEBUG("Applied pre option: {} = {}", opt.name, it->second);
            }
        }
    }
    
    return failures;
}

std::vector<std::string> SrtSocketOptions::apply_post(SRTSOCKET sock) const {
    std::vector<std::string> failures;
    
    // 应用 POST 选项
    const auto& post_opts = get_post_options();
    for (const auto& opt : post_opts) {
        auto it = options_.find(opt.name);
        if (it != options_.end()) {
            if (!opt.apply(sock, it->second)) {
                failures.push_back(opt.name);
                ASRT_LOG_ERROR("Failed to set option {} = {}", opt.name, it->second);
            } else {
                ASRT_LOG_DEBUG("Applied post option: {} = {}", opt.name, it->second);
            }
        }
    }
    
    // 同时处理 rcvsyn 和 sndsyn（它们既不在 PRE 也不在 POST 列表中）
    const std::vector<std::pair<std::string, SRT_SOCKOPT>> runtime_opts = {
        {"rcvsyn", SRTO_RCVSYN},
        {"sndsyn", SRTO_SNDSYN},
        {"rcvtimeo", SRTO_RCVTIMEO},
        {"sndtimeo", SRTO_SNDTIMEO}
    };
    
    for (const auto& [name, symbol] : runtime_opts) {
        auto it = options_.find(name);
        if (it != options_.end()) {
            SocketOption opt{name, symbol, SocketOption::POST, SocketOption::BOOL, nullptr};
            if (name.find("timeo") != std::string::npos) {
                opt.type = SocketOption::INT;
            }
            
            if (!opt.apply(sock, it->second)) {
                failures.push_back(name);
                ASRT_LOG_ERROR("Failed to set {} option", name);
            } else {
                ASRT_LOG_DEBUG("Applied runtime option: {} = {}", name, it->second);
            }
        }
    }
    
    return failures;
}

} // namespace asrt
