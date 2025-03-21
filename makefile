#rpath thing so exe looks in libs for shared libs
#	-Xlinker --verbose
CC=gcc
CFLAGS=-std=gnu23 -g -O0 -march=native	\
	-DCGLM_FORCE_DEPTH_ZERO_TO_ONE	\
	-DCGLM_FORCE_LEFT_HANDED	\
	-IGrogLibsC/uthash/src	\
	-IGrogLibsC/dxvk-native/include/native/windows	\
	-IGrogLibsC/dxvk-native/include/native/directx \
	-IGrogLibsC/cglm/include	\
	-Wall				\
	-Wl,-rpath='libs',--disable-new-dtags

SOURCES=$(wildcard *.c)
LIBS=-lvulkan -lm -lUtilityLib -lPhysicsLib -lMaterialLib -lMeshLib -lTerrainLib -lInputLib -lAudioLib
LDFLAGS=-Llibs

all: Summoning

Summoning: $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o Summoning $(LIBS)	\
	GrogLibsC/dxvk-native/build/src/dxgi/libdxvk_dxgi.so	\
	GrogLibsC/dxvk-native/build/src/d3d11/libdxvk_d3d11.so