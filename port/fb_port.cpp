#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatbuffers/reflection.h"
#include "flatbuffers/reflection_generated.h"

#include <unistd.h>

int read_bytes(unsigned char *buf, int len) {
  return read(0, buf, len);
}

int read_message(unsigned char *buf) {
  // read the first 4 bytes as the message length
  read_bytes(buf, 4);
  int len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  // read the message
  return read_bytes(buf, len);
}

int write_bytes(unsigned char *buf, int len) {
  return write(1, buf, len);
}

int write_message(unsigned char *buf, int len) {
  // first send the message length
  unsigned char li[4];
  li[0] = (len & 0xff000000) >> 24;
  li[1] = (len & 0x00ff0000) >> 16;
  li[2] = (len & 0x0000ff00) >> 8;
  li[3] = (len & 0x000000ff);
  write_bytes(li, 4);
  // send the data
  int res = write_bytes(buf, len);
  return res;
}

int main () {
  // we define a maximum message size, so we do not
  // have to allocate the buffer for each message
  const int max_message_size = 5 * 1024 * 1024; // 5 MB
  unsigned char buf[max_message_size];

  int message_size;

  int unsiged char ok_message[2] =

  // options for the parser
  flatbuffers::GeneratorOptions opts = flatbuffers::GeneratorOptions();
  opts.strict_json = true;
  opts.indent_step = -1;

  flatbuffers::Parser parser;

  // loop until stdin is closed
  while((message_size = read_message(buf)) > 0 && message_size <= max_message_size) {
    parser.builder_.Clear();
    // modes: 0 => json to fb; 1 => fb to json; 2 => schema
    int mode = buf[0];
    if(mode == 0) {
      // the data is the json as string add null termination
      buf[message_size] = 0;
      parser.Parse((const char *) &buf[1]);
      write_message(parser.builder_.GetBufferPointer(), parser.builder_.GetSize());
      write_message(buf, message_size);
    } else if(mode == 1) {
      // the data is the binary fb, push it into the parser
      parser.builder_.PushBytes((const uint8_t *) &buf[1], message_size-1);
      std::string json;
      flatbuffers::GenerateText(parser, parser.builder_.GetBufferPointer(), opts, &json);
      write_message((unsigned char *) json.c_str(), json.size());
    } else if(mode == 2) {
      // the data is the schema as string add null termination
      buf[message_size] = 0;
      parser.Parse((const char *) &buf[1]);

      // no reply
    }
  }
}
