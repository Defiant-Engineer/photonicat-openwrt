# SPDX-License-Identifier: GPL-2.0-or-later

PROG:=pcat2-display
OBJS:=pcat2-display.o

CFLAGS+=-Wall
LIBS?=-lubus -lubox -lblobmsg_json -luci

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROG) *.o

.PHONY: all clean
