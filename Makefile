#
# Cross‑platform Makefile for CS4390 P2P project skeleton
# - On Linux / POSIX: builds peer and tracker using BSD sockets.
# - On Windows (MinGW): builds peer using Winsock (links with -lws2_32).
#

# Detect Windows vs non‑Windows to set Winsock library.
ifeq ($(OS),Windows_NT)
  WSLIB = -lws2_32
else
  WSLIB =
endif

CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = $(WSLIB)

PEER_SRC    = skeleton_peer.c
TRACKER_SRC = skeleton_tracker.c

PEER_OBJ    = $(PEER_SRC:.c=.o)
TRACKER_OBJ = $(TRACKER_SRC:.c=.o)

PEER_BIN    = peer
TRACKER_BIN = tracker

.PHONY: all clean

all: $(PEER_BIN) $(TRACKER_BIN)

$(PEER_BIN): $(PEER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TRACKER_BIN): $(TRACKER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(PEER_BIN) $(TRACKER_BIN) $(PEER_OBJ) $(TRACKER_OBJ)

