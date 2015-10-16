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

#include <limits.h>
#include <stdbool.h>

#define BMAP_INVALID_OFF UINT_MAX

struct bmap_interface {
	void *(*alloc)(size_t nbits);		/* allocate a structure enough for managing nbits of bits */
	void (*free)(void *);			/* free */
        void (*set)(void *, unsigned int b);	/* set one bit */
        bool (*isset)(void *, unsigned int b);	/* test one bit */
	unsigned int (*first_set)(void *, unsigned int b);	/* find first bit equal or bigger than b */
};

extern struct bmap_interface bmap_dumb;
extern struct bmap_interface bmap_simple;
extern struct bmap_interface bmap_p64;
extern struct bmap_interface bmap_p64_naive;
extern struct bmap_interface bmap_p64v2;
extern struct bmap_interface bmap_p64v3;
extern struct bmap_interface bmap_p64v3r;
extern struct bmap_interface bmap_p64v3r2;
extern struct bmap_interface bmap_p64v3r3;
extern struct bmap_interface bmap_p8;
extern struct bmap_interface bmap_p32;
