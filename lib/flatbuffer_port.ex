defmodule FlatbufferPort do

  def open_port() do
    path = Path.join(:code.priv_dir(:flatbuffer_port), "fb_port")
    Port.open({:spawn_executable, path},  [:binary, {:packet, 4}, {:parallelism, true}])
  end

  def json_to_fb(port, json) when is_binary(json)  do
    Port.command(port, << 0, json :: binary >>)
  end

  def fb_to_json(port, fb) when is_binary(fb)  do
    Port.command(port, << 1, fb :: binary >>)
  end

  def load_schema(port, schema) when is_binary(schema)  do
    Port.command(port, << 2, schema :: binary >>)
  end

  def parse_reponse("ok"), do: :ok
  def parse_reponse("error: " <> reason), do: {:error, reason}
  def parse_reponse(data), do: {:ok, data}

end
