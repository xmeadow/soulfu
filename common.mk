.PHONY: data

SDP = sdp/sdp
SLC = slc/slc
SSC = ssc/ssc

GIT_REMOTE_URL := $(shell git remote get-url origin 2>/dev/null | sed 's|^https://||;s|^git@\([^:]*\):\(.*\)|\1/\2|;s|\.git$$||' || echo unknown)
BUILD_YEAR     := $(shell date +%Y)
GIT_TAG        := $(shell git tag --sort=-creatordate 2>/dev/null | head -1 || echo unknown)
GIT_HASH       := $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)
GIT_VERSION    := $(GIT_TAG) $(GIT_HASH)

DATAFILE = datafile.sdf
DATADIR = datadir

LANTXT = $(wildcard language/*.txt)
LANFILES = $(patsubst language/%.txt,$(DATADIR)/%.LAN, $(LANTXT))

SCRIPTSRC = $(wildcard $(DATADIR)/*.SRC)
SCRIPTRUN = $(SCRIPTSRC:.SRC=.RUN)

DATAEXT = DAT DDD JPG MUS OGG PCX SRC SRF TXT
DATADEP = $(foreach ext,$(DATAEXT),$(wildcard $(DATADIR)/*.$(ext))) $(LANFILES) $(SCRIPTRUN)

data: $(DATAFILE)

clean:
	rm -rf $(TARGET) $(TARGET).exe $(DATAFILE) $(LANFILES) $(SCRIPTRUN) $(DATADIR)/WMAIN.SRC.RUN resource.res

# TBD: ssc should compile only things that changed since last build
$(DATAFILE): $(DATADEP) | $(SDP)
	$(SDP) -p -n -i $(DATADIR) -o $(DATAFILE)

$(DATADIR)/%.LAN: language/%.txt | $(SLC)
	$(SLC) --to-lan $< -o $@

$(SCRIPTRUN): $(SCRIPTSRC) | $(SSC)
	@ORIG_MTIME=$$(stat -c %y $(DATADIR)/WMAIN.SRC); \
	TMPBAK=$$(mktemp); \
	cp $(DATADIR)/WMAIN.SRC $$TMPBAK; \
	sed -i 's|www\.AaronBishopGames\.com|$(GIT_REMOTE_URL)|' $(DATADIR)/WMAIN.SRC; \
	sed -i 's|__YEAR__|$(BUILD_YEAR)|' $(DATADIR)/WMAIN.SRC; \
	sed -i 's|__GIT_VERSION__|$(GIT_VERSION)|' $(DATADIR)/WMAIN.SRC; \
	$(SSC) -c -i $(DATADIR); \
	SSC_EXIT=$$?; \
	cp $$TMPBAK $(DATADIR)/WMAIN.SRC; \
	rm -f $$TMPBAK; \
	touch -d "$$ORIG_MTIME" $(DATADIR)/WMAIN.SRC; \
	exit $$SSC_EXIT

$(SDP): | sdp
	make -C sdp

$(SLC): | slc
	make -C slc

$(SSC): | ssc
	make -C ssc
