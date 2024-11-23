include ./include/Makefile.inc

SOURCES_CLIENT := $(wildcard ./client/*.c)
SOURCES_SERVER := $(wildcard ./server/*.c)
SOURCES_COMMON := $(wildcard ./utils/*.c)

OBJECTS_CLIENT := ./$(TARGET_CLIENT).o $(SOURCES_CLIENT:.c=.o)
OBJECTS_SERVER := ./server.o $(SOURCES_SERVER:.c=.o)
OBJECTS_COMMON := $(SOURCES_COMMON:.c=.o)
OBJECTS = $(OBJECTS_SERVER) $(OBJECTS_CLIENT) $(OBJECTS_COMMON)

all: $(TARGET_SERVER)

$(TARGET_CLIENT): $(OBJECTS_CLIENT) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o $@

$(TARGET_SERVER): $(OBJECTS_SERVER) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: all clean
