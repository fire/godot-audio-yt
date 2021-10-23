#include "decoder.hpp"

#include "ebml/buffer_stream.hpp"

webm::CuePoint::CuePoint(
		const uint64_t p_pos,
		const double p_time,
		const double p_duration) :
		pos(p_pos), time(p_time), duration(p_duration) {
}

webm::CuePoint::~CuePoint() {
}

void webm::Decoder::DecoderContext::delete_cluster(const std::vector<const ebml::Element *> &p_cluster) {
	for (const ebml::Element *const block : p_cluster) {
		delete block;
	}
}

void webm::Decoder::DecoderContext::trim_clusters() {
	// Assumes the frame buffer is locked.

	static const uint64_t MAX_PRIOR_FRAME_BUFFERS = 3;

	// Check if there are too many frame buffers before us.
	const int64_t extra = active_cluster - MAX_PRIOR_FRAME_BUFFERS;
	if (extra > 0) {
		for (uint64_t i = 0; i < (uint64_t)extra; ++i) {
			delete_cluster(clusters[i]);
		}
		clusters.erase(clusters.begin(), clusters.begin() + extra);

		current_cluster += extra;
		active_cluster -= extra;
	}
}

webm::Decoder::DecoderContext::DecoderContext() {
}

webm::Decoder::DecoderContext::~DecoderContext() {
	const std::lock_guard<std::mutex> lock(mutex);

	if (opus != nullptr) {
		opus_decoder_destroy(opus);
	}
	if (opus_pcm != nullptr) {
		delete[] opus_pcm;
	}
	for (const std::vector<const ebml::Element *> &cluster : clusters) {
		delete_cluster(cluster);
	}
}

void webm::Decoder::debug_print_element(const ebml::Element *const p_element) {
	std::cout << "{";
	p_element->debug_print();
	if (p_element->reg.type == ELEMENT_TYPE_MASTER) {
		ebml::ElementMaster *const e = (ebml::ElementMaster *)p_element;
		std::cout << ", \"children\": [";

		bool first = true;
		for (const ebml::Element *const child : stream->range(e)) {
			if (!first) {
				std::cout << ", ";
			}

			debug_print_element(child);

			first = false;
		}
		std::cout << "]";
	}
	std::cout << "}";
}

double webm::Decoder::get_time(const uint64_t p_time_scale, const double p_raw_time) {
	return p_raw_time * p_time_scale / 1000000000.0;
}

// Load track info and cues from stream.
void webm::Decoder::load_headers() {
	// INFO ELEMENTS (TIMECODE SCALE, DURATION)
	auto parse_segment_info = [&](
									  const ebml::ElementMaster *const p_info,
									  uint64_t &r_time_scale,
									  double &r_duration) {
		auto search = stream->range(p_info).search();

		const auto time_scale = search.get<ELEMENT_TIMECODE_SCALE, ebml::ElementUint>();
		if (time_scale == nullptr)
			return;
		const auto duration = search.get<ELEMENT_DURATION, ebml::ElementFloat>();
		if (duration == nullptr)
			return;

		r_time_scale = time_scale->value;
		r_duration = duration->value;
	};

	// TRACK ELEMENTS
	auto parse_segment_tracks = [&](
										const ebml::ElementMaster *const p_tracks,
										uint64_t &r_number,
										double &r_sampling_rate,
										uint64_t &r_channels) {
		for (const ebml::Element *const track : stream->range(p_tracks)) {
			if (track->reg.id != ELEMENT_TRACK_ENTRY) {
#ifdef __EXCEPTIONS
				throw std::runtime_error("Tracks element is not a TrackEntry.");
#else
				continue;
#endif
			}
			const auto t = (const ebml::ElementMaster *)track;

			auto search = stream->range(t).search();

			const auto codec_id = search.get<ELEMENT_CODEC_ID, ebml::ElementString>();
			if (codec_id == nullptr)
				continue;

			if (codec_id->value == "A_OPUS") {
				const auto number = search.get<ELEMENT_TRACK_NUMBER, ebml::ElementUint>();
				if (number == nullptr)
					continue;
				const auto audio = search.get<ELEMENT_AUDIO, ebml::ElementMaster>();
				if (audio == nullptr)
					continue;

				auto search_audio = stream->range(audio).search();

				const auto sampling_rate = search_audio.get<ELEMENT_SAMPLING_FREQUENCY, ebml::ElementFloat>();
				if (sampling_rate == nullptr)
					continue;
				const auto channels = search_audio.get<ELEMENT_CHANNELS, ebml::ElementUint>();
				if (channels == nullptr)
					continue;

				r_number = number->value;
				r_sampling_rate = sampling_rate->value;
				r_channels = channels->value;
				return;
			}
		}

#ifdef __EXCEPTIONS
		throw std::runtime_error("This stream does not have an opus audio track.");
#endif
	};

	struct RawCuePoint {
		uint64_t raw_time;
		uint64_t pos;

		RawCuePoint(const uint64_t p_raw_time, const uint64_t p_pos) :
				raw_time(p_raw_time), pos(p_pos) {}
	};

	// CUES ELEMENTS (CUE TIME, CUE TRACK POSITIONS)
	auto parse_segment_cues = [&](
									  const ebml::ElementMaster *const p_segment,
									  const ebml::ElementMaster *const p_cues,
									  std::vector<RawCuePoint> &r_items) {
		for (const ebml::Element *const cue : stream->range(p_cues)) {
			if (cue->reg.id != ELEMENT_CUE_POINT) {
#ifdef __EXCEPTIONS
				throw std::runtime_error("Cues element is not a CuePoint.");
#else
				continue;
#endif
			}
			const auto c = (const ebml::ElementMaster *)cue;

			auto search = stream->range(c).search();
			const auto cue_time = search.get<ELEMENT_CUE_TIME, ebml::ElementUint>();
			const auto track_positions = search.get<ELEMENT_CUE_TRACK_POSITIONS, ebml::ElementMaster>();

			auto search_track_positions = stream->range(track_positions).search();
			const auto cluster_position = search_track_positions.get<ELEMENT_CUE_CLUSTER_POSITION, ebml::ElementUint>();

			r_items.emplace_back(cue_time->value, p_segment->from + cluster_position->value);
		}
	};

	struct SeekItem {
		ebml::ElementID id;
		uint64_t pos;

		SeekItem(const ebml::ElementID p_id, const uint64_t p_pos) :
				id(p_id), pos(p_pos) {}
	};

	// SEEK HEAD
	auto parse_seek_head = [&](const ebml::ElementMaster *const p_seek_head, std::vector<SeekItem> &r_items) {
		for (const ebml::Element *const seek : stream->range(p_seek_head)) {
			if (seek->reg.id != ELEMENT_SEEK) {
#ifdef __EXCEPTIONS
				throw std::runtime_error("SeekHead element is not a Seek.");
#else
				continue;
#endif
			}
			const auto s = (const ebml::ElementMaster *)seek;

			auto search = stream->range(s).search();
			const auto seek_id = search.get<ELEMENT_SEEK_ID, ebml::ElementBinary>();
			const auto seek_position = search.get<ELEMENT_SEEK_POSITION, ebml::ElementUint>();

			ebml::BufferStream id_stream(seek_id->data, seek_id->size);

			uint64_t pos = 0;
			ebml::ElementID id;
			id_stream.read_id(pos, id);

			r_items.emplace_back(id, p_seek_head->pos + seek_position->value);
		}
	};

	// SEGMENT ELEMENTS (SEEK HEAD, INFO, TRACKS, CUES)
	auto parse_segment = [&](const ebml::ElementMaster *const p_segment) {
		// First, search for the seek head element. It should be the first element but for simplicity we will just
		// iterate until we find it.

		auto search = stream->range(p_segment).search();
		const auto seek_head = search.get<ELEMENT_SEEK_HEAD, ebml::ElementMaster>();

		std::vector<SeekItem> seek_items;
		parse_seek_head(seek_head, seek_items);

		bool parsed_info = false, parsed_tracks = false, parsed_cues = false;

		// Info
		uint64_t time_scale = 0;
		double raw_duration = 0.0;

		// Tracks
		uint64_t number = 0;
		double sampling_rate = 0.0;
		uint64_t channels = 0;

		// Cues
		std::vector<RawCuePoint> raw_cues;

		for (const SeekItem &seek_item : seek_items) {
			uint64_t pos = seek_item.pos;
			const ebml::Element *child;
			stream->read_element(pos, child);

			switch (child->reg.id) {
				case ELEMENT_INFO: {
					const ebml::ElementMaster *const c = (const ebml::ElementMaster *)child;
					parse_segment_info(c, time_scale, raw_duration);
					parsed_info = true;
				} break;
				case ELEMENT_TRACKS: {
					const ebml::ElementMaster *const c = (const ebml::ElementMaster *)child;
					parse_segment_tracks(c, number, sampling_rate, channels);
					parsed_tracks = true;
				} break;
				case ELEMENT_CUES: {
					const ebml::ElementMaster *const c = (const ebml::ElementMaster *)child;
					parse_segment_cues(p_segment, c, raw_cues);
					parsed_cues = true;
				} break;
			}

			delete child;

			if (parsed_info && parsed_tracks && parsed_cues) {
				break;
			}
		}

		if (!parsed_info) {
#ifdef __EXCEPTIONS
			throw std::runtime_error("Segment seek head does not have an info element.");
#else
			return;
#endif
		}
		if (!parsed_tracks) {
#ifdef __EXCEPTIONS
			throw std::runtime_error("Segment seek head does not have a tracks element.");
#else
			return;
#endif
		}
		if (!parsed_cues) {
#ifdef __EXCEPTIONS
			throw std::runtime_error("Segment seek head does not have a cues element.");
#else
			return;
#endif
		}

		if (raw_cues.empty()) {
#ifdef __EXCEPTIONS
			throw std::runtime_error("Segment seek head does not have any cues.");
#else
			return;
#endif
		}

		const double &duration = get_time(time_scale, raw_duration);

		for (uint64_t i = 0; i < raw_cues.size(); ++i) {
			const RawCuePoint &cue = raw_cues[i];
			const double &start = get_time(time_scale, cue.raw_time);

			double end;
			if (i + 1 < raw_cues.size()) {
				end = get_time(time_scale, raw_cues[i + 1].raw_time);
			} else {
				end = duration;
			}

			context.cues.push_back(CuePoint(cue.pos, start, end - start));
		}

		int opus_create_result;
		OpusDecoder *const opus = opus_decoder_create(sampling_rate, channels, &opus_create_result);
		if (opus_create_result != OPUS_OK) {
#ifdef __EXCEPTIONS
			throw std::runtime_error("Failed to create opus decoder.");
#else
			return;
#endif
		}

		context.time_scale = time_scale;
		context.duration = duration;
		context.track = number;
		context.sampling_rate = sampling_rate;
		context.channels = channels;

		context.opus = opus;
		context.opus_frame_samples = sampling_rate * 0.06 + 0.5;
		context.opus_pcm = new float[context.opus_frame_samples * channels];

		context.ready = true;
	};

	// MAIN ELEMENTS (EBML, SEGMENT)
	auto parse_file = [&]() {
		auto search = stream->range().search();
		const auto segment = search.get<ELEMENT_SEGMENT, ebml::ElementMaster>();
		parse_segment(segment);
	};

	parse_file();
}

void webm::Decoder::_thread_func(void *p_self) {
	Decoder *const self = (Decoder *)p_self;
#ifdef __EXCEPTIONS
	try {
#endif
		self->thread_func();
#ifdef __EXCEPTIONS
	} catch (const std::exception &e) {
		std::cerr << "Decoder failed with an exception: '" << e.what() << "'." << std::endl;
		self->terminate_thread = true;
	}
#endif
}

void webm::Decoder::thread_func() {
	if (stream->get_length() == 0) {
#ifdef __EXCEPTIONS
		throw std::runtime_error("Decoder stream is empty.");
#else
		return;
#endif
	}

	load_headers();

	auto get_cue_before_time = [&](const double &p_time) -> uint64_t {
		// TODO: Binary search will be faster instead.
		for (uint64_t i = 1; i < context.cues.size(); ++i) {
			const CuePoint &cluster = context.cues[i];
			if (cluster.time > p_time) {
				return i - 1;
			}
		}
		return context.cues.size() - 1;
	};

	auto read_cluster = [&](
								const ebml::ElementMaster *const p_cluster,
								std::vector<const ebml::Element *> &r_blocks) {
#ifdef __EXCEPTIONS
		try {
#endif
			r_blocks.reserve(r_blocks.size() + 100);

			for (uint64_t pos = p_cluster->from; pos < p_cluster->to;) {
				const ebml::Element *block;
				stream->read_element(pos, block);

				r_blocks.push_back(block);
			}
#ifdef __EXCEPTIONS
		} catch (const std::exception &e) {
			std::cerr << "Cluster read failed with an exception: '" << e.what() << "'." << std::endl;
		}
#endif
	};

	while (!terminate_thread) {
		bool seek_job;
		double seek_time;
		{
			std::lock_guard<std::mutex> lock(seeking.mutex);

			seek_job = seeking.job;
			seek_time = seeking.time;

			seeking.job = false;
		}

		if (seek_job) {
			const uint64_t &cue_index = get_cue_before_time(seek_time);
			const CuePoint &cue = context.cues[cue_index];

			const double percent = (seek_time - cue.time) / cue.duration;
			if (percent >= 1.0) {
				std::lock_guard<std::mutex> lock(context.mutex);

				for (const std::vector<const ebml::Element *> &cluster : context.clusters) {
					context.delete_cluster(cluster);
				}
				context.clusters.clear();

				context.current_cluster = context.cues.size();
				context.active_cluster = 0;
				context.active_block = 0;

				context.opus_pcm_size = 0;
			} else if (context.current_cluster <= cue_index && cue_index < context.current_cluster + context.clusters.size()) {
				// We already have this cluster in the cache.

				std::lock_guard<std::mutex> lock(context.mutex);

				context.active_cluster = cue_index - context.current_cluster;
				context.active_block = percent * context.clusters[context.active_cluster].size();
				context.trim_clusters();

				context.opus_pcm_size = 0;
			} else {
				// We do not have this cluster in the cache, so load it.

				{
					std::lock_guard<std::mutex> lock(context.mutex);

					for (const std::vector<const ebml::Element *> &cluster : context.clusters) {
						context.delete_cluster(cluster);
					}
					context.clusters.clear();

					context.opus_pcm_size = 0;
				}

				const ebml::Element *element;
				uint64_t pos = cue.pos;
				stream->read_element(pos, element);

				std::vector<const ebml::Element *> blocks;
				read_cluster((const ebml::ElementMaster *)element, blocks);

				std::lock_guard<std::mutex> lock(context.mutex);

				context.clusters.push_back(blocks);

				context.current_cluster = cue_index;
				context.active_cluster = 0;
				context.active_block = percent * blocks.size();
			}
		}

		const uint64_t &load_next = context.current_cluster + context.clusters.size();
		if (load_next < context.cues.size()) {
			const CuePoint &cue = context.cues[load_next];
			if (cue.time < position + 10.0) {
				uint64_t pos = cue.pos;
				const ebml::Element *element;
				stream->read_element(pos, element);

				std::vector<const ebml::Element *> blocks;
				read_cluster((const ebml::ElementMaster *)element, blocks);

				std::lock_guard<std::mutex> lock(context.mutex);

				context.clusters.push_back(blocks);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

double webm::Decoder::get_sample_rate() const {
	if (context.ready) {
		return context.sampling_rate;
	} else {
		return 0.0;
	}
}

double webm::Decoder::get_duration() const {
	if (context.ready) {
		return context.duration;
	} else {
		return 0.0;
	}
}

double webm::Decoder::get_position() const {
	return position;
}

void webm::Decoder::seek(const double p_time) {
	const std::lock_guard<std::mutex> lock(seeking.mutex);

	position = p_time;
	seeking.time = p_time;
	seeking.job = true;
}

void webm::Decoder::sample(
		audio::AudioFrame *const p_buffer,
		const uint64_t p_frames,
		bool &r_active,
		bool &r_buffering) {
	// If an error occured at some point, play silence.
	if (terminate_thread) {
		for (uint64_t i = 0; i < p_frames; ++i) {
			p_buffer[i] = audio::AudioFrame();
		}
		r_active = true;
		r_buffering = false;
		return;
	}

	// If context is not ready, keep active while we're waiting.
	if (!context.ready) {
		for (uint64_t i = 0; i < p_frames; ++i) {
			p_buffer[i] = audio::AudioFrame();
		}
		r_active = true;
		if(++context.sample_attempts > 10) {
			r_buffering = true;
		}
		return;
	}

	const std::lock_guard<std::mutex> slock(seeking.mutex);
	const std::lock_guard<std::mutex> clock(context.mutex);

	const auto parse_block = [&](const ebml::Element *const p_block) -> bool {
		switch (p_block->reg.id) {
			case ELEMENT_TIMECODE: {
				context.opus_pcm_index = 0;
				context.opus_pcm_size = 0;
			}
				return true;
			case ELEMENT_SIMPLE_BLOCK: {
				const ebml::ElementBinary *const block = (const ebml::ElementBinary *)p_block;

				ebml::BufferStream block_stream(block->data, block->size);

				uint64_t pos = 0;

				int64_t track;
				block_stream.read_int(pos, track);

				// Ignore blocks that are not the audio track.
				if (uint64_t(track) != context.track) {
					return false;
				}

				pos += 2; // Consume timecode.
				pos += 1; // Consume flags.

				const int samples = opus_decode_float(
						context.opus,
						block->data + pos,
						block->size - pos,
						context.opus_pcm,
						context.opus_frame_samples,
						0);

				if (samples < 0) {
#ifdef __EXCEPTIONS
					throw std::runtime_error("Failed to decode opus block.");
#else
					std::cerr << "Failed to decode opus block." << std::endl;
					return false;
#endif
				}

				context.opus_pcm_index = 0;
				context.opus_pcm_size = samples;
			}
				return true;
			case ELEMENT_BLOCK_GROUP: {
				// No behaviour for BlockGroup.
			}
				return true;
			default: {
				std::cerr << "Invalid audio block: " << p_block->reg.name << "." << std::endl;
			}
				return false;
		}
	};

	for (uint64_t pos = 0; pos < p_frames;) {
		if(context.current_cluster + context.active_cluster >= context.cues.size()) {
			r_active = false;
			r_buffering = false;
			return;
		}

		if (seeking.job || context.active_cluster >= context.clusters.size()) {
			for (; pos < p_frames; ++pos) {
				p_buffer[pos] = audio::AudioFrame();
			}
			r_active = true;
			if(++context.sample_attempts > 10) {
				r_buffering = true;
			}
			return;
		}

		if (context.opus_pcm_index >= context.opus_pcm_size) {
			while (context.active_cluster < context.clusters.size()) {
				const std::vector<const ebml::Element *> blocks = context.clusters[context.active_cluster];
				if (context.active_block < blocks.size()) {
					const ebml::Element *const block = blocks[context.active_block];
					const bool parse_flag = parse_block(block);

					++context.active_block;

					if (!parse_flag) {
						r_active = false;
						r_buffering = false;
						return;
					}

					break;
				}
				// Go to next cluster.
				++context.active_cluster;
				context.active_block = 0;

				context.trim_clusters();
			}
		}

		if (context.opus_pcm_index < context.opus_pcm_size) {
			const uint64_t copy = std::min(context.opus_pcm_size - context.opus_pcm_index, p_frames - pos);
			const uint64_t pcm_index = context.opus_pcm_index * context.channels;
			for (uint64_t i = 0; i < copy; ++i) {
				p_buffer[pos + i] = audio::AudioFrame(
						context.opus_pcm[pcm_index + i * context.channels],
						context.opus_pcm[pcm_index + i * context.channels + 1]);
			}

			position += copy / get_sample_rate();
			pos += copy;
			context.opus_pcm_index += copy;
			r_buffering = false;
		}
	}

	r_active = context.current_cluster + context.active_cluster < context.cues.size();
	context.sample_attempts = 0;
}

webm::Decoder::Decoder(ebml::Stream *const p_stream) :
		stream(p_stream), thread(std::thread(_thread_func, this)) {
}

webm::Decoder::~Decoder() {
	terminate_thread = true;
	thread.join();
}
