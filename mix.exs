defmodule FlatbufferPort.Mixfile do
  use Mix.Project

  def project do
    [
      app: :flatbuffer_port,
      version: "0.0.1",
      elixir: "~> 1.0",
      build_embedded: Mix.env == :prod,
      start_permanent: Mix.env == :prod,
      compilers: Mix.compilers ++ [:port],
      deps: deps()
   ]
  end

  def application do
    [applications: []]
  end

  defp deps do
    [
      {:flatbuffers, [github: "google/flatbuffers", tag: "v1.11.0", compile: false, app: false]},
      {:jason, "~> 1.1.2", [only: :test]}
    ]
  end
end

defmodule Mix.Tasks.Compile.Port do
  def run(_) do
    if Mix.shell.cmd("make priv/fb_port") != 0 do
      raise Mix.Error, message: "Could not run `make priv/fb_port`. Do you have make and clang installed?"
    end
  end
end
