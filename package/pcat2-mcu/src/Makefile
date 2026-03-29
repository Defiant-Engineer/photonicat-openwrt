# SPDX-License-Identifier: GPL-2.0-or-later

PROG:=pcat2-mcud
OBJS:=pcat2-mcud.o pcat2-mcu.o

CFLAGS+=-Wall
LDLIBS+=-lubox -lubus -lblobmsg_json

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROG) *.o

.PHONY: all clean
