#pragma once

extern const char *younow_get_ingest(const char *server, const char *key);
extern const char *younow_get_ingest_proxy(const char *server, const char *key,
					   const char *socks_proxy);