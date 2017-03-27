# Makefile

# Default compilation configuration
CONF := debug

# Project directories
# Relative to Makefile
BIN_DIR := bin
OBJ_DIR := obj
SRC_DIR := src
INC_DIR := inc
DIST_DIR := dist

# Compiler information
CC := gcc
CFLAGS :=  -std=gnu99 -Wall -Wextra -Werror
IFLAGS := -I$(INC_DIR)
LDFLAGS := -ludev

# If CONF isn't "release", explicitly override it to be "debug"
ifeq ($(CONF), release)
	CONF := release
	CFLAGS := $(CFLAGS) -O3
else
	CONF := debug
	CFLAGS := $(CFLAGS) -g
endif

# Project files and targets relative to directories above
BINS := Dell-Gateway-5000-IO-Tool
SRCS := canctl.c main.c
OBJS := canctl.o main.o
INCS := canctl.h cfg.h version.h args.h

# Concatenate project directories with project files
BINS := $(patsubst %,$(BIN_DIR)/$(CONF)/%,$(BINS))
OBJS := $(patsubst %,$(OBJ_DIR)/$(CONF)/%,$(OBJS))
SRCS := $(patsubst %,$(SRC_DIR)/%,$(SRCS))
INCS := $(patsubst %,$(INC_DIR)/%,$(INCS))


all: $(BINS)

$(BINS): $(HID_O) $(OBJS) | $(BIN_DIR)/$(CONF) $(DIST_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)
	@if [ -e $@ -a "$(CONF)" = "release" ]; then cp -v $@ $(DIST_DIR)/; fi

$(OBJ_DIR)/$(CONF)/%.o: $(SRC_DIR)/%.c $(INCS) | $(OBJ_DIR)/$(CONF)
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

# Make directory. Will be called only if needed.
$(BIN_DIR)/$(CONF) $(OBJ_DIR)/$(CONF) $(DIST_DIR):
	mkdir -p $@

.PHONY: clean distclean all

clean:
	rm -rf $(BIN_DIR)/$(CONF)/* $(OBJ_DIR)/$(CONF)/* $(SRC_DIR)/*~

distclean:
	rm -rf $(BIN_DIR)/ $(OBJ_DIR)/ $(SRC_DIR)/*~
