#pragma once

#include "audio/decoder.hpp"
#include "ebml/stream.hpp"

#include <opus/opus.h>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace webm {
/**
 * Cue point found inside the cues element.
 */
struct CuePoint {
	/**
	 * Position of the related cluster element, in global space.
	 */
	uint64_t pos;

	/**
	 * Timestamp of this cue point, in seconds.
	 */
	double time;

	/**
	 * Duration of this cue point, in seconds.
	 */
	double duration;

	CuePoint(const uint64_t p_pos, const double p_time, const double p_duration);
	virtual ~CuePoint();
};

/**
 * Manages parsing, decoding, sampling, and seeking of an opus audio track inside a webm container.
 */
class Decoder : public audio::Decoder {
	ebml::Stream *const stream;

	bool terminate_thread = false;
	std::thread thread;

	double position = 0.0;

	struct DecoderContext {
		bool ready = false;
		uint64_t sample_attempts = 0;

		uint64_t time_scale;
		double duration;
		uint64_t track;
		double sampling_rate;
		uint64_t channels;
		std::vector<CuePoint> cues;

		OpusDecoder *opus;
		uint64_t opus_frame_samples;
		float *opus_pcm = nullptr;
		uint64_t opus_pcm_index = 0;
		uint64_t opus_pcm_size = 0;

		std::mutex mutex;
		std::vector<std::vector<const ebml::Element *>> clusters;
		uint64_t current_cluster = 0;
		uint64_t active_cluster = 0;
		uint64_t active_block = 0;

		void delete_cluster(const std::vector<const ebml::Element *> &p_cluster);
		void trim_clusters();

		DecoderContext();
		virtual ~DecoderContext();
	} context;

	struct {
		std::mutex mutex;
		double time = 0.0;
		bool job = true;
	} seeking;

protected:
	void debug_print_element(const ebml::Element *const p_element);

	inline static double get_time(const uint64_t p_time_scale, const double p_raw_time);

	void load_headers();

	static void _thread_func(void *p_self);
	void thread_func();

public:
	virtual double get_sample_rate() const;
	virtual double get_duration() const;
	virtual double get_position() const;

	virtual void seek(const double p_time);
	virtual void sample(audio::AudioFrame *const p_buffer, const uint64_t p_frames, bool &r_active, bool &r_buffering);

	Decoder(ebml::Stream *const p_stream);
	virtual ~Decoder();
};
}; // namespace webm
