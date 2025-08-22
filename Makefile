# --------------------------------------------------------------------
#  This file is part of jnutils, made public 2023, (c) Jeff Nye.
# --------------------------------------------------------------------
.PHONY: all clean dirs run


TARGET  = ./bin/cbp_conv
CPP     = g++
PKGCONF = pkg-config

DEP  = -MMD -MP
DEF  = -DSTRING_DEFINE="\"v1.1.1\""
INC  = -Iinc $(shell $(PKGCONF) --cflags libarchive)
OPT  = -O3 -pipe -march=native -mtune=native 
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
	-rm -f output/sample.jsonl.gz
	$(TARGET) --in traces/sample_int_trace.gz --out output/sample.jsonl.gz
	$(TARGET) --in output/sample.jsonl.gz --out output/sample.0.txt
	$(TARGET) --in output/sample.jsonl.gz --out output/sample.1.txt.gz
	$(TARGET) --in output/sample.jsonl.gz --out output/sample.2.txt --limit 100
	$(TARGET) --in output/sample.jsonl.gz --out output/sample.3.txt.gz --limit 100

#unit:
#	$(MAKE) pyt3 GROUP=$@
#
#integration:
#	$(MAKE) pyt3 GROUP=$@
#
#stress:
#	$(MAKE) pyt3 GROUP=$@
#
#pyt3:
#	python3 -m pytest -m "$(GROUP)" -q 

#	$(TARGET) --in traces/sample_int_trace.gz --limit 100

help-%:
	@echo $* = $($*)

-include $(ALL_DEP)

clean:
	@rm -rf obj/* $(TARGET)

