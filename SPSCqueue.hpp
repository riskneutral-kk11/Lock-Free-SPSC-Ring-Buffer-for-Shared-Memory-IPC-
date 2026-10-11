#include <atomic> 
#include <iostream>
#include "types.hpp"

template <size_t Capacity>
class SPSC_Queue {
static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "SPSC_Queue capacity must be a power of 2");
private: 
    static constexpr std::size_t mask_ = Capacity - 1;

    alignas(64) std::atomic<std::size_t> pushPointer_ {};
    std::size_t popPointerLocal_ {}; // Rigtorp Optimization

    alignas(64) std::atomic<std::size_t> popPointer_ {};
    std::size_t pushPointerLocal_ {}; // Rigtorp Optimization

    alignas(64) ring::Tick slots_[Capacity];

public: 
    auto capacity() const { return Capacity; }
    auto size(std::size_t pushPtr, std::size_t popPtr) const { return pushPtr - popPtr; }
    auto full(std::size_t pushPtr, std::size_t popPtr) const { return (size(pushPtr,popPtr) == Capacity); }
    auto empty(std::size_t pushPtr, std::size_t popPtr) const { return (size(pushPtr,popPtr) == 0); }

    auto push(const ring::Tick& tick){
        auto pushPointer = pushPointer_.load(std::memory_order_relaxed);
        if(full(pushPointer,popPointerLocal_)){
            popPointerLocal_ = popPointer_.load(std::memory_order_acquire);
            if(full(pushPointer,popPointerLocal_)){
                return false;
            }
        }
        slots_[pushPointer & mask_] = tick;
        pushPointer_.store(pushPointer + 1,std::memory_order_release);
        return true;
    }

    auto pop(ring::Tick& tick){
        auto popPointer = popPointer_.load(std::memory_order_relaxed);
        if(empty(pushPointerLocal_,popPointer)){
            pushPointerLocal_ = pushPointer_.load(std::memory_order_acquire);
            if(empty(pushPointerLocal_,popPointer)){
                return false;
            }
        }
        tick = slots_[popPointer & mask_];
        popPointer_.store(popPointer + 1,std::memory_order_release);
        return true;
    }
};