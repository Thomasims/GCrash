This module dumps the lua stack if the server segfaults.
It also allows you to define a crash handler in lua able to inspect the stack and write to the dump file.
Read `lua/gcrash.lua` for information on function behavior.

## Installing

Get the released (or build from source) `gmsv_gcrash_linux.dll` and put it in
`SERVER_ROOT/garrysmod/lua/bin/`, You may have to create the `bin` folder.
If you want to use the default lua handler, you can put `gcrash.lua` in `lua/autorun/server/`.

## Building

If you wish to build the module yourself, the build files are included in this repo.

### _Linux_

1. Get `premake5` and place it next to `BuildProjects.bat`
2. Run `premake5 --os=linux gmake2`
3. CD into `projects/linux/`
4. `make` optionally with `config=(debug|release)`
5. Output is `build/gmsv_gcrash_linux.dll` (the makefile renames the created .so to the GMod module format)

### _Windows_

Windows is not yet supported.

## TODO

- Improve the default lua handler