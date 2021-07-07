#include "obs.h"
#include "obs-module.h"

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <memory>
#include <numeric>
#include <algorithm>
#include <functional>

extern "C" {
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
}

#define TAG "[ffmpeg-afilter] "

#ifdef STANDALONE_MODULE
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ffmpeg-afilters", "en-US");

MODULE_EXPORT const char *obs_module_description(void)
{
	return "FFMPEG Audio Filters";
}
#endif

using namespace std;

set<string> g_filter_black_list_{
	"abench",     "afade",         "afifo",         "aloop",
	"ametadata",  "aperms",        "areverse",      "anull",
	"replaygain", "silencedetect", "silenceremove", "volumedetect"};

template<class T> class IAudioFilter {
protected:
	obs_properties_t *properties() { return obs_properties_create(); }

	void update(obs_data_t *s) {}

public:
	static void Destroy(void *data)
	{
		static_cast<T *>(data)->~T();
		bfree(data);
	}

	static void Update(void *data, obs_data_t *s)
	{
		static_cast<T *>(data)->update(s);
	}

	static void *Create(obs_data_t *settings, obs_source_t *filter)
	{
		void *memBlock = bzalloc(sizeof(T));
		T *thiz = new (memBlock) T(settings, filter);
		return thiz;
	}

	static obs_audio_data *FilterAudio(void *data, obs_audio_data *audio)
	{
		return static_cast<T *>(data)->filterAudio(audio);
	}

	static void Defaults(obs_data_t *settings) {}

	static obs_properties_t *Properties(void *data)
	{
		return static_cast<T *>(data)->properties();
	}
};

template<class T> void RegisterFilter()
{
	obs_source_info info{};
	info.id = T::Id();
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_AUDIO;
	info.get_name = &T::Name;
	info.create = &T::Create;
	info.destroy = &T::Destroy;
	info.update = &T::Update;
	info.filter_audio = &T::FilterAudio;
	info.get_defaults = &T::Defaults;
	info.get_properties = &T::Properties;
	obs_register_source(&info);
}

// ========== filter list
class FFAFilterCollection {
public:
	using CollectionType = map<string, const AVFilter *>;

	static FFAFilterCollection &Get();

	bool Init();
	CollectionType::const_iterator begin() const;
	CollectionType::const_iterator end() const;
	CollectionType::const_iterator find(const string &key) const;

private:
	CollectionType filter_list_;
};

// ========== filter options
class FFAFilterOpts {
	// for global
public:
	using UPtr = unique_ptr<FFAFilterOpts>;
	static const FFAFilterOpts *Get(const string &name);

private:
	static mutex cache_mutex_;
	static map<string, FFAFilterOpts::UPtr> cache_;

	// for instance
public:
	struct UnitDef {
		const char *name;
		int64_t val;
		const char *help;
	};

	using Units = vector<UnitDef>;
	using UnitsPtr = shared_ptr<Units>;

	struct Opt {
		const char *name;
		const char *help;
		int max;
		int min;
		int type;
		decltype(AVOption::default_val) default_val;
		UnitsPtr units;
	};

	void AddToProperties(obs_properties *props, obs_data *settings,
			     const obs_audio_info &oai) const;
	AVDictionary *LoadOBSSettings(AVFilterContext *filterCtx,
				      obs_data *settings) const;

private:
	FFAFilterOpts() {}
	bool Load(const AVFilter *filter);

	string filter_name_;
	list<Opt> opts_;
	map<string, UnitsPtr> units_;
};

// ========== filter
class FilterGraph {
	AVFilterGraph *graph_ = nullptr;
	AVFilterContext *inputFilter_ = nullptr;
	AVFilterContext *mainFilter_ = nullptr;
	AVFilterContext *outputFilter_ = nullptr;

	obs_audio_info oai_ = obs_audio_info{};

public:
	FilterGraph() {}
	~FilterGraph();

	bool initFilter(obs_data *settings, const obs_audio_info *oai);

	obs_audio_data *
	filterAudio(obs_audio_data *audio,
		    array<vector<float>, MAX_AV_PLANES> &frame_datas, bool end);
};

class auto_adapt_string {
	std::string buf_;

public:
	auto_adapt_string() {}

	auto_adapt_string(const char *str)
	{
		if (str)
			buf_ = str;
	}

	auto_adapt_string(const string &str) { buf_ = str; }

	template<class T> auto_adapt_string(const T &val)
	{
		buf_ = to_string(val);
	}

	operator const string &() const { return buf_; }
	operator const char *() const { return buf_.c_str(); };
};

int64_t speaker_layout_to_av_ch_layout(speaker_layout x)
{
	switch (x) {
	case speaker_layout::SPEAKERS_MONO:
		return AV_CH_LAYOUT_MONO;
	case speaker_layout::SPEAKERS_UNKNOWN:
	case speaker_layout::SPEAKERS_STEREO:
		return AV_CH_LAYOUT_STEREO;
	case speaker_layout::SPEAKERS_2POINT1:
		return AV_CH_LAYOUT_2POINT1;
	case speaker_layout::SPEAKERS_4POINT0:
		return AV_CH_LAYOUT_4POINT0;
	case speaker_layout::SPEAKERS_4POINT1:
		return AV_CH_LAYOUT_4POINT1;
	case speaker_layout::SPEAKERS_5POINT1:
		return AV_CH_LAYOUT_5POINT1;
	case speaker_layout::SPEAKERS_7POINT1:
		return AV_CH_LAYOUT_7POINT1;
	default:
		return speaker_layout_to_av_ch_layout(
			speaker_layout::SPEAKERS_UNKNOWN);
	}
}

speaker_layout av_ch_layout_to_speaker_layout(int x)
{
	switch (x) {
	case AV_CH_LAYOUT_MONO:
		return speaker_layout::SPEAKERS_MONO;
	case AV_CH_LAYOUT_STEREO:
		return speaker_layout::SPEAKERS_STEREO;
	case AV_CH_LAYOUT_2POINT1:
		return speaker_layout::SPEAKERS_2POINT1;
	case AV_CH_LAYOUT_4POINT0:
		return speaker_layout::SPEAKERS_4POINT0;
	case AV_CH_LAYOUT_4POINT1:
		return speaker_layout::SPEAKERS_4POINT1;
	case AV_CH_LAYOUT_5POINT1:
		return speaker_layout::SPEAKERS_5POINT1;
	case AV_CH_LAYOUT_7POINT1:
		return speaker_layout::SPEAKERS_7POINT1;
	default:
		return speaker_layout::SPEAKERS_UNKNOWN;
	}
}

class FFAFilter : public IAudioFilter<FFAFilter> {
	static bool obs_properties_add_avfilter(obs_properties *props,
						obs_data *settings,
						const obs_audio_info &oai,
						const AVFilter *filter);

	static bool obs_properties_hide_other_avclass(obs_properties *props,
						      const AVFilter *filter);

	mutex graph_mutex_;
	unique_ptr<FilterGraph> filter_graph_;
	unique_ptr<FilterGraph> next_filter_graph_;
	string current_filter_name_;
	array<vector<float>, MAX_AV_PLANES> last_audio_data_bufs_;
	obs_audio_data last_audio_data_;

public:
	FFAFilter(obs_data_t *settings, obs_source_t *filter);
	~FFAFilter() {}

	static const char *Name(void *type_data)
	{
		return "FFMPEG AudioFilter";
	}

	static const char *Id() { return "ffmpeg-audio-filter"; }

	void update(obs_data_t *s);
	obs_audio_data *filterAudio(obs_audio_data *audio);
	obs_properties_t *properties();
};

FFAFilterCollection &FFAFilterCollection::Get()
{
	static FFAFilterCollection instance;
	return instance;
}

bool FFAFilterCollection::Init()
{
	const AVFilter *flt = NULL;
	void *opaque = nullptr;
	bool has_filter_found = false;

#if LIBAVFILTER_VERSION_MAJOR >= 7
	while (flt = av_filter_iterate(&opaque))
#else
	while ((flt = avfilter_next(flt)) != NULL)
#endif
	{
		if (g_filter_black_list_.find(flt->name) !=
		    g_filter_black_list_.end())
			continue;
		if (avfilter_pad_count(flt->inputs) != 1)
			continue;
		if (avfilter_pad_count(flt->outputs) != 1)
			continue;
		auto inpad_type = avfilter_pad_get_type(flt->inputs, 0);
		if (!(inpad_type & AVMediaType::AVMEDIA_TYPE_AUDIO))
			continue;
		auto outpad_type = avfilter_pad_get_type(flt->outputs, 0);
		if (!(outpad_type & AVMEDIA_TYPE_AUDIO))
			continue;

		filter_list_.insert(make_pair(flt->name, flt));
		has_filter_found = true;
	}

	return true;
}

FFAFilterCollection::CollectionType::const_iterator
FFAFilterCollection::begin() const
{
	return filter_list_.begin();
}

FFAFilterCollection::CollectionType::const_iterator
FFAFilterCollection::end() const
{
	return filter_list_.end();
}

FFAFilterCollection::CollectionType::const_iterator
FFAFilterCollection::find(const string &key) const
{
	return filter_list_.find(key);
}

const FFAFilterOpts *FFAFilterOpts::Get(const string &name)
{
	lock_guard<mutex> lg(cache_mutex_);
	{ // cached
		auto it = cache_.find(name);
		if (it != cache_.end())
			return it->second.get();
	}
	{ // cache miss
		auto &newItem = cache_[name];
		auto filterCollection = FFAFilterCollection::Get();
		auto it = filterCollection.find(name);
		if (it != filterCollection.end()) {
			auto tmpItem = FFAFilterOpts::UPtr(new FFAFilterOpts());
			if (tmpItem->Load(it->second)) {
				newItem.swap(tmpItem);
				return newItem.get();
			}
		}

		return nullptr;
	}
}

bool FFAFilterOpts::Load(const AVFilter *filter)
{
	if (!filter)
		return false;

	auto opts = filter->priv_class ? filter->priv_class->option
						 ? filter->priv_class->option
						 : nullptr
				       : nullptr;
	if (!opts)
		return true;

	map<int, Opt *> offsets;
	filter_name_ = filter->name;

	for (int i = 0; opts[i].name; ++i) {
		auto &opt = opts[i];

		// check if alias
		if (opt.offset > 0) {
			auto it = offsets.find(opt.offset);
			if (it != offsets.end()) {
				int oldlen = strlen(it->second->name);
				int newlen = strlen(opt.name);
				if (newlen > oldlen)
					it->second->name = opt.name;
				continue;
			}
		}

		auto get_units = [this](const char *unitname) {
			auto it = units_.find(unitname);
			if (it == units_.end()) {
				auto &new_item = units_[unitname];
				new_item.reset(new vector<UnitDef>());
				return new_item;
			} else
				return it->second;
		};

		// for const
		if (opt.type == AV_OPT_TYPE_CONST) {
			get_units(opt.unit)->emplace_back(UnitDef{
				opt.name, opt.default_val.i64, opt.help});
		}
		// for options
		else {
			opts_.push_back({});
			auto &new_opt = opts_.back();

			new_opt.name = opt.name;
			new_opt.help = opt.help;
			new_opt.type = opt.type;
			new_opt.max = opt.max;
			new_opt.min = opt.min;
			new_opt.default_val = opt.default_val;

			if (opt.unit)
				new_opt.units = get_units(opt.unit);

			offsets[opt.offset] = &new_opt;
		}
	}

	return true;
}

static bool obs_property_set_visible(obs_properties *props,
				     const std::string &name)
{
	auto prop = obs_properties_get(props, name.c_str());
	if (prop) {
		obs_property_set_visible(prop, true);
		return true;
	}
	return false;
}

void FFAFilterOpts::AddToProperties(obs_properties *props, obs_data *settings,
				    const obs_audio_info &oai) const
{
	auto obs_properties_add_int_unit = [&](obs_properties *props,
					       const Opt &opt,
					       const string &optname) {
		auto unit = opt.units;
		auto prop = obs_properties_add_list(
			props, optname.c_str(), opt.name,
			obs_combo_type::OBS_COMBO_TYPE_LIST,
			obs_combo_format::OBS_COMBO_FORMAT_INT);
		for (auto x : *unit) {
			string itemstr;
			itemstr += x.name;
			if (x.help) {
				itemstr += ": ";
				itemstr += x.help;
			}
			obs_property_list_add_int(prop, itemstr.c_str(), x.val);
		}
	};

	auto obs_properties_add_flags = [&](const Opt &opt,
					    const string &optname) {
		auto units = opt.units;
		for (auto x : *units) {
			string flagname;
			flagname += optname;
			flagname += "+";
			flagname += x.name;

			string flaghelp;
			flaghelp += opt.name;
			flaghelp += ": ";
			if (x.help && *x.help)
				flaghelp += x.help;
			else
				flaghelp += x.name;

			if (obs_property_set_visible(props, flagname))
				continue;

			auto prop = obs_properties_add_bool(
				props, flagname.c_str(),
				flaghelp.c_str()); // enough space in ui to show desc
			bool default_val = (opt.default_val.i64 & x.val) != 0;
			obs_data_set_default_bool(settings, flagname.c_str(),
						  default_val);

			if (x.help)
				obs_property_set_long_description(prop, x.help);
		}
	};

	auto obs_property_list_add_chlayout = [&](const Opt &opt,
						  const string &optname) {
		auto prop = obs_properties_add_list(
			props, optname.c_str(), opt.name,
			obs_combo_type::OBS_COMBO_TYPE_LIST,
			obs_combo_format::OBS_COMBO_FORMAT_INT);
		obs_property_list_add_int(prop, "Mono", AV_CH_LAYOUT_MONO);
		obs_property_list_add_int(prop, "Stereo", AV_CH_LAYOUT_STEREO);
		obs_property_list_add_int(prop, "2.1", AV_CH_LAYOUT_2POINT1);
		obs_property_list_add_int(prop, "4.0", AV_CH_LAYOUT_4POINT0);
		obs_property_list_add_int(prop, "4.1", AV_CH_LAYOUT_4POINT1);
		obs_property_list_add_int(prop, "5.1", AV_CH_LAYOUT_5POINT1);
		obs_property_list_add_int(prop, "7.1", AV_CH_LAYOUT_7POINT1);
	};

	auto obs_properties_add_regular_type = [&](const Opt &opt,
						   auto_adapt_string optname) {
		if (obs_property_set_visible(props, optname))
			return true;

		obs_property *prop = nullptr;

		if (opt.type == AV_OPT_TYPE_FLOAT ||
		    opt.type == AV_OPT_TYPE_DOUBLE ||
		    opt.type == AV_OPT_TYPE_DURATION) {
			prop = obs_properties_add_float(props, optname,
							opt.name, opt.min,
							opt.max, 0.1);
			obs_data_set_default_double(settings, optname,
						    opt.default_val.dbl);
		} else if (opt.type == AV_OPT_TYPE_INT ||
			   opt.type == AV_OPT_TYPE_INT64) {
			if (opt.units) {
				obs_properties_add_int_unit(props, opt,
							    optname);
			} else {
				prop = obs_properties_add_int(props, optname,
							      opt.name, opt.min,
							      opt.max, 1);
			}
			obs_data_set_default_int(settings, optname,
						 opt.default_val.i64);
		} else if (opt.type == AV_OPT_TYPE_STRING) {
			prop = obs_properties_add_text(
				props, optname, opt.name,
				obs_text_type::OBS_TEXT_DEFAULT);
			obs_data_set_default_string(settings, optname,
						    opt.default_val.str);
		} else if (opt.type == AV_OPT_TYPE_BOOL) {
			prop = obs_properties_add_bool(props, optname,
						       opt.name);
			obs_data_set_default_bool(settings, optname,
						  opt.default_val.i64);
		} else if (opt.type == AV_OPT_TYPE_CHANNEL_LAYOUT) {
			obs_property_list_add_chlayout(opt, optname);
			obs_data_set_default_int(
				settings, optname,
				speaker_layout_to_av_ch_layout(oai.speakers));
		} else {
			blog(LOG_WARNING, TAG "unsupport type %d for %s",
			     opt.type, optname);
		}

		if (prop && opt.help)
			obs_property_set_long_description(prop, opt.help);
		return true;
	};

	for (auto &opt : opts_) {
		// construct option name
		auto_adapt_string optname = filter_name_ + "/" + opt.name;

		obs_property *prop = nullptr;

		// add property in correct type
		if (opt.type == AV_OPT_TYPE_FLAGS && opt.units)
			obs_properties_add_flags(opt, optname);
		else
			obs_properties_add_regular_type(opt, optname);
	}
}

AVDictionary *FFAFilterOpts::LoadOBSSettings(AVFilterContext *filterCtx,
					     obs_data *settings) const
{
	AVDictionary *optdict = nullptr;

	auto obs_data_get_str = [&](obs_data_item *item) -> auto_adapt_string {
		assert(item != nullptr);
		switch (obs_data_item_gettype(item)) {
		case OBS_DATA_NUMBER:
			switch (obs_data_item_numtype(item)) {
			case OBS_DATA_NUM_INT:
				return obs_data_item_get_int(item);
			case OBS_DATA_NUM_DOUBLE:
				return obs_data_item_get_double(item);
			default:
				assert(false);
				return {};
			}
		case OBS_DATA_STRING:
			return obs_data_item_get_string(item);
		case OBS_DATA_BOOLEAN:
			return obs_data_item_get_bool(item) ? 1 : 0;
		default:
			assert(false);
			return {};
		}
	};

	auto obs_data_to_regular_avopt = [&](const char *obsname,
					     const char *optname) {
		auto item = obs_data_item_byname(settings, obsname);
		if (item) {
			auto strval = obs_data_get_str(item);
			int err = av_dict_set(&optdict, optname, strval, 0);
			return true;
		} else
			return false;
	};

	auto obs_data_to_flag_avopt = [&](const char *optname,
					  const Units &units) {
		int64_t flags = 0;
		for (auto &x : units) {
			auto_adapt_string flagname =
				std::string(optname) + "+" + x.name;
			if (obs_data_has_user_value(settings, flagname))
				if (obs_data_get_bool(settings, flagname))
					flags |= x.val;
		}
		av_opt_set(&optdict, optname, auto_adapt_string(flags), 0);
		return true;
	};

	for (auto &opt : opts_) {
		// construct option name
		string optname = filter_name_ + "/" + opt.name;

		// add property in correct type
		if (opt.type == AV_OPT_TYPE_FLAGS && opt.units) {
			obs_data_to_flag_avopt(optname.c_str(), *opt.units);
		} else if (obs_data_has_user_value(settings, optname.c_str())) {
			obs_data_to_regular_avopt(optname.c_str(), opt.name);
		}
	}

	return optdict;
}

mutex FFAFilterOpts::cache_mutex_;
map<string, FFAFilterOpts::UPtr> FFAFilterOpts::cache_;

FilterGraph::~FilterGraph()
{
	avfilter_free(inputFilter_);
	avfilter_free(mainFilter_);
	avfilter_free(outputFilter_);
	avfilter_graph_free(&graph_);
}

bool FilterGraph::initFilter(obs_data *settings, const obs_audio_info *oai)
{
	auto filter_name = obs_data_get_string(settings, "filter_name");
	if (!filter_name || !*filter_name)
		return false;

	oai_ = *oai;

	const AVFilter *filter = avfilter_get_by_name(filter_name);
	if (!filter)
		return false;

	auto graph = avfilter_graph_alloc();
	AVFilterContext *input_buffer_filter = 0, *output_buffer_filter = 0,
			*main_filter = 0;
	AVDictionary *main_filter_optdict = 0;

	auto on_failed = [&]() -> bool {
		avfilter_free(input_buffer_filter);
		avfilter_free(output_buffer_filter);
		avfilter_free(main_filter);
		av_dict_free(&main_filter_optdict);
		avfilter_graph_free(&graph);
		return false;
	};

	// ====== init input buffer
	input_buffer_filter = avfilter_graph_alloc_filter(
		graph, avfilter_get_by_name("abuffer"), "src");
	char ch_layout[64];
	av_get_channel_layout_string(
		ch_layout, sizeof(ch_layout), 0,
		speaker_layout_to_av_ch_layout(oai->speakers));
	av_opt_set(input_buffer_filter, "channel_layout", ch_layout,
		   AV_OPT_SEARCH_CHILDREN);
	av_opt_set(input_buffer_filter, "sample_fmt",
		   av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP),
		   AV_OPT_SEARCH_CHILDREN);
	av_opt_set_q(input_buffer_filter, "time_base",
		     AVRational{1, 1000000000}, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(input_buffer_filter, "sample_rate", oai->samples_per_sec,
		       AV_OPT_SEARCH_CHILDREN);
	if (avfilter_init_str(input_buffer_filter, nullptr) < 0) {
		blog(LOG_WARNING, TAG "fail to apply options for inputbuffer");
		return on_failed();
	}

	// ====== init output buffer
	output_buffer_filter = avfilter_graph_alloc_filter(
		graph, avfilter_get_by_name("abuffersink"), "sink");
	int64_t channel_layouts[] = {
		speaker_layout_to_av_ch_layout(oai->speakers), -1};
	av_opt_set_int_list(output_buffer_filter, "channel_layouts",
			    channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
	int sample_fmts[] = {AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE};
	av_opt_set_int_list(output_buffer_filter, "sample_fmts", sample_fmts,
			    AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	int sample_rates[] = {oai->samples_per_sec, -1};
	av_opt_set_int_list(output_buffer_filter, "sample_rates", sample_rates,
			    -1, AV_OPT_SEARCH_CHILDREN);
	if (avfilter_init_str(output_buffer_filter, nullptr) < 0) {
		blog(LOG_WARNING, TAG "fail to apply options for outputbuffer");
		return on_failed();
	}

	// ====== init main filter
	main_filter = avfilter_graph_alloc_filter(graph, filter, "main_filter");
	auto filterOpts = FFAFilterOpts::Get(filter->name);
	if (!filterOpts) {
		blog(LOG_WARNING, TAG "fail to parse %s options", filter_name);
		return on_failed();
	}
	main_filter_optdict =
		filterOpts->LoadOBSSettings(main_filter, settings);
	if (avfilter_init_dict(main_filter, &main_filter_optdict) < 0) {
		blog(LOG_WARNING, TAG "fail to apply options for %s",
		     filter_name);
		return on_failed();
	}

	// ====== link filters
	if (avfilter_link(input_buffer_filter, 0, main_filter, 0) < 0) {
		blog(LOG_WARNING, TAG "fail to connect inputbuffer to %s",
		     filter_name);
		return on_failed();
	}
	if (avfilter_link(main_filter, 0, output_buffer_filter, 0) < 0) {
		blog(LOG_WARNING, TAG "fail to connect %s to outputbuffer",
		     filter_name);
		return on_failed();
	}

	// ====== config graph
	if (avfilter_graph_config(graph, nullptr) >= 0) {
		graph_ = graph;
		inputFilter_ = input_buffer_filter;
		mainFilter_ = main_filter;
		outputFilter_ = output_buffer_filter;
		blog(LOG_INFO, TAG "use %s", filter_name);
	} else {
		blog(LOG_WARNING, TAG "create %s failed", filter_name);
		return on_failed();
	}

	av_dict_free(&main_filter_optdict);

	return true;
}

obs_audio_data *
FilterGraph::filterAudio(obs_audio_data *audio,
			 array<vector<float>, MAX_AV_PLANES> &frame_datas,
			 bool end)
{
	AVFrame *fin = nullptr, *fout = nullptr;
	auto result = [&](obs_audio_data *audio) {
		av_frame_unref(fin);
		av_frame_unref(fout);
		return audio;
	};

	if (!graph_)
		return result(audio);

	// ====== feed data into graph
	fin = av_frame_alloc();
	fin->sample_rate = oai_.samples_per_sec;
	fin->format = AV_SAMPLE_FMT_FLTP;
	fin->channel_layout = speaker_layout_to_av_ch_layout(oai_.speakers);
	fin->nb_samples = audio->frames;
	fin->pts = audio->timestamp;
	fin->linesize[0] = audio->frames * sizeof(float);
	fin->channels = av_get_channel_layout_nb_channels(fin->channel_layout);
	for (int i = 0; i < fin->channels; ++i)
		fin->data[i] = audio->data[i];

	int inerr = av_buffersrc_add_frame_flags(inputFilter_, fin, 0);
	if (inerr < 0)
		return result(audio);
	if (end)
		av_buffersrc_add_frame_flags(inputFilter_, nullptr, 0);

	// ====== retrieve data from graph
	fout = av_frame_alloc();
	int outerr = av_buffersink_get_frame(outputFilter_, fout);
	if (outerr == AVERROR(EAGAIN))
		return result(nullptr);
	else if (end && outerr == AVERROR_EOF)
		return result(nullptr);
	else if (outerr < 0)
		return result(audio);

	for (int i = 0; i < fout->channels; ++i) {
		frame_datas[i].resize(fout->nb_samples);
		copy_n((float *)fout->extended_data[i], fout->nb_samples,
		       frame_datas[i].data());
		audio->data[i] = (uint8_t *)frame_datas[i].data();
	}
	audio->timestamp = av_rescale_q(
		fout->pts, av_buffersink_get_time_base(outputFilter_),
		AVRational{1, 1000000000});
	audio->frames = fout->nb_samples;

	return result(audio);
}

bool FFAFilter::obs_properties_add_avfilter(obs_properties *props,
					    obs_data *settings,
					    const obs_audio_info &oai,
					    const AVFilter *filter)
{
	auto filterOpts = FFAFilterOpts::Get(filter->name);
	if (!filterOpts)
		return false;

	filterOpts->AddToProperties(props, settings, oai);
	return true;
}

bool FFAFilter::obs_properties_hide_other_avclass(obs_properties *props,
						  const AVFilter *filter)
{
	auto cur_prop = obs_properties_first(props);
	do {
		if (!cur_prop)
			break;
		const char *prop_name = obs_property_name(cur_prop);
		if (!prop_name)
			continue;
		const char *slashpos = strchr(prop_name, '/');
		if (slashpos) {
			int name_len = slashpos - prop_name;
			if (strncmp(filter->name, prop_name, name_len) != 0)
				obs_property_set_visible(cur_prop, false);
		}

	} while (obs_property_next(&cur_prop));

	return true;
}

FFAFilter::FFAFilter(obs_data_t *settings, obs_source_t *filter)
{
	update(settings);
}

void FFAFilter::update(obs_data_t *s)
{
	lock_guard<mutex> lg(graph_mutex_);
	next_filter_graph_.reset(new FilterGraph());
	obs_audio_info oai;
	obs_get_audio_info(&oai);
	next_filter_graph_->initFilter(s, &oai);
}

obs_audio_data *FFAFilter::filterAudio(obs_audio_data *audio)
{
	lock_guard<mutex> lg(graph_mutex_);
	if (!filter_graph_ && next_filter_graph_)
		filter_graph_.swap(next_filter_graph_);

	if (!filter_graph_)
		return audio;

	last_audio_data_ = *audio;

	auto result = filter_graph_->filterAudio(
		&last_audio_data_, last_audio_data_bufs_, !!next_filter_graph_);
	if (next_filter_graph_) {
		filter_graph_.swap(next_filter_graph_);
		next_filter_graph_.reset();
	}

	return result;
}

obs_properties_t *FFAFilter::properties()
{
	auto props = obs_properties_create();
	obs_properties_set_param(props, this, nullptr);

	auto filterlist = obs_properties_add_list(
		props, "filter_name", obs_module_text("Filter"),
		obs_combo_type::OBS_COMBO_TYPE_LIST,
		obs_combo_format::OBS_COMBO_FORMAT_STRING);

	for (auto &x : FFAFilterCollection::Get())
		obs_property_list_add_string(filterlist, x.first.c_str(),
					     x.first.c_str());

	auto callback = [](obs_properties_t *props, obs_property_t *property,
			   obs_data_t *settings) -> bool {
		auto filterCollection = FFAFilterCollection::Get();
		FFAFilter *thiz = (FFAFilter *)obs_properties_get_param(props);
		const char *filtername =
			obs_data_get_string(settings, "filter_name");

		if (!filtername)
			return true;

		auto it = filterCollection.find(filtername);
		if (it == filterCollection.end())
			return true;

		auto &filter = it->second;

		obs_audio_info oai;
		obs_get_audio_info(&oai);

		obs_properties_hide_other_avclass(props, filter);
		obs_properties_add_avfilter(props, settings, oai, filter);

		thiz->current_filter_name_ = filtername;
		return true;
	};

	obs_property_set_modified_callback(filterlist, callback);
	return props;
}

extern "C" {
bool register_ffmepg_audio_filter()
{
#if LIBAVFILTER_VERSION_MAJOR < 7
	avfilter_register_all();
#endif
	if (FFAFilterCollection::Get().Init()) {
		RegisterFilter<FFAFilter>();
		return true;
	} else
		return false;
}
#ifdef STANDALONE_MODULE
bool obs_module_load()
{
	auto log_callback = [](void *ud, int level, const char *fmt,
			       va_list args) { blogva(LOG_INFO, fmt, args); };
	av_log_set_callback(log_callback);

	if (register_ffmepg_audio_filter())
		return true;
	else
		return false;
}

void obs_module_unload() {}
#endif
};