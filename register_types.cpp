#include "register_types.h"
#include "core/class_db.h"
#include "audio_stream_yt.hpp"
#include "youtube.hpp"

static yt::YouTube *gdaudioyt_youtube = nullptr;

void register_gdaudioyt_types() {
	ClassDB::register_class<AudioStreamYT>();
	ClassDB::register_class<yt::VideoData>();
	ClassDB::register_class<yt::YouTubeSearchJob>();
	ClassDB::register_class<yt::YouTubeGetVideoJob>();
	
	ClassDB::register_class<yt::YouTube>();
	gdaudioyt_youtube = memnew(yt::YouTube);
	Engine::get_singleton()->add_singleton(Engine::Singleton("YouTube", yt::YouTube::get_singleton()));
}

void unregister_gdaudioyt_types() {
	memdelete(gdaudioyt_youtube);
}
