ROOT_PATH=.

include common.mak

default:
	make -C tol
	make -C vm
	make -C app

clean:
	make -C tol clean
	make -C vm clean
	make -C app clean
