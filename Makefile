CC     := gcc
RM     := rm -f

LDFLAGS := -pipe -flto

CFILES := main.c

FLAGS  := -Wall -Wextra -pipe -O2 -ggdb -flto -march=native -s -MMD -MP
OBJDIR := bin
BINARY := bin/pdfview
OBJS   := $(CFILES:%.c=$(OBJDIR)/%.o)
HEADER_DEPS := $(CFILES:%.c=$(OBJDIR)/%.d)

OS := $(shell cat /etc/os-release | rg "Fedora Linux")
ifneq ($(OS),)
LIBRARIES := -lraylib -lmupdf-third -lmupdf      \
			 -ljpeg -lpng -ljbig2dec -lleptonica \
			 -lopenjp2 -lfreetype -lharfbuzz     \
			 -lgumbo -lstdc++ -ltesseract -lm -lz
else
LIBRARIES := -lraylib -lmupdf -ljpeg -lpng -ljbig2dec -lopenjp2 \
			 -lfreetype -lharfbuzz -lgumbo -lm -lz -lgs -llcms2 \
			 -lstdc++ -ltesseract -lleptonica
endif

.PHONY: all install
all: $(OBJDIR) $(BINARY)

install: all
	cp $(BINARY) /home/oskar/.local/bin/

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINARY): $(OBJS)
	$(CC) $(LDFLAGS) -o $(BINARY) $(OBJS) $(LIBRARIES)

-include $(HEADER_DEPS)
$(OBJDIR)/%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(BINARY)
	$(RM) $(OBJS)
	$(RM) $(HEADER_DEPS)
	$(RM) -r $(OBJDIR)
