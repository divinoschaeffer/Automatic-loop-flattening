CC = gcc
ROOTDIR = $(shell pwd)

CFLAGS  = -g -Wall -I$(ROOTDIR)/clan/include -L$(ROOTDIR)/clan -I$(ROOTDIR)/clan/osl/include -L$(ROOTDIR)/clan/osl
OUTPUT_DIR = .dist
TARGET = collapse

all: $(TARGET)

$(TARGET): main.c main.h
	$(CC) $(CFLAGS) -o $(OUTPUT_DIR)/$(TARGET) main.c main.h

clean:
	$(RM) $(TARGET)