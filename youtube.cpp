#include "youtube.hpp"

#include "http_stream.hpp"
#include "local_stream.hpp"

#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "modules/regex/regex.h"

String regex_match(const String &p_regex, const String &p_string, const uint64_t p_group = 1) {
	RegEx regex;
	const Error err = regex.compile(p_regex);
	ERR_FAIL_COND_V_MSG(err != OK, "", "Failed to parse regex.");

	const Ref<RegExMatch> match = regex.search(p_string);

	if (!match.is_valid()) {
		return "";
	}

	return match->get_string(p_group);
}

/**
 * Fetch the player url and the player response.
 * 
 * The player url is a url to a JavaScript file: (/s/player/.../base.js).
 * The player response is a large json object containing playback information.
 * {
 *     "responseContext": { ... },
 *     "playabilityStatus": { ... },
 *     "streamingData": { ... },
 *     ...
 * }
 * 
 * @param[in] p_id The YouTube video id.
 * @param[out] r_response The player response data.
 * @returns Whether the response was successful.
 */
bool _fetch_player_response(const String p_id, yt::PlayerResponse &r_response, const bool *p_terminate_threads) {
	const String path = String("/watch?v={0}&hl=en").format(varray(p_id));

	String response = yt::YouTube::get_singleton()->request(
			yt::YOUTUBE_HOST,
			path,
			"",
			"",
			yt::DEFAULT_HEADERS,
			p_terminate_threads);

	String player_url = regex_match(
			R"X("(?:PLAYER_JS_URL|jsUrl)"\s*:\s*"([^"]+)")X",
			response);

	if (player_url.rfind("//", 0) == 0) {
		player_url = player_url.insert(0, "https:");
	} else if (player_url.rfind("/", 0)) {
		player_url = player_url.insert(0, yt::YOUTUBE_HOST);
	}

	String player_data_raw = regex_match(
			R"X(ytInitialData\s*=\s*({.+?})\s*;\s*(?:var\s+meta|</script|\n))X",
			response);

	Variant player_data;
	{
		String err_str;
		int err_line;
		const Error json_error = JSON::parse(player_data_raw, player_data, err_str, err_line);
		if (json_error != OK) {
			print_line("");
			print_line("");
			print_line("");
			print_line(response);
			print_line("");
			print_line("");
			print_line("");
			print_line(player_data_raw);
			print_line("");
			print_line("");
			print_line("");
			ERR_PRINT("Failed to parse player data json.");
			return false;
		}
	}

	String player_response_raw = regex_match(
			R"X(ytInitialPlayerResponse\s*=\s*({.+?})\s*;\s*(?:var\s+meta|</script|\n))X",
			response);

	Variant player_response;
	{
		String err_str;
		int err_line;
		const Error json_error = JSON::parse(player_response_raw, player_response, err_str, err_line);
		if (json_error != OK) {
			print_line("RESPONSE...");
			print_line("");
			print_line(response);
			print_line("");
			print_line("DATA...");
			print_line("");
			print_line(player_data_raw);
			print_line("");
			ERR_PRINT("Failed to parse player response json.");
			return false;
		}
	}

	r_response.player_url = player_url;
	r_response.player_data = player_data;
	r_response.player_response = player_response;

	return true;
};

String yt::YouTube::request(
		const String p_host,
		const String p_path,
		const String p_body,
		const String p_file,
		const Vector<String> p_headers,
		const bool *p_terminate_threads) {
	HTTPClient client;

	if (p_terminate_threads == nullptr) {
		p_terminate_threads = &terminate_threads;
	}

	const Error connect_err = client.connect_to_host(p_host);
	ERR_FAIL_COND_V_MSG(connect_err != OK, "", "Failed to connect to the host.");

	while (client.get_status() == HTTPClient::STATUS_RESOLVING || client.get_status() == HTTPClient::STATUS_CONNECTING) {
		OS::get_singleton()->delay_usec(1);
		client.poll();
	}

	const bool connect_failed = client.get_status() != HTTPClient::STATUS_CONNECTED;
	ERR_FAIL_COND_V_MSG(connect_failed, "", "Failed to connect to the host.");

	if (!p_body.empty()) {
		const Error request_err = client.request(HTTPClient::METHOD_POST, p_path, p_headers, p_body);
		ERR_FAIL_COND_V_MSG(request_err != OK, "", "Failed to perform request.");
	} else {
		const Error request_err = client.request(HTTPClient::METHOD_GET, p_path, p_headers);
		ERR_FAIL_COND_V_MSG(request_err != OK, "", "Failed to perform request.");
	}

	while (client.get_status() == HTTPClient::STATUS_REQUESTING) {
		OS::get_singleton()->delay_usec(1);
		client.poll();
	}

	const bool request_failed = client.get_status() != HTTPClient::STATUS_BODY && client.get_status() != HTTPClient::STATUS_CONNECTED;
	ERR_FAIL_COND_V_MSG(request_failed, "", "Failed to perform request.");

	if (client.has_response()) {
		if (client.get_response_body_length() == 0) {
			List<String> headers;
			const Error headers_err = client.get_response_headers(&headers);
			if (headers_err != OK) {
				ERR_FAIL_V_MSG("", "Failed to read headers.");
			}

			String redirect;
			for (int i = 0; i < headers.size(); ++i) {
				const String header = headers[i];
				if (header.begins_with("Location: ")) {
					redirect = header.substr(10);
					client.close();
					break;
				}
			}

			if (redirect.empty()) {
				ERR_FAIL_V_MSG("", "Server replied with empty response.");
			}

			if (redirect.begins_with("//")) {
				redirect = redirect.insert(0, "https:");
			} else if (redirect.begins_with("/")) {
				redirect = redirect.insert(0, p_host);
			}

			String new_scheme, new_host, new_path;
			int new_port;
			const Error url_err = redirect.parse_url(new_scheme, new_host, new_port, new_path);
			ERR_FAIL_COND_V_MSG(url_err != OK, "", "Failed to parse redirect url.");

			if (!redirect.empty()) {
				return request(new_scheme + new_host, new_path, p_body, p_file, p_headers);
			}

			ERR_FAIL_V_MSG("", "Response body length was zero.");
		}

		if (p_file.empty()) {
			// Download to a string.
			PoolByteArray response;

			while (client.get_status() == HTTPClient::STATUS_BODY) {
				if (*p_terminate_threads) {
					return "";
				}
				client.poll();
				const PoolByteArray &chunk = client.read_response_body_chunk();
				if (!chunk.empty()) {
					const uint64_t pos = response.size();
					response.resize(response.size() + chunk.size());
					for (int i = 0; i < chunk.size(); ++i) {
						response.set(pos + i, chunk[i]);
					}
				} else {
					OS::get_singleton()->delay_usec(1);
				}
			}

			String text;
			const PoolByteArray::Read r = response.read();
			text.parse_utf8((const char *)r.ptr(), response.size());

			return text;
		} else {
			// Create directory.
			DirAccess *const dir = DirAccess::create_for_path(p_file);
			if (!dir->dir_exists(p_file.get_base_dir())) {
				const Error dir_err = dir->make_dir_recursive(p_file.get_base_dir());
				ERR_FAIL_COND_V_MSG(dir_err != OK, "", "Failed to create directory: '" + p_file.get_base_dir() + "'.");
			}

			// Create file.
			const String tmp_file = p_file + ".part";

			dir->remove(p_file);

			Error file_err;
			FileAccess *const file = FileAccess::open(tmp_file, FileAccess::WRITE, &file_err);
			ERR_FAIL_COND_V_MSG(file_err != OK, "", "Failed to create file: '" + tmp_file + "'.");

			// Download to a file.
			while (client.get_status() == HTTPClient::STATUS_BODY) {
				if (*p_terminate_threads) {
					return "";
				}
				client.poll();
				const PoolByteArray &chunk = client.read_response_body_chunk();
				if (!chunk.empty()) {
					const PoolByteArray::Read r = chunk.read();
					file->store_buffer(r.ptr(), chunk.size());
				} else {
					OS::get_singleton()->delay_usec(1);
				}
			}

			file->close();
			memdelete(file);

			const Error rename_err = dir->rename(tmp_file, p_file);
			ERR_FAIL_COND_V_MSG(rename_err != OK, "", "Failed to rename file from '" + tmp_file + "' to '" + p_file + "'.");

			memdelete(dir);
		}
	}

	return "";
}

void yt::YouTubeSearchTask::_bind_methods() {
	ADD_SIGNAL(MethodInfo("completed", PropertyInfo(Variant::ARRAY, "results", PROPERTY_HINT_RESOURCE_TYPE, "VideoData")));
}

void yt::YouTubeGetVideoTask::_bind_methods() {
	ADD_SIGNAL(MethodInfo("completed", PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_RESOURCE_TYPE, "VideoData")));
}

void yt::VideoData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_id", "id"), &VideoData::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &VideoData::get_id);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VideoData::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VideoData::get_channel);

	ClassDB::bind_method(D_METHOD("set_title", "title"), &VideoData::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &VideoData::get_title);

	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &VideoData::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &VideoData::get_duration);

	ClassDB::bind_method(D_METHOD("set_views", "views"), &VideoData::set_views);
	ClassDB::bind_method(D_METHOD("get_views"), &VideoData::get_views);

	ClassDB::bind_method(D_METHOD("set_from_artist", "from_artist"), &VideoData::set_from_artist);
	ClassDB::bind_method(D_METHOD("get_from_artist"), &VideoData::get_from_artist);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "id"), "set_id", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "channel"), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "duration"), "set_duration", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "views"), "set_views", "get_views");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "from_artist"), "set_from_artist", "get_from_artist");
}

void yt::VideoData::create(
		const String p_id,
		const String p_channel,
		const String p_title,
		const double p_duration,
		const int64_t p_views,
		const bool p_from_artist) {
	id = p_id;
	channel = p_channel;
	title = p_title;
	duration = p_duration;
	views = p_views;
	from_artist = p_from_artist;
}

void yt::VideoData::set_id(const String p_id) {
	id = p_id;
}

void yt::VideoData::set_channel(const String p_channel) {
	channel = p_channel;
}

void yt::VideoData::set_title(const String p_title) {
	title = p_title;
}

void yt::VideoData::set_duration(const double p_duration) {
	duration = p_duration;
}

void yt::VideoData::set_views(const int64_t p_views) {
	views = p_views;
}

void yt::VideoData::set_from_artist(const bool p_from_artist) {
	from_artist = p_from_artist;
}

String yt::VideoData::get_id() const {
	return id;
}

String yt::VideoData::get_channel() const {
	return channel;
}

String yt::VideoData::get_title() const {
	return title;
}

double yt::VideoData::get_duration() const {
	return duration;
}

int64_t yt::VideoData::get_views() const {
	return views;
}

bool yt::VideoData::get_from_artist() const {
	return from_artist;
}

String yt::VideoData::to_string() {
	return "VideoData(" + id + ")";
}

yt::VideoData::VideoData() {
}

yt::VideoData::~VideoData() {
}

yt::YouTube *yt::YouTube::singleton;

void yt::YouTube::_bind_methods() {
	ClassDB::bind_method(D_METHOD("search", "query"), &yt::YouTube::search);
	ClassDB::bind_method(D_METHOD("get_video", "id"), &yt::YouTube::get_video);
}

void yt::YouTube::_thread_search(Ref<YouTubeSearchTask> p_task) {
	const String path = "/youtubei/v1/search?key=AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8";

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");

	auto get_body = [&]() -> Dictionary {
		Dictionary body;
		{
			Dictionary context;
			{
				Dictionary client;
				{
					client["clientName"] = "WEB";
					client["clientVersion"] = "2.20201021.03.00";
				}
				context["client"] = client;
			}
			body["context"] = context;
		}
		body["query"] = p_task->query;
		return body;
	};

	const String body = JSON::print(get_body());

	const String search_response_raw = request(yt::YOUTUBE_HOST, path, body, "", headers);

	Variant search_response;
	{
		String err_str;
		int err_line;
		const Error json_error = JSON::parse(search_response_raw, search_response, err_str, err_line);
		ERR_FAIL_COND_MSG(json_error != OK, "Failed to parse player response json.");
	}

	auto try_runs = [&](const Variant p_source) -> String {
		bool valid;
		String simple = p_source.get("simpleText", &valid);

		if (valid) {
			return simple;
		}

		return p_source.get("runs").get(0).get("text");
	};

	auto parse_duration = [&](const Variant p_duration) -> double {
		Vector<String> nums = try_runs(p_duration).split(":");
		nums.invert();

		double duration = 0.0;
		for (int i = 0; i < nums.size(); ++i) {
			const double v = nums[i].to_double();
			switch (i) {
				case 0: {
					duration += v;
				} break;
				case 1: {
					duration += v * 60.0;
				} break;
				case 2: {
					duration += v * 3600.0;
				} break;
			}
		}

		return duration;
	};

	Array results;

	const Array &sections = search_response
									.get("contents")
									.get("twoColumnSearchResultsRenderer")
									.get("primaryContents")
									.get("sectionListRenderer")
									.get("contents");

	for (int i = 0; i < sections.size(); ++i) {
		const Variant &section = sections[i];
		const Array &contents = section
										.get("itemSectionRenderer")
										.get("contents");

		for (int j = 0; j < contents.size(); ++j) {
			bool video_valid;
			const Variant &item = contents.get(j).get("videoRenderer", &video_valid);
			if (!video_valid) {
				continue;
			}

			bool from_artist = false;
			const Array &badges = item.get("ownerBadges");
			for (int k = 0; k < badges.size(); ++k) {
				bool badge_valid;
				const Variant &iconType = badges[k].get("metadataBadgeRenderer").get("icon").get("iconType", &badge_valid);
				if (!badge_valid) {
					continue;
				}

				if (iconType == "OFFICIAL_ARTIST_BADGE") {
					from_artist = true;
				}
			}

			Ref<yt::VideoData> result;
			result.instance();
			result->create(
					item.get("videoId"),
					try_runs(item.get("ownerText")),
					try_runs(item.get("title")),
					parse_duration(item.get("lengthText")),
					try_runs(item.get("viewCountText")).to_int64(),
					from_artist);
			results.push_back(result);
		}
	}

	p_task->emit_signal("completed", results);
}

Ref<yt::YouTubeSearchTask> yt::YouTube::search(const String p_query) {
	Ref<YouTubeSearchTask> task;
	task.instance();
	task->query = p_query;
	task_threads.push_back(std::thread(&yt::YouTube::_thread_search, this, task));
	return task;
}

void yt::YouTube::_thread_get_video(Ref<YouTubeGetVideoTask> p_task) {
	yt::PlayerResponse response;
	const bool response_flag = _fetch_player_response(p_task->id, response, &terminate_threads);
	if (!response_flag) {
		return;
	}

	const Variant details = response.player_response.get("videoDetails");

	const Array &contents = response.player_data
									.get("contents")
									.get("twoColumnWatchNextResults")
									.get("results")
									.get("results")
									.get("contents");

	bool from_artist = false;
	for (int i = 0; i < contents.size(); ++i) {
		const Array &badges = contents[i]
									  .get("videoSecondaryInfoRenderer")
									  .get("owner")
									  .get("videoOwnerRenderer")
									  .get("badges");
		for (int k = 0; k < badges.size(); ++k) {
			bool badge_valid;
			const Variant &iconType = badges[k].get("metadataBadgeRenderer").get("icon").get("iconType", &badge_valid);
			if (!badge_valid) {
				continue;
			}

			if (iconType == "OFFICIAL_ARTIST_BADGE") {
				from_artist = true;
			}
		}
	}

	Ref<yt::VideoData> result;
	result.instance();
	result->create(
			p_task->id,
			details.get("author"),
			details.get("title"),
			String(details.get("lengthSeconds")).to_int64(),
			String(details.get("viewCount")).to_int64(),
			from_artist);

	p_task->emit_signal("completed", result);
}

Ref<yt::YouTubeGetVideoTask> yt::YouTube::get_video(const String p_id) {
	Ref<YouTubeGetVideoTask> task;
	task.instance();
	task->id = p_id;
	task_threads.push_back(std::thread(&yt::YouTube::_thread_get_video, this, task));
	return task;
}

yt::YouTube *yt::YouTube::get_singleton() {
	return singleton;
}

void yt::YouTube::_thread_download_cache(const String p_playback_url, const String p_local_path) {
	String scheme, host, path;
	int port;
	const Error err = p_playback_url.parse_url(scheme, host, port, path);
	ERR_FAIL_COND_MSG(err != OK, "Failed to parse playback url.");

	request(scheme + host, path, "", p_local_path);
}

void yt::YouTube::download_cache(const String p_playback_url, const String p_local_path) {
	task_threads.push_back(std::thread(&yt::YouTube::_thread_download_cache, this, p_playback_url, p_local_path));
}

yt::YouTube::YouTube() {
	singleton = this;
}

yt::YouTube::~YouTube() {
	terminate_threads = true;
	for (std::thread &thread : task_threads) {
		thread.join();
	}

	singleton = nullptr;
}

void yt::Player::_thread_func(void *p_self) {
	Player *const self = (Player *)p_self;
	self->thread_func();
}

// Hours spent: 16
void yt::Player::thread_func() {
	const String local_path = String("user://youtube_cache/{0}.webm").format(varray(id));

	auto create_local_stream = [&]() {
		playback.stream = new LocalStream(local_path);
		playback.decoder = new webm::Decoder(playback.stream);
		playback.decoder->seek(playback.start_pos);
		playback.ready = true;
	};

	auto create_youtube_stream = [&]() {
		auto fetch_scrambler_funcs = [&](const PlayerResponse &p_player, std::vector<ScramblerFunction> &r_scrambler) {
			const String player_script = YouTube::get_singleton()->request(
					YOUTUBE_HOST,
					p_player.player_url,
					"",
					"",
					yt::DEFAULT_HEADERS,
					&terminate_thread);

			ERR_FAIL_COND_MSG(player_script.empty(), "Player script is empty.");

			const String scrambler_body = regex_match(
					R"X((?:\w+)=function\(\w+\){(\w+)=\1\.split\(\x22{2}\);(.*?;)return\s+\1\.join\(\x22{2}\)})X",
					player_script,
					2);
			const String scrambler_obj_name = regex_match(
					R"X(([\$_\w]+).\w+\(\w+,\d+\);)X",
					scrambler_body);

			const String scrambler_definition = regex_match(
					String(R"X((?s)var\s+{0}=\{(\w+:function\(\w+(,\w+)?\)\{(.*?)\}),?\};)X").format(varray(scrambler_obj_name)),
					player_script);

			const Vector<String> statements = scrambler_body.split(";", false);
			for (int statement_index = 0; statement_index < statements.size(); ++statement_index) {
				const String statement = statements[statement_index];

				const String func_name = regex_match(
						R"X(\w+(?:.|\[)(\"?\w+(?:\")?)\]?\()X",
						statement);

				const String slice_match = regex_match(
						String(R"X({0}:\bfunction\b\([a],b\).(\breturn\b)?.?\w+\.)X").format(varray(func_name)),
						scrambler_definition,
						0);
				if (!slice_match.empty()) {
					int64_t index = regex_match(
							R"X(\(\w+,(\d+)\))X",
							statement)
											.to_int64();

					r_scrambler.emplace_back(ScramblerFunction::Type::SLICE, index);
					continue;
				}

				const String swap_match = regex_match(
						String(R"X({0}:\bfunction\b\(\w+\,\w\).\bvar\b.\bc=a\b)X").format(varray(func_name)),
						scrambler_definition,
						0);
				if (!swap_match.empty()) {
					int64_t index = regex_match(
							R"X(\(\w+,(\d+)\))X",
							statement)
											.to_int64();

					r_scrambler.emplace_back(ScramblerFunction::Type::SWAP, index);
					continue;
				}

				const String reverse_match = regex_match(
						String(R"X({0}:\bfunction\b\(\w+\))X").format(varray(func_name)),
						scrambler_definition,
						0);
				if (!reverse_match.empty()) {
					r_scrambler.emplace_back(ScramblerFunction::Type::REVERSE, 0);
					continue;
				}
			}
		};

		auto parse_playback_url = [&](const PlayerResponse &p_player) -> String {
			const Array formats = p_player.player_response.get("streamingData").get("adaptiveFormats");

			Variant best_format;
			uint64_t best_bitrate = 0;
			for (int i = 0; i < formats.size(); ++i) {
				const Variant format = formats[i];
				if (format.get("mimeType") == "audio/webm; codecs=\"opus\"") {
					uint64_t bitrate = format.get("bitrate");
					if (bitrate > best_bitrate) {
						best_format = format;
						best_bitrate = bitrate;
					}
				}
			}

			bool valid;
			String playback_url = best_format.get("url", &valid);
			if (!valid) {
				auto get_cipher_data = [&]() -> Dictionary {
					const String raw = best_format.get("signatureCipher");
					const Vector<String> elements = raw.split("&");
					Dictionary dict;
					for (int i = 0; i < elements.size(); ++i) {
						const Vector<String> pair = elements[i].split("=");
						const String key = pair[0].replace("+", " ").http_unescape();
						const String value = pair[1].replace("+", " ").http_unescape();
						dict[key] = value;
					}

					return dict;
				};

				const Dictionary cipher_data = get_cipher_data();

				playback_url = cipher_data["url"];
				const String signature_param = cipher_data["sp"];
				const String signature_scrambled = cipher_data["s"];

				auto decipher_url = [&](const std::vector<ScramblerFunction> &p_scrambler) {
					String signature = signature_scrambled;
					for (size_t i = 0; i < p_scrambler.size(); ++i) {
						const ScramblerFunction &func = p_scrambler[i];

						switch (func.type) {
							case ScramblerFunction::Type::SLICE: {
								const int64_t index = func.index % signature.size();
								signature = signature.substr(index);
							} break;
							case ScramblerFunction::Type::SWAP: {
								const int64_t index = func.index % signature.size();

								const CharType temp = signature[0];
								signature[0] = signature[index];
								signature[index] = temp;
							} break;
							case ScramblerFunction::Type::REVERSE: {
								const int size = signature.size() - 1;
								for (int index = 0; index < size / 2; ++index) {
									const int j = size - index - 1;
									const CharType temp = signature[index];
									signature[index] = signature[j];
									signature[j] = temp;
								}
							} break;
						}
					}

					playback_url += vformat("&ratebypass=yes&%s=%s", signature_param, signature.http_escape());
				};

				if (scrambler_cache.empty()) {
					MutexLock lock(scrambler_cache_mutex);
					fetch_scrambler_funcs(p_player, scrambler_cache);
				}

				decipher_url(scrambler_cache);
			}

			return playback_url;
		};

		PlayerResponse response;
		const bool response_flag = _fetch_player_response(id, response, &terminate_thread);
		if (!response_flag) {
			return;
		}

		const String playback_url = parse_playback_url(response);

		playback.stream = new HttpStream(playback_url);
		playback.decoder = new webm::Decoder(playback.stream);
		playback.ready = true;

		YouTube::get_singleton()->download_cache(local_path, playback_url);
	};

	if (FileAccess::exists(local_path)) {
		create_local_stream();
	} else {
		create_youtube_stream();
	}
}

double yt::Player::get_sample_rate() const {
	if (!playback.ready) {
		return 0.0;
	}

	return playback.decoder->get_sample_rate();
}

double yt::Player::get_duration() const {
	if (!playback.ready) {
		return 0.0;
	}

	return playback.decoder->get_duration();
}

double yt::Player::get_position() const {
	if (!playback.ready) {
		return playback.start_pos;
	}

	return playback.decoder->get_position();
}

void yt::Player::seek(const double p_time) {
	if (!playback.ready) {
		playback.start_pos = p_time;
		return;
	}

	playback.decoder->seek(p_time);
}

void yt::Player::sample(
		audio::AudioFrame *const p_buffer,
		const uint64_t p_frames,
		bool &r_active,
		bool &r_buffering) {
	if (!playback.ready) {
		for (uint64_t i = 0; i < p_frames; ++i) {
			p_buffer[i] = audio::AudioFrame();
		}
		r_active = true;
		r_buffering = ++playback.sample_attempts > 3;
		return;
	}

	playback.decoder->sample(p_buffer, p_frames, r_active, r_buffering);
}

yt::Player::Player(const String p_id) :
		id(p_id), thread(std::thread(_thread_func, this)) {
}

yt::Player::~Player() {
	terminate_thread = true;
	thread.join();
}
