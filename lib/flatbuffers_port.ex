defmodule FlatbuffersPort do

  def open_port() do
    path = "./path"
    Port.open({:spawn_executable, path},  [:binary, {:packet, 4}, {:parallelism, true}])
  end


end
