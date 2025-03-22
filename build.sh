#!/bin/sh

build_mode="debug" # temporary for now

build_dir="build"
mkdir -p $build_dir

compile_opts="-std=c++11 -g -O0" # -fsanitize=address

include_path=include
include_opts="-I $include_path"

files="source/main.cpp $include_path/glad/glad.c"
build_opts="$build_dir/main"

lib_path="libs/SDL2"
link_opts="-L $lib_path -lSDL2 -lpthread -lm -ldl"

build_command="clang++ $compile_opts $include_opts $files $link_opts -o $build_opts"

printf "Building Project...\n"
printf "$build_command\n\n"
$build_command
printf "\nBuild Complete!\n\n"
