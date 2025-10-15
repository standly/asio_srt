#!/bin/bash

# 编译取消功能测试

set -e

echo "编译 test_cancellation.cpp..."

g++ -std=c++20 \
    -I/usr/include \
    -o test_cancellation \
    test_cancellation.cpp \
    -pthread \
    -lsrt

echo "编译完成！"
echo ""
echo "运行测试："
echo "./test_cancellation"

