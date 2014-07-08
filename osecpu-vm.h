#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int Int32; // 32bit�ȏ�ł���΂悢�i64bit�ȏ�ł��悢�j.

typedef struct _PReg {
	void *p;
} PReg;

typedef struct _Label {
	int typ;
} Label;

typedef struct _Defines {
	// ���x����\���̂̒�`.
	int maxLabels;
	Label label[4096];
} Defines;

typedef struct _HH4Reader {
	const unsigned char *p, *p1;
	int half, len, errorCode;
} HH4Reader;

typedef struct _Jitc {
	Defines *defines;
	const Int32 *src, *src1;
	Int32 *dst, *dst1;
	Int32 dr[4];
	int errorCode;
	HH4Reader *hh4r;
	const unsigned char *hh4src1;
	Int32 *hh4dst, *hh4dst1;
} Jitc;

typedef struct _VM {
	Int32 r[0x40];
	int bit[0x40];
	PReg p[0x40];
	const Int32 *ip, *ip1; /* instruction-pointer, program-counter */
	Int32 dr[4];
	const Defines *defines;
	int errorCode;
} VM;

// jitc.c : JIT�R���p�C���֌W
int instrLengthSimple(Int32 opecode); // �ȒP�Ȗ��߂ɂ��Ė��ߒ���Ԃ�.
int instrLength(const Int32 *src, const Int32 *src1); // ���ߒ���Ԃ�.
int jitcStep(Jitc *jitc); // OSECPU���߂���������؂���. �G���[���Ȃ����0��Ԃ�.
int jitcAll(Jitc *jitc);
void jitcSetRetCode(int *pRC, int value);

#define JITC_BAD_OPECODE		1
#define JITC_BAD_BITS			2
#define JITC_BAD_RXX			3
#define JITC_BAD_PXX			4
#define JITC_BAD_CND			5
#define JITC_BAD_LABEL			6
#define JITC_SRC_OVERRUN		7
#define JITC_DST_OVERRUN		8
#define JITC_HH4_DST_OVERRUN	9
#define JITC_HH4_BITLENGTH_OVER	10

// exec.c : VM�C���^�v���^�֌W
int execStep(VM *r); // ���؍ς݂�OSECPU���߂���������s����.
int execAll(VM *vm);

#define EXEC_BAD_BITS			1
#define EXEC_BITS_RANGE_OVER	2
#define EXEC_ABORT_OPECODE_M1	0xffff

// JIT�R���p�C����VM�C���^�v���^�̑o���ɕK�v�Ȋ֐��́Ajitc.c�̂ق��ɂ���.

// hh4.c : hh4�֌W
void hh4Init(HH4Reader *hh4r, void *p, int half, void *p1);
int hh4Get4bit(HH4Reader *hh4r);
Int32 hh4GetUnsigned(HH4Reader *hh4r);
Int32 hh4GetSigned(HH4Reader *hh4r);
Int32 *hh4Decode(Jitc *jitc);
unsigned char *hh4StrToBin(unsigned char *src, unsigned char *src1, unsigned char *dst, unsigned char *dst1);



