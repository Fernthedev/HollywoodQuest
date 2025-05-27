#pragma once

namespace Hollywood {
    // a simple limiter for no latency
    struct SimpleLimiter {
        SimpleLimiter() = default;

        // smoothness is the amount of samples it takes to increase the gain by 1 after it has been decreased by an above-limit sample
        void init(int channels, int smoothness);
        float process(float data);
        void clear();

       private:
        int channels;
        int channel;
        float max;
        float decrease;
    };
}
