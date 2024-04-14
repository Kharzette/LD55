#rpath thing so exe looks in libs for shared libs
#	-Xlinker --verbose
CC=gcc
CFLAGS=-std=gnu2x -g -O0 -march=native	\
	-DCGLM_FORCE_DEPTH_ZERO_TO_ONE	\
	-DCGLM_FORCE_LEFT_HANDED	\
	-IGrogLibsC/SDL/include	\
	-IGrogLibsC/uthash/src	\
	-IGrogLibsC/dxvk-native/include/native/windows	\
	-IGrogLibsC/dxvk-native/include/native/directx \
	-IGrogLibsC/cglm/include	\
	-Wall				\
	-Wl,-rpath='libs',--disable-new-dtags

SOURCES=$(wildcard *.c)
LIBS=-lvulkan -lm -lUtilityLib -lMaterialLib -lMeshLib -lTerrainLib -lInputLib -lAudioLib
LDFLAGS=-Llibs

all: Summoning

Summoning: $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o Summoning $(LIBS)	\
	GrogLibsC/SDL/build/libSDL3.so	\
	GrogLibsC/libpng/build/libpng.so	\
	GrogLibsC/AudioLib/FAudio/build/libFAudio.so	\
	GrogLibsC/dxvk-native/build/src/dxgi/libdxvk_dxgi.so	\
	GrogLibsC/dxvk-native/build/src/d3d11/libdxvk_d3d11.so