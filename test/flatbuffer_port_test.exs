defmodule FlatbufferPortTest do
  use ExUnit.Case

  def collect_response() do
    receive do
      {_port, {:data, data}}  ->  {:response, data}
    after
      3000 -> :timeout
    end
  end

  test "start a port" do
    port = FlatbufferPort.open_port()
    assert is_port(port)
    assert is_list Port.info(port)
    Port.close(port)
    assert nil == Port.info(port)
  end

  test "add a schema" do
    port = FlatbufferPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response()
    Port.close(port)
  end

  test "return error when schema cannot be parsed" do
    port = FlatbufferPort.open_port()
    {:ok, schema} = File.read("test/quests_error.fbs")
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "error: could not parse schema"} == collect_response()
    Port.close(port)
  end

  test "convert json to flatbuffer" do
    port = FlatbufferPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, json} = File.read("test/quests.json")
    {:ok, flatbuffer} = File.read("test/quests.bin")
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response()
    FlatbufferPort.json_to_fb(port, json)
    {:response, response} = collect_response()
    assert byte_size(flatbuffer) == byte_size(response)
    assert flatbuffer == response
  end

  test "return error when json cannot be parsed" do
    port = FlatbufferPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, json} = File.read("test/quests_error.json")
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response()
    FlatbufferPort.json_to_fb(port, json)
    assert {:response, "error: 1:0: error: unknown field: questsx"} == collect_response()
  end

  test "convert flatbuffer to json" do
    port = FlatbufferPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, json} = File.read("test/quests.json")
    {:ok, flatbuffer} = File.read("test/quests.bin")
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response()
    FlatbufferPort.fb_to_json(port, flatbuffer)
    {:response, converted_json} = collect_response()
    assert Poison.decode!(json) == Poison.decode!(converted_json)
  end

  test "return error when flatbuffer has wrong identitifier" do
    port = FlatbufferPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, flatbuffer} = File.read("test/quests_error.bin")
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response()
    FlatbufferPort.fb_to_json(port, flatbuffer)
    assert {:response, "error: no schema for this binary"} == collect_response()
  end

  test "parse responses" do
   assert {:ok, "some_data"} == FlatbufferPort.parse_reponse("some_data")
   assert :ok == FlatbufferPort.parse_reponse("ok")
   assert {:error, "some_reason"} == FlatbufferPort.parse_reponse("error: some_reason")
 end

end
