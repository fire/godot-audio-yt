#pragma once

#include <cstdint>

namespace audio {

/**
 * Single frame of audio consisting of left and right channels.
 */
struct AudioFrame {
	float l;
	float r;
	
	AudioFrame();
	AudioFrame(const float p_l, const float p_r);
	virtual ~AudioFrame();
};

/**
 * Abstract class that manages an audio stream.
 */
class Decoder {
public:
	/**
	 * @returns The sample rate of the stream, or `0.0` if the stream is not loaded.
	 */
	virtual double get_sample_rate() const = 0;
	
	/**
	 * @returns The duration of the stream, or `0.0` if the stream is not loaded.
	 */
	virtual double get_duration() const = 0;
	
	/**
	 * @returns The position of the stream.
	 */
	virtual double get_position() const = 0;
	
	/**
	 * Changes the position of playback to `p_time`.
	 * 
	 * If the stream is not loaded, this time will be remembered until it is loaded.
	 * 
	 * @param[in] p_time The time to seek to.
	 */
	virtual void seek(const double p_time) = 0;
	
	/**
	 * Read an arbitrary amount of audio samples from the stream.
	 * 
	 * Advances the stream's position by the amount of time elapsed.
	 * 
	 * @param[in] p_buffer Pointer of the array to write into.
	 * @param[in] p_frames Number of audio frames to read.
	 * @param[out] r_active Whether or not the stream should continue.
	 * @param[out] r_buffering Whether or not the stream is buffering.
	 */
	virtual void sample(AudioFrame * const p_buffer, const uint64_t p_frames, bool &r_active, bool &r_buffering) = 0;
	
	virtual ~Decoder();
};

};
