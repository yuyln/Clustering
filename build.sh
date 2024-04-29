#!/bin/sh
set -xe
CFLAGS="-I thirdparty -O3 -Wall -Wextra -pedantic -g -ggdb -DDEBUG"
CLIBS="-lm -fopenmp"
CC="gcc"

if ! test -f ./thirdparty/stb_image.o; then
    $CC $CFLAGS -DSTB_IMAGE_IMPLEMENTATION -x c -c ./thirdparty/stb_image.h -lm
    mv ./stb_image.o ./thirdparty/stb_image.o
fi

if ! test -f ./thirdparty/stb_image_write.o; then
    $CC $CFLAGS -DSTB_IMAGE_WRITE_IMPLEMENTATION -x c -c ./thirdparty/stb_image_write.h -lm
    mv ./stb_image_write.o ./thirdparty/stb_image_write.o
fi

FILES="`find . -type f -name "*.c"` `find ./thirdparty -type f -name "*.o"`"
$CC $CFLAGS -o main $FILES $CLIBS
