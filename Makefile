# pixoo.library - controls a Divoom Pixoo 64 over HTTP, for AmigaOS 3.2
# Cross build with bebbo's amiga-gcc (m68k-amigaos-gcc) and NDK 3.2.

PREFIX  ?= /opt/amiga
CC      := $(PREFIX)/bin/m68k-amigaos-gcc
STRIP   := $(PREFIX)/bin/m68k-amigaos-strip
SFDC    := $(PREFIX)/bin/sfdc
CPU     ?= -m68000
B       ?= build
O       ?= bin

CFLAGS  := $(CPU) -Os -fno-common -fomit-frame-pointer -fno-toplevel-reorder -fno-builtin \
           -Wall -Wextra -Wno-unused-parameter -Wno-pointer-sign -Iinclude -Isrc/library
LIBFLAGS:= -nostartfiles -nostdlib
CTLFLAGS:= $(CPU) -Os -Wall -Wextra -Wno-pointer-sign -fno-common -Iinclude -noixemul -Wl,-u,___stkinit

# pixoo_lib.c must come first: it starts with "moveq #-1,d0; rts"
LIBSRC  := src/library/pixoo_lib.c src/library/pixoo_dev.c src/library/pixoo_core.c src/library/pixoo_image.c src/library/pixoo_font.c src/library/pixoo_net.c
LIBOBJ  := $(LIBSRC:src/library/%.c=$(B)/%.o)
LIBHDR  := src/library/pixoo_core.h src/library/pixoo_image.h src/library/pixoo_net.h src/library/pixoo_private.h include/libraries/pixoo.h

# shared tools (git submodule): charcheck, docs, icons, Aminet archive
DEVKIT  := amiga-devkit
ifeq ($(wildcard $(DEVKIT)/make/charcheck.mk),)
$(error $(DEVKIT) is missing: git submodule update --init)
endif

# the demo (Shell script and pictures) is copied next to the programs
DEMOFILES := $(shell find demo -type f)

all: charcheck $(O)/pixoo.library $(O)/PixooCtl $(DEMOFILES:%=$(O)/%)

# Amiga sources and texts must be ISO-8859-1 (amiga-devkit/make/charcheck.mk).
# Not checked: README*.md, CLAUDE.md, TECHNICAL.md and tools/*.py (UTF-8).
LATIN1  := $(wildcard src/library/*.c src/library/*.h include/*/*.h include/fd/*.fd sfd/*.sfd \
           src/pixooctl/*.c demo/PixooDemo test/*.c test/*.expected package/* LICENSE Makefile)
include $(DEVKIT)/make/charcheck.mk

$(B)/%.o: src/library/%.c $(LIBHDR)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/pixoo.library: $(LIBOBJ)
	@mkdir -p $(O)
	$(CC) $(CPU) $(LIBFLAGS) -o $@.debug $(LIBOBJ) -L$(PREFIX)/m68k-amigaos/libnix/lib -Wl,--start-group -lgcc -lnix20 -Wl,--end-group
	$(STRIP) -o $@ $@.debug

$(O)/PixooCtl: src/pixooctl/pixooctl.c include/libraries/pixoo.h include/clib/pixoo_protos.h include/proto/pixoo.h include/inline/pixoo.h
	@mkdir -p $(O)
	$(CC) $(CTLFLAGS) -o $@ $<
	$(STRIP) $@

$(O)/demo/%: demo/%
	@mkdir -p $(dir $@)
	cp $< $@

# pictures of the demo, drawn by tools/mkdemo.py
pictures:
	python3 tools/mkdemo.py

# proto, inline and fd headers from the SFD (clib/pixoo_protos.h is written
# by hand: it is the documented reference of the functions)
# built-in font from the ASCII art in tools/mkfont.py
font:
	python3 tools/mkfont.py

headers: sfd/pixoo_lib.sfd
	@mkdir -p include/proto include/inline include/fd
	$(SFDC) --quiet --mode=proto  --target=m68k-amigaos --output=include/proto/pixoo.h sfd/pixoo_lib.sfd
	$(SFDC) --quiet --mode=macros --target=m68k-amigaos --output=include/inline/pixoo.h sfd/pixoo_lib.sfd
	$(SFDC) --quiet --mode=fd     --target=m68k-amigaos --output=include/fd/pixoo_lib.fd sfd/pixoo_lib.sfd

# --- host tests -----------------------------------------------------------
# The portable core (drawing, Base64, JSON, HTTP), the device commands and
# the socket code run on
# the host too; test/hosttest talks to the mock Pixoo test/mockpixoo.py.
HOSTCC  := cc -g -Wall -Wextra -fsanitize=address,undefined -DPIXOO_HOST -Iinclude -Isrc/library
HOSTSRC := src/library/pixoo_dev.c src/library/pixoo_core.c src/library/pixoo_image.c src/library/pixoo_font.c src/library/pixoo_net.c
test/hosttest: test/hosttest.c $(HOSTSRC) $(filter-out include/%,$(LIBHDR)) include/libraries/pixoo.h
	$(HOSTCC) -o $@ test/hosttest.c $(HOSTSRC)

check: test/hosttest
	@mkdir -p build/test
	@python3 test/mockpixoo.py --port 18080 --log build/test/requests.log --ppm build/test/frame.ppm & \
	pid=$$!; sleep 1; \
	./test/hosttest 127.0.0.1 18080 > build/test/hosttest.out 2>&1; rc=$$?; \
	kill $$pid 2>/dev/null; wait $$pid 2>/dev/null; \
	cat build/test/hosttest.out; \
	if [ $$rc -eq 0 ] && diff -u test/requests.expected build/test/requests.log; then echo "ok   check"; \
	else echo "FAIL check"; exit 1; fi

check-update: test/hosttest
	@$(MAKE) --no-print-directory check || true
	cp build/test/requests.log test/requests.expected

# --- documentation (Doxygen on the host, not part of "all") ----------------
include $(DEVKIT)/make/docs.mk
DOCVERSION  := $(shell sed -n 's/^.define LIBVERSION *\([0-9]*\).*/\1/p' src/library/pixoo_private.h).$(shell sed -n 's/^.define LIBREVISION *\([0-9]*\).*/\1/p' src/library/pixoo_private.h)
DOC_SOURCES := $(wildcard src/library/*.c src/library/*.h) include/libraries/pixoo.h include/clib/pixoo_protos.h src/pixooctl/pixooctl.c test/hosttest.c
$(eval $(call DOXYGEN_DOCS,docs,pixoo.library,$(DOCVERSION),$(DOC_SOURCES),build/docs))
$(eval $(call DOXYGEN_DOCS,docs-lib,pixoo.library,$(DOCVERSION),include/libraries/pixoo.h include/clib/pixoo_protos.h,build/docs/pixoo.library))
# AmigaGuide reference of the public headers (for the Aminet archive)
DOCDATE     := $(shell sed -n 's/^.define LIBDATE *"\([^"]*\)".*/\1/p' src/library/pixoo_private.h)
$(eval $(call DOXYGEN_GUIDE,guide,pixoo.library,$(DOCVERSION) ($(DOCDATE)),include/libraries/pixoo.h include/clib/pixoo_protos.h,build/pixoo.library.guide))

# --- Aminet archive -------------------------------------------------------
# dist/PixooLib.lha (tools/mkdist.py; needs doxygen and jlha or lha)
dist: all guide
	python3 tools/mkdist.py

clean:
	rm -rf build bin dist test/hosttest

.PHONY: all clean check check-update headers font pictures dist
