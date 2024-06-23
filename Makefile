CC = gcc
CFLAGS = -Wall -pthread
SRCDIR = src
BINDIR = bin
TARGET = $(BINDIR)/use_threadpool
OBJS = $(BINDIR)/threadpool.o $(BINDIR)/use_threadpool.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/threadpool.o: $(SRCDIR)/threadpool.c $(SRCDIR)/threadpool.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/use_threadpool.o: $(SRCDIR)/use_threadpool.c $(SRCDIR)/threadpool.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BINDIR)/*.o $(TARGET)

.PHONY: all clean