//
// Common handler type erasure utilities
//
#pragma once

#include <memory>
#include <utility>

namespace acore {
namespace detail {

// Type-erased handler for void() signature
struct void_handler_base {
    virtual ~void_handler_base() = default;
    virtual void invoke() = 0;
};

template<typename Handler>
struct void_handler_impl : void_handler_base {
    Handler handler_;
    explicit void_handler_impl(Handler&& h) : handler_(std::move(h)) {}
    void invoke() override { std::move(handler_)(); }
};

// Type-erased handler for void(bool) signature
struct bool_handler_base {
    virtual ~bool_handler_base() = default;
    virtual void invoke(bool result) = 0;
};

template<typename Handler>
struct bool_handler_impl : bool_handler_base {
    Handler handler_;
    explicit bool_handler_impl(Handler&& h) : handler_(std::move(h)) {}
    void invoke(bool result) override { std::move(handler_)(result); }
};

// Cancellable wrapper for void() handlers
struct cancellable_void_handler_base {
    uint64_t id_;
    std::unique_ptr<void_handler_base> inner_;
    
    template<typename Handler>
    cancellable_void_handler_base(uint64_t id, Handler&& h) 
        : id_(id)
        , inner_(std::make_unique<void_handler_impl<Handler>>(std::forward<Handler>(h)))
    {}
    
    virtual ~cancellable_void_handler_base() = default;
    
    void invoke() {
        // 注意：如果已取消（inner_ 为空），不执行
        // 这是合法的：cancel() 后的 invoke() 调用会被静默忽略
        if (inner_) {
            inner_->invoke();
        }
    }
    
    void cancel() {
        inner_.reset();  // 立即释放 handler
    }
};

} // namespace detail
} // namespace acore

