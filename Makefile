include ./include/Makefile.inc

SOURCES_CLIENT := $(wildcard ./client/*.c)
SOURCES_SERVER := $(wildcard ./server/*.c)
SOURCES_COMMON := $(wildcard ./utils/*.c)

OBJECTS_CLIENT := ./client.o $(SOURCES_CLIENT:.c=.o)
OBJECTS_SERVER := ./server.o $(SOURCES_SERVER:.c=.o)
OBJECTS_COMMON := $(SOURCES_COMMON:.c=.o)
OBJECTS = $(OBJECTS_SERVER) $(OBJECTS_CLIENT) $(OBJECTS_COMMON)

all: $(TARGET_SERVER)

$(TARGET_CLIENT): $(OBJECTS_CLIENT) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o $@.out

$(TARGET_SERVER): $(OBJECTS_SERVER) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o $@.out

clean:
	rm -rf $(OBJECTS) $(TARGET_SERVER).out $(TARGET_CLIENT).out

.PHONY: all clean
