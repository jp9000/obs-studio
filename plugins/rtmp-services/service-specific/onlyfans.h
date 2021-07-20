#pragma once

struct onlyfans_ingest {
	const char *name;
	const char *url;
	int64_t rtt;
};

extern void onlyfans_ingests_lock(void);
extern void onlyfans_ingests_unlock(void);
extern size_t onlyfans_ingest_count(void);
extern struct onlyfans_ingest get_onlyfans_ingest(size_t idx);
extern struct onlyfans_ingest get_onlyfans_ingest_by_url(const char *url);
//!<
extern void add_onlyfans_ingest(const char *name, const char *title,
				const char *url);
