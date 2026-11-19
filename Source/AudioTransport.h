#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dop
{
// Exactly one producer and one consumer. Slots belong exclusively to the
// producer until release publication, then to the consumer until release return.
// No concurrent reset, overwrite, allocation, lock, or retry loop. A full queue
// drops new data; the audio thread never waits for a GUI/learning consumer.
template <typename T, std::size_t Capacity>
class SpscQueue
{
    static_assert (Capacity > 1 && std::is_trivially_copyable<T>::value);
    static_assert (std::atomic<std::size_t>::is_always_lock_free);
public:
    bool push (const T& value) noexcept
    {
        const auto w = write.load (std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == read.load (std::memory_order_acquire)) return false;
        slots[w] = value;
        write.store (next, std::memory_order_release);
        return true;
    }
    bool pop (T& value) noexcept
    {
        const auto r = read.load (std::memory_order_relaxed);
        if (r == write.load (std::memory_order_acquire)) return false;
        value = slots[r];
        read.store ((r + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    std::array<T, Capacity> slots {};
    alignas (64) std::atomic<std::size_t> write { 0 };
    alignas (64) std::atomic<std::size_t> read { 0 };
};

constexpr int kLiveFft = 4096;
struct LiveFrame
{
    std::array<float, kLiveFft> input {}, output {};
    double sampleRate = 48000.0;
    uint64_t generation = 0;
    int end = 0; // oldest sample in the producer-owned circular frame
};
struct LearnBlock
{
    std::array<float, 512> samples {};
    std::array<float, 512> raw {};            // Mono-Eingang vor jeder Bearbeitung (Autopilot-Mitschnitt)
    std::array<double, 512> loudnessPower {}; // sum of independently K-weighted channel powers
    uint64_t generation = 0;
    int count = 0;
    int channels = 1;
    int clipped = 0; // input samples at or above full scale (before any processing)
};
static_assert (std::atomic<uint64_t>::is_always_lock_free);
}
