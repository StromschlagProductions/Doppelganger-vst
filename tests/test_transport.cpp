#include "TestSupport.h"
#include "../Source/AudioTransport.h"
#include <thread>
#include <memory>
void stress()
{
    auto queue = std::make_unique<dop::SpscQueue<dop::LiveFrame, 4>>();
    constexpr uint64_t count = 20000;
    std::thread producer ([&]
    {
        dop::LiveFrame frame;
        for (uint64_t n = 1; n <= count; ++n)
        {
            frame.generation = n; frame.sampleRate = (double) n;
            frame.input.fill ((float) n); frame.output.fill (-(float) n);
            // Retry only in the test harness: production always drops on full.
            while (! queue->push (frame)) std::this_thread::yield();
        }
    });
    dop::LiveFrame frame;
    for (uint64_t n = 1; n <= count; ++n)
    {
        while (! queue->pop (frame)) std::this_thread::yield();
        CHECK (frame.generation == n && frame.sampleRate == (double) n);
        for (size_t i = 0; i < frame.input.size(); ++i) CHECK (frame.input[i] == (float) n && frame.output[i] == -(float) n);
        if (n % 100 == 0) std::this_thread::yield();
    }
    producer.join(); CHECK (! queue->pop (frame));
}
int main()
{
    dop::SpscQueue<int, 3> q; int v = 0;
    CHECK (! q.pop (v)); CHECK (q.push (1)); CHECK (q.push (2)); CHECK (! q.push (3));
    CHECK (q.pop (v) && v == 1); CHECK (q.push (3)); CHECK (q.pop (v) && v == 2); CHECK (q.pop (v) && v == 3);
    std::thread second (stress); stress(); second.join();
    std::puts ("Concurrent independent SPSC instances passed");
}
