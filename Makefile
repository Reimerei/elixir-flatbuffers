CC = g++
CFLAGS  = -g -Wall -std=c++11
INCLUDES = -Iport/lib/flatbuffers/include

all: port/idl_parser.o port/idl_gen_text.o priv/fb_port clean

port/idl_parser.o: port/lib/flatbuffers/src/idl_parser.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o port/idl_parser.o port/lib/flatbuffers/src/idl_parser.cpp

port/idl_gen_text.o: port/lib/flatbuffers/src/idl_gen_text.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o port/idl_gen_text.o port/lib/flatbuffers/src/idl_gen_text.cpp

priv/fb_port: port/idl_gen_text.o port/idl_parser.o port/fb_port.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o priv/fb_port port/idl_gen_text.o port/idl_parser.o port/fb_port.cpp

clean:
	$(RM) port/*.o
