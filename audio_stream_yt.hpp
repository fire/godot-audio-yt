#pragma once

#include "audio/decoder.hpp"
#include "webm/decoder.hpp"
#include "youtube.hpp"

#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"

class AudioStreamYT;

class AudioStreamPlaybackYT : public AudioStreamPlaybackResampled {
	GDCLASS(AudioStreamPlaybackYT, AudioStreamPlaybackResampled)
	friend class AudioStreamYT;

	Ref<AudioStreamYT> base;
	bool active = false;
	bool buffering = false;

	audio::Decoder *decoder = nullptr;

	struct {
		double time;
		bool job = false;
	} seeking;

protected:
	static void _bind_methods();

	virtual void _mix_internal(AudioFrame *p_buffer, int p_frames);
	virtual float get_stream_sampling_rate();

public:
	virtual void start(float p_from_pos = 0.0);
	virtual void stop();
	virtual bool is_playing() const;
	virtual int get_loop_count() const;
	virtual float get_playback_position() const;
	virtual void seek(float p_time);

	bool is_buffering() const;

	AudioStreamPlaybackYT();
	~AudioStreamPlaybackYT();
};

class AudioStreamYT : public AudioStream {
	GDCLASS(AudioStreamYT, AudioStream)

	friend class AudioStreamPlaybackYT;

	String id;
	double duration = 0.0;

protected:
	static void _bind_methods();

public:
	void create(const String &p_id);
	String get_id() const;

	virtual Ref<AudioStreamPlayback> instance_playback();
	virtual String get_stream_name() const;

	virtual float get_length() const;

	AudioStreamYT();
	~AudioStreamYT();
};
