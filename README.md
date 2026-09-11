# Susie64 Kitty plugin

This software is a Susie x64 plugin for Kitty digital 8-color images (`.kty` / `.kt4`).


## Build

Open `src/ifkty.sln` in Visual Studio 2022 and build the `Release|x64`
configuration. The output file is `x64/Release/ifkty.sph`.


## Thanks

The Susie interface layer shares code with `ifpi`, which is based on
MIYASAKA Masaru's Susie32 Pi Plug-in.

The KTY/KT4 decoder is an independent implementation based on analysis of
xgload and other publicly available implementations.

See [`KTY_FORMAT.md`](KTY_FORMAT.md) for the reverse-engineered KTY/KT4
format description.

## License

MIT
