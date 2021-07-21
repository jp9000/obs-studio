#include "window-utils.h"

#include <util/platform.h>

#define WINDOW_NAME ((NSString *)kCGWindowName)
#define WINDOW_NUMBER ((NSString *)kCGWindowNumber)
#define OWNER_NAME ((NSString *)kCGWindowOwnerName)
#define OWNER_PID ((NSNumber *)kCGWindowOwnerPID)

static NSComparator win_info_cmp = ^(NSDictionary *o1, NSDictionary *o2) {
	NSComparisonResult res = [o1[OWNER_NAME] compare:o2[OWNER_NAME]];
	if (res != NSOrderedSame)
		return res;

	res = [o1[OWNER_PID] compare:o2[OWNER_PID]];
	if (res != NSOrderedSame)
		return res;

	res = [o1[WINDOW_NAME] compare:o2[WINDOW_NAME]];
	if (res != NSOrderedSame)
		return res;

	return [o1[WINDOW_NUMBER] compare:o2[WINDOW_NUMBER]];
};

NSArray *enumerate_windows(void)
{
	NSArray *arr = (NSArray *)CGWindowListCopyWindowInfo(
		kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
	[arr autorelease];

	return [arr sortedArrayUsingComparator:win_info_cmp];
}

#define WAIT_TIME_MS 500
#define WAIT_TIME_US WAIT_TIME_MS * 1000
#define WAIT_TIME_NS WAIT_TIME_US * 1000

bool find_window(cocoa_window_t cw, obs_data_t *settings)
{
	uint64_t ts = os_gettime_ns();

	if (cw->last_search_time + WAIT_TIME_NS > ts)
		return false;

	cw->last_search_time = ts;

	assert(pthread_mutex_lock(&cw->mutex) == 0);

	if (!cw->window_name.length && !cw->owner_name.length)
		goto unlock;

	for (NSDictionary *dict in enumerate_windows()) {
		bool owner_names_match =
			(!cw->owner_name.length && !dict[OWNER_NAME]) ||
			[cw->owner_name isEqualToString:dict[OWNER_NAME]];
		bool window_names_match =
			(!cw->window_name.length && !dict[WINDOW_NAME]) ||
			[cw->window_name isEqualToString:dict[WINDOW_NAME]];

		if (!owner_names_match || !window_names_match)
			continue;

		assert(pthread_mutex_unlock(&cw->mutex) == 0);

		cw->window_id = [dict[WINDOW_NUMBER] intValue];
		cw->owner_pid = [dict[OWNER_PID] intValue];

		obs_data_set_int(settings, "window", cw->window_id);
		obs_data_set_int(settings, "owner_pid", cw->owner_pid);

		return true;
	}

unlock:
	assert(pthread_mutex_unlock(&cw->mutex) == 0);

	return false;
}

void init_window(cocoa_window_t cw, obs_data_t *settings)
{
	assert(pthread_mutex_init(&cw->mutex, NULL) == 0);

	cw->window_id = obs_data_get_int(settings, "window");
	cw->owner_pid = obs_data_get_int(settings, "owner_pid");

	cw->owner_name = @(obs_data_get_string(settings, "owner_name"));
	cw->window_name = @(obs_data_get_string(settings, "window_name"));
	[cw->owner_name retain];
	[cw->window_name retain];

	cw->last_search_time = 0;

	if (!cw->window_name.length && !cw->owner_name.length)
		return;

	NSNumber *window_id = @(cw->window_id);
	NSNumber *owner_pid = @(cw->owner_pid);

	for (NSDictionary *dict in enumerate_windows()) {
		bool owner_names_match =
			(!cw->owner_name.length && !dict[OWNER_NAME]) ||
			[cw->owner_name isEqualToString:dict[OWNER_NAME]];
		bool ids_match =
			[owner_pid isEqualToNumber:dict[OWNER_PID]] &&
			[window_id isEqualToNumber:dict[WINDOW_NUMBER]];
		bool window_names_match =
			(!cw->window_name.length && !dict[WINDOW_NAME]) ||
			[cw->window_name isEqualToString:dict[WINDOW_NAME]];

		if (owner_names_match && (ids_match || window_names_match)) {
			cw->window_id = [dict[WINDOW_NUMBER] intValue];
			cw->owner_pid = [dict[OWNER_PID] intValue];

			obs_data_set_int(settings, "window", cw->window_id);
			obs_data_set_int(settings, "owner_pid", cw->owner_pid);

			break;
		}
	}
}

void destroy_window(cocoa_window_t cw)
{
	[cw->owner_name release];
	[cw->window_name release];

	pthread_mutex_destroy(&cw->mutex);
}

void update_window(cocoa_window_t cw, obs_data_t *settings)
{
	assert(pthread_mutex_lock(&cw->mutex) == 0);

	[cw->owner_name release];
	[cw->window_name release];

	cw->owner_name = @(obs_data_get_string(settings, "owner_name"));
	cw->window_name = @(obs_data_get_string(settings, "window_name"));
	[cw->owner_name retain];
	[cw->window_name retain];

	assert(pthread_mutex_unlock(&cw->mutex) == 0);

	cw->owner_pid = obs_data_get_int(settings, "owner_pid");
	cw->window_id = obs_data_get_int(settings, "window");
}

static inline char *make_name(NSString *owner, NSString *name)
{
	if (!owner.length)
		return bstrdup("");

	NSString *nsstr = [NSString stringWithFormat:@"[%@] %@", owner, name];
	char *str = bstrdup_n(nsstr.UTF8String, nsstr.length);

	return str;
}

static inline bool window_changed_internal(obs_property_t *p,
					   obs_data_t *settings)
{
	NSNumber *window_id = @(obs_data_get_int(settings, "window"));
	NSNumber *owner_pid = @(obs_data_get_int(settings, "owner_pid"));
	NSString *owner_name = @(obs_data_get_string(settings, "owner_name"));
	NSString *window_name = @(obs_data_get_string(settings, "window_name"));

	bool show_empty_names = obs_data_get_bool(settings, "show_empty_names");

	NSDictionary *win_info = @{
		OWNER_NAME: owner_name,
		OWNER_PID: owner_pid,
		WINDOW_NAME: window_name,
		WINDOW_NUMBER: window_id,
	};

	NSArray *arr = enumerate_windows();
	NSDictionary *cur = nil;

	for (NSDictionary *dict in arr) {
		if ([window_id isEqualToNumber:dict[WINDOW_NUMBER]]) {
			cur = dict;
			break;
		}
	}

	bool window_found = cur != nil;
	bool window_added = window_found;

	obs_property_list_clear(p);
	for (NSDictionary *dict in arr) {
		if (!window_added &&
		    win_info_cmp(win_info, dict) == NSOrderedAscending) {
			window_added = true;

			char *name = make_name(owner_name, window_name);

			size_t idx = obs_property_list_add_int(
				p, name, window_id.intValue);

			bfree(name);

			obs_property_list_item_disable(p, idx, true);
		}

		if (!show_empty_names &&
		    (!dict[WINDOW_NAME] || ![dict[WINDOW_NAME] length]) &&
		    ![window_id isEqualToNumber:dict[WINDOW_NUMBER]])
			continue;

		char *name = make_name(dict[OWNER_NAME], dict[WINDOW_NAME]);

		obs_property_list_add_int(p, name,
					  [dict[WINDOW_NUMBER] intValue]);

		bfree(name);
	}

	if (!window_added) {
		char *name = make_name(owner_name, window_name);

		size_t idx =
			obs_property_list_add_int(p, name, window_id.intValue);

		bfree(name);

		obs_property_list_item_disable(p, idx, true);
	}

	if (!window_found)
		return true;

	obs_data_set_int(settings, "window", [cur[WINDOW_NUMBER] intValue]);
	obs_data_set_int(settings, "owner_pid", [cur[OWNER_PID] intValue]);
	obs_data_set_string(settings, "owner_name",
			    [cur[OWNER_NAME] UTF8String]);
	obs_data_set_string(settings, "window_name",
			    [cur[WINDOW_NAME] UTF8String]);

	return true;
}

static bool window_changed(obs_properties_t *props, obs_property_t *p,
			   obs_data_t *settings)
{
	UNUSED_PARAMETER(props);

	@autoreleasepool {
		return window_changed_internal(p, settings);
	}
}

static bool toggle_empty_names(obs_properties_t *props, obs_property_t *p,
			       obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	return window_changed(props, obs_properties_get(props, "window"),
			      settings);
}

void window_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "window", kCGNullWindowID);
	obs_data_set_default_bool(settings, "show_empty_names", false);
}

void add_window_properties(obs_properties_t *props)
{
	obs_property_t *window_list = obs_properties_add_list(
		props, "window", obs_module_text("WindowUtils.Window"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_set_modified_callback(window_list, window_changed);

	obs_property_t *empty = obs_properties_add_bool(
		props, "show_empty_names",
		obs_module_text("WindowUtils.ShowEmptyNames"));
	obs_property_set_modified_callback(empty, toggle_empty_names);
}

void show_window_properties(obs_properties_t *props, bool show)
{
	obs_property_set_visible(obs_properties_get(props, "window"), show);
	obs_property_set_visible(obs_properties_get(props, "show_empty_names"),
				 show);
}
