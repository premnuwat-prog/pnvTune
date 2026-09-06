#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace prem {
constexpr double pi = 3.14159265358979323846;
struct Settings {
    int key = 0, scale = 0;
    std::array<int, 5> chordRoot{4, 0, 0, 0, 0};
    std::array<int, 5> chordQuality{};
    std::array<bool, 5> chordEnabled{};
    float retuneMs = 12, amount = 100, humanize = 10, mix = 100, outputDb = 0, vocalGateDb = -42;
    bool bypass = false;
};

inline int scaleMask(int key, int scale) {
    constexpr int masks[] = { 0xfff, 0xab5, 0x5ad, 0x9ad, 0x295 };
    const int base = masks[std::clamp(scale, 0, 4)];
    return ((base << key) | (base >> (12 - key))) & 0xfff;
}
inline int chordMask(int root, int quality) {
    constexpr int masks[] = { 0x091, 0x089, 0x491, 0x891, 0x489, 0x085, 0x0a1, 0x049 };
    const int base = masks[std::clamp(quality, 0, 7)];
    return ((base << root) | (base >> (12 - root))) & 0xfff;
}
inline bool allowedNote(int midi, int key, int scale, int borrowedMask = 0) {
    const int pc = (midi % 12 + 12) % 12;
    const int mask = scaleMask(key, scale) | borrowedMask;
    return (mask & (1 << pc)) != 0;
}
inline float nearestNote(float midi, int key, int scale, float previous = -1,
                         int borrowedMask = 0) {
    float best = std::round(midi), distance = 100;
    for (int n = (int)std::floor(midi) - 12; n <= (int)std::ceil(midi) + 12; ++n) {
        if (!allowedNote(n, key, scale, borrowedMask)) continue;
        const float d = std::abs(midi - (float)n);
        if (d < distance) { best = (float)n; distance = d; }
    }
    // A small hysteresis prevents chatter at scale-note boundaries.
    if (previous >= 0 && allowedNote((int)previous, key, scale, borrowedMask)
        && std::abs(midi - previous) < distance + 0.12f) return previous;
    return best;
}

class PitchEngine {
public:
    void prepare(double rate) {
        sampleRate = rate;
        latency = (int)std::ceil(rate * 0.012);
        decimation = std::max(1, (int)std::round(rate / 12000.0));
        analysisRate = rate / decimation;
        maxLag = std::min(250, (int)std::ceil(analysisRate / 90.0));
        minLag = std::max(2, (int)std::floor(analysisRate / 1000.0));
        window = (int)std::ceil(analysisRate * 0.016);
        frameSize = window + maxLag + 2;
        hop = std::max(1, (int)std::round(analysisRate * 0.004));
        for (auto& channel : ring) channel.assign((size_t)std::ceil(rate * 0.08) + 8, 0);
        analysis.assign((size_t)frameSize, 0);
        frame.assign((size_t)frameSize, 0);
        yin.assign((size_t)maxLag + 2, 0);
        smoothFast = 1.0 - std::exp(-1.0 / (rate * 0.005));
        gateAttack = 1.0 - std::exp(-1.0 / (rate * 0.002));
        gateRelease = 1.0 - std::exp(-1.0 / (rate * 0.035));
        reset();
    }
    void reset() {
        for (auto& channel : ring) std::fill(channel.begin(), channel.end(), 0);
        std::fill(analysis.begin(), analysis.end(), 0);
        write = analysisWrite = count = hopCount = decCount = 0;
        decSum = low1 = low2 = 0;
        frequency = confidence = inputMidi = correctionCents = smoothedMidi = 0;
        targetMidi = -1;
        pendingTarget = jumpCandidate = -1; pendingFrames = jumpFrames = missingFrames = midiHistoryCount = 0;
        midiHistory.fill(0);
        phase = 0.5; period = sampleRate / 220.0; ratio = 1;
        active = gateEnvelope = 0; mixSmooth = 1; gainSmooth = 1; vocalGateOpen = false;
    }
    int latencySamples() const { return latency; }
    float frequency = 0, confidence = 0, inputMidi = 0, targetMidi = -1, correctionCents = 0;
    bool vocalGateOpen = false;

    void process(float* const* data, int channels, int samples, const Settings& s) {
        if (ring[0].empty()) return;
        const double correctionAlpha = 1 - std::exp(-1.0 / (sampleRate * std::max(0.001, s.retuneMs * 0.001)));
        const double targetGain = std::pow(10.0, s.outputDb / 20.0);
        const double targetMix = s.bypass ? 0 : s.mix * 0.01;
        for (int i = 0; i < samples; ++i) {
            float input[2] = {data[0][i], channels > 1 ? data[1][i] : data[0][i]};
            for (auto& v : input) if (!std::isfinite(v)) v = 0;
            const double magnitude = std::abs(input[0]);
            gateEnvelope += (magnitude > gateEnvelope ? gateAttack : gateRelease) * (magnitude - gateEnvelope);
            // Two low-pass stages precede decimation; analysis always follows the left/mono vocal.
            const double a = 1 - std::exp(-2 * pi * 1800.0 / sampleRate);
            low1 += a * (input[0] - low1); low2 += a * (low1 - low2);
            decSum += low2;
            if (++decCount == decimation) {
                analysis[(size_t)analysisWrite] = (float)(decSum / decimation);
                analysisWrite = (analysisWrite + 1) % frameSize;
                decCount = 0; decSum = 0;
                count = std::min(count + 1, frameSize);
                if (++hopCount >= hop) {
                    hopCount = 0;
                    if (count == frameSize) detect(s);
                }
            }
            double wantedCents = 0;
            const bool voiced = frequency >= 90 && confidence > 0.8f && targetMidi >= 0;
            if (voiced) {
                wantedCents = (targetMidi - inputMidi) * 100.0;
                const double deadband = s.humanize * 0.2;
                wantedCents = std::copysign(std::max(0.0, std::abs(wantedCents) - deadband), wantedCents);
                wantedCents *= s.amount * 0.01;
            }
            correctionCents += (float)(correctionAlpha * (wantedCents - correctionCents));
            ratio = std::pow(2.0, correctionCents / 1200.0);
            // Two overlapping read heads are separated by one estimated vocal period.
            // This reduces phase cancellation compared with fixed-size delay grains.
            if (voiced) period += smoothFast * (sampleRate / frequency - period);
            period = std::clamp(period, sampleRate / 1000.0, sampleRate / 90.0);
            const double span = 2 * period;
            phase += (1 - ratio) / span;
            phase -= std::floor(phase);
            const double phaseB = phase < 0.5 ? phase + 0.5 : phase - 0.5;
            const double weight = 0.5 - 0.5 * std::cos(2 * pi * phase);
            active += smoothFast * ((voiced && s.amount > 0 ? 1.0 : 0.0) - active);
            mixSmooth += smoothFast * (targetMix - mixSmooth);
            gainSmooth += smoothFast * (targetGain - gainSmooth);
            for (int ch = 0; ch < channels; ++ch) {
                ring[(size_t)ch][(size_t)write] = input[ch];
                const double dry = read(ch, latency);
                const double shifted = weight * read(ch, latency + (phase - 0.5) * span)
                                     + (1 - weight) * read(ch, latency + (phaseB - 0.5) * span);
                const double wet = dry + active * (shifted - dry);
                data[ch][i] = (float)((dry + mixSmooth * (wet - dry)) * gainSmooth);
            }
            write = (write + 1) % (int)ring[0].size();
        }
    }
private:
    double sampleRate = 48000, analysisRate = 12000, smoothFast = 0, gateAttack = 0, gateRelease = 0;
    int latency = 576, decimation = 4, maxLag = 134, minLag = 12, window = 192, frameSize = 328, hop = 48;
    int write = 0, analysisWrite = 0, count = 0, hopCount = 0, decCount = 0;
    double decSum = 0, low1 = 0, low2 = 0, phase = 0.5, period = 218, ratio = 1;
    double active = 0, mixSmooth = 1, gainSmooth = 1, gateEnvelope = 0;
    float smoothedMidi = 0, pendingTarget = -1, jumpCandidate = -1;
    int pendingFrames = 0, jumpFrames = 0, missingFrames = 0, midiHistoryCount = 0;
    std::array<float, 3> midiHistory{};
    std::array<std::vector<float>, 2> ring;
    std::vector<float> analysis, frame, yin;
    double read(int ch, double delay) const {
        const auto& buffer = ring[(size_t)ch];
        double position = write - delay;
        if (position < 0) position += buffer.size();
        int index = (int)position;
        const double t = position - index;
        const int size = (int)buffer.size();
        const double y0 = buffer[(size_t)((index - 1 + size) % size)];
        const double y1 = buffer[(size_t)index];
        const double y2 = buffer[(size_t)((index + 1) % size)];
        const double y3 = buffer[(size_t)((index + 2) % size)];
        const double a0 = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
        const double a1 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double a2 = -0.5 * y0 + 0.5 * y2;
        return ((a0 * t + a1) * t + a2) * t + y1;
    }
    void losePitch() {
        if (++missingFrames >= 3) {
            frequency = confidence = inputMidi = 0; targetMidi = pendingTarget = jumpCandidate = -1;
            pendingFrames = jumpFrames = midiHistoryCount = 0;
        }
    }
    void detect(const Settings& s) {
        double energy = 0;
        for (int j = 0; j < frameSize; ++j) {
            frame[(size_t)j] = analysis[(size_t)((analysisWrite + j) % frameSize)];
            energy += frame[(size_t)j] * frame[(size_t)j];
        }
        const double gateThreshold = std::pow(10.0, s.vocalGateDb / 20.0);
        vocalGateOpen = gateEnvelope >= gateThreshold;
        if (!vocalGateOpen) { losePitch(); return; }
        if (energy / frameSize < 0.00001) { losePitch(); return; }
        double sum = 0;
        yin[0] = 1;
        for (int lag = 1; lag <= maxLag; ++lag) {
            double difference = 0;
            for (int j = 0; j < window; ++j) {
                const double d = frame[(size_t)j] - frame[(size_t)(j + lag)];
                difference += d * d;
            }
            sum += difference;
            yin[(size_t)lag] = sum > 1e-15 ? (float)(difference * lag / sum) : 1;
        }
        int selected = -1;
        for (int lag = minLag; lag < maxLag; ++lag) {
            if (yin[(size_t)lag] < 0.15f) {
                while (lag + 1 <= maxLag && yin[(size_t)(lag + 1)] < yin[(size_t)lag]) ++lag;
                selected = lag; break;
            }
        }
        if (selected < 0) { losePitch(); return; }
        bool continuityRescue = false;
        if (inputMidi != 0) {
            const float primaryMidi = (float)(69 + 12 * std::log2((analysisRate / selected) / 440.0));
            const float octaveDistance = std::abs(primaryMidi - smoothedMidi);
            if (std::abs(octaveDistance - 12.0f) < 0.8f || std::abs(octaveDistance - 24.0f) < 0.8f) {
                int nearby = -1;
                float nearbyValue = 0.60f;
                for (int lag = minLag + 1; lag < maxLag; ++lag) {
                    if (yin[(size_t)lag] > yin[(size_t)(lag - 1)] || yin[(size_t)lag] > yin[(size_t)(lag + 1)]) continue;
                    const float midi = (float)(69 + 12 * std::log2((analysisRate / lag) / 440.0));
                    if (std::abs(midi - smoothedMidi) < 2.0f && yin[(size_t)lag] < nearbyValue) {
                        nearby = lag; nearbyValue = yin[(size_t)lag];
                    }
                }
                if (nearby >= 0) { selected = nearby; continuityRescue = true; }
            }
        }
        double refined = selected;
        if (selected > 1 && selected < maxLag) {
            const double l = yin[(size_t)(selected - 1)], c = yin[(size_t)selected], r = yin[(size_t)(selected + 1)];
            const double denom = l - 2 * c + r;
            if (std::abs(denom) > 1e-12) refined += std::clamp(0.5 * (l - r) / denom, -0.5, 0.5);
        }
        const float candidateFrequency = (float)(analysisRate / refined);
        const float candidateConfidence = continuityRescue ? std::max(0.81f, 1 - yin[(size_t)selected]) : 1 - yin[(size_t)selected];
        if (candidateFrequency < 90 || candidateFrequency > 1000 || candidateConfidence < 0.8f) { losePitch(); return; }
        missingFrames = 0;
        const float rawMidi = (float)(69 + 12 * std::log2(candidateFrequency / 440.0));
        // A pitched instrument leaking into the vocal mic often produces brief, large jumps.
        // Keep following the established vocal until a distant candidate remains stable for
        // three analysis frames. This adds only 12 ms when the singer really changes register.
        if (inputMidi != 0 && std::abs(rawMidi - smoothedMidi) > 3.5f) {
            if (jumpCandidate >= 0 && std::abs(rawMidi - jumpCandidate) < 0.7f) ++jumpFrames;
            else { jumpCandidate = rawMidi; jumpFrames = 1; }
            if (jumpFrames < 3) return;
        }
        jumpCandidate = -1; jumpFrames = 0;
        midiHistory[(size_t)(midiHistoryCount % 3)] = rawMidi; ++midiHistoryCount;
        float filtered = rawMidi;
        if (midiHistoryCount >= 3) {
            auto sorted = midiHistory; std::sort(sorted.begin(), sorted.end()); filtered = sorted[1];
        }
        if (inputMidi == 0 || std::abs(filtered - smoothedMidi) > 3.0f) smoothedMidi = filtered;
        else smoothedMidi += 0.42f * (filtered - smoothedMidi);
        frequency = candidateFrequency; confidence = candidateConfidence; inputMidi = smoothedMidi;
        int borrowedMask = 0;
        for (size_t chord = 0; chord < s.chordEnabled.size(); ++chord)
            if (s.chordEnabled[chord]) borrowedMask |= chordMask(s.chordRoot[chord], s.chordQuality[chord]);
        const float proposed = nearestNote(inputMidi, s.key, s.scale, targetMidi, borrowedMask);
        if (targetMidi < 0) { targetMidi = proposed; pendingTarget = -1; pendingFrames = 0; }
        else if (proposed == targetMidi) { pendingTarget = -1; pendingFrames = 0; }
        else if (proposed == pendingTarget) {
            if (++pendingFrames >= 2) { targetMidi = proposed; pendingTarget = -1; pendingFrames = 0; }
        } else { pendingTarget = proposed; pendingFrames = 1; }
    }
};
}
