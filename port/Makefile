CC = g++
CFLAGS  = -g -Wall -std=c++11
INCLUDES = -Ilib/flatbuffers/include

all: idl_parser idl_gen_text fb_port clean

idl_parser: lib/flatbuffers/src/idl_parser.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o idl_parser.o lib/flatbuffers/src/idl_parser.cpp

idl_gen_text: lib/flatbuffers/src/idl_gen_text.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o idl_gen_text.o lib/flatbuffers/src/idl_gen_text.cpp

fb_port: idl_gen_text.o idl_parser.o fb_port.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o fb_port idl_gen_text.o idl_parser.o fb_port.cpp

clean:
	$(RM) *.o
