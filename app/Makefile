
ROOT_PATH=..
include $(ROOT_PATH)/common.mak

TARGETS=$(addsuffix .ose, $(basename $(wildcard *.ask)))
OSECTOLS=../tol/osectols

default:
	make $(TARGETS)

%.ose : %.ask
	$(CC_NATIVE) -E -o $*_0ask.txt -x c $<
	$(OSECTOLS) tool:aska in:$*_0ask.txt out:$*_1oas.txt
	$(CC_NATIVE) -E -P -o $*_2cas.txt -x c $*_1oas.txt
	$(OSECTOLS) tool:lbstk in:$*_2cas.txt out:$*_3cas.txt lst:$*_3lbl.txt
	$(OSECTOLS) tool:db2bin in:$*_3cas.txt out:$@
	$(OSECTOLS) tool:appack in:$@ out:$*_.ose

clean:
	-$(REMOVE) *.txt

src_only:
	make clean
	-$(REMOVE) *.ose
