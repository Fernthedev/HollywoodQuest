#include "limiter.hpp"

using namespace Hollywood;

void SimpleLimiter::init(int channels, int smoothness) {
    this->channels = channels;
    if (smoothness > 0)
        decrease = 1 / (float) smoothness;
    else
        decrease = std::numeric_limits<float>::max();
    clear();
}

float SimpleLimiter::process(float data) {
    float gain = std::abs(data);
    if (gain > max)
        max = gain;
    else if (channel == channels - 1)
        max = std::max(max - decrease, (float) 1);
    channel = ++channel % channels;
    return data / max;
}

void SimpleLimiter::clear() {
    channel = 0;
    max = 1;
}
