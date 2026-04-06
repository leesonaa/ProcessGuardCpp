CXX      = x86_64-w64-mingw32-g++
WINDRES  = x86_64-w64-mingw32-windres
STRIP    = x86_64-w64-mingw32-strip

TARGET   = ProcessGuard.exe
SRCDIR   = src
RESDIR   = res
OUTDIR   = dist

CXXFLAGS = -std=c++17 -O2 -Wall \
            -DUNICODE -D_UNICODE \
            -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 \
            -finput-charset=utf-8 -fwide-exec-charset=UTF-16LE \
            -I$(SRCDIR)

LDFLAGS  = -mwindows -municode \
            -lpsapi -lshell32 -lcomctl32 -lshlwapi \
            -lgdi32 -lcomdlg32 -ldwmapi \
            -lole32 -luuid \
            -static -lstdc++ -lpthread

SRCS     = $(SRCDIR)/main.cpp
OBJS     = $(OUTDIR)/main.o $(OUTDIR)/resource.o

all: $(OUTDIR) $(OUTDIR)/$(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/resource.o: $(RESDIR)/ProcessGuard.rc $(RESDIR)/ProcessGuard.ico
	$(WINDRES) -c 65001 -i $< -o $@ --input-format=rc --output-format=coff

$(OUTDIR)/main.o: $(SRCS) $(SRCDIR)/common.h $(SRCDIR)/config.h \
                  $(SRCDIR)/engine.h $(SRCDIR)/selfguard.h $(SRCDIR)/window.h
	$(CXX) $(CXXFLAGS) -c $(SRCS) -o $@

$(OUTDIR)/$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	$(STRIP) $@
	@echo "Build OK: $@"

clean:
	rm -rf $(OUTDIR)

release: all
	@mkdir -p release
	@cp $(OUTDIR)/$(TARGET) release/$(TARGET)
	@echo "Release: release/$(TARGET)"

.PHONY: all clean release
