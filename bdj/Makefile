JAVAC ?= javac
JAR ?= jar
BDJ_API_JAR ?=
SRC := $(shell find src -name '*.java')
OUT := build/classes
JAR_OUT := build/00000.jar

ifeq ($(strip $(BDJ_API_JAR)),)
$(error BDJ_API_JAR is undefined; point it at your BD-J API/stub JAR)
endif

all: $(JAR_OUT)

$(JAR_OUT): $(SRC)
	mkdir -p $(OUT)
	$(JAVAC) -source 8 -target 8 -cp "$(BDJ_API_JAR)" -d $(OUT) $(SRC)
	mkdir -p build
	$(JAR) cf $(JAR_OUT) -C $(OUT) .

clean:
	rm -rf build

.PHONY: all clean
