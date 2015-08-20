defmodule FlatbuffersPortTest do
  use ExUnit.Case

  def collect_response() do
    receive do
        {_port, {:data, data}}  ->  {:response, data}
    after
      3000 ->:timeout
    end
  end

  test "start a port" do
    port = FlatbuffersPort.open_port()
    assert is_port(port)
    assert is_list Port.info(port)
    Port.close(port)
    assert :undefined == Port.info(port)
  end

  test "add a schema" do
    port = FlatbuffersPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    FlatbuffersPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    Port.close(port)
  end

  test "return error when schema cannot be parsed" do
    port = FlatbuffersPort.open_port()
    {:ok, schema} = File.read("test/quests_error.fbs")
    FlatbuffersPort.load_schema(port, schema)
    assert {:response, "error: could not parse schema"} == collect_response
    Port.close(port)
  end

  test "convert json to flatbuffer" do
    port = FlatbuffersPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, json} = File.read("test/quests.json")
    {:ok, flatbuffer} = File.read("test/quests.bin")
    FlatbuffersPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    FlatbuffersPort.json_to_fb(port, json)
    assert {:response, flatbuffer} == collect_response
  end

  test "return error when json cannot be parsed" do
    port = FlatbuffersPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, json} = File.read("test/quests_error.json")
    FlatbuffersPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    FlatbuffersPort.json_to_fb(port, json)
    assert {:response, "error: could not parse json"} == collect_response
  end

  test "convert flatbuffer to json" do
    port = FlatbuffersPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, json} = File.read("test/quests.json")
    {:ok, flatbuffer} = File.read("test/quests.bin")
    FlatbuffersPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    FlatbuffersPort.fb_to_json(port, flatbuffer)
    assert {:response, json} == collect_response
  end

  test "return error when flatbuffer cannot be parsed" do
    port = FlatbuffersPort.open_port()
    {:ok, schema} = File.read("test/quests.fbs")
    {:ok, flatbuffer} = File.read("test/quests_error.bin")
    FlatbuffersPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    FlatbuffersPort.fb_to_json(port, flatbuffer)
    assert {:response, "error"} == collect_response
  end

end
