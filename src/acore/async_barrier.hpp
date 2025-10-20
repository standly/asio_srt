//
// Async Barrier - 异步同步屏障
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <atomic>
#include "handler_traits.hpp"

namespace acore {

/**
 * @brief 异步同步屏障
 * 
 * 特性：
 * 1. 多个协程在同步点等待彼此
 * 2. 所有参与者到达后同时释放
 * 3. 可重用（支持多轮同步）
 * 4. 线程安全（使用 strand）
 * 
 * 适用场景：
 * - 多阶段算法同步
 * - 音视频流同步
 * - 并行测试同步启动
 * - 分布式协调
 * 
 * 使用示例：
 * @code
 * auto barrier = std::make_shared<async_barrier>(io_ctx, 3);
 * 
 * // 3 个worker协程
 * for (int i = 0; i < 3; ++i) {
 *     co_spawn(io_ctx, [barrier, i]() -> awaitable<void> {
 *         while (true) {
 *             // 阶段 1：准备
 *             prepare_data(i);
 *             co_await barrier->async_arrive_and_wait(use_awaitable);
 *             
 *             // 阶段 2：处理（所有worker都准备好了）
 *             process_data(i);
 *             co_await barrier->async_arrive_and_wait(use_awaitable);
 *             
 *             // 阶段 3：输出（所有worker都处理完了）
 *             output_results(i);
 *             co_await barrier->async_arrive_and_wait(use_awaitable);
 *         }
 *     }, detached);
 * }
 * @endcode
 */
class async_barrier : public std::enable_shared_from_this<async_barrier> {
private:
    using executor_type = asio::any_io_executor;
    
    asio::strand<executor_type> strand_;
    size_t num_participants_;        // 参与者数量
    size_t arrived_count_{0};        // 已到达数量（仅在 strand 内访问）
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;  // 等待队列
    std::atomic<uint64_t> generation_{0};  // 当前代（用于区分不同轮次）

public:
    // 禁止拷贝和移动
    async_barrier(const async_barrier&) = delete;
    async_barrier& operator=(const async_barrier&) = delete;
    async_barrier(async_barrier&&) = delete;
    async_barrier& operator=(async_barrier&&) = delete;
    
    /**
     * @brief 构造函数（创建内部 strand）- io_context 版本
     * @param io_ctx ASIO io_context
     * @param num_participants 参与者数量（必须 > 0）
     */
    explicit async_barrier(asio::io_context& io_ctx, size_t num_participants)
        : strand_(asio::make_strand(io_ctx.get_executor()))
        , num_participants_(num_participants)
    {
        if (num_participants == 0) {
            throw std::invalid_argument("num_participants must be > 0");
        }
    }
    
    /**
     * @brief 构造函数（创建内部 strand）- executor 版本
     * @param ex executor
     * @param num_participants 参与者数量（必须 > 0）
     */
    explicit async_barrier(executor_type ex, size_t num_participants)
        : strand_(asio::make_strand(ex))
        , num_participants_(num_participants)
    {
        if (num_participants == 0) {
            throw std::invalid_argument("num_participants must be > 0");
        }
    }
    
    /**
     * @brief 构造函数（使用外部 strand）
     * 
     * 使用场景：当 barrier 与其他组件共享 strand 时
     * 
     * @param strand 外部提供的 strand
     * @param num_participants 参与者数量（必须 > 0）
     */
    explicit async_barrier(asio::strand<executor_type> strand, size_t num_participants)
        : strand_(strand)
        , num_participants_(num_participants)
    {
        if (num_participants == 0) {
            throw std::invalid_argument("num_participants must be > 0");
        }
    }
    
    /**
     * @brief 到达屏障并等待所有参与者
     * 
     * 当最后一个参与者到达时，唤醒所有等待者
     * 然后屏障自动重置，可以进行下一轮
     * 
     * 用法：
     * @code
     * co_await barrier->async_arrive_and_wait(use_awaitable);
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_arrive_and_wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    self->arrived_count_++;
                    
                    if (self->arrived_count_ >= self->num_participants_) {
                        // 所有参与者都到达了，唤醒所有等待者
                        
                        // 先唤醒当前到达者
                        std::move(handler)();
                        
                        // 再唤醒所有之前的等待者
                        while (!self->waiters_.empty()) {
                            auto h = std::move(self->waiters_.front());
                            self->waiters_.pop_front();
                            h->invoke();
                        }
                        
                        // 重置屏障，进入下一代
                        self->arrived_count_ = 0;
                        self->generation_.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        // 还有参与者未到达，加入等待队列
                        self->waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(handler)>>(std::move(handler))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 到达屏障但不等待（计数后立即返回）
     * 
     * 用法：
     * @code
     * barrier->arrive();  // 增加计数但不等待
     * do_other_work();
     * co_await barrier->wait(use_awaitable);  // 稍后再等待
     * @endcode
     */
    void arrive() {
        asio::post(strand_, [self = shared_from_this()]() {
            self->arrived_count_++;
            
            if (self->arrived_count_ >= self->num_participants_) {
                // 所有参与者都到达了，唤醒所有等待者
                while (!self->waiters_.empty()) {
                    auto h = std::move(self->waiters_.front());
                    self->waiters_.pop_front();
                    h->invoke();
                }
                
                // 重置屏障
                self->arrived_count_ = 0;
                self->generation_.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    /**
     * @brief 等待所有参与者到达
     * 
     * 需要先调用 arrive() 或其他协程调用 async_arrive_and_wait()
     * 
     * 用法：
     * @code
     * barrier->arrive();  // 先到达
     * // ... 做其他工作 ...
     * co_await barrier->wait(use_awaitable);  // 再等待
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (self->arrived_count_ >= self->num_participants_) {
                        // 已经所有人都到达了，立即完成
                        // （这种情况通常不会发生，因为到达时会重置）
                        std::move(handler)();
                    } else {
                        // 加入等待队列
                        self->waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(handler)>>(std::move(handler))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 到达并减少参与者计数
     * 
     * 当一个参与者永久离开时使用
     * 
     * 注意：这会改变屏障的参与者数量，可能导致当前轮次的屏障提前触发
     */
    void arrive_and_drop() {
        asio::post(strand_, [self = shared_from_this()]() {
            // 关键：必须先减少参与者数，再增加到达数
            // 否则会过早触发屏障
            // 
            // 例如：num_participants=3, arrived_count=1
            // 如果先++arrived_count（变成2），再--num_participants（变成2），
            // 则 arrived_count >= num_participants 为true，错误触发
            //
            // 正确做法：先--num_participants（变成2），再++arrived_count（变成2），
            // 但这时还没有第3个人到达，不应该触发
            //
            // 更正确的做法：同时减少两者，效果相当于只减少 num_participants
            self->num_participants_--;
            self->arrived_count_++;
            
            if (self->arrived_count_ >= self->num_participants_) {
                // 所有参与者都到达了，唤醒所有等待者
                while (!self->waiters_.empty()) {
                    auto h = std::move(self->waiters_.front());
                    self->waiters_.pop_front();
                    h->invoke();
                }
                
                // 重置屏障
                self->arrived_count_ = 0;
                self->generation_.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    /**
     * @brief 获取参与者数量
     */
    size_t get_num_participants() const noexcept {
        return num_participants_;
    }
    
    /**
     * @brief 获取当前代数（轮次）
     * 
     * 每次所有参与者到达后，代数增加 1
     * 可用于调试或统计
     */
    uint64_t get_generation() const noexcept {
        return generation_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief 获取已到达的参与者数量
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_arrived_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->arrived_count_);
                });
            },
            token
        );
    }
    
    /**
     * @brief 获取等待者数量
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_waiting_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->waiters_.size());
                });
            },
            token
        );
    }
    
    /**
     * @brief 重置屏障到初始状态
     * 
     * 警告：这会取消所有等待者！
     */
    void reset() {
        asio::post(strand_, [self = shared_from_this()]() {
            self->arrived_count_ = 0;
            self->waiters_.clear();
        });
    }
    
    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }
};

} // namespace acore

