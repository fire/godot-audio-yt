#pragma once

#include "audio/decoder.hpp"
#include "ebml/stream.hpp"
#include "webm/decoder.hpp"

#include "core/map.h"
#include "core/reference.h"
#include "core/variant.h"

#include <functional>

namespace yt {

static const char * const YOUTUBE_HOST = "https://www.youtube.com";

static const Vector<String> DEFAULT_HEADERS = []() {
	Vector<String> list;
	list.push_back("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.101 Safari/537.36");
	return list;
}();

struct ScramblerFunction {
	enum Type {
		SLICE, SWAP, REVERSE
	};
	
	Type type;
	int64_t index;
};

static Mutex scrambler_cache_mutex;
static Vector<ScramblerFunction> scrambler_cache;

struct PlayerResponse {
	String player_url;
	Variant player_data;
	Variant player_response;
};

class VideoData : public Reference {
	GDCLASS(VideoData, Reference);
	
	String id;
	String channel;
	String title;
	double duration;
	int64_t views;
	bool from_artist;
	
protected:
	static void _bind_methods();
	
public:
	void create(
		const String p_id,
		const String p_channel,
		const String p_title,
		const double p_duration,
		const int64_t p_views,
		const bool p_from_artist
	);
	
	void set_id(const String p_id);
	void set_channel(const String p_channel);
	void set_title(const String p_title);
	void set_duration(const double p_duration);
	void set_views(const int64_t p_views);
	void set_from_artist(const bool p_from_artist);
	
	String get_id() const;
	String get_channel() const;
	String get_title() const;
	double get_duration() const;
	int64_t get_views() const;
	bool get_from_artist() const;
	
	String to_string();
	
	VideoData();
	virtual ~VideoData();
};

class YouTube;

class YouTubeSearchJob : public Reference {
	GDCLASS(YouTubeSearchJob, Reference);
	
protected:
	static void _bind_methods();
};

class YouTubeGetVideoJob : public Reference {
	GDCLASS(YouTubeGetVideoJob, Reference);
	
protected:
	static void _bind_methods();
};

class YouTube : public Object {
	GDCLASS(YouTube, Object);
	
	static YouTube *singleton;
	
	bool terminate_threads = false;
	std::vector<std::thread> job_threads;
	
protected:
	static void _bind_methods();
	
public:
	static YouTube *get_singleton();
	
	Ref<yt::YouTubeSearchJob> search(const String &p_query);
	Ref<yt::YouTubeGetVideoJob> get_video(const String &p_id);
	
	void download_cache(const String &p_local_path, const String &p_playback_url);
	
	YouTube();
	virtual ~YouTube();
};

class Player : public audio::Decoder {
	const String id;
	
	bool terminate_thread = false;
	std::thread thread;
	
	struct Playback {
		bool ready = false;
		uint64_t sample_attempts = 0;
		double start_pos = 0.0;
		ebml::Stream *stream = nullptr;
		webm::Decoder *decoder = nullptr;
		
		~Playback() {
			if(decoder != nullptr) {
				delete decoder;
			}
			if(stream != nullptr) {
				delete stream;
			}
		}
	} playback;
	
protected:
	static void _thread_func(void *p_self);
	void thread_func();
	
public:
	virtual double get_sample_rate() const;
	virtual double get_duration() const;
	virtual double get_position() const;
	virtual void seek(const double p_time);
	virtual void sample(audio::AudioFrame * const p_buffer, const uint64_t p_frames, bool &r_active, bool &r_buffering);
	
	Player(const String p_id);
	virtual ~Player();
};

};
