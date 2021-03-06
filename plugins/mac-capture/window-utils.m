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

bool find_window(cocoa_window_t cw, obs_data_t *settings, bool force)
{
	if (!force && cw->next_search_time > os_gettime_ns())
		return false;

	cw->next_search_time = os_gettime_ns() + WAIT_TIME_NS;

	pthread_mutex_lock(&cw->name_lock);

	if (!cw->window_name.length && !cw->owner_name.length)
		goto invalid_name;

	for (NSDictionary *dict in enumerate_windows()) {
		if (![cw->owner_name isEqualToString:dict[OWNER_NAME]])
			continue;

		if (![cw->window_name isEqualToString:dict[WINDOW_NAME]])
			continue;

		pthread_mutex_unlock(&cw->name_lock);

		NSNumber *window_id = (NSNumber *)dict[WINDOW_NUMBER];
		cw->window_id = window_id.intValue;
		NSNumber *owner_pid = (NSNumber *)dict[OWNER_PID];
		cw->owner_pid = owner_pid.intValue;

		obs_data_set_int(settings, "window", cw->window_id);
		obs_data_set_int(settings, "owner_pid", cw->owner_pid);
		return true;
	}

invalid_name:
	pthread_mutex_unlock(&cw->name_lock);
	return false;
}

void init_window(cocoa_window_t cw, obs_data_t *settings)
{
	pthread_mutex_init(&cw->name_lock, NULL);

	cw->owner_name = @(obs_data_get_string(settings, "owner_name"));
	cw->window_name = @(obs_data_get_string(settings, "window_name"));
	[cw->owner_name retain];
	[cw->window_name retain];

	// Find initial window.
	pthread_mutex_lock(&cw->name_lock);

	if (!cw->window_name.length && !cw->owner_name.length)
		goto invalid_name;

	NSNumber *owner_pid = @(obs_data_get_int(settings, "owner_pid"));
	NSNumber *window_id = @(obs_data_get_int(settings, "window"));
	for (NSDictionary *dict in enumerate_windows()) {
		bool owner_names_match =
			[cw->owner_name isEqualToString:dict[OWNER_NAME]];
		bool ids_match =
			[owner_pid isEqualToNumber:dict[OWNER_PID]] &&
			[window_id isEqualToNumber:dict[WINDOW_NUMBER]];
		bool window_names_match =
			[cw->window_name isEqualToString:dict[WINDOW_NAME]];

		if (owner_names_match && (ids_match || window_names_match)) {
			pthread_mutex_unlock(&cw->name_lock);

			NSNumber *window_id = (NSNumber *)dict[WINDOW_NUMBER];
			cw->window_id = window_id.intValue;
			NSNumber *owner_pid = (NSNumber *)dict[OWNER_PID];
			cw->owner_pid = owner_pid.intValue;

			obs_data_set_int(settings, "window", cw->window_id);
			obs_data_set_int(settings, "owner_pid", cw->owner_pid);
			return;
		}
	}

invalid_name:
	pthread_mutex_unlock(&cw->name_lock);
	return;
}

void destroy_window(cocoa_window_t cw)
{
	pthread_mutex_destroy(&cw->name_lock);
	[cw->owner_name release];
	[cw->window_name release];
}

void update_window(cocoa_window_t cw, obs_data_t *settings)
{
	pthread_mutex_lock(&cw->name_lock);
	[cw->owner_name release];
	[cw->window_name release];
	cw->owner_name = @(obs_data_get_string(settings, "owner_name"));
	cw->window_name = @(obs_data_get_string(settings, "window_name"));
	[cw->owner_name retain];
	[cw->window_name retain];
	pthread_mutex_unlock(&cw->name_lock);

	cw->owner_pid = obs_data_get_int(settings, "owner_pid");
	cw->window_id = obs_data_get_int(settings, "window");
}

static inline const char *make_name(NSString *owner, NSString *name)
{
	if (!owner.length)
		return "";

	NSString *str = [NSString stringWithFormat:@"[%@] %@", owner, name];
	return str.UTF8String;
}

static inline NSDictionary *find_window_dict(NSArray *arr, int window_id)
{
	for (NSDictionary *dict in arr) {
		NSNumber *wid = (NSNumber *)dict[WINDOW_NUMBER];
		if (wid.intValue == window_id)
			return dict;
	}

	return nil;
}

static inline bool window_changed_internal(obs_property_t *p,
					   obs_data_t *settings)
{
	int window_id = obs_data_get_int(settings, "window");
	NSString *window_owner = @(obs_data_get_string(settings, "owner_name"));
	NSString *window_name = @(obs_data_get_string(settings, "window_name"));

	NSDictionary *win_info = @{
		OWNER_NAME: window_owner,
		WINDOW_NAME: window_name,
	};

	NSArray *arr = enumerate_windows();

	bool show_empty_names = obs_data_get_bool(settings, "show_empty_names");

	NSDictionary *cur = find_window_dict(arr, window_id);
	bool window_found = cur != nil;
	bool window_added = window_found;

	obs_property_list_clear(p);
	for (NSDictionary *dict in arr) {
		NSString *owner = (NSString *)dict[OWNER_NAME];
		NSString *name = (NSString *)dict[WINDOW_NAME];
		NSNumber *wid = (NSNumber *)dict[WINDOW_NUMBER];

		if (!window_added &&
		    win_info_cmp(win_info, dict) == NSOrderedAscending) {
			window_added = true;
			size_t idx = obs_property_list_add_int(
				p, make_name(window_owner, window_name),
				window_id);
			obs_property_list_item_disable(p, idx, true);
		}

		if (!show_empty_names && !name.length &&
		    window_id != wid.intValue)
			continue;

		obs_property_list_add_int(p, make_name(owner, name),
					  wid.intValue);
	}

	if (!window_added) {
		size_t idx = obs_property_list_add_int(
			p, make_name(window_owner, window_name), window_id);
		obs_property_list_item_disable(p, idx, true);
	}

	if (!window_found)
		return true;

	NSString *owner = (NSString *)cur[OWNER_NAME];
	NSString *window = (NSString *)cur[WINDOW_NAME];

	obs_data_set_string(settings, "owner_name", owner.UTF8String);
	obs_data_set_string(settings, "window_name", window.UTF8String);

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
