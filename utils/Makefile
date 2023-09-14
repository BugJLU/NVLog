.PHONY: all clean

CC=gcc

TARGETS=nvpcctl fio_test

nvpcctl: nvpcctl.c
	$(CC) -o nvpcctl nvpcctl.c

fio_test: fio_test.c
	$(CC) -o fio_test fio_test.c

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
