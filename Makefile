# DMC1 PC Port - Makefile
# Сборка нативного x86_64 Windows EXE

# Compiler
CC = x86_64-w64-mingw32-gcc
AR = x86_64-w64-mingw32-ar

# Flags
CFLAGS = -Wall -Wextra -O2 -std=c11
CFLAGS += -Iinclude
CFLAGS += -DWIN32 -D_WINDOWS

# Libraries
LDFLAGS = -lopengl32 -lglu32 -lopenal32 -lm -lwinmm

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/sys/win32.c \
       $(SRC_DIR)/input/gamepad.c \
       $(SRC_DIR)/render/opengl.c \
       $(SRC_DIR)/audio/openal.c \
       $(SRC_DIR)/game/player.c \
       $(SRC_DIR)/game/camera.c \
       $(SRC_DIR)/game/world.c \
       $(SRC_DIR)/game/scene.c

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Output
TARGET = DMC1.exe

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	echo "Build successful: $(TARGET)"

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Run
run: $(TARGET)
	./$(TARGET)

# Install data
install-data:
	mkdir -p data
	cp -r ../dmc1-patching/extracted/DATA data/

# Phony targets
.PHONY: all clean run install-data
