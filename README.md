# craftos-kindle
A fork of CraftOS-native designed to run with kterm on a Kindle E-Reader. Uses code from craftos-efi and craftosos which are both made by JackMacWindows and are under the MIT license

## Building
To build, you must have:
* Any C89 or later compiler
* Lua 5.1
* ncurses
* POSIX headers including:
  * `unistd.h`
  * `dirent.h`
  * `glob.h`
  * `sys/stat[vfs].h`
  * `libgen.h`

Running `make` should be enough, but you may need to change `-llua.5.1` in the Makefile if the library is under a different name, such as `-llua` or `-llua51`.

This also currently only makes the test build, not the arm build.
