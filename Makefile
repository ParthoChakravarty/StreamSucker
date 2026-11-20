
CC = gcc

TARGET = build/stream-sucker

SRC_DIR = src
INC_DIR = include

SOURCES = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/downloader.c \
    $(SRC_DIR)/extractor.c \
    $(SRC_DIR)/frames.c \
    $(SRC_DIR)/process.c

OBJECTS = $(SOURCES:.c=.o)

CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 \
         -I$(INC_DIR)

LDFLAGS =

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p build
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)
	@echo ""
	@echo "Build successful: $(TARGET)"

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Build files cleaned."

