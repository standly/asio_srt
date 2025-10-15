#!/bin/bash

# 编译 waitgroup 测试

set -e

echo "编译 test_waitgroup.cpp..."

g++ -std=c++20 \
    -I../../3rdparty/asio-1.36.0/include \
    -I/usr/include \
    -o test_waitgroup \
    test_waitgroup.cpp \
    -pthread

echo "编译完成！"
echo ""
echo "运行测试："
echo "./test_waitgroup"

