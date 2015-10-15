# Searching for set elements in at bitmap

Tests to see the efficiency of various approches to implementing
bitmaps used for finding set elements.

Imagine the earlier test I did for bitmap intersections
(https://github.com/art4711/bmap), but now we need to find out which
bits were actually set after the interection.

The API we provide:

 * alloc/free - Allocate and free the bitmap

 * set - Sets one bit in the bitmap

 * isset - Check if the bit is set (this is just a helper function
   that I needed to implement to actually generate the "random" data
   we're testing with).

 * first_set(b) - This is the meat of the test. Find the first set bit
   in the bitmap that is equal to or larger than `b`.