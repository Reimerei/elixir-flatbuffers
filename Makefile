CC = clang++
CFLAGS  = -g -Wall -std=c++11
ifneq ("$(wildcard deps/flatbuffers)","")
	FLATBUFFER_DIR = deps/flatbuffers
else
	FLATBUFFER_DIR = ../flatbuffers
endif
INCLUDES = -I$(FLATBUFFER_DIR)/include
TARGET_DIR = priv

all: $(TARGET_DIR)/idl_parser.o $(TARGET_DIR)/idl_gen_text.o $(TARGET_DIR)/fb_port clean

$(TARGET_DIR)/idl_parser.o: $(FLATBUFFER_DIR)/src/idl_parser.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $(TARGET_DIR)/idl_parser.o $(FLATBUFFER_DIR)/src/idl_parser.cpp

$(TARGET_DIR)/idl_gen_text.o: $(FLATBUFFER_DIR)/src/idl_gen_text.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $(TARGET_DIR)/idl_gen_text.o $(FLATBUFFER_DIR)/src/idl_gen_text.cpp

$(TARGET_DIR)/fb_port: $(TARGET_DIR)/idl_gen_text.o $(TARGET_DIR)/idl_parser.o $(TARGET_DIR)/fb_port.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET_DIR)/fb_port $(TARGET_DIR)/idl_gen_text.o $(TARGET_DIR)/idl_parser.o $(TARGET_DIR)/fb_port.cpp

clean:
	$(RM) $(TARGET_DIR)/*.o
