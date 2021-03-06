/******************************************************************************
    Copyright (C) 2014 by Ruwen Hahn <palana@stunned.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <string.h>
#include <assert.h>

#include <util/bmem.h>
#include <util/c99defs.h>
#include <util/darray.h>

#include "obs-internal.h"

struct obs_hotkey_name_map_edge;
struct obs_hotkey_name_map_node;
struct obs_hotkey_name_map;

typedef struct obs_hotkey_name_map_edge obs_hotkey_name_map_edge_t;
typedef struct obs_hotkey_name_map_node obs_hotkey_name_map_node_t;
typedef struct obs_hotkey_name_map obs_hotkey_name_map_t;

struct obs_hotkey_name_map_node {
	bool is_leaf;
	union {
		int val;
		DARRAY(obs_hotkey_name_map_edge_t) children;
	};
};

struct obs_hotkey_name_map {
	obs_hotkey_name_map_node_t root;
	obs_hotkey_name_map_node_t *leaves;
	size_t num_leaves;
	size_t next_leaf;
};

struct obs_hotkey_name_map_edge_prefix {
	uint8_t prefix_len;
	char *prefix;
};

#define NAME_MAP_COMPRESS_LENGTH \
	(sizeof(struct obs_hotkey_name_map_edge_prefix) - sizeof(uint8_t))

struct obs_hotkey_name_map_edge {
	union {
		struct {
			uint8_t prefix_len;
			char *prefix;
		};
		struct {
			uint8_t compressed_len;
			char compressed_prefix[NAME_MAP_COMPRESS_LENGTH];
		};
	};
	struct obs_hotkey_name_map_node *node;
};

static inline obs_hotkey_name_map_node_t *new_node(void)
{
	return bzalloc(sizeof(obs_hotkey_name_map_node_t));
}

static inline obs_hotkey_name_map_node_t *new_leaf(void)
{
	obs_hotkey_name_map_t *name_map = obs->hotkeys.name_map;
	obs_hotkey_name_map_node_t *node =
		&name_map->leaves[name_map->next_leaf];
	name_map->next_leaf += 1;

	node->is_leaf = true;
	return node;
}

static inline char *get_prefix(obs_hotkey_name_map_edge_t *e)
{
	return e->prefix_len >= NAME_MAP_COMPRESS_LENGTH ? e->prefix
							 : e->compressed_prefix;
}

static void set_prefix(obs_hotkey_name_map_edge_t *e, const char *prefix,
		       size_t l)
{
	assert(e->prefix_len == 0);

	e->compressed_len = (uint8_t)l;
	if (l < NAME_MAP_COMPRESS_LENGTH)
		strncpy(e->compressed_prefix, prefix, l);
	else
		e->prefix = bstrdup_n(prefix, l);
}

static obs_hotkey_name_map_edge_t *add_leaf(obs_hotkey_name_map_node_t *node,
					    const char *key, size_t l, int v)
{
	obs_hotkey_name_map_edge_t *e = da_push_back_new(node->children);

	set_prefix(e, key, l);

	e->node = new_leaf();
	e->node->val = v;

	return e;
}

static void shrink_prefix(obs_hotkey_name_map_edge_t *e, size_t l)
{
	bool old_comp = e->prefix_len < NAME_MAP_COMPRESS_LENGTH;
	bool new_comp = l < NAME_MAP_COMPRESS_LENGTH;

	char *str = get_prefix(e);

	e->prefix_len = (uint8_t)l;
	if (get_prefix(e) != str)
		strncpy(get_prefix(e), str, l);
	else
		str[l] = 0;

	if (!old_comp && new_comp)
		bfree(str);
}

static void connect(obs_hotkey_name_map_edge_t *e,
		    obs_hotkey_name_map_node_t *n)
{
	e->node = n;
}

static void reduce_edge(obs_hotkey_name_map_edge_t *e, const char *key,
			size_t l, int v)
{
	const char *str = get_prefix(e), *str_ = key;
	size_t common_length = 0;
	while (*str == *str_) {
		common_length += 1;
		str += 1;
		str_ += 1;
	}

	obs_hotkey_name_map_node_t *new_node_ = new_node();
	obs_hotkey_name_map_edge_t *tail =
		da_push_back_new(new_node_->children);

	connect(tail, e->node);
	set_prefix(tail, str, e->prefix_len - common_length);

	add_leaf(new_node_, str_, l - common_length, v);

	connect(e, new_node_);
	shrink_prefix(e, common_length);
}

enum obs_hotkey_name_map_edge_compare_result {
	RES_MATCHES,
	RES_NO_MATCH,
	RES_COMMON_PREFIX,
	RES_PREFIX_MATCHES,
};

static enum obs_hotkey_name_map_edge_compare_result
compare_prefix(obs_hotkey_name_map_edge_t *edge, const char *key, size_t l)
{
	uint8_t pref_len = edge->prefix_len;
	const char *str = get_prefix(edge);
	size_t i = 0;

	for (; i < l && i < pref_len; i++)
		if (str[i] != key[i])
			break;

	if (i != 0 && pref_len == i)
		return l == i ? RES_MATCHES : RES_PREFIX_MATCHES;
	if (i != 0)
		return pref_len == i ? RES_PREFIX_MATCHES : RES_COMMON_PREFIX;
	return RES_NO_MATCH;
}

static void insert(obs_hotkey_name_map_edge_t *edge,
		   obs_hotkey_name_map_node_t *node, const char *key, size_t l,
		   int v)
{
	if (node->is_leaf && l > 0) {
		obs_hotkey_name_map_node_t *new_node_ = new_node();
		connect(edge, new_node_);

		obs_hotkey_name_map_edge_t *edge =
			da_push_back_new(new_node_->children);
		connect(edge, node);
		add_leaf(new_node_, key, l, v);
		return;
	}

	if (node->is_leaf && l == 0) {
		node->val = v;
		return;
	}

	for (size_t i = 0; i < node->children.num; i++) {
		obs_hotkey_name_map_edge_t *e = &node->children.array[i];

		switch (compare_prefix(e, key, l)) {
		case RES_NO_MATCH:
			continue;

		case RES_MATCHES:
		case RES_PREFIX_MATCHES:
			insert(e, e->node, key + e->prefix_len,
			       l - e->prefix_len, v);
			return;

		case RES_COMMON_PREFIX:
			reduce_edge(e, key, l, v);
			return;
		}
	}

	add_leaf(node, key, l, v);
}

static void obs_hotkey_name_map_insert(obs_hotkey_name_map_t *trie,
				       const char *key, int v)
{
	if (!trie || !key)
		return;

	insert(NULL, &trie->root, key, strlen(key), v);
}

static bool obs_hotkey_name_map_lookup(obs_hotkey_name_map_t *trie,
				       const char *key, int *v)
{
	if (!trie || !key)
		return false;

	size_t len = strlen(key);
	obs_hotkey_name_map_node_t *n = &trie->root;

	size_t i = 0;
	for (; i < n->children.num;) {
		obs_hotkey_name_map_edge_t *e = &n->children.array[i];

		switch (compare_prefix(e, key, len)) {
		case RES_NO_MATCH:
			i++;
			continue;

		case RES_COMMON_PREFIX:
			return false;

		case RES_PREFIX_MATCHES:
			key += e->prefix_len;
			len -= e->prefix_len;
			n = e->node;
			i = 0;
			continue;

		case RES_MATCHES:
			n = e->node;
			if (!n->is_leaf) {
				for (size_t j = 0; j < n->children.num; j++) {
					if (n->children.array[j].prefix_len)
						continue;

					if (v)
						*v = n->children.array[j]
							     .node->val;
					return true;
				}
				return false;
			}

			if (v)
				*v = n->val;
			return true;
		}
	}

	return false;
}

static void show_node(obs_hotkey_name_map_node_t *node, int in)
{
	if (node->is_leaf) {
		printf(": % 3d\n", node->val);
		return;
	}

	printf("\n");
	for (int i = 0; i < in; i += 2)
		printf("| ");
	printf("%lu:\n", (unsigned long)node->children.num);

	for (size_t i = 0; i < node->children.num; i++) {
		for (int i = 0; i < in; i += 2)
			printf("| ");
		printf("\\ ");

		obs_hotkey_name_map_edge_t *e = &node->children.array[i];
		printf("%s", get_prefix(e));
		show_node(e->node, in + 2);
	}
}

void trie_print_size(obs_hotkey_name_map_t *trie)
{
	show_node(&trie->root, 0);
}

static const char *obs_key_names[] = {
#define OBS_HOTKEY(x) #x,
#include "obs-hotkeys.h"
#undef OBS_HOTKEY
};

const char *obs_key_to_name(obs_key_t key)
{
	if (key >= OBS_KEY_LAST_VALUE) {
		blog(LOG_ERROR,
		     "obs-hotkey.c: queried unknown key "
		     "with code %d",
		     (int)key);
		return "";
	}

	return obs_key_names[key];
}

static obs_key_t obs_key_from_name_fallback(const char *name)
{
#define OBS_HOTKEY(x)              \
	if (strcmp(#x, name) == 0) \
		return x;
#include "obs-hotkeys.h"
#undef OBS_HOTKEY
	return OBS_KEY_NONE;
}

static void init_name_map(void)
{
	obs->hotkeys.name_map = bzalloc(sizeof(struct obs_hotkey_name_map));
	obs_hotkey_name_map_t *name_map = obs->hotkeys.name_map;

#define OBS_HOTKEY(x) name_map->num_leaves += 1;
#include "obs-hotkeys.h"
#undef OBS_HOTKEY

	size_t size = sizeof(obs_hotkey_name_map_node_t) * name_map->num_leaves;
	name_map->leaves = bzalloc(size);

#define OBS_HOTKEY(x) obs_hotkey_name_map_insert(name_map, #x, x);
#include "obs-hotkeys.h"
#undef OBS_HOTKEY
}

obs_key_t obs_key_from_name(const char *name)
{
	if (!obs)
		return obs_key_from_name_fallback(name);

	if (pthread_once(&obs->hotkeys.name_map_init_token, init_name_map))
		return obs_key_from_name_fallback(name);

	int v = 0;
	if (obs_hotkey_name_map_lookup(obs->hotkeys.name_map, name, &v))
		return v;

	return OBS_KEY_NONE;
}

static void free_node(obs_hotkey_name_map_node_t *node, bool release);

static void free_edge(obs_hotkey_name_map_edge_t *edge)
{
	free_node(edge->node, true);

	if (edge->prefix_len < NAME_MAP_COMPRESS_LENGTH)
		return;

	bfree(get_prefix(edge));
}

static void free_node(obs_hotkey_name_map_node_t *node, bool release)
{
	if (!node->is_leaf) {
		for (size_t i = 0; i < node->children.num; i++)
			free_edge(&node->children.array[i]);

		da_free(node->children);
	}

	if (release && !node->is_leaf)
		bfree(node);
}

void obs_hotkey_name_map_free(void)
{
	if (!obs || !obs->hotkeys.name_map)
		return;

	free_node(&obs->hotkeys.name_map->root, false);
	bfree(obs->hotkeys.name_map->leaves);
	bfree(obs->hotkeys.name_map);
}
