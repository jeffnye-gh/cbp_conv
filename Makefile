# --------------------------------------------------------------------
#  This file is part of jnutils, made public 2023, (c) Jeff Nye.
# --------------------------------------------------------------------
.PHONY: all clean run test unit functional cov one


TARGET  = ./bin/cbp_conv
CPP     = g++
PKGCONF = pkg-config

DEP  = -MMD -MP
DEF  = -DSTRING_DEFINE="\"v1.1.1\""
INC  = -Iinc $(shell $(PKGCONF) --cflags libarchive)
#OPT  = -O3 -pipe -march=native -mtune=native 
OPT  = -O0 -g
STD  = -std=gnu++17
WARN = -Wall
LIBS     := $(shell $(PKGCONF) --libs libarchive)

CFLAGS   = $(OPT) $(DEP) $(DEF) $(INC)
CPPFLAGS = $(CFLAGS) $(STD)

LDFLAGS  ?=

# Files
ALL_SRC = $(wildcard src/*.cpp)
ALL_OBJ = $(subst src,obj,$(ALL_SRC:.cpp=.o))
ALL_DEP = $(ALL_OBJ:.o=.d))

all: $(TARGET)

$(TARGET): $(ALL_OBJ)
	@mkdir -p obj bin
	$(CPP) $(CPPFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

obj/%.o: src/%.cpp
	@mkdir -p bin
	$(CPP) $(CPPFLAGS) -c $< -o $@

run: $(TARGET)
	@mkdir -p output
	-rm -f out/*
	$(MAKE) one I=traces/int_trace     O=out/int_trace.asm     L=1024

#	$(MAKE) one I=traces/int_trace     O=out/int_trace.txt     L=128

#works
#	$(MAKE) one I=traces/int_trace     O=out/int_trace.txt     L=128
#	$(MAKE) one I=traces/int_trace     O=out/int_trace.txt.gz  L=128 
#	$(MAKE) one I=traces/int_trace     O=out/int_trace.txt.xz  L=128 
#	$(MAKE) one I=traces/int_trace     O=out/int_trace.txt.bz2 L=128 
#	\
#	$(MAKE) one I=traces/int_trace.gz  O=out/int_trace_gz.txt  L=128 
#	\
#	$(MAKE) one I=traces/int_trace.xz  O=out/int_trace_xz.txt  L=128 
#	\
#	$(MAKE) one I=traces/int_trace.bz2 O=out/int_trace_xz.txt  L=128 

# TODO:
	#$(MAKE) one I=traces/int_trace.gz  O=out/int_trace_gz.xz   L=128 
	#$(MAKE) one I=traces/int_trace.gz  O=out/int_trace_qz.bz2  L=128 
	#$(MAKE) one I=traces/int_trace.xz  O=out/int_trace_xz.gz   L=128 
	#$(MAKE) one I=traces/int_trace.xz  O=out/int_trace_xz.bz2  L=128 
	#$(MAKE) one I=traces/int_trace.bz2 O=out/int_trace_xz.gz   L=128 
	#$(MAKE) one I=traces/int_trace.bz2 O=out/int_trace_xz.xz  L=128 


#	$(MAKE) one I=traces/int_trace.bz2 O=out/int_trace_bz.txt L=128 
#	\

one: $(TARGET)
	$(TARGET) --in $(I) --out $(O) --limit $(L)

unit:
	pytest -m unit
functional:
	pytest -m functional
test:
	pytest -m "unit or functional"
cov:
	pytest -m "unit or functional" --cov=cbp_conv \
            --cov-report=term-missing --cov-report=html

help-%:
	@echo $* = $($*)

-include $(ALL_DEP)

clean:
	@rm -rf obj/* $(TARGET) bin/*

