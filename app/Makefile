
TARGETS=$(addsuffix .ose, $(basename $(wildcard *.ask)))
OSECTOLS=../tol/osectols

default:
	make $(TARGETS)

%.ose : %.ask
	gcc -E -o $*_0ask.txt -x c $<
	$(OSECTOLS) tool:aska in:$*_0ask.txt out:$*_1oas.txt
	gcc -E -P -o $*_2cas.txt -x c $*_1oas.txt
	$(OSECTOLS) tool:lbstk in:$*_2cas.txt out:$*_3cas.txt lst:$*_3lbl.txt
	$(OSECTOLS) tool:db2bin in:$*_3cas.txt out:$@
	$(OSECTOLS) tool:appack in:$@ out:$*_.ose

clean:
	-rm *.txt

src_only:
	make clean
	-rm *.ose
