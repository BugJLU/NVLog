.PHONY: all clean

CC=gcc

TARGETS=nvpcctl

nvpcctl: nvpcctl.c
	$(CC) -o nvpcctl nvpcctl.c

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
