#pragma once

struct showroom_ingest {
	const char *url;
	const char *key;
};

extern struct showroom_ingest *showroom_get_ingest(const char *server,
						   const char *access_key);

extern void free_showroom_data();
