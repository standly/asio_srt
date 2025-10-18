# SRT 选项管理系统 V2

## 概述

新的选项管理系统基于 SRT 官方应用程序的实现方式，提供了更规范和可靠的选项处理。

## 主要改进

### 1. 基于官方选项定义

选项定义与 SRT 官方保持一致：

```cpp
struct SocketOption {
    enum Type { STRING = 0, INT, INT64, BOOL, ENUM };
    enum Binding { PRE = 0, POST };
    
    std::string name;       // 选项名称
    SRT_SOCKOPT symbol;     // SRT 选项符号
    Binding binding;        // 设置时机
    Type type;             // 值类型
    const std::map<std::string, int>* valmap;  // 枚举映射
};
```

### 2. 简化的时机分类

只有两个时机：
- **PRE**: 必须在 bind/connect/listen 之前设置
- **POST**: 可以在任何时候设置

### 3. 完整的选项注册表

系统自动维护所有支持的选项：

#### PRE 选项（35个）
- 传输模式：transtype, messageapi, tsbpdmode, tlpktdrop, nakreport
- 缓冲区：mss, fc, sndbuf, rcvbuf
- 延迟：latency, rcvlatency, peerlatency, conntimeo, peeridletimeo
- 加密：pbkeylen, passphrase, kmrefreshrate, kmpreannounce, enforcedencryption
- 网络：ipttl, iptos, ipv6only
- 其他：minversion, streamid, congestion, payloadsize, packetfilter, retransmitalgo

#### POST 选项（7个）
- 带宽控制：maxbw, inputbw, mininputbw, oheadbw
- 其他：snddropdelay, drifttracer, lossmaxttl

### 4. 智能类型转换

自动识别并转换选项值类型：

```cpp
// BOOL 类型识别
const std::set<std::string> true_names = { "1", "yes", "on", "true" };
const std::set<std::string> false_names = { "0", "no", "off", "false" };

// ENUM 类型映射
const std::map<std::string, int> enummap_transtype = {
    { "live", SRTT_LIVE },
    { "file", SRTT_FILE }
};
```

### 5. 特殊选项处理

#### Linger 选项
```cpp
// linger 需要特殊的结构体
if (key == "linger") {
    linger lin;
    lin.l_linger = std::stoi(value);
    lin.l_onoff = lin.l_linger > 0 ? 1 : 0;
    srt_setsockopt(sock, 0, SRTO_LINGER, &lin, sizeof(linger));
}
```

#### 运行时选项
一些选项如 `rcvsyn`、`sndsyn`、`rcvtimeo`、`sndtimeo` 可以在连接后设置。

## 使用示例

### 基本使用

```cpp
// 创建选项管理器
SrtSocketOptions options;

// 单个选项设置
options.set_option("latency=200");
options.set_option("messageapi=true");
options.set_option("maxbw=-1");

// 批量设置
std::map<std::string, std::string> batch = {
    {"sndbuf", "8388608"},
    {"rcvbuf", "8388608"},
    {"payloadsize", "1316"}
};
options.set_options(batch);
```

### 应用到 Socket

```cpp
// 创建时设置选项
std::map<std::string, std::string> opts = {
    {"latency", "100"},
    {"messageapi", "1"},
    {"conntimeo", "3000"}
};

SrtSocket socket(opts, reactor);

// 连接前自动应用 PRE 选项
co_await socket.async_connect("host", 9000);

// 连接后可以设置 POST 选项
socket.set_option("maxbw=100000000");
```

### 错误处理

```cpp
// apply_pre 和 apply_post 返回失败的选项列表
auto failures = options.apply_pre(sock);
if (!failures.empty()) {
    for (const auto& opt : failures) {
        std::cerr << "Failed to set: " << opt << std::endl;
    }
}
```

## 选项验证

系统会验证：
1. 选项名称是否有效
2. 值类型是否匹配
3. 枚举值是否在允许范围内

无效选项会记录警告但仍会保存（兼容新版本）。

## 最佳实践

1. **使用构造时配置**
   ```cpp
   // 推荐：在构造时一次性设置所有选项
   SrtSocket socket(all_options, reactor);
   
   // 而不是：
   SrtSocket socket(reactor);
   socket.set_option("opt1=val1");
   socket.set_option("opt2=val2");
   ```

2. **检查失败选项**
   ```cpp
   auto failures = socket.apply_pre_options();
   if (!failures.empty()) {
       // 处理失败的选项
   }
   ```

3. **使用类型安全的值**
   ```cpp
   // BOOL 类型
   options["messageapi"] = "true";  // 或 "1", "yes", "on"
   
   // INT64 类型
   options["maxbw"] = "-1";  // 无限制
   options["inputbw"] = "10000000";  // 10 Mbps
   ```

## 与旧版本的区别

1. **移除了 `PRE_BIND` 时机**：现在统一为 `PRE`
2. **移除了 `set(SRT_SOCKOPT, value)` 方法**：使用字符串方式设置
3. **选项定义更加规范**：与官方保持一致
4. **更好的错误报告**：返回失败选项列表而不是简单的 bool

## 性能优化

1. **静态注册表**：选项定义只初始化一次
2. **批量应用**：减少系统调用次数
3. **类型缓存**：避免重复的类型转换

## 扩展性

添加新选项只需在注册表中添加定义：

```cpp
// 在 init_option_registry() 中添加
pre_options_.push_back({
    "new_option",           // 名称
    SRTO_NEW_OPTION,       // 符号
    SocketOption::PRE,     // 时机
    SocketOption::INT,     // 类型
    nullptr                // 枚举映射（如果需要）
});
```
