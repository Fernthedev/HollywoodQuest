#include "limiter.hpp"

#include <algorithm>
#include <cstdlib>

void Hollywood::SimpleLimiter::init(int channels, int smoothness) {
    this->channels = channels;
    if (smoothness > 0)
        decrease = 1 / (float) smoothness;
    else
        decrease = std::numeric_limits<float>::max();
    clear();
}

float Hollywood::SimpleLimiter::process(float data) {
    float gain = std::abs(data);
    if (gain > max)
        max = gain;
    else if (channel == channels - 1)
        max = std::max(max - decrease, (float) 1);
    channel = ++channel % channels;
    return data / max;
}

void Hollywood::SimpleLimiter::clear() {
    channel = 0;
    max = 1;
}
