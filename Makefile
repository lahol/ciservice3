CC = gcc
PKG_CONFIG = pkg-config

CFLAGS = -Wall -g `$(PKG_CONFIG) --cflags glib-2.0 gio-2.0`
LIBS = `$(PKG_CONFIG) --libs glib-2.0 gio-2.0` -lcinet -lciclient

PREFIX = /usr

VERSION := '$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch \"$(shell git describe --tags --always --all | sed s:heads/::)\")'

APPNAME := ciservice

CFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -DAPPNAME=\"${APPNAME}\"
CFLAGS += -DSYSCONFIGDIR=\"/etc\"

src = $(wildcard *.c)
obj = $(src:.c=.o)
header = $(wildcard *.h)

all: ciservice

ciservice: $(obj)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c $(header)
	$(CC) $(CFLAGS) -c -o $@ $<

install: ciservice
	install ciservice $(PREFIX)/bin

clean:
	rm -f ciservice $(obj)

.PHONY: all clean install
