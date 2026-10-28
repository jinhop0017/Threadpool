CC = gcc
CFLAGS = -Wall -pthread
SRCDIR = src
BINDIR = bin
TARGETS = $(BINDIR)/use_threadpool $(BINDIR)/use_futures

all: $(TARGETS)

$(BINDIR)/use_threadpool: $(BINDIR)/threadpool.o $(BINDIR)/use_threadpool.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/use_futures: $(BINDIR)/threadpool.o $(BINDIR)/use_futures.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/threadpool.h | $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -p $@

clean:
	rm -f $(BINDIR)/*.o $(TARGETS)

.PHONY: all clean
