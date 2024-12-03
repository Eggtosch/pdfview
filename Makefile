CC     := gcc
RM     := rm -f

LDFLAGS := -pipe -flto

CFILES := main.c

INCLUDES := -Ibin/include/
FLAGS  := -Wall -Wextra $(INCLUDES) -pipe -O2 -ggdb -flto -march=native -s -MMD -MP
OBJDIR := bin
BINARY := bin/pdfview
OBJS   := $(CFILES:%.c=$(OBJDIR)/%.o)
HEADER_DEPS := $(CFILES:%.c=$(OBJDIR)/%.d)

OS := $(shell cat /etc/os-release | rg "Fedora Linux")
ifneq ($(OS),)
LIBRARIES := bin/libraylib.a -lmupdf-third -lmupdf      \
			 -ljpeg -lpng -ljbig2dec -lleptonica 		\
			 -lopenjp2 -lfreetype -lharfbuzz     		\
			 -lgumbo -lstdc++ -ltesseract -lm -lz
else
LIBRARIES := bin/libraylib.a -lmupdf -ljpeg -lpng -ljbig2dec -lopenjp2 \
			 -lfreetype -lharfbuzz -lgumbo -lm -lz -lgs -llcms2 		\
			 -lstdc++ -ltesseract -lleptonica
endif

.PHONY: all install
all: $(OBJDIR) $(BINARY)

install: all
	cp $(BINARY) /home/oskar/.local/bin/

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINARY): $(OBJS) | bin/libraylib.a
	$(CC) $(LDFLAGS) -o $(BINARY) $(OBJS) $(LIBRARIES)

bin/libraylib.a:
	git clone --depth=1 --branch 5.0 https://github.com/raysan5/raylib.git bin/raylib/
	cd bin/raylib && \
	git apply ../../raylib-empty-event.patch && \
	mkdir build && \
	cd build && \
	cmake ../ && \
	make -j20
	cp bin/raylib/build/raylib/libraylib.a bin/
	mkdir bin/include/
	cp bin/raylib/build/raylib/include/* bin/include/

-include $(HEADER_DEPS)
$(OBJDIR)/%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(BINARY)
	$(RM) $(OBJS)
	$(RM) $(HEADER_DEPS)
	$(RM) -r $(OBJDIR)
