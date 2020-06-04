CROSS_COMPILE?=
ifneq (,$(CROSS_COMPILE))
CXX:=$(CROSS_COMPILE)g++
endif

BIN?=bin
CXXFLAGS+=-Iextra -Isrc -I. -Isrc/common

$(BIN)/client.exe: CXXFLAGS+=$(shell pkg-config sdl2 --cflags)
$(BIN)/client.exe: LDFLAGS+=$(shell pkg-config sdl2 --libs)

#CXXFLAGS+=-g3
#LDFLAGS+=-g

CXXFLAGS+=-O3

CXXFLAGS+=-Wall -Wextra -Werror

HOST:=$(shell $(CXX) -dumpmachine | sed 's/.*-//')

all: all_targets

-include $(HOST).mk

#------------------------------------------------------------------------------

common.srcs:=\
	src/common/socket_$(HOST).cpp\
	src/common/safe_main.cpp\
	src/common/stats.cpp\
	src/common/span.cpp\

#------------------------------------------------------------------------------

client.srcs:=\
	src/client/app.cpp\
	src/client/display.cpp\
	src/client/main.cpp\
	src/client/picloader.cpp\
	src/client/packer.cpp\
	src/client/scene_ingame.cpp\
	src/client/scene_paused.cpp\
	extra/glad/glad.cpp\
	$(common.srcs)\

$(BIN)/client.exe: $(client.srcs:%=$(BIN)/%.o)
TARGETS+=$(BIN)/client.exe

#------------------------------------------------------------------------------

server.srcs:=\
	src/server/main.cpp\
	src/server/server.cpp\
	src/server/gamelogic.cpp\
	$(common.srcs)\

$(BIN)/server.exe: $(server.srcs:%=$(BIN)/%.o)
TARGETS+=$(BIN)/server.exe

#------------------------------------------------------------------------------

all_targets: $(TARGETS)

$(BIN)/%.exe:
	@mkdir -p $(dir $@)
	$(CXX) -o "$@" $^ $(LDFLAGS)

$(BIN)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -o "$@" $< $(CXXFLAGS)
	@$(CXX) -MM -MT "$@" -c -o "$(BIN)/$*.cpp.o.dep" $< $(CXXFLAGS)

-include $(shell test -d $(BIN) && find $(BIN) -name "*.dep")

clean:
	rm -rf $(BIN)
