#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatbuffers/reflection.h"
#include "flatbuffers/reflection_generated.h"

#include <unistd.h>

int read_bytes(unsigned char *buf, int len)
{
  int i, got = 0;

  do
  {
    if ((i = read(0, buf + got, len - got)) <= 0)
      return (i);
    got += i;
  } while (got < len);

  return (len);
}

int read_message(unsigned char *buf)
{
  // read the first 4 bytes as the message length
  read_bytes(buf, 4);
  int len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  // read the message
  return read_bytes(buf, len);
}

int write_bytes(unsigned char *buf, int len)
{
  int i, wrote = 0;

  do
  {
    if ((i = write(1, buf + wrote, len - wrote)) <= 0)
      return (i);
    wrote += i;
  } while (wrote < len);

  return (len);
}

int write_message(unsigned char *buf, int len)
{
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

int write_message(std::string text)
{
  return write_message((unsigned char *)text.c_str(), text.size());
}

int main()
{
  // we define a maximum message size, so we do not
  // have to allocate the buffer for each message
  const int max_message_size = 5 * 1024 * 1024; // 5 MB
  unsigned char buf[max_message_size];

  int message_size;

  flatbuffers::Parser parser;
  parser.opts.strict_json = true;
  parser.opts.indent_step = -1;

  // loop until stdin is closed
  while ((message_size = read_message(buf)) > 0 && message_size <= max_message_size)
  {
    parser.builder_.Clear();
    // modes: 0 => json to fb; 1 => fb to json; 2 => schema
    int mode = buf[0];

    if (mode == 0)
    {
      // the data is the json as string add null termination
      buf[message_size] = 0;

      if (parser.Parse((const char *)&buf[1]))
      {
        write_message(parser.builder_.GetBufferPointer(), parser.builder_.GetSize());
      }
      else
      {
        std::string error = "error: " + parser.error_;
        write_message(error);
      }
    }
    else if (mode == 1)
    {
      // the data is the binary fb, push it into the parser
      parser.builder_.PushFlatBuffer((const uint8_t *)&buf[1], message_size - 1);

      // check if file identitifier matches a schema
      if (!parser.root_struct_def_ || !flatbuffers::BufferHasIdentifier(&buf[1], parser.file_identifier_.c_str()))
      {
        write_message("error: no schema for this binary");
      }
      else
      {
        std::string json;
        flatbuffers::GenText(parser, parser.builder_.GetBufferPointer(), &json);
        write_message((unsigned char *)json.c_str(), json.size());
      }
    }
    else if (mode == 2)
    {
      // the data is the schema as string add null termination
      buf[message_size] = 0;

      if (parser.Parse((const char *)&buf[1]))
      {
        write_message("ok");
      }
      else
      {
        write_message("error: could not parse schema");
      }
    }
  }
}
