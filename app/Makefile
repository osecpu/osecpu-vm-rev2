
OSECTOLS=../tol/osectols

%.ose : %.ask Makefile
	gcc -E -P -C -x c $*.ask | ../tol/rmcomment > $*.m0
	$(OSECTOLS) tool:aska in:$*.m0 out:$*.m1
	gcc -E -x c $*.m1 -o $*.m2
	$(OSECTOLS) tool:lbstk in:$*.m2 out:$*.m3 lst:$*.lst
	grep -v -e '^#' $*.m3 > $*.m4
	$(OSECTOLS) tool:db2bin in:$*.m4 out:$*.ose
	$(OSECTOLS) tool:appack in:$*.ose out:$*.packed.ose
	
