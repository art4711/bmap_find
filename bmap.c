/*
 * Copyright (c) 2015 Artur Grabowski <art@blahonga.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#include "bmap.h"

struct simple_bmap {
	unsigned int sz;
	int64_t data[];
};

static void *
simple_alloc(size_t nbits)
{
	struct simple_bmap *bmap = calloc(sizeof(*bmap) + (nbits + 63) / CHAR_BIT, 1);
	bmap->sz = nbits;
	return bmap;
}

#define SIMPLE_SLOT(bit) ((bit) >> 6)
#define SIMPLE_MASK(bit) (1LLU << ((bit) & ((1 << 6) - 1)))

static void
simple_set(void *v, unsigned int b)
{
	struct simple_bmap *bmap = v;
	unsigned int off = SIMPLE_SLOT(b);
	uint64_t mask = SIMPLE_MASK(b);

	bmap->data[off] |= mask;
}

static bool
simple_isset(void *v, unsigned int b)
{
	struct simple_bmap *bmap = v;
	unsigned int off = SIMPLE_SLOT(b);
	uint64_t mask = SIMPLE_MASK(b);

	return (bmap->data[off] & mask) != 0;
}

static unsigned int
dumb_first_set(void *v, unsigned int b)
{
	struct simple_bmap *bmap = v;
	unsigned int i;
	for (i = b; i < bmap->sz; i++) {
		unsigned int off = SIMPLE_SLOT(i);
		uint64_t mask = SIMPLE_MASK(i);
		if (bmap->data[off] & mask) {
			return i;
		}
	}
	return BMAP_INVALID_OFF;
}

struct bmap_interface bmap_dumb = { simple_alloc, free, simple_set, simple_isset, dumb_first_set };
