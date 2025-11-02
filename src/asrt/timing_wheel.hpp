#ifndef ASRT_TIMING_WHEEL_HPP
#define ASRT_TIMING_WHEEL_HPP

#include <vector>
#include <list>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <memory>
#include <algorithm> // For std::max

namespace asrt {

/**
 * @brief A simple timing wheel for managing large numbers of timers efficiently.
 * 
 * This implementation is NOT thread-safe. All operations (add, remove, tick)
 * must be synchronized externally.
 *
 * @tparam IdType The type of identifier for timers (e.g., socket descriptor).
 *                Must be hashable if used with unordered_map.
 */
template<typename IdType>
class TimingWheel {
public:
    /**
     * @brief Constructs a TimingWheel.
     * @param wheel_size The number of slots in the wheel. A larger size reduces
     *                   the chance of long lists in each slot but uses more memory.
     * @param tick_interval The duration of a single tick, defining the wheel's granularity.
     */
    TimingWheel(size_t wheel_size = 256, std::chrono::milliseconds tick_interval = std::chrono::milliseconds(100))
        : wheel_size_(wheel_size),
          tick_interval_(tick_interval),
          current_slot_(0),
          wheel_(wheel_size),
          last_tick_time_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Adds a new timer or updates an existing one.
     * @param id The unique identifier for the timer.
     * @param timeout The duration from now after which the timer should expire.
     */
    void add(const IdType& id, std::chrono::milliseconds timeout) {
        // If a timer for this ID already exists, remove it first to update it.
        remove(id);

        long long tick_count = (timeout.count() + tick_interval_.count() - 1) / tick_interval_.count();
        tick_count = std::max(1LL, tick_count); // Ensure at least one tick delay

        size_t rounds = static_cast<size_t>(tick_count - 1) / wheel_size_;
        size_t slot = (current_slot_ + static_cast<size_t>(tick_count)) % wheel_size_;

        auto& slot_list = wheel_[slot];
        slot_list.push_front({id, rounds});
        timer_map_[id] = {slot_list.begin(), slot};
    }

    /**
     * @brief Removes a timer, if it exists.
     * @param id The ID of the timer to remove.
     */
    void remove(const IdType& id) {
        auto it = timer_map_.find(id);
        if (it != timer_map_.end()) {
            const auto& pos = it->second;
            wheel_[pos.slot].erase(pos.it);
            timer_map_.erase(it);
        }
    }

    /**
     * @brief Advances the wheel and returns a list of expired timer IDs.
     * Automatically checks elapsed time and performs the appropriate number of ticks.
     * Can be called at any frequency; internally tracks time to ensure correct tick intervals.
     * @return A vector of IDs for timers that have expired.
     */
    std::vector<IdType> tick() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_time_);
        
        // ✅ 只有当经过足够的时间时才 tick
        if (elapsed < tick_interval_) {
            return {};  // 时间不够，返回空
        }
        
        // ✅ 计算需要 tick 的次数（处理累积的时间）
        int ticks_to_process = static_cast<int>(elapsed.count() / tick_interval_.count());
        
        std::vector<IdType> expired_ids;
        
        for (int i = 0; i < ticks_to_process; ++i) {
            current_slot_ = (current_slot_ + 1) % wheel_size_;
            
            auto& slot_list = wheel_[current_slot_];
            
            for (auto it = slot_list.begin(); it != slot_list.end(); ) {
                if (it->rounds == 0) {
                    expired_ids.push_back(it->id);
                    timer_map_.erase(it->id);
                    it = slot_list.erase(it);
                } else {
                    it->rounds--;
                    ++it;
                }
            }
        }
        
        // ✅ 更新最后 tick 时间（只增加实际 tick 的时间，避免漂移）
        last_tick_time_ += std::chrono::milliseconds(ticks_to_process * tick_interval_.count());
        
        return expired_ids;
    }

private:
    struct TimerNode {
        IdType id;
        size_t rounds; // How many full wheel rotations are left
    };

    struct TimerPosition {
        typename std::list<TimerNode>::iterator it;
        size_t slot;
    };

    size_t wheel_size_;
    std::chrono::milliseconds tick_interval_;
    size_t current_slot_;
    
    // The wheel itself. Each slot contains a list of timers.
    std::vector<std::list<TimerNode>> wheel_;
    
    // For O(1) removal, map an ID to its position (iterator and slot index) in the wheel.
    std::unordered_map<IdType, TimerPosition> timer_map_;
    
    // ✅ 记录上次 tick 的时间，用于精确控制 tick 间隔
    std::chrono::steady_clock::time_point last_tick_time_;
};

} // namespace asrt

#endif // ASRT_TIMING_WHEEL_HPP
