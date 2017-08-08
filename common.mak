
TOLSET_PATH=$(ROOT_PATH)/../z_tools

ifeq (\$(OS),Windows_NT)
	# or Windows

else
	UNAME = \${shell uname}
ifeq (\$(UNAME),Linux)
	# for Linux
endif

ifeq (\$(UNAME),Darwin)
	# for MacOSX
	CC = waaaa
endif

endif

