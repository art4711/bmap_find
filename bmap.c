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
#include <assert.h>

#include "bmap.h"

struct simple_bmap {
	unsigned int sz;
	uint64_t data[];
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
#define SIMPLE_SLOT_TO_B(s) ((s) << 6)

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

/*
 * Check each bit linearly.
 */
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

/*
 * Check each 64 bit slot individually with
 * special care for the first slot.
 */
static unsigned int
simple_first_set(void *v, unsigned int b)
{
	struct simple_bmap *bmap = v;
	unsigned int slot = SIMPLE_SLOT(b);
	unsigned int maxslot = SIMPLE_SLOT(bmap->sz + 63);
	uint64_t first_slot = ~(SIMPLE_MASK(b) - 1) & bmap->data[slot];

	if (first_slot) {
		return SIMPLE_SLOT_TO_B(slot) + __builtin_ffsll(first_slot) - 1;
	}
        slot++;		/* first slot checked, move on. */
        for (; slot < maxslot; slot++) {
                if (bmap->data[slot])
                        return SIMPLE_SLOT_TO_B(slot) + __builtin_ffsll(bmap->data[slot]) - 1;
        }
        return BMAP_INVALID_OFF;
}

struct bmap_interface bmap_simple = { simple_alloc, free, simple_set, simple_isset, simple_first_set };


/*
 * 64 bit pyramid.
 */
#define P64_LEVELS 6
struct p64_bmap {
	unsigned int sz;
	uint64_t *lvl[P64_LEVELS];
};

#define P64_LM(l) ((P64_LEVELS - ((l) + 1)) * 6)
#define P64_SLOT(b, l) ((uint64_t)SIMPLE_SLOT((b) >> P64_LM(l)))
#define P64_MASK(b, l) ((uint64_t)SIMPLE_MASK((b) >> P64_LM(l)))

static void *
p64_alloc(size_t nbits)
{
	struct p64_bmap *pb;
	size_t sz;
	int l;

	sz = sizeof(*pb);
	for (l = 0; l < 6; l++) {
		sz += (P64_SLOT(nbits + 63, l) + 1) * sizeof(uint64_t);
	}
	pb = calloc(sz, 1);
	uint64_t *a = (uint64_t *)(pb + 1);
	for (l = 0; l < 6; l++) {
		pb->lvl[l] = a;
		a += P64_SLOT(nbits + 63, l) + 1;
	}
	pb->sz = nbits;
	return pb;
}

static void
p64_set(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	int l;

	for (l = 0; l < P64_LEVELS; l++) {
		pb->lvl[l][P64_SLOT(b, l)] |= P64_MASK(b, l);
	}
}

static bool
p64_isset(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	return (pb->lvl[5][P64_SLOT(b, 5)] & P64_MASK(b, 5)) != 0;
}

static unsigned int
p64_first_set(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	unsigned int l;
	uint64_t masked;
	unsigned int slot;

	if (b > pb->sz)
		return BMAP_INVALID_OFF;

	/*
	 * quick check for the initial lvl 5 slot being populated.
	 * This saves a lot of effort in very dense bitmaps.
	 */
	slot = P64_SLOT(b, 5);
	masked = ~(P64_MASK(b, 5) - 1) & pb->lvl[5][P64_SLOT(b, 5)];
	b = slot << 6;
	if (masked)
		return b + __builtin_ffsll(masked) - 1;
	b += 64;

	for (l = 0; l < 6; l++) {
		slot = P64_SLOT(b, l);
		masked = ~(P64_MASK(b, l) - 1) & pb->lvl[l][slot];
		if (masked) {
			unsigned int min = ((slot << 6) + __builtin_ffsll(masked) - 1) << P64_LM(l);
			if (min > b)
				b = min;
		} else {
			if (l == 0)
				return BMAP_INVALID_OFF;
			b = (slot + 1) << P64_LM(l - 1);
			l -= 2;
		}
	}
	return b;
}

struct bmap_interface bmap_p64 = { p64_alloc, free, p64_set, p64_isset, p64_first_set };

static unsigned int
p64_first_set_no_l5_peek(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	unsigned int l;
	uint64_t masked;
	unsigned int slot;

	for (l = 0; l < 6; l++) {
		slot = P64_SLOT(b, l);
		masked = ~(P64_MASK(b, l) - 1) & pb->lvl[l][slot];
		if (masked) {
			unsigned int min = ((slot << 6) + __builtin_ffsll(masked) - 1) << P64_LM(l);
			if (min > b)
				b = min;
		} else {
			if (l == 0)
				return BMAP_INVALID_OFF;
			b = (slot + 1) << P64_LM(l - 1);
			l -= 2;
		}
	}
	return b;
}

struct bmap_interface bmap_p64_naive = { p64_alloc, free, p64_set, p64_isset, p64_first_set_no_l5_peek };

static const uint64_t log2_64 = 6;
static const uint64_t p64v2_levels = 6;

/*
 * 64 bit pyramid v2.
 */

/*
 * The types are uint64_t (despite only dealing with 32 bit values) to
 * make the math simpler.
 */

/* log2 of how many bits one bit covers at this level. */
static inline uint64_t
p64v2_lm(uint64_t l)
{
	return (p64v2_levels - l - 1) * log2_64;
}

static inline uint64_t
p64v2_slot(uint64_t b, uint64_t l)
{
	return b >> (log2_64 + p64v2_lm(l));
}

static inline uint64_t
p64v2_mask(uint64_t b, uint64_t l)
{
	return 1LLU << ((b >> p64v2_lm(l)) & ((1 << log2_64) - 1));
}

static inline uint64_t *
p64v2_pbslot(struct p64_bmap *pb, uint64_t b, uint64_t l)
{
	return &pb->lvl[l][p64v2_slot(b, l)];
}

static void *
p64v2_alloc(size_t nbits)
{
	struct p64_bmap *pb;
	size_t sz;
	int l;

	sz = sizeof(*pb);
	for (l = 0; l < 6; l++) {
		sz += (p64v2_slot(nbits + 63, l) + 1) * sizeof(uint64_t);
	}
	pb = calloc(sz, 1);
	uint64_t *a = (uint64_t *)(pb + 1);
	for (l = 0; l < 6; l++) {
		pb->lvl[l] = a;
		a += p64v2_slot(nbits + 63, l) + 1;
	}
	pb->sz = nbits;
	return pb;
}

static void
p64v2_set(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	int l;

	for (l = 0; l < p64v2_levels; l++) {
		*p64v2_pbslot(pb, b, l) |= p64v2_mask(b, l);
	}
}

static bool
p64v2_isset(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	return (*p64v2_pbslot(pb, b, 5) & p64v2_mask(b, 5)) != 0;
}

static unsigned int
p64v2_first_set(void *v, unsigned int b)
{
	struct p64_bmap *pb = v;
	unsigned int l;
	uint64_t masked;
	unsigned int slot;

	if (b > pb->sz)
		return BMAP_INVALID_OFF;

	/*
	 * quick check for the initial lvl 5 slot being populated.
	 * This saves a lot of effort in very dense bitmaps.
	 */
	slot = p64v2_slot(b, 5);
	masked = ~(p64v2_mask(b, 5) - 1) & pb->lvl[5][slot];
	b = slot << log2_64;
	if (masked)
		return b + __builtin_ffsll(masked) - 1;
	b += 64;

	for (l = 0; l < 6; l++) {
		slot = p64v2_slot(b, l);
		masked = ~(p64v2_mask(b, l) - 1) & pb->lvl[l][slot];
		if (masked) {
			unsigned int min = ((slot << log2_64) + __builtin_ffsll(masked) - 1) << p64v2_lm(l);
			if (min > b)
				b = min;
		} else {
			if (l == 0)
				return BMAP_INVALID_OFF;
			b = (slot + 1) << p64v2_lm(l - 1);
			l -= 2;
		}
	}
	return b;
}

struct bmap_interface bmap_p64v2 = { p64v2_alloc, free, p64v2_set, p64v2_isset, p64v2_first_set };

struct p64v3_bmap {
	unsigned int sz;
	unsigned int levels;
	uint64_t *lvl[];
};

/* log2 of how many bits one bit covers at this level. */
static inline uint64_t
p64v3_bpb(uint64_t l)
{
	return l * log2_64;
}

/* log2 of how many bits one slot covers at this level. */
static inline uint64_t
p64v3_bps(uint64_t l)
{
	return (l + 1) * log2_64;
}

static inline uint64_t
p64v3_slot(uint64_t b, uint64_t l)
{
	return b >> p64v3_bps(l);
}

static inline uint64_t
p64v3_mask(uint64_t b, uint64_t l)
{
	return 1LLU << ((b >> p64v3_bpb(l)) & ((1 << log2_64) - 1));
}

/* How many slots do we need to cover nbits on this level */
static inline uint64_t
p64v3_slots_per_level(uint64_t nbits, uint64_t l)
{
	return p64v3_slot(nbits, l) + 1;
}

static inline uint64_t *
p64v3_pbslot(struct p64v3_bmap *pb, uint64_t b, uint64_t l)
{
	return &pb->lvl[l][p64v3_slot(b, l)];
}

static void *
p64v3_alloc(size_t nbits)
{
	struct p64v3_bmap *pb;
	size_t sz;
	int l;
	int levels;

	for (levels = 0; p64v3_slots_per_level(nbits, levels) > 1; levels++)
		;
	sz = sizeof(*pb);
	for (l = 0; l < levels; l++) {
		sz += p64v3_slots_per_level(nbits, l) * sizeof(uint64_t);
	}
	levels++;
	sz += levels * sizeof(uint64_t **);
	pb = calloc(sz, 1);
	uint64_t *a = (uint64_t *)((uint64_t **)(pb + 1) + levels);
	for (l = 0; l < levels; l++) {
		pb->lvl[l] = a;
		a += p64v3_slots_per_level(nbits, l);
	}
	pb->sz = nbits;
	pb->levels = levels;
	return pb;
}

static void
p64v3_set(void *v, unsigned int b)
{
	struct p64v3_bmap *pb = v;
	int l;

	for (l = 0; l < pb->levels; l++) {
		*p64v3_pbslot(pb, b, l) |= p64v3_mask(b, l);
	}
}

static bool
p64v3_isset(void *v, unsigned int b)
{
	struct p64v3_bmap *pb = v;
	return (*p64v3_pbslot(pb, b, 0) & p64v3_mask(b, 0)) != 0;
}

static unsigned int
p64v3_first_set(void *v, unsigned int b)
{
	struct p64v3_bmap *pb = v;
	unsigned int slot;
	uint64_t masked;
	int l;

	if (b > pb->sz)
		return BMAP_INVALID_OFF;

	/*
	 * quick check for the initial lvl 0 slot being populated.
	 * This saves a lot of effort in very dense bitmaps.
	 */
	slot = p64v3_slot(b, 0);
	masked = ~(p64v3_mask(b, 0) - 1) & pb->lvl[0][slot];
	b = slot << log2_64;
	if (masked)
		return b + __builtin_ffsll(masked) - 1;
	b += 64;

	for (l = pb->levels - 1; l >= 0; l--) {
		slot = p64v3_slot(b, l);
		masked = ~(p64v3_mask(b, l) - 1) & pb->lvl[l][slot];
		if (masked) {
			unsigned int min = ((slot << log2_64) + __builtin_ffsll(masked) - 1) << p64v3_bpb(l);
			if (min > b)
				b = min;
		} else {
			if (l == pb->levels - 1)
				return BMAP_INVALID_OFF;
			b = (slot + 1) << p64v3_bps(l);
			l += 2;
		}
	}
	return b;
}

struct bmap_interface bmap_p64v3 = { p64v3_alloc, free, p64v3_set, p64v3_isset, p64v3_first_set };
