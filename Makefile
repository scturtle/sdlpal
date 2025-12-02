TARGET ?= sdlpal

HOST ?=
CC = $(HOST)gcc
CXX = $(HOST)g++
PKG_CONFIG ?= $(HOST)pkg-config

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl3)
FT_CFLAGS  := $(shell $(PKG_CONFIG) --cflags freetype2)
FT_LIBS    := $(shell $(PKG_CONFIG) --libs freetype2)

CPPFLAGS := -I. -Iadplug $(FT_CFLAGS) $(SDL_CFLAGS)

COMMON_FLAGS := -g -Wall -O2

CFLAGS   := $(COMMON_FLAGS) -std=gnu99 $(CFLAGS)
CXXFLAGS := $(COMMON_FLAGS) -std=c++11 $(CXXFLAGS)

LDLIBS := $(SDL_LIBS) $(FT_LIBS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDLIBS += -framework OpenGL -liconv
else
    LDLIBS += -lGL -pthread
endif

SOURCES  := . adplug
CFILES   := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
CPPFILES := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))
OBJFILES := $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

DEPFILES := $(OBJFILES:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJFILES) $(DEPFILES)

-include $(DEPFILES)
