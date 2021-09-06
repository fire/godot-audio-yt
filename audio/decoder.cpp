#include "decoder.hpp"

audio::AudioFrame::AudioFrame() : l(0.0), r(0.0) {
}

audio::AudioFrame::AudioFrame(const float p_l, const float p_r) : l(p_l), r(p_r) {
}

audio::AudioFrame::~AudioFrame() {
}

audio::Decoder::~Decoder() {
}
