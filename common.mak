
TOLSET_PATH=$(ROOT_PATH)/../z_tools

ifeq ($(OS),Windows_NT)
	# for Windows(MinGW)
	CC_NATIVE = gcc
	CFLAGS_VM = -Wall -Os -Wl,-s,-lgdi32
	REMOVE = rm

else
	UNAME = \${shell uname}
ifeq ($(UNAME),Linux)
	# for Linux
endif

ifeq ($(UNAME),Darwin)
	# for MacOSX
	CC_NATIVE = gcc
	CFLAGS_VM = -Wall -arch x86_64 -Wno-deprecated -framework GLUT -framework OpenGL
	REMOVE = rm
endif

endif

