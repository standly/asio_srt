// 检查 SRT 默认的 messageapi 设置
#include <srt/srt.h>
#include <iostream>

int main() {
    // 初始化 SRT
    srt_startup();
    
    // 创建一个 socket
    SRTSOCKET sock = srt_create_socket();
    if (sock == SRT_INVALID_SOCK) {
        std::cerr << "Failed to create socket: " << srt_getlasterror_str() << std::endl;
        srt_cleanup();
        return 1;
    }
    
    // 检查默认的 messageapi 值
    bool messageapi = false;
    int opt_len = sizeof(messageapi);
    
    if (srt_getsockopt(sock, 0, SRTO_MESSAGEAPI, &messageapi, &opt_len) == SRT_ERROR) {
        std::cerr << "Failed to get SRTO_MESSAGEAPI: " << srt_getlasterror_str() << std::endl;
    } else {
        std::cout << "Default SRTO_MESSAGEAPI value: " << (messageapi ? "true" : "false") << std::endl;
    }
    
    // 检查默认的 payloadsize
    int payloadsize = 0;
    opt_len = sizeof(payloadsize);
    
    if (srt_getsockopt(sock, 0, SRTO_PAYLOADSIZE, &payloadsize, &opt_len) == SRT_ERROR) {
        std::cerr << "Failed to get SRTO_PAYLOADSIZE: " << srt_getlasterror_str() << std::endl;
    } else {
        std::cout << "Default SRTO_PAYLOADSIZE value: " << payloadsize << std::endl;
    }
    
    // 检查默认的 transtype
    int transtype = 0;
    opt_len = sizeof(transtype);
    
    if (srt_getsockopt(sock, 0, SRTO_TRANSTYPE, &transtype, &opt_len) == SRT_ERROR) {
        std::cerr << "Failed to get SRTO_TRANSTYPE: " << srt_getlasterror_str() << std::endl;
    } else {
        std::cout << "Default SRTO_TRANSTYPE value: " << transtype;
        switch (transtype) {
            case SRTT_LIVE: std::cout << " (LIVE)"; break;
            case SRTT_FILE: std::cout << " (FILE)"; break;
            default: std::cout << " (UNKNOWN)"; break;
        }
        std::cout << std::endl;
    }
    
    // 关闭 socket
    srt_close(sock);
    
    // 清理
    srt_cleanup();
    
    return 0;
}
