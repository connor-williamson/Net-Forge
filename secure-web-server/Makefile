CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2 \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pie \
         -Wformat -Wformat-security -Wnull-dereference \
         -Wshadow -Wdouble-promotion -Iinclude

LDFLAGS_HTTPS = -lssl -lcrypto

SRCDIR = src
OBJDIR = build

HTTP_SRC = $(SRCDIR)/http_server.c
HTTPS_SRC = $(SRCDIR)/https_server.c
UTILS_SRC = $(SRCDIR)/utils.c

HTTP_OBJ = $(OBJDIR)/http_server.o
HTTPS_OBJ = $(OBJDIR)/https_server.o
UTILS_OBJ = $(OBJDIR)/utils.o
UTILS_SSL_OBJ = $(OBJDIR)/utils_ssl.o

HTTP_BIN = $(OBJDIR)/http_server
HTTPS_BIN = $(OBJDIR)/https_server

all: $(HTTP_BIN) $(HTTPS_BIN)

$(HTTP_BIN): $(HTTP_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(HTTPS_BIN): $(HTTPS_OBJ) $(UTILS_SSL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_HTTPS)

$(OBJDIR)/http_server.o: $(HTTP_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/https_server.o: $(HTTPS_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/utils.o: $(UTILS_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/utils_ssl.o: $(UTILS_SRC)
	$(CC) $(CFLAGS) -DUSE_SSL -c $< -o $@

clean:
	rm -f $(OBJDIR)/*.o $(HTTP_BIN) $(HTTPS_BIN)

.PHONY: all clean