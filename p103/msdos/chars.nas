	ORG 0x100
	MOV AH,0x02
	MOV DL,0x20
lp:
	INT 0x21
	INC DX
	CMP DL,0x7f
	JNE	lp
	MOV	DL,0x0a
	INT 0x21
    RET
