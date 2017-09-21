Elixir Flatbuffers
==============

Parses flatbuffers from/to json using a port that links the native C++ implementation.
The communication is done via erlang packets to optimise the IO throughput.
See `priv/fb_port.cpp` for the C++ side of the implementation.

Once loaded, schemas are kept within the memory of the port so they don't have to be loaded for each conversion.
It is possible to load multiple schemas.

The result of a conversion will be send back to the process that called the function. The response can be parsed with `parse_repsonse/1`.

Usage
==============

```elixir

  # start the port
  port = FlatbufferPort.open_port()

  # load a schema
  FlatbufferPort.load_schema(port, schema)

  # convert a json string to flatbuffer
  FlatbufferPort.json_to_fb(port, json)

  # convert a flatbuffer to json
  FlatbufferPort.fb_to_json(port, flatbuffer)

  # wait for result
  receive do
      {^port, {:data, data}} -> FlatbufferPort.parse_reponse(data)
  after
    3_000 -> :timeout
  end
```

