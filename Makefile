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
STAT_IMPL=p64 dumb
STAT_OPS=check populate
STAT_CASES=huge-sparse large-sparse mid-dense mid-mid mid-sparse small-sparse


#for a in statdir/*-$$case-$$op ; do printf "$$a ->\n" ; $(MINISTAT) -q statdir/$(REF_STAT)-$$case-$$op $$a | tail -2 | head -1 | awk '{ print $1 }' ; done ;
cmp_stats::
	for op in $(STAT_OPS) ; do \
		for case in $(STAT_CASES) ; do \
			for impl in $(STAT_IMPL) ; do \
				printf "$$impl-$$case-$$op vs. $(REF_STAT)-$$case-$$op\n" ; $(MINISTAT) -q statdir/$(REF_STAT)-$$case-$$op statdir/$$impl-$$case-$$op | tail -2 | head -1 ;\
			done ; \
		done ; \
	done

clean::
	rm $(OBJS) bmap

$(OBJS): bmap.h

bmap: $(OBJS)
	cc -Wall -Werror -o bmap $(OBJS) $(LIBS.$(OSNAME))
