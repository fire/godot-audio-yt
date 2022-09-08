#pragma once

#include "audio/decoder.hpp"
#include "ebml/stream.hpp"
#include "webm/decoder.hpp"

#include "core/templates/hash_map.h"
#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

#include <functional>

namespace yt {
static const char *const YOUTUBE_HOST = "https://www.youtube.com";

static const Vector<String> DEFAULT_HEADERS = []() {
	Vector<String> list;
	list.push_back("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.101 Safari/537.36");
	return list;
}();

struct ScramblerFunction {
	enum Type {
		SLICE,
		SWAP,
		REVERSE
	};

	Type type;
	int64_t index;

	ScramblerFunction(const Type p_type, const int64_t p_index) :
			type(p_type), index(p_index) {}
};

static Mutex scrambler_cache_mutex;
static std::vector<ScramblerFunction> scrambler_cache;

struct PlayerResponse {
	String player_url;
	Variant player_data;
	Variant player_response;
};

class VideoData : public RefCounted {
	GDCLASS(VideoData, RefCounted);

	String id;
	String channel;
	String title;
	double duration = 0.0;
	int64_t views = 0;
	bool from_artist = false;

protected:
	static void _bind_methods();

public:
	void create(
			const String p_id,
			const String p_channel,
			const String p_title,
			const double p_duration,
			const int64_t p_views,
			const bool p_from_artist);

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

class YouTubeSearchTask : public RefCounted {
	GDCLASS(YouTubeSearchTask, RefCounted);

protected:
	static void _bind_methods();

public:
	String query;
};

class YouTubeGetVideoTask : public RefCounted {
	GDCLASS(YouTubeGetVideoTask, RefCounted);

protected:
	static void _bind_methods();

public:
	String id;
};

class YouTube : public Object {
	GDCLASS(YouTube, Object);

	static YouTube *singleton;

	bool terminate_threads = false;
	std::vector<std::thread> task_threads;

protected:
	static void _bind_methods();

public:
	static YouTube *get_singleton();

	String request(
			const String p_host,
			const String p_path,
			const String p_body = String(),
			const String p_file = String(),
			const Vector<String> p_headers = DEFAULT_HEADERS,
			const bool *p_terminate_threads = nullptr);

	void _thread_search(const Ref<YouTubeSearchTask> p_task);
	Ref<YouTubeSearchTask> search(const String p_query);

	void _thread_get_video(const Ref<YouTubeGetVideoTask> p_task);
	Ref<YouTubeGetVideoTask> get_video(const String p_id);

	void _thread_download_cache(const String p_playback_url, const String p_local_path);
	void download_cache(const String p_playback_url, const String p_local_path);

	YouTube();
	virtual ~YouTube();
};

class Player : public audio::Decoder {
	const String id;
	const String local_path = String("user://youtube_cache/{0}.webm").format(varray(id));

	bool terminate_thread = false;
	std::thread thread;

	struct Playback {
		bool ready = false;
		double start_pos = 0.0;
		ebml::Stream *stream = nullptr;
		webm::Decoder *decoder = nullptr;

		~Playback() {
			if (decoder != nullptr) {
				delete decoder;
			}
			if (stream != nullptr) {
				delete stream;
			}
		}
	} playback;

protected:
	void _thread_func();

public:
	virtual double get_sample_rate() const;
	virtual double get_duration() const;
	virtual double get_position() const;
	virtual void seek(const double p_time);
	virtual void sample(audio::AudioFrame *const p_buffer, const uint64_t p_frames, bool &r_active, bool &r_buffering);

	Player(const String p_id);
	virtual ~Player();
};
}; // namespace yt
