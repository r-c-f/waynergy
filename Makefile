CFLAGS=-DUSE_INTRINSIC_MASK -D_GNU_SOURCE -DUSYNERGY_LITTLE_ENDIAN
LDFLAGS=-lwayland-client
PROT=wlr-virtual-pointer-unstable-v1.h virtual-keyboard-unstable-v1.h
PROT_C=$(PROT:h=c)


all: swaynergy swaynergy-clip-update

swaynergy-clip-update: swaynergy
	cp $< $@

swaynergy: $(PROT) clip.o config.o net.o main.o os.o clip-update.o wayland.o uSynergy.o wlr-virtual-pointer-unstable-v1.o virtual-keyboard-unstable-v1.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o swaynergy clip.o config.o net.o main.o os.o clip-update.o wayland.o uSynergy.o wlr-virtual-pointer-unstable-v1.o virtual-keyboard-unstable-v1.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

wlr-virtual-pointer-unstable-v1.h: $(@:h=xml) 
	wayland-scanner client-header $(@:h=xml)  $@

virtual-keyboard-unstable-v1.h: $(@:h=xml)
	wayland-scanner client-header $(@:h=xml)  $@

wlr-virtual-pointer-unstable-v1.c: $(@:c=xml) 
	wayland-scanner private-code $(@:c=xml) $@

virtual-keyboard-unstable-v1.c: $(@:c=xml)
	wayland-scanner private-code $(@:c=xml) $@

.PHONY: clean homeinstall distclean install

clean:
	rm *.o swaynergy swaynergy-clip-update

install: swaynergy swaynergy-clip-update
	mkdir -p "${PREFIX}/bin"
	install -m755 $^ ${PREFIX}/bin

homeinstall: swaynergy swaynergy-clip-update 
	cp -f $^ ~/bin

distclean:
	git clean -xfd
