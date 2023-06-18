#!/bin/sh

g++ -O3 -fopenmp -c `pkg-config SDL2 --cflags` `pkg-config SDL2_image --cflags``pkg-config SDL2_mixer --cflags` `pkg-config SDL2_ttf --cflags` `pkg-config dinrhiw --cflags` `pkg-config libavcodec --cflags` `pkg-config libavformat --cflags` `pkg-config libavutil --cflags` -fdata-sections -ffunction-sections SDLAVCodec.cpp

g++ -O3 -fopenmp -c `pkg-config SDL2 --cflags` `pkg-config SDL2_image --cflags``pkg-config SDL2_mixer --cflags` `pkg-config SDL2_ttf --cflags` `pkg-config dinrhiw --cflags` -fdata-sections -ffunction-sections hermitecurve.cpp

g++ -O3 -fopenmp -c `pkg-config SDL2 --cflags` `pkg-config SDL2_image --cflags``pkg-config SDL2_mixer --cflags` `pkg-config SDL2_ttf --cflags` `pkg-config dinrhiw --cflags` -fdata-sections -ffunction-sections SDLtest.cpp

g++ -fopenmp SDLtest.o SDLAVCodec.o hermitecurve.o -fdata-sections -ffunction-sections -Wl,-gc-sections `pkg-config SDL2 --libs` `pkg-config SDL2_image --libs` `pkg-config SDL2_mixer --libs` `pkg-config SDL2_ttf --libs` `pkg-config dinrhiw --libs` `pkg-config libavcodec --libs` `pkg-config libavformat --libs` `pkg-config libavutil --libs` -o SDLtest

# strip SDLtest.exe

# upx -9 SDLtest.exe

# 10 KB alone, 829 KB with SDL2!
