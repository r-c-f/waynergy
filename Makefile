CFLAGS+=-DUSE_INTRINSIC_MASK -D_GNU_SOURCE -DUSYNERGY_LITTLE_ENDIAN
LDFLAGS=-lwayland-client -lxkbcommon
PROT=wlr-virtual-pointer-unstable-v1.xml virtual-keyboard-unstable-v1.xml xdg-output-unstable-v1.xml wlr-data-control-unstable-v1.xml
PROT_H=$(PROT:xml=prot.h)
PROT_C=$(PROT:xml=prot.c)


all: swaynergy swaynergy-clip-update

swaynergy-clip-update: swaynergy
	cp $< $@

swaynergy: $(PROT_H) $(PROT_C) clip.o config.o net.o main.o os.o clip-update.o wayland.o wl_key.o wl_mouse.o uSynergy.o log.o wlr-virtual-pointer-unstable-v1.prot.o virtual-keyboard-unstable-v1.prot.o xdg-output-unstable-v1.prot.o wlr-data-control-unstable-v1.prot.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o swaynergy clip.o config.o net.o main.o os.o clip-update.o wayland.o wl_key.o wl_mouse.o uSynergy.o log.o wlr-virtual-pointer-unstable-v1.prot.o virtual-keyboard-unstable-v1.prot.o xdg-output-unstable-v1.prot.o wlr-data-control-unstable-v1.prot.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.prot.c: $(@:prot.c=xml)
	wayland-scanner private-code $(@:prot.c=xml) $@

%.prot.h: $(@:prot.h=xml)
	wayland-scanner client-header $(@:prot.h=xml) $@

.PHONY: clean homeinstall distclean install

clean:
	rm *.o swaynergy swaynergy-clip-update

distclean: clean
	rm *.prot.c *.prot.h

install: swaynergy swaynergy-clip-update
	mkdir -p "${PREFIX}/bin"
	install -m755 $^ ${PREFIX}/bin

homeinstall: swaynergy swaynergy-clip-update 
	cp -f $^ ~/bin

