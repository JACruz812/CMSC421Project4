# Add the names of your .c files from your assignment here.
SRCS = memefs.c

# This line is the name of the program you are building
BIN = run

# Compiler flags to pass to the system's C compiler while building
# the source files
CFLAGS = -g -Wall -Wextra -pedantic -Wno-unused-parameter -std=gnu11 `pkg-config fuse --cflags`

# Flags to pass to the linker while linking your program
# You shouldn't need to change this at all.
LDFLAGS = -g `pkg-config fuse --libs`

###################################################################
# You shouldn't need to touch anything below this line...         #
###################################################################
BUILD_DIR = build
OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SRCS)))

.PHONY: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

all: $(BIN)

clean:
	-rm -rf $(BUILD_DIR)
	-rm -f $(BIN)

.SECONDEXPANSION:

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) -c $< -o $@ $(CFLAGS)
