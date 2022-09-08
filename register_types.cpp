#include "register_types.h"
#include "audio_stream_yt.hpp"
#include "core/object/class_db.h"
#include "youtube.hpp"

static yt::YouTube *gdaudioyt_youtube = nullptr;

void initialize_audio_yt_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  ClassDB::register_class<AudioStreamYT>();
  ClassDB::register_class<yt::VideoData>();
  ClassDB::register_class<yt::YouTubeSearchTask>();
  ClassDB::register_class<yt::YouTubeGetVideoTask>();

  ClassDB::register_class<yt::YouTube>();
  gdaudioyt_youtube = memnew(yt::YouTube);
  Engine::get_singleton()->add_singleton(
      Engine::Singleton("YouTube", yt::YouTube::get_singleton()));
}

void uninitialize_audio_yt_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  memdelete(gdaudioyt_youtube);
}
