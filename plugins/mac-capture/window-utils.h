#import <CoreGraphics/CGWindow.h>
#import <Cocoa/Cocoa.h>

#include <util/threading.h>
#include <obs-module.h>

struct cocoa_window {
	CGWindowID window_id;
	int owner_pid;

	pthread_mutex_t name_lock;
	NSString *owner_name;
	NSString *window_name;

	uint64_t next_search_time;
};
typedef struct cocoa_window *cocoa_window_t;

NSArray *enumerate_cocoa_windows(void);

bool find_window(cocoa_window_t cw, obs_data_t *settings, bool force);

void init_window(cocoa_window_t cw, obs_data_t *settings);

void destroy_window(cocoa_window_t cw);

void update_window(cocoa_window_t cw, obs_data_t *settings);

void window_defaults(obs_data_t *settings);

void add_window_properties(obs_properties_t *props);

void show_window_properties(obs_properties_t *props, bool show);
