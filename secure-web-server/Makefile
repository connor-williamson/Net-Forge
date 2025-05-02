CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2 \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pie \
         -Wformat -Wformat-security -Wnull-dereference -Wshadow -Wdouble-promotion \
         -Isrc -Iinclude

LDFLAGS_HTTPS = -lssl -lcrypto

SRCDIR = src
INCLUDEDIR = include
OBJDIR = build
TESTDIR = test

# Source files
HTTP_SRC = $(SRCDIR)/http_server.c
HTTPS_SRC = $(SRCDIR)/https_server.c
MIME_SRC = $(SRCDIR)/mime.c
LOGGER_SRC = $(SRCDIR)/logger.c
RESPONSE_SRC = $(SRCDIR)/response.c

# Object files
HTTP_OBJ = $(OBJDIR)/http_server.o
HTTPS_OBJ = $(OBJDIR)/https_server.o
MIME_OBJ = $(OBJDIR)/mime.o
LOGGER_OBJ = $(OBJDIR)/logger.o
RESPONSE_OBJ_HTTP = $(OBJDIR)/response.o
RESPONSE_OBJ_HTTPS = $(OBJDIR)/response_ssl.o

# Binaries
HTTP_BIN = $(OBJDIR)/http_server
HTTPS_BIN = $(OBJDIR)/https_server

# Build all
all: $(HTTP_BIN) $(HTTPS_BIN)

# Generic rule to compile .c to .o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Special SSL build of response.c
$(RESPONSE_OBJ_HTTPS): $(RESPONSE_SRC)
	$(CC) $(CFLAGS) -DUSE_SSL -c $< -o $@

# Binaries
$(HTTP_BIN): $(HTTP_OBJ) $(MIME_OBJ) $(LOGGER_OBJ) $(RESPONSE_OBJ_HTTP)
	$(CC) $(CFLAGS) -o $@ $^

$(HTTPS_BIN): $(HTTPS_OBJ) $(MIME_OBJ) $(LOGGER_OBJ) $(RESPONSE_OBJ_HTTPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_HTTPS)

# Bash test runner
# Ensure 'logs/' exists and is writable.
# If you see 'Permission denied', run:
# sudo rm -f logs/*.log && sudo chown -R $(whoami) logs && chmod -R u+rw logs
# This ensures you can write to logs during testing.
test: $(HTTP_BIN) $(HTTPS_BIN)
	@printf "Running HTTP test script...\n\n"
	@bash $(TESTDIR)/test_http.sh
	@printf "\nRunning HTTPS test script...\n\n"
	@bash $(TESTDIR)/test_https.sh


clean:
	rm -f $(OBJDIR)/*.o $(HTTP_BIN) $(HTTPS_BIN)

.PHONY: all clean test