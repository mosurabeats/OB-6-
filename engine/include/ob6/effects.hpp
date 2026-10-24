// Stereo chorus and ping-pong delay. Buffers are allocated once in
// prepare(); process() is allocation-free.
#pragma once

#include <vector>

#include "dsp.hpp"

namespace ob6 {

class DelayLine {
public:
    void prepare(size_t maxSamples) {
        buf_.assign(maxSamples + 4, 0.0f);
        pos_ = 0;
    }
    void clear() { std::fill(buf_.begin(), buf_.end(), 0.0f); }
    void write(float x) {
        buf_[pos_] = x;
        if (++pos_ == buf_.size()) pos_ = 0;
    }
    // Linear-interpolated read `delay` samples behind the last write.
    float read(float delay) const {
        const float maxDelay = static_cast<float>(buf_.size() - 3);
        delay = clampf(delay, 1.0f, maxDelay);
        const size_t whole = static_cast<size_t>(delay);
        const float frac = delay - static_cast<float>(whole);
        const size_t n = buf_.size();
        const size_t i0 = (pos_ + n - whole) % n;
        const size_t i1 = (i0 + n - 1) % n;
        return buf_[i0] + (buf_[i1] - buf_[i0]) * frac;
    }

private:
    std::vector<float> buf_;
    size_t pos_ = 0;
};

class Chorus {
public:
    void prepare(float fs) {
        fs_ = fs;
        left_.prepare(static_cast<size_t>(0.05f * fs));
        right_.prepare(static_cast<size_t>(0.05f * fs));
        phase_ = 0.0f;
    }
    void clear() {
        left_.clear();
        right_.clear();
    }
    void process(float in, float& outL, float& outR, float rate, float depth) {
        const float hz = 0.1f * std::pow(50.0f, clampf(rate, 0.0f, 1.0f));
        phase_ += hz / fs_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        const float base = 0.007f * fs_;
        const float swing = 0.004f * fs_ * clampf(depth, 0.0f, 1.0f);
        left_.write(in);
        right_.write(in);
        outL = left_.read(base + swing * std::sin(kTwoPi * phase_));
        outR = right_.read(base + swing * std::cos(kTwoPi * phase_));
    }

private:
    float fs_ = 48000.0f;
    float phase_ = 0.0f;
    DelayLine left_, right_;
};

class PingPongDelay {
public:
    void prepare(float fs) {
        fs_ = fs;
        left_.prepare(static_cast<size_t>(1.6f * fs));
        right_.prepare(static_cast<size_t>(1.6f * fs));
        dampL_ = dampR_ = 0.0f;
        smoothedTime_ = -1.0f;
    }
    void clear() {
        left_.clear();
        right_.clear();
        dampL_ = dampR_ = 0.0f;
    }
    void process(float in, float& outL, float& outR, float time, float feedback) {
        const float target = 0.01f * std::pow(150.0f, clampf(time, 0.0f, 1.0f)) * fs_;
        if (smoothedTime_ < 0.0f) smoothedTime_ = target;
        smoothedTime_ += (target - smoothedTime_) * 0.0005f;
        const float wetL = left_.read(smoothedTime_);
        const float wetR = right_.read(smoothedTime_);
        // One-pole low-pass in the loop so repeats darken like an analog delay.
        dampL_ += (wetL - dampL_) * 0.35f;
        dampR_ += (wetR - dampR_) * 0.35f;
        left_.write(in + softClip(dampR_ * feedback));
        right_.write(softClip(dampL_ * feedback));
        outL = wetL;
        outR = wetR;
    }

private:
    float fs_ = 48000.0f;
    float smoothedTime_ = -1.0f;
    float dampL_ = 0.0f, dampR_ = 0.0f;
    DelayLine left_, right_;
};

}  // namespace ob6
