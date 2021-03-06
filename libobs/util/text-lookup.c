/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "dstr.h"
#include "text-lookup.h"
#include "lexer.h"
#include "platform.h"

/* ------------------------------------------------------------------------- */

struct text_leaf {
	char *lookup, *value;
};

static inline void text_leaf_destroy(struct text_leaf *leaf)
{
	if (leaf) {
		bfree(leaf->lookup);
		bfree(leaf->value);
		bfree(leaf);
	}
}

/* ------------------------------------------------------------------------- */

struct text_node {
	struct dstr str;
	struct text_node *first_subnode;
	struct text_leaf *leaf;

	struct text_node *next;
};

static void text_node_destroy(struct text_node *node)
{
	struct text_node *subnode;

	if (!node)
		return;

	subnode = node->first_subnode;
	while (subnode) {
		struct text_node *destroy_node = subnode;

		subnode = subnode->next;
		text_node_destroy(destroy_node);
	}

	dstr_free(&node->str);
	if (node->leaf)
		text_leaf_destroy(node->leaf);
	bfree(node);
}

static struct text_node *text_node_bychar(struct text_node *node, char ch)
{
	struct text_node *subnode = node->first_subnode;

	while (subnode) {
		if (!dstr_is_empty(&subnode->str) &&
		    subnode->str.array[0] == ch)
			return subnode;

		subnode = subnode->next;
	}

	return NULL;
}

static struct text_node *text_node_byname(struct text_node *node,
					  const char *name)
{
	struct text_node *subnode = node->first_subnode;

	while (subnode) {
		if (astrcmpi_n(subnode->str.array, name, subnode->str.len) == 0)
			return subnode;

		subnode = subnode->next;
	}

	return NULL;
}

/* ------------------------------------------------------------------------- */

struct text_lookup {
	struct dstr language;
	struct text_node *top;
};

static void lookup_createsubnode(const char *lookup_val, struct text_leaf *leaf,
				 struct text_node *node)
{
	struct text_node *new = bzalloc(sizeof(struct text_node));
	new->leaf = leaf;
	new->next = node->first_subnode;
	dstr_copy(&new->str, lookup_val);

	node->first_subnode = new;
}

static void lookup_splitnode(const char *lookup_val, size_t len,
			     struct text_leaf *leaf, struct text_node *node)
{
	struct text_node *split = bzalloc(sizeof(struct text_node));

	dstr_copy(&split->str, node->str.array + len);
	split->leaf = node->leaf;
	split->first_subnode = node->first_subnode;
	node->first_subnode = split;

	dstr_resize(&node->str, len);

	if (lookup_val[len] != 0) {
		node->leaf = NULL;
		lookup_createsubnode(lookup_val + len, leaf, node);
	} else {
		node->leaf = leaf;
	}
}

static inline void lookup_replaceleaf(struct text_node *node,
				      struct text_leaf *leaf)
{
	text_leaf_destroy(node->leaf);
	node->leaf = leaf;
}

static void lookup_addstring(const char *lookup_val, struct text_leaf *leaf,
			     struct text_node *node)
{
	struct text_node *child;

	/* value already exists, so replace */
	if (!lookup_val || !*lookup_val) {
		lookup_replaceleaf(node, leaf);
		return;
	}

	child = text_node_bychar(node, *lookup_val);
	if (child) {
		size_t len;

		for (len = 0; len < child->str.len; len++) {
			char val1 = child->str.array[len],
			     val2 = lookup_val[len];

			if (val1 >= 'A' && val1 <= 'Z')
				val1 += 0x20;
			if (val2 >= 'A' && val2 <= 'Z')
				val2 += 0x20;

			if (val1 != val2)
				break;
		}

		if (len == child->str.len) {
			lookup_addstring(lookup_val + len, leaf, child);
			return;
		} else {
			lookup_splitnode(lookup_val, len, leaf, child);
		}
	} else {
		lookup_createsubnode(lookup_val, leaf, node);
	}
}

static void lookup_getstringtoken(struct lexer *lex, struct strref *token)
{
	const char *temp = lex->offset;
	bool was_backslash = false;

	while (*temp != 0 && *temp != '\n') {
		if (!was_backslash) {
			if (*temp == '\\') {
				was_backslash = true;
			} else if (*temp == '"') {
				temp++;
				break;
			}
		} else {
			was_backslash = false;
		}

		++temp;
	}

	token->len += (size_t)(temp - lex->offset);

	if (*token->array == '"') {
		token->array++;
		token->len--;

		if (*(temp - 1) == '"')
			token->len--;
	}

	lex->offset = temp;
}

static bool lookup_gettoken(struct lexer *lex, struct strref *str)
{
	struct base_token temp;

	base_token_clear(&temp);
	strref_clear(str);

	while (lexer_getbasetoken(lex, &temp, PARSE_WHITESPACE)) {
		char ch = *temp.text.array;

		if (!str->array) {
			/* comments are designated with a #, and end at LF */
			if (ch == '#') {
				while (ch != '\n' && ch != 0)
					ch = *(++lex->offset);
			} else if (temp.type == BASETOKEN_WHITESPACE) {
				strref_copy(str, &temp.text);
				break;
			} else {
				strref_copy(str, &temp.text);
				if (ch == '"') {
					lookup_getstringtoken(lex, str);
					break;
				} else if (ch == '=') {
					break;
				}
			}
		} else {
			if (temp.type == BASETOKEN_WHITESPACE ||
			    *temp.text.array == '=') {
				lex->offset -= temp.text.len;
				break;
			}

			if (ch == '#') {
				lex->offset--;
				break;
			}

			str->len += temp.text.len;
		}
	}

	return (str->len != 0);
}

static inline bool lookup_goto_nextline(struct lexer *p)
{
	struct strref val;
	bool success = true;

	strref_clear(&val);

	while (true) {
		if (!lookup_gettoken(p, &val)) {
			success = false;
			break;
		}
		if (*val.array == '\n')
			break;
	}

	return success;
}

static char *convert_string(const char *str, size_t len)
{
	struct dstr out;
	out.array = bstrdup_n(str, len);
	out.capacity = len + 1;
	out.len = len;

	dstr_replace(&out, "\\n", "\n");
	dstr_replace(&out, "\\t", "\t");
	dstr_replace(&out, "\\r", "\r");
	dstr_replace(&out, "\\\"", "\"");

	return out.array;
}

static void lookup_addfiledata(struct text_lookup *lookup,
			       const char *file_data)
{
	struct lexer lex;
	struct strref name, value;

	lexer_init(&lex);
	lexer_start(&lex, file_data);
	strref_clear(&name);
	strref_clear(&value);

	while (lookup_gettoken(&lex, &name)) {
		struct text_leaf *leaf;
		bool got_eq = false;

		if (*name.array == '\n')
			continue;
	getval:
		if (!lookup_gettoken(&lex, &value))
			break;
		if (*value.array == '\n')
			continue;
		else if (!got_eq && *value.array == '=') {
			got_eq = true;
			goto getval;
		}

		leaf = bmalloc(sizeof(struct text_leaf));
		leaf->lookup = bstrdup_n(name.array, name.len);
		leaf->value = convert_string(value.array, value.len);

		lookup_addstring(leaf->lookup, leaf, lookup->top);

		if (!lookup_goto_nextline(&lex))
			break;
	}

	lexer_free(&lex);
}

static inline bool lookup_getstring(const char *lookup_val, const char **out,
				    struct text_node *node)
{
	struct text_node *child;
	char ch;

	if (!node)
		return false;

	child = text_node_byname(node, lookup_val);
	if (!child)
		return false;

	lookup_val += child->str.len;
	ch = *lookup_val;
	if (ch)
		return lookup_getstring(lookup_val, out, child);

	if (!child->leaf)
		return false;

	*out = child->leaf->value;
	return true;
}

/* ------------------------------------------------------------------------- */

lookup_t *text_lookup_create(const char *path)
{
	struct text_lookup *lookup = bzalloc(sizeof(struct text_lookup));

	if (!text_lookup_add(lookup, path)) {
		bfree(lookup);
		lookup = NULL;
	}

	return lookup;
}

bool text_lookup_add(lookup_t *lookup, const char *path)
{
	struct dstr file_str;
	char *temp = NULL;
	FILE *file;

	file = os_fopen(path, "rb");
	if (!file)
		return false;

	os_fread_utf8(file, &temp);
	dstr_init_move_array(&file_str, temp);
	fclose(file);

	if (!file_str.array)
		return false;

	if (!lookup->top)
		lookup->top = bzalloc(sizeof(struct text_node));

	dstr_replace(&file_str, "\r", " ");
	lookup_addfiledata(lookup, file_str.array);
	dstr_free(&file_str);

	return true;
}

void text_lookup_destroy(lookup_t *lookup)
{
	if (lookup) {
		dstr_free(&lookup->language);
		text_node_destroy(lookup->top);

		bfree(lookup);
	}
}

bool text_lookup_getstr(lookup_t *lookup, const char *lookup_val,
			const char **out)
{
	if (lookup)
		return lookup_getstring(lookup_val, out, lookup->top);
	return false;
}
