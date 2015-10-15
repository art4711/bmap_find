OSNAME ?= $(shell uname -s)
OSNAME := $(shell echo $(OSNAME) | tr A-Z a-z)

# Get this from github.com/art4711/stopwatch.
STOPWATCHPATH=../timing
SRCS.linux=$(STOPWATCHPATH)/stopwatch_linux.c
SRCS.darwin=$(STOPWATCHPATH)/stopwatch_mach.c

LIBS.linux=-lrt
LIBS.darwin=

MINISTAT=../ministat/ministat

SRCS=$(SRCS.$(OSNAME)) bmap.c bmap_test.c

OBJS=$(SRCS:.c=.o)

MACHFLAGS= -msse4.2 -mpopcnt -mavx 
#MACHFLAGS=-mpopcnt
CFLAGS=-I$(STOPWATCHPATH) -g -O3 -Wall -Werror $(MACHFLAGS)

.PHONY: run clean genstats cmp_stats

run:: bmap
	./bmap

genstats:: bmap
	./bmap statdir

REF_STAT=simple
STAT_IMPL=p64 p64-naive dumb
STAT_OPS=check populate
STAT_CASES=huge-sparse large-sparse mid-dense mid-mid mid-sparse small-sparse

cmp_stats::
	for op in $(STAT_OPS) ; do \
		for impl in $(STAT_IMPL) ; do \
			for case in $(STAT_CASES) ; do \
				printf "\n$$impl-$$case-$$op vs. $(REF_STAT)-$$case-$$op\n" ; $(MINISTAT) -c 98 -q statdir/$(REF_STAT)-$$case-$$op statdir/$$impl-$$case-$$op ;\
			done ; \
		done ; \
	done

clean::
	rm $(OBJS) bmap

$(OBJS): bmap.h

bmap: $(OBJS)
	cc -Wall -Werror -o bmap $(OBJS) $(LIBS.$(OSNAME))
