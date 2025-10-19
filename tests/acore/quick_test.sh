#!/bin/bash

# 快速测试脚本 - 验证所有异步组件是否可以编译

cd /home/ubuntu/codes/cpp/asio_srt/tests/acore

CXXFLAGS="-std=c++20 -I../../3rdparty/asio-1.36.0/include -I../../src -lpthread"

echo "======================================"
echo "编译测试 - ACORE 异步组件"
echo "======================================"
echo ""

# 测试列表
tests=(
    "test_async_mutex"
    "test_async_periodic_timer"
    "test_async_rate_limiter"
    "test_async_barrier"
    "test_async_auto_reset_event"
    "test_async_latch"
)

passed=0
failed=0

for test in "${tests[@]}"; do
    echo -n "编译 $test ... "
    if g++ $CXXFLAGS ${test}.cpp -o /tmp/${test} 2>/dev/null; then
        echo "✅ 成功"
        ((passed++))
    else
        echo "❌ 失败"
        ((failed++))
    fi
done

echo ""
echo "======================================"
echo "编译结果汇总"
echo "======================================"
echo "✅ 成功: $passed"
echo "❌ 失败: $failed"
echo ""

if [ $failed -eq 0 ]; then
    echo "🎉 所有组件编译成功！"
    echo ""
    
    # 检查是否有参数来运行所有测试
    if [ "$1" == "--run-all" ]; then
        echo "运行所有测试..."
        echo ""
        
        for test in "${tests[@]}"; do
            echo ""
            echo "======================================"
            echo "运行: $test"
            echo "======================================"
            /tmp/${test}
        done
    else
        echo "运行快速测试 (只运行 async_mutex，其他可能耗时较长)..."
        echo "提示: 运行 './quick_test.sh --run-all' 来执行所有测试"
        echo ""
        /tmp/test_async_mutex
    fi
else
    echo "⚠️  有组件编译失败，请检查错误"
fi

