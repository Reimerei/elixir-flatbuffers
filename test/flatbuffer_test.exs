defmodule FlatbufferTest do
  use ExUnit.Case

  # These are some basic tests for the behaviour of the flatbuffer library, so we know for sure how it behaves.

  setup do

    schema =
    """
      file_identifier "test";

      table Foo {
        a:bool;
        b:float;
        c:int;
        d:string;
      }

      root_type Foo;
    """

    {:ok, %{schema: schema}}
  end

  test "serialize and deserialize all scalars properly", %{schema: schema} do
    json =
    """
      {
        "a" : true,
        "b" : 1.1234,
        "c" : 123,
        "d" : "a_test_string"
      }
    """
    port = FlatbufferPort.open_port()
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    FlatbufferPort.json_to_fb(port, json)
    {:response, fb} = collect_response
    FlatbufferPort.fb_to_json(port, fb)
    {:response, json_looped} = collect_response
    assert Poison.decode(json) == Poison.decode(json_looped)
  end

  test "order of values in the json should not matter", %{schema: schema} do
    json =
    """
      {
        "b" : 1.1234,
        "d" : "a_test_string",
        "c" : 123,
        "a" : true
      }
    """
    port = FlatbufferPort.open_port()
    FlatbufferPort.load_schema(port, schema)
    assert {:response, "ok"} == collect_response
    FlatbufferPort.json_to_fb(port, json)
    {:response, fb} = collect_response
    FlatbufferPort.fb_to_json(port, fb)
    {:response, json_looped} = collect_response
    assert Poison.decode(json) == Poison.decode(json_looped)
  end

  # test "discard extra values in the json", %{schema: schema} do
  #   json =
  #   """
  #     {
  #       "a" : true,
  #       "b" : 1.1234,
  #       "c" : 123,
  #       "d" : "a_test_string",
  #       "e" : "should_be_ignored"
  #     }
  #   """
  #   port = FlatbufferPort.open_port()
  #   FlatbufferPort.load_schema(port, schema)
  #   assert {:response, "ok"} == collect_response
  #   FlatbufferPort.json_to_fb(port, json)
  #   {:response, fb} = collect_response
  #   FlatbufferPort.fb_to_json(port, fb)
  #   {:response, json_looped} = collect_response
  #   assert Poison.decode(json) == Poison.decode(json_looped)
  # end


  def collect_response() do
    receive do
        {_port, {:data, data}}  ->  {:response, data}
    after
      3000 -> :timeout
    end
  end



end
