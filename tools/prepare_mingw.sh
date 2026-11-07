#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/deps
cd build/deps
curl --proto '=https' --tlsv1.2 --fail --location --connect-timeout 20 --max-time 120 --retry 3 https://www.lua.org/ftp/lua-5.4.8.tar.gz -o lua.tar.gz
curl --proto '=https' --tlsv1.2 --fail --location --connect-timeout 20 --max-time 120 --retry 3 https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz -o sdl.tar.gz
sha256sum --check <<'HASHES'
4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae  lua.tar.gz
83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2  sdl.tar.gz
HASHES
tar --no-same-owner -xzf lua.tar.gz
tar --no-same-owner -xzf sdl.tar.gz
cd lua-5.4.8/src
mkdir -p mingw-objects
for source in *.c; do
    case "$source" in lua.c|luac.c) continue ;; esac
    x86_64-w64-mingw32-gcc -O2 -c "$source" -o "mingw-objects/${source%.c}.o"
done
x86_64-w64-mingw32-ar rcs liblua.a mingw-objects/*.o
