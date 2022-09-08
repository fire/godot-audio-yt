#include "audio_stream_yt.hpp"

#include "http_stream.hpp"

void AudioStreamPlaybackYT::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_buffering"), &AudioStreamPlaybackYT::is_buffering);
}

int AudioStreamPlaybackYT::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	audio::AudioFrame *buffer = (audio::AudioFrame *)p_buffer;
	decoder->sample(buffer, p_frames, active, buffering);

	for (int i = 0; i < p_frames; ++i) {
		const audio::AudioFrame frame = buffer[i];
		p_buffer[i] = AudioFrame(frame.l, frame.r);
	}

	const double duration = decoder->get_duration();
	if (duration > 0.0) {
		base->duration = duration;
	}
}

float AudioStreamPlaybackYT::get_stream_sampling_rate() {
	if (decoder == nullptr) {
		return 0.0;
	}

	return float(decoder->get_sample_rate());
}

void AudioStreamPlaybackYT::start(float p_from_pos) {
	active = true;
	delete decoder;
	decoder = new yt::Player(base->get_id().utf8().get_data());
	seek(p_from_pos);
	begin_resample();
}

void AudioStreamPlaybackYT::stop() {
	active = false;
}

bool AudioStreamPlaybackYT::is_playing() const {
	return active;
}

int AudioStreamPlaybackYT::get_loop_count() const {
	return 0;
}

float AudioStreamPlaybackYT::get_playback_position() const {
	ERR_FAIL_COND_V(decoder == nullptr, 0.0);

	return float(decoder->get_position());
}

void AudioStreamPlaybackYT::seek(float p_time) {
	ERR_FAIL_COND(decoder == nullptr);

	decoder->seek(p_time);
}

bool AudioStreamPlaybackYT::is_buffering() const {
	return buffering;
}

AudioStreamPlaybackYT::AudioStreamPlaybackYT() {
	decoder = new yt::Player("");
}

AudioStreamPlaybackYT::~AudioStreamPlaybackYT() {
	if (decoder != nullptr) {
		delete decoder;
	}
}

void AudioStreamYT::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "id"), &AudioStreamYT::create);
	ClassDB::bind_method(D_METHOD("get_id"), &AudioStreamYT::get_id);
}

void AudioStreamYT::create(const String &p_id) {
	ERR_FAIL_COND_MSG(!id.is_empty(), "Stream has already been created.");
	ERR_FAIL_COND_MSG(p_id.is_empty(), "Given id is empty.");

	id = p_id;
}

String AudioStreamYT::get_id() const {
	return id;
}

Ref<AudioStreamPlayback> AudioStreamYT::instantiate_playback() {
	Ref<AudioStreamPlaybackYT> playback;

	ERR_FAIL_COND_V_MSG(id.is_empty(), playback, "No source specified. Please call the 'create' method.");

	playback.instantiate();
	playback->base = Ref<AudioStreamYT>(this);
	return playback;
}

String AudioStreamYT::get_stream_name() const {
	return "";
}

float AudioStreamYT::get_length() const {
	return duration;
}

AudioStreamYT::AudioStreamYT() {
}

AudioStreamYT::~AudioStreamYT() {
}
