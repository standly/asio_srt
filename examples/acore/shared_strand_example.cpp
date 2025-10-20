//
// 共享 Strand 的正确使用示例
//
// 演示：
// 1. ✅ 安全的使用模式（协程、纯回调）
// 2. ❌ 危险的使用模式（会被注释掉）
// 3. 📊 性能对比（共享 vs 独立）
//

#include "acore/async_mutex.hpp"
#include "acore/async_queue.hpp"
#include "acore/async_rate_limiter.hpp"
#include <asio.hpp>
#include <iostream>
#include <chrono>
#include <vector>

using namespace acore;
using namespace std::chrono_literals;

// ================== 示例 1: 安全的协程使用 ==================

asio::awaitable<void> safe_coroutine_example() {
    auto& io_context = co_await asio::this_coro::executor;
    
    // 创建共享 strand
    auto shared_strand = asio::make_strand(io_context);
    
    // 多个相关组件共享 strand
    auto mutex = std::make_shared<async_mutex>(shared_strand.get_inner_executor());
    auto queue = std::make_shared<async_queue<int>>(io_context);  // 内部会创建 strand
    
    std::cout << "=== 示例 1: 协程中使用共享 Strand ===" << std::endl;
    
    // ✅ 安全：co_await 会暂停协程，释放 strand
    for (int i = 0; i < 3; ++i) {
        auto guard = co_await mutex->async_lock(asio::use_awaitable);
        std::cout << "  [协程] 获得锁 #" << i << std::endl;
        
        // 模拟一些工作
        co_await asio::steady_timer(io_context, 100ms).async_wait(asio::use_awaitable);
        
        std::cout << "  [协程] 释放锁 #" << i << std::endl;
    }  // guard 析构，自动释放锁
    
    std::cout << "  ✅ 协程示例完成\n" << std::endl;
}

// ================== 示例 2: 安全的回调使用 ==================

void safe_callback_example(asio::io_context& io_context) {
    std::cout << "=== 示例 2: 纯回调中使用共享 Strand ===" << std::endl;
    
    auto shared_strand = asio::make_strand(io_context);
    auto mutex = std::make_shared<async_mutex>(shared_strand.get_inner_executor());
    
    // ✅ 安全：链式异步回调
    std::function<void(int)> process_iteration;
    process_iteration = [&, mutex, count = std::make_shared<int>(0)](int iteration) {
        if (iteration >= 3) {
            std::cout << "  ✅ 回调示例完成\n" << std::endl;
            return;
        }
        
        // 异步锁定
        mutex->lock([&, mutex, iteration, count]() {
            std::cout << "  [回调] 获得锁 #" << iteration << std::endl;
            
            // 模拟异步工作
            auto timer = std::make_shared<asio::steady_timer>(io_context, 100ms);
            timer->async_wait([&, mutex, timer, iteration, count](auto ec) {
                std::cout << "  [回调] 释放锁 #" << iteration << std::endl;
                mutex->unlock();
                
                // 继续下一次迭代
                process_iteration(iteration + 1);
            });
        });
    };
    
    process_iteration(0);
}

// ================== 示例 3: 模块化设计（推荐） ==================

struct NetworkModule {
    asio::strand<asio::any_io_executor> strand_;
    std::shared_ptr<async_queue<std::string>> queue_;
    std::shared_ptr<async_mutex> mutex_;
    
    explicit NetworkModule(asio::io_context& io)
        : strand_(asio::make_strand(io))
        , queue_(std::make_shared<async_queue<std::string>>(io))
        , mutex_(std::make_shared<async_mutex>(strand_.get_inner_executor()))
    {
        std::cout << "  [模块] 网络模块创建（共享 strand）" << std::endl;
    }
    
    asio::awaitable<void> process_messages() {
        std::cout << "  [模块] 开始处理消息..." << std::endl;
        
        for (int i = 0; i < 3; ++i) {
            auto guard = co_await mutex_->async_lock(asio::use_awaitable);
            
            // ✅ 在同一个 strand 上，零开销访问
            queue_->push("Message " + std::to_string(i));
            
            std::cout << "  [模块] 处理消息 #" << i << std::endl;
            
            co_await asio::steady_timer(strand_, 50ms).async_wait(asio::use_awaitable);
        }
        
        std::cout << "  [模块] ✅ 模块化示例完成\n" << std::endl;
    }
};

asio::awaitable<void> modular_design_example() {
    std::cout << "=== 示例 3: 模块化设计 ===" << std::endl;
    
    auto& io = co_await asio::this_coro::executor;
    auto io_context_ptr = dynamic_cast<asio::io_context*>(&io.context());
    
    if (io_context_ptr) {
        NetworkModule network(*io_context_ptr);
        co_await network.process_messages();
    }
}

// ================== 示例 4: 性能对比 ==================

asio::awaitable<void> performance_comparison() {
    std::cout << "=== 示例 4: 性能对比（共享 vs 独立 Strand） ===" << std::endl;
    
    auto& io = co_await asio::this_coro::executor;
    auto io_context_ptr = dynamic_cast<asio::io_context*>(&io.context());
    if (!io_context_ptr) co_return;
    
    auto& io_context = *io_context_ptr;
    
    const int num_locks = 1000;
    
    // 方案 A: 共享 strand（10 个 mutex）
    {
        auto shared_strand = asio::make_strand(io_context);
        std::vector<std::shared_ptr<async_mutex>> mutexes;
        
        for (int i = 0; i < 10; ++i) {
            mutexes.push_back(
                std::make_shared<async_mutex>(shared_strand.get_inner_executor())
            );
        }
        
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0; i < num_locks; ++i) {
            auto& mutex = mutexes[i % 10];
            auto guard = co_await mutex->async_lock(asio::use_awaitable);
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        std::cout << "  共享 Strand (10 个 mutex): " 
                  << num_locks << " 次锁定耗时 " << ms << "ms" << std::endl;
    }
    
    // 方案 B: 独立 strand（10 个 mutex）
    {
        std::vector<std::shared_ptr<async_mutex>> mutexes;
        
        for (int i = 0; i < 10; ++i) {
            mutexes.push_back(
                std::make_shared<async_mutex>(io_context.get_executor())
            );
        }
        
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0; i < num_locks; ++i) {
            auto& mutex = mutexes[i % 10];
            auto guard = co_await mutex->async_lock(asio::use_awaitable);
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        std::cout << "  独立 Strand (10 个 mutex): " 
                  << num_locks << " 次锁定耗时 " << ms << "ms" << std::endl;
    }
    
    std::cout << "  💡 提示：独立 strand 可以并发，通常更快" << std::endl;
    std::cout << "  💡 但如果组件需要协作，共享 strand 可以减少开销\n" << std::endl;
}

// ================== 示例 5: 危险场景（已注释） ==================

/*
// ❌ 危险示例（不要运行！）：在 post 中同步等待

void DANGEROUS_deadlock_example(asio::io_context& io_context) {
    auto shared_strand = asio::make_strand(io_context);
    auto mutex = std::make_shared<async_mutex>(shared_strand.get_inner_executor());
    
    // ❌ 这会死锁！
    asio::post(shared_strand, [mutex]() {
        // 当前在 strand 上执行
        
        // 尝试锁定（需要 strand 来处理内部操作）
        mutex->lock([](){ 
            // 这个回调永远不会被调用
            // 因为 strand 正在执行外层 post
        });
        
        // 如果这里有同步等待，会永远阻塞
    });
    
    std::cout << "❌ 死锁！程序挂起..." << std::endl;
}
*/

// ================== 主函数 ==================

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════╗
║  共享 Strand 安全使用示例                     ║
║                                              ║
║  本示例演示：                                 ║
║  1. ✅ 协程中使用（推荐）                     ║
║  2. ✅ 纯回调使用                            ║
║  3. ✅ 模块化设计                            ║
║  4. 📊 性能对比                              ║
║                                              ║
║  ⚠️  危险场景已注释，请勿运行                 ║
╚══════════════════════════════════════════════╝
)" << std::endl;

    try {
        asio::io_context io_context;
        
        // 运行所有安全示例
        asio::co_spawn(io_context, 
            []() -> asio::awaitable<void> {
                // 示例 1: 协程
                co_await safe_coroutine_example();
                
                // 示例 3: 模块化
                co_await modular_design_example();
                
                // 示例 4: 性能对比
                co_await performance_comparison();
            },
            asio::detached
        );
        
        // 示例 2: 回调（不能在协程中启动，因为它使用回调）
        safe_callback_example(io_context);
        
        // 运行
        io_context.run();
        
        std::cout << R"(
╔══════════════════════════════════════════════╗
║  ✅ 所有示例运行完成                          ║
║                                              ║
║  关键要点：                                   ║
║  • 协程中使用 co_await - 安全且清晰          ║
║  • 纯异步回调 - 不阻塞 strand                ║
║  • 模块内共享，模块间独立 - 平衡性能          ║
║  • 永远不要在 strand 回调中同步等待          ║
║                                              ║
║  📚 详细文档：                                ║
║  docs/design/SHARED_STRAND_SAFETY.md         ║
╚══════════════════════════════════════════════╝
)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

