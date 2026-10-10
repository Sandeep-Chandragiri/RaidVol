CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -Iinclude

TARGET = raidvol

SOURCES = \
	src/main.c \
	src/device.c \
	src/raid0.c \
	src/raid1.c \
	src/volume.c \
	src/metadata.c \
	src/raid5.c \
	src/checksum.c \
	src/checksum_store.c \
	src/benchmark.c

OBJECTS = $(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
