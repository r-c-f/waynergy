CFLAGS+=-D_GNU_SOURCE -DUSYNERGY_LITTLE_ENDIAN
LDFLAGS=-lwayland-client -lxkbcommon
PROT:=idle.xml wlr-virtual-pointer-unstable-v1.xml virtual-keyboard-unstable-v1.xml xdg-output-unstable-v1.xml wlr-data-control-unstable-v1.xml
PROT_H=$(PROT:xml=prot.h)
PROT_C=$(PROT_H:h=c)
PROT_O=$(PROT_H:h=o)
SRCFILES_H:=clip.h config.h net.h os.h  sig.h wayland.h uSynergy.h log.h
SRCFILES_C=$(SRCFILES_H:h=c) wl_idle.c wl_key.c wl_mouse.c main.c clip-update.c
SRCFILES_O=$(SRCFILES_C:c=o)

all: waynergy waynergy-clip-update waynergy-clipmon

waynergy-clip-update: waynergy
	cp $< $@

waynergy-clipmon: clipmon.c $(PROT_H) $(PROT_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(PROT_O) clipmon.c

waynergy: $(PROT_H) $(PROT_O) $(SRCFILES_H) $(SRCFILES_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(PROT_O) $(SRCFILES_O)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.prot.c: $(@:prot.c=xml)
	wayland-scanner private-code $(@:prot.c=xml) $@

%.prot.h: $(@:prot.h=xml)
	wayland-scanner client-header $(@:prot.h=xml) $@

.PHONY: clean homeinstall distclean install

clean:
	rm *.o waynergy waynergy-clip-update

distclean: clean
	rm *.prot.c *.prot.h

install: waynergy waynergy-clip-update
	mkdir -p "${PREFIX}/bin"
	install -m755 $^ ${PREFIX}/bin

homeinstall: waynergy waynergy-clip-update 
	cp -f $^ ~/bin

