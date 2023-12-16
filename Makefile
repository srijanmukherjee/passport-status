CC=gcc
LIBS=libcurl tidy
CFLAGS=-Wall -Werror -pedantic $(shell pkg-config --cflags $(LIBS))
LDFLAGS=$(shell pkg-config --libs $(LIBS))

OUTPUT_DIR=build
SOURCE_DIR=src
SOURCES=main.c
EXECUTABLE_NAME=status
EXECUTABLE=$(OUTPUT_DIR)/$(EXECUTABLE_NAME)

SOURCES:=$(addprefix $(SOURCE_DIR)/, $(SOURCES))

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(SOURCES) $(LDFLAGS)

clean:
	rm build/*