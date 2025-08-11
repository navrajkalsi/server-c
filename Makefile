#
# Saves the compiled binary in: '/usr/local/bin'
# Saves static files in: '/usr/local/share/server-c'
#
# ?= sets a var if not set already
# := does not look ahead for vars not defined yet
#
# Pass in DESTDIR to do a custom install

SHELL = /bin/sh

# Conventions followed: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html
# To change architecture dependent/bin installation dir, change exec_prefix
# To change architecture independent files dir, change prefix
# sysconfdir: used for config files that are machine-specific, all files should be text files
# includedir: used for header files
prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
datadir ?= $(prefix)/share
sysconfdir ?= $(prefix)/exec
includedir ?= $(prefix)/include

INSTALL ?= install

# Project Specific
# obj could be skipped as only one file at this point
NAME := server-c
SRC := main.c
OBJ := $(SRC:.c=.o)
# CFLAGS ?= -Wall -Werror -Wextra -g
# Dev Flags
CFLAGS ?= -Wall -Werror -Wextra -Wconversion -g -fsanitize=address,undefined
LDFLAGS ?= -lmagic
# Static_Dir for server files
STATIC_DIR ?= $(datadir)/$(NAME)/static
# Static_Dir for development only, installs binary and static files in the same dir
# Have to pass in absolute path in the program
# STATIC_DIR = $(abspath $(DESTDIR)$(datadir)/$(NAME)/static)
CFLAGS += -DSTATIC_DIR="\"$(STATIC_DIR)\""
CC = gcc

# Defines that the labels are commands and not files to run
.PHONY: all clean install uninstall

# Build the binary
all: $(NAME)

# Building the binary
# Looks for definition of every .o file
# $@ is for target
# $^ is for all the dependencies
# $< is for input src file
$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling each .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Builds first
install: all
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 $(NAME) $(DESTDIR)$(bindir)/$(NAME)
	mkdir -p $(DESTDIR)$(datadir)/$(NAME)/static
	$(INSTALL) -m 0644 static/* $(DESTDIR)$(datadir)/$(NAME)/static

uninstall:
	rm -r $(DESTDIR)$(bindir)/$(NAME)
	rm -rf $(DESTDIR)$(datadir)/$(NAME)

clean:
	rm -f $(NAME) $(OBJ)
