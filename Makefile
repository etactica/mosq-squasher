# new, simple simple makefile, hopefully suitable for both netbeans and openwrt sdk
# Should be able to "do the right thing" with include dirs and paths

EXECUTABLE=mosq-squasher

MAIN=main.c
SOURCES+=mosq-manage.c
SOURCES+=uglylogging.c

LDFLAGS+= -lmosquitto -levent_core -lz

# Shouldn't need to change anything under here...
CFLAGS+=-Wall -Wextra -std=gnu99 -g
OBJECTS=$(SOURCES:.c=.o)
MAIN_OBJECT=$(MAIN:.c=.o)
TEST_SOURCES=$(wildcard test_*.c)
TEST_OBJECTS=$(TEST_SOURCES:.c=.o)
TEST_LOGS=$(TEST_SOURCES:.c=.c.xml)

TEST_EXECUTABLES=$(TEST_SOURCES:.c=.test)
# FIXME - probably need a way for the app to specify this part...
TEST_LDFLAGS+=-Wl,--whole-archive -lcheck -Wl,--no-whole-archive

all: $(SOURCES) $(EXECUTABLE)

VER:=$(shell ../../../guess-rev.sh)
	
all: version.h $(EXECUTABLE)
	@echo "rebuilding...."

$(EXECUTABLE): $(MAIN_OBJECT) $(OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

version.h: version.h.in
	sed -e "s/%WCREV%/$(VER)/g" $< > $@

%o: %c
	$(CC) -c $(CFLAGS) $< -o $@
	
test_%.test: test_%.o $(TEST_OBJECTS) $(OBJECTS)
	$(CC) $< $(OBJECTS) $(LDFLAGS) $(TEST_LDFLAGS) -o $@

compiletests: $(TEST_EXECUTABLES)
	@echo "building tests is done..."

tests: compiletests
	@echo "running tests....$^"
	@for t in $(TEST_EXECUTABLES); do ./$$t 2>/dev/null; done

test:	tests

clean:
	rm -rf $(EXECUTABLE) $(MAIN_OBJECT) $(OBJECTS)
	rm -rf $(TEST_OBJECTS) $(TEST_EXECUTABLES) $(TEST_LOGS)
	rm -rf version.h

.PHONY: all clean version.h tests compiletests
