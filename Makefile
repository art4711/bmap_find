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

REF_STAT=inter64_postcount

cmp_stats::
	for a in statdir/* ; do printf "$$a -> " ; $(MINISTAT) -q statdir/$(REF_STAT) $$a | tail -2 | head -1 | awk '{ print $1 }' ; done

clean::
	rm $(OBJS) bmap

$(OBJS): bmap.h

bmap: $(OBJS)
	cc -Wall -Werror -o bmap $(OBJS) $(LIBS.$(OSNAME))
