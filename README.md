# Searching for set elements in a bitmap

Tests to see the efficiency of various approches to implementing
bitmaps used for finding set elements.

Imagine the earlier test I did for bitmap intersections
(https://github.com/art4711/bmap), but now we need to find out which
bits were actually set after the intersection.

## The problem

We have a large amount of unique objects that we can easily
enumerate. We need to be able to make a set of some of those objects
and pass them around in our program. The obvious implementation for
this is an array. But at a certain point collecting the objects into
an array takes too much memory and becomes too slow. This is
especially interesting when performing unions and intersections
between sets. Storing the enumerations of those objects into a bitmap
can save memory and make unions and intersections faster. It's cheap
to populate a bitmap from an array and we can then extract the
relevant elements from the bitmap sorted and deduplicated (relevant
for unions).

Bitmaps also have very predictable memory usage. If you know that
your objects are numbered between 0 and X, you need X/8 bytes of
memory for the bitmap. Collecting the objects into an array requires
resizing the array, which leads to copies, which while it's called
"amortized O(1)" is expensive in practice anyway. Cache isn't free.

So bitmaps have very nice properties. But they do have a problem. If
the number of elements we will collect into a set isn't known in
advance or hard to estimate we can end up with a very sparse bitmap.
Other than wasting space there's also a problem of iterating over
all the set bits in it.

Bitmaps happen to fit perfectly for an application I'm working on, but
we've always had a problem to estimate how dense a set will become and
when to switch between a sorted array and a bitmap. In one use case
we made up an estimate and then moved it around until all benchmarks
agreed. The worry is that this is dependent on our particular use case
at this time and might not be true later.

So we need to figure out a data structure that's space and time
efficient to populate, but also allows for efficient traversal.

## The API a bitmap implementation needs to provide

 * alloc/free - Allocate and free the bitmap

 * set - Sets one bit in the bitmap

 * isset - Check if the bit is set (this is just a helper function
   that I needed to implement to actually generate the "random" data
   we're testing with).

 * first_set(b) - This is the meat of the test. Find the first set bit
   in the bitmap that is equal to or larger than `b`.

The bitmaps only need to handle up to 2^32 bits.

I've been debating adding a `foreach` function, but it doesn't really
matter for my application and can be trivially implemented as:

    last = 0;
    while ((last = first_set(last)) != INVALID)
        last++;

Which will be almost as effcient (minus some savings we can get from
not having to recalculate some offsets in `first_set`).

## The implementations

### dumb

The bitmap is just an array of 64 bit elements. `alloc`, `free`, `set`
and `isset` are trivial. `first_set` checks every bit starting from
`b` until it finds one set bit.

### simple

Same as `dumb` except that `first_set` now checks a whole 64 bit word
at a time until it finds a non-zero one. This implementation depends
on the compiler providing `__builtin_ffsll` which both gcc and clang
do.

### p64-naive - 64 bit pyramid.

6 levels of bitmaps. The lowest level (5), is a normal bitmap with 64
bit slots. The levels above are bitmaps indicating which of the slots
in the next level are not zero. The idea is that this allows us to
take bigger strides forward when traversing sparse bitmaps.

### p64

Same as p64-naive, but with a particular optimization in `first_set`
that allow us to quickly find matches in dense bitmaps.

### p64v2

Same as `p64`, but with cleaner code.

### p64v3

Like `p64` but upside down and with a dynamic number of levels.

### p64v3r

Like `p64v3` but with `first_set` implemented recursively.

### p64v3r2 and p64v2r3

Loke `p64v2r`, but testing two different ways of starting the
recursion.

## The tests

### populate

We populate a bitmap from an array of elements. 

### check

We walk the bitmap by:

    last = 0;
    while ((last = first_set(last)) != INVALID)
	last++;

and check that the returned elements match the elements of the array
we used to populate the bitmap.

## The sets

I haven't polished the sizes of the sets or been too ambitious in
generating many sets, but the current results show what I need to
know. If someone has a different use case, feel free to add tests
and make a pull request. The most interesting case here would
probably be to add a set with many clusters and large mostly empty
areas between.

I'm aware that 25 million isn't that "huge", but it's all I needed to
know and the `dumb` and `simple` implementations take too long to run
on larger bitmaps.

### small-sparse

10 bits set in a space of 1000 possible bits.

### mid-sparse

100 set, 1M space.

### mid-mid

10k in 1M

### mid-dense

0.5M in 1M

### large-sparse

10 in 10M

### huge-sparse

10 in 25M

## The results.

There are no relevant units here, we could calculate the time various
implementations spend per bit, but that's not as interesting as how
much faster/slower various operations are on the different
implementations. The only relevant thing is how the different
implementations compare.

The reference implementation is `simple` everything is compared to it.


### populate

#### dumb

There shouldn't be any difference between `simple` and `dumb` here,
they both use the exact same code.

    dumb-huge-sparse-populate vs. simple-huge-sparse-populate
    x statdir/simple-huge-sparse-populate
    + statdir/dumb-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0         1e-06             0         3e-08 1.7144661e-07
    + 100             0         1e-06             0         2e-08 1.4070529e-07
    No difference proven at 98.0% confidence
    
    dumb-large-sparse-populate vs. simple-large-sparse-populate
    x statdir/simple-large-sparse-populate
    + statdir/dumb-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0         1e-06             0         8e-08 2.7265992e-07
    + 100             0      0.000135             0      1.66e-06 1.3476669e-05
    No difference proven at 98.0% confidence
    
    dumb-mid-dense-populate vs. simple-mid-dense-populate
    x statdir/simple-mid-dense-populate
    + statdir/dumb-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.110867      0.144534      0.120212    0.12173365  0.0068416292
    + 100      0.112454      0.140728      0.118668    0.11940546  0.0042432277
    Difference at 98.0% confidence
    	-0.00232819 +/- 0.00187258
    	-1.91253% +/- 1.53826%
    	(Student's t, pooled s = 0.00569267)
     
    dumb-mid-mid-populate vs. simple-mid-mid-populate
    x statdir/simple-mid-mid-populate
    + statdir/dumb-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.002087      0.004113      0.002303    0.00238652  0.0003583565
    + 100       0.00207      0.004117      0.002306    0.00250836  0.0004883469
    No difference proven at 98.0% confidence
     
    dumb-mid-sparse-populate vs. simple-mid-sparse-populate
    x statdir/simple-mid-sparse-populate
    + statdir/dumb-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       2.2e-05       7.2e-05       2.8e-05     2.827e-05 7.7157258e-06
    + 100       2.1e-05       7.5e-05       2.6e-05     2.736e-05 9.0033888e-06
    No difference proven at 98.0% confidence
     
    dumb-small-sparse-populate vs. simple-small-sparse-populate
    x statdir/simple-small-sparse-populate
    + statdir/dumb-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.002195      0.003058      0.002324    0.00236736 0.00016493075
    + 100      0.002202      0.003485      0.002331    0.00242904 0.00025802247
    No difference proven at 98.0% confidence

ministat sees a difference, but this is due to the machine I'm running
this on being noisy and the resolution of the timers sucks. But this
basically tells us that differences of 2% are just noise (or I should
have used a higher confidence).

#### p64

It's much more interesting to compare to `p64` (this is exactly the
same as `p64-naive` +/- noise).

    p64-huge-sparse-populate vs. simple-huge-sparse-populate
    x statdir/simple-huge-sparse-populate
    + statdir/p64-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0         1e-06             0         3e-08 1.7144661e-07
    + 100             0       2.8e-05         1e-06       8.5e-07 2.7866021e-06
    Difference at 98.0% confidence
    	8.2e-07 +/- 6.49389e-07
    	2733.33% +/- 2164.63%
    	(Student's t, pooled s = 1.97415e-06)
    
    p64-large-sparse-populate vs. simple-large-sparse-populate
    x statdir/simple-large-sparse-populate
    + statdir/p64-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0         1e-06             0         8e-08 2.7265992e-07
    + 100         1e-06       2.7e-05         1e-06      1.26e-06       2.6e-06
    Difference at 98.0% confidence
    	1.18e-06 +/- 6.08076e-07
    	1475% +/- 760.095%
    	(Student's t, pooled s = 1.84856e-06)
    
    p64-mid-dense-populate vs. simple-mid-dense-populate
    x statdir/simple-mid-dense-populate
    + statdir/p64-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.110867      0.144534      0.120212    0.12173365  0.0068416292
    + 100      0.362134      0.410082      0.378707    0.37947826  0.0094957551
    Difference at 98.0% confidence
    	0.257745 +/- 0.00272229
    	211.728% +/- 2.23627%
    	(Student's t, pooled s = 0.00827579)
    
    p64-mid-mid-populate vs. simple-mid-mid-populate
    x statdir/simple-mid-mid-populate
    + statdir/p64-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.002087      0.004113      0.002303    0.00238652  0.0003583565
    + 100      0.007243      0.009811      0.007418    0.00760898 0.00049404604
    Difference at 98.0% confidence
    	0.00522246 +/- 0.000141962
    	218.832% +/- 5.94851%
    	(Student's t, pooled s = 0.000431567)
    
    p64-mid-sparse-populate vs. simple-mid-sparse-populate
    x statdir/simple-mid-sparse-populate
    + statdir/p64-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       2.2e-05       7.2e-05       2.8e-05     2.827e-05 7.7157258e-06
    + 100       7.9e-05      0.000211       7.9e-05     8.748e-05 1.8417975e-05
    Difference at 98.0% confidence
    	5.921e-05 +/- 4.64475e-06
    	209.445% +/- 16.43%
    	(Student's t, pooled s = 1.41201e-05)
    
    p64-small-sparse-populate vs. simple-small-sparse-populate
    x statdir/simple-small-sparse-populate
    + statdir/p64-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.002195      0.003058      0.002324    0.00236736 0.00016493075
    + 100      0.006885      0.009993      0.007492    0.00752581 0.00048327127
    Difference at 98.0% confidence
    	0.00515845 +/- 0.000118775
    	217.899% +/- 5.01719%
    	(Student's t, pooled s = 0.000361077)

The `large-sparse` and `huge-sparse` tests are mostly useless here. In
the actual result there's one initial cache miss and then the numbers
hover between 0 and 0.000001 after which the measurements are
truncated. This is useless.

But all the other tests show a consistent 3.1-3.2x slowdown. This is
expected and actually lower than I feared. This means that the extra
cost for p64 won't be in populating the array.

### check

#### dumb

This is where the difference between `simple` and `dumb` is. 

    dumb-huge-sparse-check vs. simple-huge-sparse-check
    x statdir/simple-huge-sparse-check
    + statdir/dumb-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000856      0.001234      0.000919    0.00093814 6.6426236e-05
    + 100      0.061269      0.072319      0.064139    0.06462294  0.0025259663
    Difference at 98.0% confidence
    	0.0636848 +/- 0.000587743
    	6788.41% +/- 62.6498%
    	(Student's t, pooled s = 0.00178675)
    
    dumb-large-sparse-check vs. simple-large-sparse-check
    x statdir/simple-large-sparse-check
    + statdir/dumb-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00076      0.001716      0.000779    0.00083314 0.00013628646
    + 100      0.061537      0.088013      0.064211    0.06494016  0.0031735456
    Difference at 98.0% confidence
    	0.064107 +/- 0.000738847
    	7694.63% +/- 88.6822%
    	(Student's t, pooled s = 0.0022461)
    
    dumb-mid-dense-check vs. simple-mid-dense-check
    x statdir/simple-mid-dense-check
    + statdir/dumb-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.220685      0.254971      0.228038    0.22901875  0.0054344739
    + 100      0.517313      0.612694      0.534536    0.53653778    0.01114508
    Difference at 98.0% confidence
    	0.307519 +/- 0.00288411
    	134.277% +/- 1.25933%
    	(Student's t, pooled s = 0.00876773)
    
    dumb-mid-mid-check vs. simple-mid-mid-check
    x statdir/simple-mid-mid-check
    + statdir/dumb-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.010274      0.017926      0.010719    0.01107091  0.0012050884
    + 100       0.07466      0.087193       0.07886    0.07915763  0.0027447457
    Difference at 98.0% confidence
    	0.0680867 +/- 0.000697252
    	615.006% +/- 6.29805%
    	(Student's t, pooled s = 0.00211965)
    
    dumb-mid-sparse-check vs. simple-mid-sparse-check
    x statdir/simple-mid-sparse-check
    + statdir/dumb-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00084      0.002878      0.000974     0.0009775 0.00021171357
    + 100      0.063333      0.071094      0.067018    0.06718423  0.0018533315
    Difference at 98.0% confidence
    	0.0662067 +/- 0.000433888
    	6773.07% +/- 44.3876%
    	(Student's t, pooled s = 0.00131903)
    
    dumb-small-sparse-check vs. simple-small-sparse-check
    x statdir/simple-small-sparse-check
    + statdir/dumb-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.004232       0.00537      0.004442    0.00451139 0.00025890527
    + 100      0.070262       0.08348      0.074601    0.07486289  0.0025231068
    Difference at 98.0% confidence
    	0.0703515 +/- 0.000589956
    	1559.42% +/- 13.077%
    	(Student's t, pooled s = 0.00179347)

Since `simple` checks 64 bits at a time we expect it to be up to that faster.

The `mid-dense` set is insteresting here since it shows the smallest
difference. The reason for this is that it is a very densely populated set,
every other bit on average is set. But even here `simple` wins by a large
margin.

#### p64-naive

Now, this is where the interesting part comes.

    p64-naive-huge-sparse-check vs. simple-huge-sparse-check
    x statdir/simple-huge-sparse-check
    + statdir/p64-naive-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000856      0.001234      0.000919    0.00093814 6.6426236e-05
    + 100         2e-06       1.1e-05         3e-06      2.82e-06 1.2978615e-06
    Difference at 98.0% confidence
    	-0.00093532 +/- 1.54537e-05
    	-99.6994% +/- 1.64727%
    	(Student's t, pooled s = 4.69794e-05)
    
    p64-naive-large-sparse-check vs. simple-large-sparse-check
    x statdir/simple-large-sparse-check
    + statdir/p64-naive-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00076      0.001716      0.000779    0.00083314 0.00013628646
    + 100         7e-06         7e-06         7e-06         7e-06 4.7496812e-13
    Difference at 98.0% confidence
    	-0.00082614 +/- 3.17002e-05
    	-99.1598% +/- 3.80491%
    	(Student's t, pooled s = 9.63691e-05)
    
    p64-naive-mid-dense-check vs. simple-mid-dense-check
    x statdir/simple-mid-dense-check
    + statdir/p64-naive-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.220685      0.254971      0.228038    0.22901875  0.0054344739
    + 100      1.829034      1.952014      1.854075     1.8571515   0.018700907
    Difference at 98.0% confidence
    	1.62813 +/- 0.00452978
    	710.917% +/- 1.97791%
    	(Student's t, pooled s = 0.0137706)
    
    p64-naive-mid-mid-check vs. simple-mid-mid-check
    x statdir/simple-mid-mid-check
    + statdir/p64-naive-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.010274      0.017926      0.010719    0.01107091  0.0012050884
    + 100      0.044361       0.05183      0.045599    0.04621578  0.0015895634
    Difference at 98.0% confidence
    	0.0351449 +/- 0.000463974
    	317.452% +/- 4.19093%
    	(Student's t, pooled s = 0.00141049)
    
    p64-naive-mid-sparse-check vs. simple-mid-sparse-check
    x statdir/simple-mid-sparse-check
    + statdir/p64-naive-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00084      0.002878      0.000974     0.0009775 0.00021171357
    + 100      0.000502      0.000652      0.000605    0.00058224 4.2376647e-05
    Difference at 98.0% confidence
    	-0.00039526 +/- 5.02214e-05
    	-40.4358% +/- 5.13774%
    	(Student's t, pooled s = 0.000152674)
    
    p64-naive-small-sparse-check vs. simple-small-sparse-check
    x statdir/simple-small-sparse-check
    + statdir/p64-naive-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.004232       0.00537      0.004442    0.00451139 0.00025890527
    + 100      0.040855      0.047868      0.043472    0.04350861  0.0015506382
    Difference at 98.0% confidence
    	0.0389972 +/- 0.000365671
    	864.417% +/- 8.10551%
    	(Student's t, pooled s = 0.00111165)
    
We can see that for the huge and large sparse bitmaps p64-naive
performs exceptionally well. The additional levels that allow us to
skip checking large portions of the bitmap kick in and we're so much
faster that the measurement resolution and noise make the comparison
not much more accurate than "at least two magnitudes better".

Unfortunately we can see serious slowdowns in `mid-dense`, `mid-mid`
and `small-sparse`. Let's see if we can do anything about that. `p64`
is the attempt to solve some of the problems.

#### p64

The difference between `p64-naive` and `p64` is just one optimization
in `first_set`. We add a check that immediately goes to the lowest
level bitmap, to the slot where `b` is and checks that slot for a
match. This should bring those fast matches to the level of
`simple`. We don't expect that much improvements for small bitmaps
(the slowdowns for them come from a different problem). So we're
aiming at fixing `mid-dense` here, nothing else.


    p64-huge-sparse-check vs. simple-huge-sparse-check
    x statdir/simple-huge-sparse-check
    + statdir/p64-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000856      0.001234      0.000919    0.00093814 6.6426236e-05
    + 100         2e-06         8e-06         2e-06      2.19e-06 6.7711509e-07
    Difference at 98.0% confidence
    	-0.00093595 +/- 1.54515e-05
    	-99.7666% +/- 1.64704%
    	(Student's t, pooled s = 4.69729e-05)
    
    p64-large-sparse-check vs. simple-large-sparse-check
    x statdir/simple-large-sparse-check
    + statdir/p64-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00076      0.001716      0.000779    0.00083314 0.00013628646
    + 100         5e-06         6e-06         5e-06      5.01e-06         1e-07
    Difference at 98.0% confidence
    	-0.00082813 +/- 3.17002e-05
    	-99.3987% +/- 3.80491%
    	(Student's t, pooled s = 9.63691e-05)
    
    p64-mid-dense-check vs. simple-mid-dense-check
    x statdir/simple-mid-dense-check
    + statdir/p64-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.220685      0.254971      0.228038    0.22901875  0.0054344739
    + 100      0.244028      0.268154      0.251725    0.25286724  0.0050118328
    Difference at 98.0% confidence
    	0.0238485 +/- 0.00171954
    	10.4133% +/- 0.75083%
    	(Student's t, pooled s = 0.00522743)
    
    p64-mid-mid-check vs. simple-mid-mid-check
    x statdir/simple-mid-mid-check
    + statdir/p64-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.010274      0.017926      0.010719    0.01107091  0.0012050884
    + 100      0.029802      0.035992      0.030514    0.03097506  0.0012444021
    Difference at 98.0% confidence
    	0.0199042 +/- 0.000402927
    	179.788% +/- 3.63951%
    	(Student's t, pooled s = 0.0012249)
    
    p64-mid-sparse-check vs. simple-mid-sparse-check
    x statdir/simple-mid-sparse-check
    + statdir/p64-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00084      0.002878      0.000974     0.0009775 0.00021171357
    + 100      0.000427       0.00079      0.000436    0.00046951 7.2605965e-05
    Difference at 98.0% confidence
    	-0.00050799 +/- 5.20599e-05
    	-51.9683% +/- 5.32583%
    	(Student's t, pooled s = 0.000158263)
    
    p64-small-sparse-check vs. simple-small-sparse-check
    x statdir/simple-small-sparse-check
    + statdir/p64-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.004232       0.00537      0.004442    0.00451139 0.00025890527
    + 100      0.020101      0.025216      0.020607    0.02100451 0.00095730719
    Difference at 98.0% confidence
    	0.0164931 +/- 0.000230669
    	365.588% +/- 5.11305%
    	(Student's t, pooled s = 0.000701238)
    
We can see here that `mid-dense` is indeed fixed and the other cases
improve somewhat too.

#### p64 vs. p64-naive

Let's compare p64 to p64-naive:

    p64-naive-huge-sparse-check vs. p64-huge-sparse-check
    x statdir/p64-huge-sparse-check
    + statdir/p64-naive-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         2e-06         8e-06         2e-06      2.19e-06 6.7711509e-07
    + 100         2e-06       1.1e-05         3e-06      2.82e-06 1.2978615e-06
    Difference at 98.0% confidence
    	6.3e-07 +/- 3.40497e-07
    	28.7671% +/- 15.5478%
    	(Student's t, pooled s = 1.03512e-06)
    
    p64-naive-large-sparse-check vs. p64-large-sparse-check
    x statdir/p64-large-sparse-check
    + statdir/p64-naive-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         5e-06         6e-06         5e-06      5.01e-06         1e-07
    + 100         7e-06         7e-06         7e-06         7e-06 4.7496812e-13
    Difference at 98.0% confidence
    	1.99e-06 +/- 2.326e-08
    	39.7206% +/- 0.464271%
    	(Student's t, pooled s = 7.07107e-08)
    
    p64-naive-mid-dense-check vs. p64-mid-dense-check
    x statdir/p64-mid-dense-check
    + statdir/p64-naive-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.244028      0.268154      0.251725    0.25286724  0.0050118328
    + 100      1.829034      1.952014      1.854075     1.8571515   0.018700907
    Difference at 98.0% confidence
    	1.60428 +/- 0.00450333
    	634.437% +/- 1.78091%
    	(Student's t, pooled s = 0.0136902)
    
    p64-naive-mid-mid-check vs. p64-mid-mid-check
    x statdir/p64-mid-mid-check
    + statdir/p64-naive-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.029802      0.035992      0.030514    0.03097506  0.0012444021
    + 100      0.044361       0.05183      0.045599    0.04621578  0.0015895634
    Difference at 98.0% confidence
    	0.0152407 +/- 0.000469555
    	49.2032% +/- 1.51591%
    	(Student's t, pooled s = 0.00142745)
    
    p64-naive-mid-sparse-check vs. p64-mid-sparse-check
    x statdir/p64-mid-sparse-check
    + statdir/p64-naive-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000427       0.00079      0.000436    0.00046951 7.2605965e-05
    + 100      0.000502      0.000652      0.000605    0.00058224 4.2376647e-05
    Difference at 98.0% confidence
    	0.00011273 +/- 1.95542e-05
    	24.0101% +/- 4.16481%
    	(Student's t, pooled s = 5.9445e-05)
    
    p64-naive-small-sparse-check vs. p64-small-sparse-check
    x statdir/p64-small-sparse-check
    + statdir/p64-naive-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.020101      0.025216      0.020607    0.02100451 0.00095730719
    + 100      0.040855      0.047868      0.043472    0.04350861  0.0015506382
    Difference at 98.0% confidence
    	0.0225041 +/- 0.000423876
    	107.139% +/- 2.01802%
    	(Student's t, pooled s = 0.00128859)
    
`p64` is faster in all cases. Great. From now on we'll use `p64` as
the reference in all comparisons, the goal now is to make something
better than it.

#### p64v2

Does cleaning up the code make any difference?

p64v2 is like p64, but with cleaner code (I probably overdid it) with
constants and inline functions instead of macros and magic numbers.

The reason for this cleanup is mostly so that I can easier copy and
paste the code and just change the names of the functions and
constants. The reason for copy and paste is to keep history so that
this README makes sense and the measurements are repeatable.

    p64v2-huge-sparse-check vs. p64-huge-sparse-check
    x statdir/p64-huge-sparse-check
    + statdir/p64v2-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         2e-06         1e-05         2e-06      2.18e-06 1.0088497e-06
    + 100         2e-06         4e-06         2e-06       2.5e-06 5.4122943e-07
    Difference at 98.0% confidence
    	3.2e-07 +/- 2.66295e-07
    	14.6789% +/- 12.2154%
    	(Student's t, pooled s = 8.09539e-07)
    
    p64v2-large-sparse-check vs. p64-large-sparse-check
    x statdir/p64-large-sparse-check
    + statdir/p64v2-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         5e-06         7e-06         6e-06      5.96e-06  4.476651e-07
    + 100         5e-06       7.9e-05         5e-06      6.74e-06 7.5124409e-06
    No difference proven at 98.0% confidence
    
    p64v2-mid-dense-check vs. p64-mid-dense-check
    x statdir/p64-mid-dense-check
    + statdir/p64v2-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.247303      0.271981      0.255494    0.25670123  0.0056335383
    + 100      0.246615      0.275596      0.255562    0.25676922  0.0061277597
    No difference proven at 98.0% confidence
    
    p64v2-mid-mid-check vs. p64-mid-mid-check
    x statdir/p64-mid-mid-check
    + statdir/p64v2-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.030097      0.035698      0.032656    0.03251763  0.0013166318
    + 100      0.029849      0.036222      0.032176    0.03240194  0.0014723703
    No difference proven at 98.0% confidence
    
    p64v2-mid-sparse-check vs. p64-mid-sparse-check
    x statdir/p64-mid-sparse-check
    + statdir/p64v2-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000427      0.000709      0.000516    0.00051223 5.6771001e-05
    + 100      0.000427      0.000835      0.000481    0.00049829  6.971128e-05
    No difference proven at 98.0% confidence
    
    p64v2-small-sparse-check vs. p64-small-sparse-check
    x statdir/p64-small-sparse-check
    + statdir/p64v2-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.020374      0.025275      0.022104    0.02207814 0.00099609471
    + 100      0.020007      0.024363      0.021987    0.02203621  0.0011344023
    No difference proven at 98.0% confidence
    
    p64v2-huge-sparse-populate vs. p64-huge-sparse-populate
    x statdir/p64-huge-sparse-populate
    + statdir/p64v2-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0       3.6e-05             0       7.1e-07 3.6743583e-06
    + 100             0         1e-06             0       3.2e-07 4.6882617e-07
    No difference proven at 98.0% confidence
    
    p64v2-large-sparse-populate vs. p64-large-sparse-populate
    x statdir/p64-large-sparse-populate
    + statdir/p64v2-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06         5e-05         1e-06      1.51e-06       4.9e-06
    + 100         1e-06         1e-06         1e-06         1e-06 5.4796965e-14
    No difference proven at 98.0% confidence
    
    p64v2-mid-dense-populate vs. p64-mid-dense-populate
    x statdir/p64-mid-dense-populate
    + statdir/p64v2-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.369172      0.407703      0.379142    0.38095742  0.0079580958
    + 100      0.369351      0.399195      0.381194    0.38223969  0.0067370766
    No difference proven at 98.0% confidence
    
    p64v2-mid-mid-populate vs. p64-mid-mid-populate
    x statdir/p64-mid-mid-populate
    + statdir/p64v2-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.007114       0.00988      0.007805    0.00801016 0.00063309448
    + 100      0.007265      0.009959      0.008035    0.00813407 0.00061046892
    No difference proven at 98.0% confidence
    
    p64v2-mid-sparse-populate vs. p64-mid-sparse-populate
    x statdir/p64-mid-sparse-populate
    + statdir/p64v2-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       6.7e-05      0.000307       8.7e-05     9.535e-05 2.5897769e-05
    + 100       6.9e-05       0.00015       9.8e-05    0.00010305 1.9248705e-05
    Difference at 98.0% confidence
    	7.7e-06 +/- 7.50548e-06
    	8.07551% +/- 7.8715%
    	(Student's t, pooled s = 2.28167e-05)
    
    p64v2-small-sparse-populate vs. p64-small-sparse-populate
    x statdir/p64-small-sparse-populate
    + statdir/p64v2-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.006899      0.010581      0.007838    0.00802454 0.00072905221
    + 100      0.007262      0.010879      0.008237    0.00836105 0.00066652532
    Difference at 98.0% confidence
    	0.00033651 +/- 0.000229765
    	4.19351% +/- 2.86328%
    	(Student's t, pooled s = 0.000698489)

The only relevant differences are in the noisy measurements, so it's
probably fine.

#### p64v3

`p64v3` turns `p64` upside down. The full bitmap is now level 0 and
the higher levels are compressed. Also, we now dynamically determine
the number of levels we need, this should help the smaller bitmaps.

    p64v3-huge-sparse-check vs. p64-huge-sparse-check
    x statdir/p64-huge-sparse-check
    + statdir/p64v3-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         2e-06         8e-06         2e-06      2.21e-06 8.2013795e-07
    + 100         2e-06       7.3e-05         3e-06       3.4e-06 7.1279787e-06
    No difference proven at 98.0% confidence
    
    p64v3-large-sparse-check vs. p64-large-sparse-check
    x statdir/p64-large-sparse-check
    + statdir/p64v3-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         5e-06       7.6e-05         5e-06      6.13e-06 7.1557775e-06
    + 100         4e-06         8e-05         5e-06      5.88e-06 7.7228246e-06
    No difference proven at 98.0% confidence
    
    p64v3-mid-dense-check vs. p64-mid-dense-check
    x statdir/p64-mid-dense-check
    + statdir/p64v3-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.250877      0.270462      0.260754    0.26055253  0.0042139472
    + 100      0.244836      0.265103      0.255659    0.25556786   0.004333112
    Difference at 98.0% confidence
    	-0.00498467 +/- 0.0014059
    	-1.91312% +/- 0.539583%
    	(Student's t, pooled s = 0.00427394)
    
    p64v3-mid-mid-check vs. p64-mid-mid-check
    x statdir/p64-mid-mid-check
    + statdir/p64v3-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.030394      0.035923      0.032744    0.03283831  0.0011910763
    + 100      0.022091      0.027486      0.023936    0.02394106  0.0012042394
    Difference at 98.0% confidence
    	-0.00889725 +/- 0.000393971
    	-27.0941% +/- 1.19973%
    	(Student's t, pooled s = 0.00119768)
    
    p64v3-mid-sparse-check vs. p64-mid-sparse-check
    x statdir/p64-mid-sparse-check
    + statdir/p64v3-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000429      0.000991      0.000509    0.00052302 9.4357688e-05
    + 100      0.000316      0.000513      0.000369    0.00036993 3.5640533e-05
    Difference at 98.0% confidence
    	-0.00015309 +/- 2.34611e-05
    	-29.2704% +/- 4.48569%
    	(Student's t, pooled s = 7.13219e-05)
    
    p64v3-small-sparse-check vs. p64-small-sparse-check
    x statdir/p64-small-sparse-check
    + statdir/p64v3-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.020985      0.025902      0.022692    0.02268463  0.0010345038
    + 100      0.008701      0.011403      0.009639    0.00969347 0.00068409512
    Difference at 98.0% confidence
    	-0.0129912 +/- 0.000288479
    	-57.2686% +/- 1.27169%
    	(Student's t, pooled s = 0.000876979)
    
    p64v3-huge-sparse-populate vs. p64-huge-sparse-populate
    x statdir/p64-huge-sparse-populate
    + statdir/p64v3-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0       3.1e-05             0       4.9e-07   3.43039e-06
    + 100             0         1e-05         1e-06      1.15e-06 1.3734495e-06
    No difference proven at 98.0% confidence
    
    p64v3-large-sparse-populate vs. p64-large-sparse-populate
    x statdir/p64-large-sparse-populate
    + statdir/p64v3-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06         3e-05         1e-06      1.39e-06 3.0281341e-06
    + 100         1e-06       1.1e-05         2e-06      1.75e-06 1.0480863e-06
    No difference proven at 98.0% confidence
    
    p64v3-mid-dense-populate vs. p64-mid-dense-populate
    x statdir/p64-mid-dense-populate
    + statdir/p64v3-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.376682      0.406025      0.390308    0.39024369  0.0066664778
    + 100      0.450944      0.493576      0.463086    0.46373317  0.0072193309
    Difference at 98.0% confidence
    	0.0734895 +/- 0.00228565
    	18.8317% +/- 0.585698%
    	(Student's t, pooled s = 0.00694841)
    
    p64v3-mid-mid-populate vs. p64-mid-mid-populate
    x statdir/p64-mid-mid-populate
    + statdir/p64v3-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.007155      0.010699       0.00783    0.00805934 0.00070068808
    + 100      0.008651      0.011676      0.009687    0.00978862 0.00070914294
    Difference at 98.0% confidence
    	0.00172928 +/- 0.000231883
    	21.4568% +/- 2.8772%
    	(Student's t, pooled s = 0.000704928)
    
    p64v3-mid-sparse-populate vs. p64-mid-sparse-populate
    x statdir/p64-mid-sparse-populate
    + statdir/p64v3-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       7.9e-05      0.000152       8.8e-05     9.887e-05 1.9319564e-05
    + 100       9.4e-05      0.000233      0.000131    0.00013446 3.1389511e-05
    Difference at 98.0% confidence
    	3.559e-05 +/- 8.57328e-06
    	35.9968% +/- 8.67127%
    	(Student's t, pooled s = 2.60629e-05)
    
    p64v3-small-sparse-populate vs. p64-small-sparse-populate
    x statdir/p64-small-sparse-populate
    + statdir/p64v3-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.007201      0.009969      0.008029    0.00819803 0.00064610595
    + 100      0.005819      0.008778      0.006389    0.00663745 0.00065427403
    Difference at 98.0% confidence
    	-0.00156058 +/- 0.000213882
    	-19.036% +/- 2.60894%
    	(Student's t, pooled s = 0.000650203)

As expected, this helps `small-sparse`, `mid-sparse` and `mid-mid`
which were the sets that needed help (see the tests for p64-naive
vs. simple). Unfortunately populate becomes even slower.

#### p64v3r

The for loop in all p64* `first_set` had code smell. I really didn't
like the adding/subtracting 2 from l just to backtrack. It worked but
it was surprising and ugly. With `p64v3` the loop made even less sense.
we're iterating down from `pb->levels - 1` ugh. This would look much
cleaner with recursion. `p64v3r` is the recursive implementation.

The code works like this: We find our slot, mask out all bits that we
aren't allowed to match (just like before), if the masked out slot is
zero it means that the higher levels led us to a false positive (we
masked out bits that were set), this means that we have to backtrack
to higher level (this is the `l += 2` in the for loop versions). If we
have a hit, go down to a lower level.

Hit when level is 0, means we have a result. Backtrack when we're at
the top level already means we have a negative result.

Each level we go up or down we adjust `b` to the lowest possible value
it can have.

This also leads the a surprising algorithm change. `p64` and all
subsequent p64* started with checking the full bitmap to optimize
dense bitmaps, if that optimization missed we started at the top level
and went down (backtracking as necessary). But the recursion shows us
that it's equally valid to start at the bitmap level and let the
recursion handle going back to the top level if it thinks it's
necessary. At this point the difference between the two approaches
hasn't been tested, the recursion just made "start at 0 and backtrack
as necessary" the more natural implementation.

Comparing against `p64v3`:

    p64v3r-huge-sparse-check vs. p64v3-huge-sparse-check
    x statdir/p64v3-huge-sparse-check
    + statdir/p64v3r-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         2e-06       1.7e-05         2e-06      2.19e-06 1.5088627e-06
    + 100         1e-06         9e-06         1e-06      1.23e-06 8.6287047e-07
    Difference at 98.0% confidence
    	-9.6e-07 +/- 4.04297e-07
    	-43.8356% +/- 18.461%
    	(Student's t, pooled s = 1.22907e-06)
    
    p64v3r-large-sparse-check vs. p64v3-large-sparse-check
    x statdir/p64v3-large-sparse-check
    + statdir/p64v3r-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         4e-06         6e-06         5e-06      4.67e-06 6.5219133e-07
    + 100         3e-06       1.9e-05         3e-06      3.49e-06 2.0024984e-06
    Difference at 98.0% confidence
    	-1.18e-06 +/- 4.89862e-07
    	-25.2677% +/- 10.4896%
    	(Student's t, pooled s = 1.48919e-06)
    
    p64v3r-mid-dense-check vs. p64v3-mid-dense-check
    x statdir/p64v3-mid-dense-check
    + statdir/p64v3r-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.247342      0.327149      0.256359    0.25739302  0.0082802183
    + 100      0.284968      0.308148      0.295692    0.29589157  0.0054489201
    Difference at 98.0% confidence
    	0.0384986 +/- 0.00230559
    	14.9571% +/- 0.895747%
    	(Student's t, pooled s = 0.00700902)
    
    p64v3r-mid-mid-check vs. p64v3-mid-mid-check
    x statdir/p64v3-mid-mid-check
    + statdir/p64v3r-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.022475      0.026862      0.023973    0.02408233 0.00099462302
    + 100      0.013548      0.017706      0.014905    0.01498044 0.00075821271
    Difference at 98.0% confidence
    	-0.00910189 +/- 0.000290905
    	-37.7949% +/- 1.20796%
    	(Student's t, pooled s = 0.000884353)
    
    p64v3r-mid-sparse-check vs. p64v3-mid-sparse-check
    x statdir/p64v3-mid-sparse-check
    + statdir/p64v3r-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000314      0.000575      0.000367    0.00037028  4.793853e-05
    + 100      0.000192      0.000353      0.000213    0.00022851 3.1715113e-05
    Difference at 98.0% confidence
    	-0.00014177 +/- 1.33698e-05
    	-38.2872% +/- 3.61074%
    	(Student's t, pooled s = 4.06445e-05)
    
    p64v3r-small-sparse-check vs. p64v3-small-sparse-check
    x statdir/p64v3-small-sparse-check
    + statdir/p64v3r-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008628      0.011991      0.009568    0.00959736 0.00067287484
    + 100      0.008726      0.011427      0.009686    0.00977493 0.00066371996
    No difference proven at 98.0% confidence

Decent performance increase overall except `mid-dense`. That slowdown
can be explained by `mid-dense` almost always hitting the "check full
bitmap" optimization which used to be done with code written with the
level hardcoded to 0 which can simplify a lot of calculations. Now, the
level is passed as a function argument.

#### p64v3r2

`p64v3r2` is like `p64v3r` but instead of just doing the natrual thing
and just starting the recursion at the full bitmap, we do the quick
check of the full bitmap, then start the recursion at the highest
level.

Comparing to `p64v3r`:

    p64v3r2-mid-dense-check vs. p64v3r-mid-dense-check
    x statdir/p64v3r-mid-dense-check
    + statdir/p64v3r2-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.284074      0.319683      0.295312    0.29636127  0.0068496646
    + 100      0.247361      0.267703      0.256071    0.25619853  0.0044370252
    Difference at 98.0% confidence
    	-0.0401627 +/- 0.00189829
    	-13.552% +/- 0.640534%
    	(Student's t, pooled s = 0.00577084)
    
    p64v3r2-mid-mid-check vs. p64v3r-mid-mid-check
    x statdir/p64v3r-mid-mid-check
    + statdir/p64v3r2-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.013605      0.017748      0.014806    0.01488863 0.00086653615
    + 100      0.019995      0.024699      0.021975    0.02202084 0.00095262276
    Difference at 98.0% confidence
    	0.00713221 +/- 0.000299537
    	47.9037% +/- 2.01185%
    	(Student's t, pooled s = 0.000910597)
    
    p64v3r2-mid-sparse-check vs. p64v3r-mid-sparse-check
    x statdir/p64v3r-mid-sparse-check
    + statdir/p64v3r2-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000205      0.000344      0.000207    0.00022428 3.0166903e-05
    + 100      0.000292      0.000527      0.000333     0.0003473 5.0009797e-05
    Difference at 98.0% confidence
    	0.00012302 +/- 1.35848e-05
    	54.8511% +/- 6.05705%
    	(Student's t, pooled s = 4.12978e-05)

`mid-dense` improves as expected, but two others get wrecked. In fact,
let's compare to `p64v3`:

    p64v3r2-huge-sparse-check vs. p64v3-huge-sparse-check
    x statdir/p64v3-huge-sparse-check
    + statdir/p64v3r2-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         2e-06       1.7e-05         2e-06      2.19e-06 1.5088627e-06
    + 100         2e-06       1.7e-05         2e-06      2.19e-06 1.5088627e-06
    No difference proven at 98.0% confidence
    
    p64v3r2-large-sparse-check vs. p64v3-large-sparse-check
    x statdir/p64v3-large-sparse-check
    + statdir/p64v3r2-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         4e-06         6e-06         5e-06      4.67e-06 6.5219133e-07
    + 100         3e-06       1.2e-05         4e-06      4.38e-06 1.5293757e-06
    No difference proven at 98.0% confidence
    
    p64v3r2-mid-dense-check vs. p64v3-mid-dense-check
    x statdir/p64v3-mid-dense-check
    + statdir/p64v3r2-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.247342      0.327149      0.256359    0.25739302  0.0082802183
    + 100      0.247361      0.267703      0.256071    0.25619853  0.0044370252
    No difference proven at 98.0% confidence
    
    p64v3r2-mid-mid-check vs. p64v3-mid-mid-check
    x statdir/p64v3-mid-mid-check
    + statdir/p64v3r2-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.022475      0.026862      0.023973    0.02408233 0.00099462302
    + 100      0.019995      0.024699      0.021975    0.02202084 0.00095262276
    Difference at 98.0% confidence
    	-0.00206149 +/- 0.000320344
    	-8.56018% +/- 1.3302%
    	(Student's t, pooled s = 0.000973849)
    
    p64v3r2-mid-sparse-check vs. p64v3-mid-sparse-check
    x statdir/p64v3-mid-sparse-check
    + statdir/p64v3r2-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000314      0.000575      0.000367    0.00037028  4.793853e-05
    + 100      0.000292      0.000527      0.000333     0.0003473 5.0009797e-05
    Difference at 98.0% confidence
    	-2.298e-05 +/- 1.61135e-05
    	-6.20611% +/- 4.3517%
    	(Student's t, pooled s = 4.89851e-05)
    
    p64v3r2-small-sparse-check vs. p64v3-small-sparse-check
    x statdir/p64v3-small-sparse-check
    + statdir/p64v3r2-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008628      0.011991      0.009568    0.00959736 0.00067287484
    + 100      0.007501      0.010297      0.008279    0.00842107 0.00062092481
    Difference at 98.0% confidence
    	-0.00117629 +/- 0.000212967
    	-12.2564% +/- 2.21901%
    	(Student's t, pooled s = 0.000647421)

Yup, the gains from `p64v3r` are almost wiped out. So it seems that it
actually pays to start from the lower levels.

#### p64v3r3

Let's check that previous "so it seems that" statement. This is what
`p64v3r3` is.

Compared to `p64v3`:

    p64v3r3-huge-sparse-check vs. p64v3-huge-sparse-check
    x statdir/p64v3-huge-sparse-check
    + statdir/p64v3r3-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         2e-06       1.7e-05         2e-06      2.19e-06 1.5088627e-06
    + 100         1e-06         2e-06         1e-06      1.19e-06 3.9427724e-07
    Difference at 98.0% confidence
    	-1e-06 +/- 3.62746e-07
    	-45.6621% +/- 16.5637%
    	(Student's t, pooled s = 1.10275e-06)
    
    p64v3r3-large-sparse-check vs. p64v3-large-sparse-check
    x statdir/p64v3-large-sparse-check
    + statdir/p64v3r3-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         4e-06         6e-06         5e-06      4.67e-06 6.5219133e-07
    + 100         3e-06         4e-06         3e-06      3.07e-06  2.564324e-07
    Difference at 98.0% confidence
    	-1.6e-06 +/- 1.63004e-07
    	-34.2612% +/- 3.49046%
    	(Student's t, pooled s = 4.95536e-07)
    
    p64v3r3-mid-dense-check vs. p64v3-mid-dense-check
    x statdir/p64v3-mid-dense-check
    + statdir/p64v3r3-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.247342      0.327149      0.256359    0.25739302  0.0082802183
    + 100      0.234121      0.254807      0.241518    0.24205306   0.004155125
    Difference at 98.0% confidence
    	-0.01534 +/- 0.00215487
    	-5.95974% +/- 0.837192%
    	(Student's t, pooled s = 0.00655084)
    
    p64v3r3-mid-mid-check vs. p64v3-mid-mid-check
    x statdir/p64v3-mid-mid-check
    + statdir/p64v3r3-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.022475      0.026862      0.023973    0.02408233 0.00099462302
    + 100      0.012052       0.01477      0.013144    0.01313037 0.00077121887
    Difference at 98.0% confidence
    	-0.010952 +/- 0.000292748
    	-45.4772% +/- 1.21562%
    	(Student's t, pooled s = 0.000889959)
    
    p64v3r3-mid-sparse-check vs. p64v3-mid-sparse-check
    x statdir/p64v3-mid-sparse-check
    + statdir/p64v3r3-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000314      0.000575      0.000367    0.00037028  4.793853e-05
    + 100      0.000179      0.000318      0.000199    0.00020605 2.3942851e-05
    Difference at 98.0% confidence
    	-0.00016423 +/- 1.24639e-05
    	-44.3529% +/- 3.36607%
    	(Student's t, pooled s = 3.78904e-05)
    
    p64v3r3-small-sparse-check vs. p64v3-small-sparse-check
    x statdir/p64v3-small-sparse-check
    + statdir/p64v3r3-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008628      0.011991      0.009568    0.00959736 0.00067287484
    + 100      0.007581      0.009733      0.008285    0.00830776 0.00047237597
    Difference at 98.0% confidence
    	-0.0012896 +/- 0.000191228
    	-13.437% +/- 1.9925%
    	(Student's t, pooled s = 0.000581335)

Improvement on all sets. We can see the same against `p64v3r2`. So it
seems we have our best candidate implementation. How does it compare
to `simple`:

    p64v3r3-huge-sparse-check vs. simple-huge-sparse-check
    x statdir/simple-huge-sparse-check
    + statdir/p64v3r3-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000856      0.001234      0.000919    0.00093814 6.6426236e-05
    + 100         1e-06         2e-06         1e-06      1.19e-06 3.9427724e-07
    Difference at 98.0% confidence
    	-0.00093695 +/- 1.5451e-05
    	-99.8732% +/- 1.64698%
    	(Student's t, pooled s = 4.69713e-05)
    
    p64v3r3-large-sparse-check vs. simple-large-sparse-check
    x statdir/simple-large-sparse-check
    + statdir/p64v3r3-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00076      0.001716      0.000779    0.00083314 0.00013628646
    + 100         3e-06         4e-06         3e-06      3.07e-06  2.564324e-07
    Difference at 98.0% confidence
    	-0.00083007 +/- 3.17003e-05
    	-99.6315% +/- 3.80492%
    	(Student's t, pooled s = 9.63693e-05)
    
    p64v3r3-mid-dense-check vs. simple-mid-dense-check
    x statdir/simple-mid-dense-check
    + statdir/p64v3r3-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.220685      0.254971      0.228038    0.22901875  0.0054344739
    + 100      0.234121      0.254807      0.241518    0.24205306   0.004155125
    Difference at 98.0% confidence
    	0.0130343 +/- 0.0015912
    	5.69137% +/- 0.694792%
    	(Student's t, pooled s = 0.00483728)
    
    p64v3r3-mid-mid-check vs. simple-mid-mid-check
    x statdir/simple-mid-mid-check
    + statdir/p64v3r3-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.010274      0.017926      0.010719    0.01107091  0.0012050884
    + 100      0.012052       0.01477      0.013144    0.01313037 0.00077121887
    Difference at 98.0% confidence
    	0.00205946 +/- 0.00033279
    	18.6024% +/- 3.00599%
    	(Student's t, pooled s = 0.00101169)
    
    p64v3r3-mid-sparse-check vs. simple-mid-sparse-check
    x statdir/simple-mid-sparse-check
    + statdir/p64v3r3-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00084      0.002878      0.000974     0.0009775 0.00021171357
    + 100      0.000179      0.000318      0.000199    0.00020605 2.3942851e-05
    Difference at 98.0% confidence
    	-0.00077145 +/- 4.95585e-05
    	-78.9207% +/- 5.06992%
    	(Student's t, pooled s = 0.000150658)
    
    p64v3r3-small-sparse-check vs. simple-small-sparse-check
    x statdir/simple-small-sparse-check
    + statdir/p64v3r3-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.004232       0.00537      0.004442    0.00451139 0.00025890527
    + 100      0.007581      0.009733      0.008285    0.00830776 0.00047237597
    Difference at 98.0% confidence
    	0.00379637 +/- 0.000125296
    	84.1508% +/- 2.77732%
    	(Student's t, pooled s = 0.000380901)

It looks like we got rid of the almost all the problems that
`p64-naive` had. `small-sparse` is still a bit wonky, but that's such
a small case it's not really worth it to do anything about
it. Populate remains slower, we'll try to deal with it later.

#### p8

Let's try different sizes of the slots, first up are 8 bits at a time.
Comparing to `p64v3b`:

    p8-huge-sparse-check vs. p64v3r-huge-sparse-check
    x statdir/p64v3r-huge-sparse-check
    + statdir/p8-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06       4.6e-05         1e-06      1.66e-06 4.5443271e-06
    + 100         2e-06       1.1e-05         2e-06      2.42e-06 1.4645991e-06
    No difference proven at 98.0% confidence
    
    p8-large-sparse-check vs. p64v3r-large-sparse-check
    x statdir/p64v3r-large-sparse-check
    + statdir/p8-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         3e-06         1e-05         3e-06      3.26e-06 9.6000842e-07
    + 100         5e-06       7.2e-05         6e-06      7.29e-06 6.9490134e-06
    Difference at 98.0% confidence
    	4.03e-06 +/- 1.63169e-06
    	123.62% +/- 50.0519%
    	(Student's t, pooled s = 4.96036e-06)
    
    p8-mid-dense-check vs. p64v3r-mid-dense-check
    x statdir/p64v3r-mid-dense-check
    + statdir/p8-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.277588      0.298965      0.287498     0.2878485  0.0049996003
    + 100      0.356894      0.385163      0.372034    0.37136252  0.0074057173
    Difference at 98.0% confidence
    	0.083514 +/- 0.00207836
    	29.0132% +/- 0.722034%
    	(Student's t, pooled s = 0.00631825)
    
    p8-mid-mid-check vs. p64v3r-mid-mid-check
    x statdir/p64v3r-mid-mid-check
    + statdir/p8-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.013536      0.017466      0.014914    0.01484559 0.00084153774
    + 100      0.022145      0.027083      0.023998    0.02401451  0.0011494921
    Difference at 98.0% confidence
    	0.00916892 +/- 0.000331365
    	61.7619% +/- 2.23207%
    	(Student's t, pooled s = 0.00100735)
    
    p8-mid-sparse-check vs. p64v3r-mid-sparse-check
    x statdir/p64v3r-mid-sparse-check
    + statdir/p8-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100       0.00019      0.000325      0.000205    0.00022475 3.2637704e-05
    + 100      0.000355      0.000984      0.000394     0.0004307 8.8662395e-05
    Difference at 98.0% confidence
    	0.00020595 +/- 2.19758e-05
    	91.6352% +/- 9.77787%
    	(Student's t, pooled s = 6.68066e-05)
    
    p8-small-sparse-check vs. p64v3r-small-sparse-check
    x statdir/p64v3r-small-sparse-check
    + statdir/p8-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008668      0.012785       0.00958    0.00969532 0.00078296108
    + 100      0.015597        0.0238      0.017201    0.01740783  0.0012612985
    Difference at 98.0% confidence
    	0.00771251 +/- 0.000345307
    	79.5488% +/- 3.56159%
    	(Student's t, pooled s = 0.00104974)
    
    p8-huge-sparse-populate vs. p64v3r-huge-sparse-populate
    x statdir/p64v3r-huge-sparse-populate
    + statdir/p8-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0       3.5e-05             0       4.6e-07 3.5030146e-06
    + 100         1e-06       5.2e-05         1e-06      1.54e-06 5.0998515e-06
    No difference proven at 98.0% confidence
    
    p8-large-sparse-populate vs. p64v3r-large-sparse-populate
    x statdir/p64v3r-large-sparse-populate
    + statdir/p8-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06       2.8e-05         1e-06      1.37e-06 2.7067256e-06
    + 100         2e-06       5.5e-05         2e-06      3.12e-06 7.3116484e-06
    No difference proven at 98.0% confidence
    
    p8-mid-dense-populate vs. p64v3r-mid-dense-populate
    x statdir/p64v3r-mid-dense-populate
    + statdir/p8-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.451669      0.488183      0.465988    0.46613407  0.0081586728
    + 100      0.740399      0.806867      0.766752    0.76819697   0.012635936
    Difference at 98.0% confidence
    	0.302063 +/- 0.00349853
    	64.8017% +/- 0.750542%
    	(Student's t, pooled s = 0.0106356)
    
    p8-mid-mid-populate vs. p64v3r-mid-mid-populate
    x statdir/p64v3r-mid-mid-populate
    + statdir/p8-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008563      0.014786      0.009512    0.00974717  0.0010219461
    + 100      0.014691      0.021071      0.016368    0.01652004  0.0013394804
    Difference at 98.0% confidence
    	0.00677287 +/- 0.000391887
    	69.4855% +/- 4.02052%
    	(Student's t, pooled s = 0.00119134)
    
    p8-mid-sparse-populate vs. p64v3r-mid-sparse-populate
    x statdir/p64v3r-mid-sparse-populate
    + statdir/p8-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       8.8e-05      0.000404      0.000126    0.00013567 4.3521516e-05
    + 100      0.000144      0.000361      0.000174     0.0001885 3.3777495e-05
    Difference at 98.0% confidence
    	5.283e-05 +/- 1.28142e-05
    	38.9401% +/- 9.44514%
    	(Student's t, pooled s = 3.89554e-05)
    
    p8-small-sparse-populate vs. p64v3r-small-sparse-populate
    x statdir/p64v3r-small-sparse-populate
    + statdir/p8-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.005811       0.00979      0.006589     0.0067828 0.00076679174
    + 100      0.009098      0.012777      0.010113    0.01024148 0.00073780872
    Difference at 98.0% confidence
    	0.00345868 +/- 0.000247512
    	50.9919% +/- 3.64911%
    	(Student's t, pooled s = 0.00075244)
    
As I suspected, it's significantly slower or the same in most cases.

#### p32

How about 32 bit slots?

    p32-huge-sparse-check vs. p64v3r-huge-sparse-check
    x statdir/p64v3r-huge-sparse-check
    + statdir/p32-huge-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06         6e-06         2e-06      1.75e-06 6.2563099e-07
    + 100         1e-06         1e-05         2e-06      1.99e-06 1.2987562e-06
    No difference proven at 98.0% confidence
    
    p32-large-sparse-check vs. p64v3r-large-sparse-check
    x statdir/p64v3r-large-sparse-check
    + statdir/p32-large-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100         3e-06       1.3e-05         3e-06      3.47e-06 1.2669457e-06
    + 100         3e-06       1.9e-05         4e-06      4.72e-06 2.4124257e-06
    Difference at 98.0% confidence
    	1.25e-06 +/- 6.33806e-07
    	36.0231% +/- 18.2653%
    	(Student's t, pooled s = 1.92678e-06)
    
    p32-mid-dense-check vs. p64v3r-mid-dense-check
    x statdir/p64v3r-mid-dense-check
    + statdir/p32-mid-dense-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.283639      0.362975      0.297368    0.29950793   0.012709457
    + 100      0.278536      0.302758      0.288494    0.28898716  0.0050136061
    Difference at 98.0% confidence
    	-0.0105208 +/- 0.00317792
    	-3.51268% +/- 1.06105%
    	(Student's t, pooled s = 0.00966091)
    
    p32-mid-mid-check vs. p64v3r-mid-mid-check
    x statdir/p64v3r-mid-mid-check
    + statdir/p32-mid-mid-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.013604      0.016902      0.014839    0.01486397 0.00079581827
    + 100      0.014425      0.020254      0.015942    0.01593847 0.00098105876
    Difference at 98.0% confidence
    	0.0010745 +/- 0.000293832
    	7.22889% +/- 1.97681%
    	(Student's t, pooled s = 0.000893253)
    
    p32-mid-sparse-check vs. p64v3r-mid-sparse-check
    x statdir/p64v3r-mid-sparse-check
    + statdir/p32-mid-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.000191      0.000343      0.000207    0.00021674 2.6079761e-05
    + 100      0.000237      0.000366      0.000261    0.00026546 2.0627245e-05
    Difference at 98.0% confidence
    	4.872e-05 +/- 7.73421e-06
    	22.4785% +/- 3.56843%
    	(Student's t, pooled s = 2.35121e-05)
    
    p32-small-sparse-check vs. p64v3r-small-sparse-check
    x statdir/p64v3r-small-sparse-check
    + statdir/p32-small-sparse-check
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008656      0.012224      0.009492    0.00972584 0.00078972951
    + 100       0.01102      0.015438      0.012323      0.012476 0.00091128688
    Difference at 98.0% confidence
    	0.00275016 +/- 0.000280485
    	28.2768% +/- 2.88391%
    	(Student's t, pooled s = 0.000852677)
    
    p32-huge-sparse-populate vs. p64v3r-huge-sparse-populate
    x statdir/p64v3r-huge-sparse-populate
    + statdir/p32-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0       3.6e-05             0       8.2e-07  3.588365e-06
    + 100             0       4.2e-05             0       7.7e-07   4.25893e-06
    No difference proven at 98.0% confidence
    
    p32-large-sparse-populate vs. p64v3r-large-sparse-populate
    x statdir/p64v3r-large-sparse-populate
    + statdir/p32-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06       3.3e-05         1e-06      1.73e-06 3.3085595e-06
    + 100         1e-06      0.000146         1e-06      2.82e-06 1.4834699e-05
    No difference proven at 98.0% confidence
    
    p32-mid-dense-populate vs. p64v3r-mid-dense-populate
    x statdir/p64v3r-mid-dense-populate
    + statdir/p32-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.440756      0.477085      0.455543    0.45674124  0.0070963826
    + 100      0.442975      0.527865      0.458514    0.45984113   0.010758253
    Difference at 98.0% confidence
    	0.00309989 +/- 0.00299773
    	0.678697% +/- 0.65633%
    	(Student's t, pooled s = 0.00911314)
    
    p32-mid-mid-populate vs. p64v3r-mid-mid-populate
    x statdir/p64v3r-mid-mid-populate
    + statdir/p32-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008337      0.013709      0.009517     0.0097025 0.00095346873
    + 100      0.008336      0.012756      0.009561     0.0096487  0.0008268858
    No difference proven at 98.0% confidence
    
    p32-mid-sparse-populate vs. p64v3r-mid-sparse-populate
    x statdir/p64v3r-mid-sparse-populate
    + statdir/p32-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       9.6e-05      0.000227      0.000104    0.00012007 2.6063735e-05
    + 100       9.9e-05      0.000185      0.000128    0.00012949 2.2415045e-05
    Difference at 98.0% confidence
    	9.42e-06 +/- 7.996e-06
    	7.84542% +/- 6.65945%
    	(Student's t, pooled s = 2.43079e-05)
    
    p32-small-sparse-populate vs. p64v3r-small-sparse-populate
    x statdir/p64v3r-small-sparse-populate
    + statdir/p32-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.005822      0.008629      0.006361    0.00653281 0.00054711725
    + 100      0.005814      0.008752      0.006531    0.00659868 0.00055600381
    No difference proven at 98.0% confidence

Huh. Not bad. It's overall slightly worse, but not much at all. It
should definitely be good to go for a 32 bit architectures.

#### p64v3switch

After putting this into the application I was actually dealing with
it became apparent that populate actually matters. While searching
through the bitmap significantly improved the overall performance,
all the gains were eaten by the populate step.

After reading the generated code it becomes quite obvious that we
spend a lot of time dealing with the general case of there being
a large amount of levels, while in reality we can only have 6.

`p64v3switch` is an attempt to deal with that.

    p64v3switch-huge-sparse-populate vs. p64v3r-huge-sparse-populate
    x statdir/p64v3r-huge-sparse-populate
    + statdir/p64v3switch-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0       3.6e-05             0       8.2e-07  3.588365e-06
    + 100             0         1e-06             0         8e-08 2.7265992e-07
    No difference proven at 98.0% confidence
    
    p64v3switch-large-sparse-populate vs. p64v3r-large-sparse-populate
    x statdir/p64v3r-large-sparse-populate
    + statdir/p64v3switch-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06       3.3e-05         1e-06      1.73e-06 3.3085595e-06
    + 100         1e-06       1.4e-05         1e-06      1.13e-06       1.3e-06
    No difference proven at 98.0% confidence
    
    p64v3switch-mid-dense-populate vs. p64v3r-mid-dense-populate
    x statdir/p64v3r-mid-dense-populate
    + statdir/p64v3switch-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.440756      0.477085      0.455543    0.45674124  0.0070963826
    + 100      0.289989      0.316283      0.300275    0.30040975  0.0050334105
    Difference at 98.0% confidence
    	-0.156331 +/- 0.00202367
    	-34.2276% +/- 0.443067%
    	(Student's t, pooled s = 0.00615199)
    
    p64v3switch-mid-mid-populate vs. p64v3r-mid-mid-populate
    x statdir/p64v3r-mid-mid-populate
    + statdir/p64v3switch-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.008337      0.013709      0.009517     0.0097025 0.00095346873
    + 100      0.005528      0.008702      0.006151    0.00634305 0.00066345116
    Difference at 98.0% confidence
    	-0.00335945 +/- 0.000270184
    	-34.6246% +/- 2.78468%
    	(Student's t, pooled s = 0.000821362)
    
    p64v3switch-mid-sparse-populate vs. p64v3r-mid-sparse-populate
    x statdir/p64v3r-mid-sparse-populate
    + statdir/p64v3switch-mid-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100       9.6e-05      0.000227      0.000104    0.00012007 2.6063735e-05
    + 100       5.5e-05      0.000163       7.6e-05     8.016e-05 2.0539059e-05
    Difference at 98.0% confidence
    	-3.991e-05 +/- 7.71858e-06
    	-33.2389% +/- 6.4284%
    	(Student's t, pooled s = 2.34646e-05)
    
    p64v3switch-small-sparse-populate vs. p64v3r-small-sparse-populate
    x statdir/p64v3r-small-sparse-populate
    + statdir/p64v3switch-small-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.005822      0.008629      0.006361    0.00653281 0.00054711725
    + 100      0.003587      0.005177      0.003918     0.0040386 0.00038855557
    Difference at 98.0% confidence
    	-0.00249421 +/- 0.000156087
    	-38.1797% +/- 2.38928%
    	(Student's t, pooled s = 0.000474506)
    
    p64v3jump-huge-sparse-populate vs. p64v3r-huge-sparse-populate
    x statdir/p64v3r-huge-sparse-populate
    + statdir/p64v3jump-huge-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100             0       3.6e-05             0       8.2e-07  3.588365e-06
    + 100             0         9e-06             0       3.6e-07  9.797959e-07
    No difference proven at 98.0% confidence
    
    p64v3jump-large-sparse-populate vs. p64v3r-large-sparse-populate
    x statdir/p64v3r-large-sparse-populate
    + statdir/p64v3jump-large-sparse-populate
        N           Min           Max        Median           Avg        Stddev
    x 100         1e-06       3.3e-05         1e-06      1.73e-06 3.3085595e-06
    + 100         1e-06         1e-05         1e-06      1.17e-06  1.137559e-06
    No difference proven at 98.0% confidence
    
    p64v3jump-mid-dense-populate vs. p64v3r-mid-dense-populate
    x statdir/p64v3r-mid-dense-populate
    + statdir/p64v3jump-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.440756      0.477085      0.455543    0.45674124  0.0070963826
    + 100      0.286183      0.342344      0.295782    0.29644032  0.0071324367
    Difference at 98.0% confidence
    	-0.160301 +/- 0.00234026
    	-35.0967% +/- 0.512383%
    	(Student's t, pooled s = 0.00711443)

Look at them numbers. Allowing the compiler to precompute some
constants really helps. I'm generally not a big fan of unrolling
loops, I consider most inlining and loop unrolling to be
counter-productive (code size is often more important than avoiding
branches when you're dealing with a large system), but applied
carefully and conservatively it can help.

#### p64v3jump

Since we're already making the code ugly, let's go full out. I happen
to know that gcc (the version I use) will generate a jump table for
that switch. But there is no way to convince gcc that `pb->levels` is
always in range of the jump table. So before doing a lookup in the
jump table gcc has to make sure that pb->levels is in range. That's an
extra branch. Also, the generated code seems to be jumping back and
forth from the jump table instead of generating more straightforward
code. Even though the branch predictor should always predict this
correctly, let's try to improve it.

`p64v3jump` does that. We're now deep into "shaving to lose weight"
territory. To achieve this we use a little known feature of gcc:
computed gotos and labels as values. If it wasn't clear, this is
far beyond standard C. 

    p64v3jump-mid-dense-populate vs. p64v3switch-mid-dense-populate
    x statdir/p64v3switch-mid-dense-populate
    + statdir/p64v3jump-mid-dense-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.289989      0.316283      0.300275    0.30040975  0.0050334105
    + 100      0.286183      0.342344      0.295782    0.29644032  0.0071324367
    Difference at 98.0% confidence
    	-0.00396943 +/- 0.00203052
    	-1.32134% +/- 0.675916%
    	(Student's t, pooled s = 0.0061728)
    
    p64v3jump-mid-mid-populate vs. p64v3switch-mid-mid-populate
    x statdir/p64v3switch-mid-mid-populate
    + statdir/p64v3jump-mid-mid-populate
        N           Min           Max        Median           Avg        Stddev
    x 100      0.005528      0.008702      0.006151    0.00634305 0.00066345116
    + 100      0.005461      0.007931      0.005996    0.00614494 0.00053166358
    Difference at 98.0% confidence
    	-0.00019811 +/- 0.000197756
    	-3.12326% +/- 3.11767%
    	(Student's t, pooled s = 0.00060118)

Let's just show these two results. The rest is noise, too few
calls. So it is maybe microscopically faster, but it is far below the
noise level of my test machine.

## Conclusion

`p64v3r` is the best implementation for first_set. Yes, `p64v3r3`
might be a hair faster in one very special case, but the code is
bigger and uglier. For populate we probably want to use the switch
version with the same argument - the jump table might be a hair
faster, but it's horribly ugly.

For my application the tests are not enough, we also need to test
iterating over the sets with values that aren't equal to the
previously returned value + 1, but that's a problem I can't really
simulate here.
