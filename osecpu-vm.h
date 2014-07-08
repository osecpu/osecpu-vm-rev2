#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int Int32; // 32bit�ȏ�ł���΂悢�i64bit�ȏ�ł��悢�j.

#define DEFINES_MAXLABELS	4096

typedef struct _PReg {
	void *p;
} PReg;

typedef struct _Label {
	int typ, opt;
	Int32 *dst;
} Label;

typedef struct _Defines {
	// ���x����\���̂̒�`.
	Label label[DEFINES_MAXLABELS];
} Defines;

typedef struct _HH4Reader {
	const unsigned char *p, *p1;
	int half, len, errorCode;
} HH4Reader;

#define JITC_DSTLOG_SIZE	16

typedef struct _OsecpuJitc {
	int phase, dstLogIndex;
	Defines *defines;
	const Int32 *src, *src1;
	Int32 *dst, *dst1, *dstLog[JITC_DSTLOG_SIZE];
		// dstLog[]�͖��߂����������̂ڂ肽���Ƃ��Ɏg��.
		//   ���Ƃ���data(2E)�����p����.
	Int32 dr[4]; // Integer
	int errorCode;
	HH4Reader *hh4r;
	const unsigned char *hh4src1;
	Int32 *hh4dst, *hh4dst1;
} OsecpuJitc;

typedef struct _OsecpuVm {
	Int32 r[0x40]; // Integer
	int bit[0x40]; // Integer
	Int32 dr[4]; // Integer
	double f[0x40]; // Float
	int bitF[0x40]; // Float
	PReg p[0x40];
	const Int32 *ip, *ip1; /* instruction-pointer, program-counter */
	const Defines *defines;
	int errorCode;
} OsecpuVm;

// osecpu-vm.c
void osecpuInit(); // ������.
int instrLengthSimple(Int32 opecode); // �ȒP�Ȗ��߂ɂ��Ė��ߒ���Ԃ�.
void instrLengthSimpleInit();
void instrLengthSimpleInitTool(int *table, int ope0, int ope1); // �������x��.
int instrLength(const Int32 *src, const Int32 *src1); // ���ߒ���Ԃ�.
void jitcSetRetCode(int *pRC, int value);
int jitcStep(OsecpuJitc *jitc); // OSECPU���߂���������؂���. �G���[���Ȃ����0��Ԃ�.
int jitcAll(OsecpuJitc *jitc);

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
#define JITC_BAD_FLIMM_MODE		11
#define JITC_BAD_FXX			12
#define JITC_LABEL_REDEFINED	13
#define JITC_BAD_LABEL_TYPE		14
#define JITC_LABEL_UNDEFINED	15

void jitcStep_checkBits32(int *pRC, int bits);
void jitcStep_checkRxx(int *pRC, int rxx);
void jitcStep_checkRxxNotR3F(int *pRC, int rxx);

int execStep(OsecpuVm *r); // ���؍ς݂�OSECPU���߂���������s����.
int execAll(OsecpuVm *vm);

#define EXEC_BAD_BITS			1
#define EXEC_BITS_RANGE_OVER	2
#define EXEC_BAD_R2				3	// SBX, SHL, SAR��r2���s�K��.
#define EXEC_DIVISION_BY_ZERO	4
#define EXEC_SRC_OVERRUN		5
#define EXEC_ABORT_OPECODE_M1	0xffff

void execStep_checkBitsRange(Int32 value, int bits, OsecpuVm *vm);

// hh4.c : hh4�֌W.
void hh4Init(HH4Reader *hh4r, void *p, int half, void *p1);
int hh4Get4bit(HH4Reader *hh4r);
Int32 hh4GetUnsigned(HH4Reader *hh4r);
Int32 hh4GetSigned(HH4Reader *hh4r);
Int32 *hh4Decode(OsecpuJitc *jitc);
unsigned char *hh4StrToBin(unsigned char *src, unsigned char *src1, unsigned char *dst, unsigned char *dst1);

// integer.c : ��������
void osecpuInitInteger();
int instrLengthInteger(const Int32 *src, const Int32 *src1);
Int32 *hh4DecodeInteger(OsecpuJitc *jitc, Int32 opecode);
int jitcStepInteger(OsecpuJitc *jitc);
void execStepInteger(OsecpuVm *vm);

// pointer.c : �|�C���^����
void osecpuInitPointer();
int instrLengthPointer(const Int32 *src, const Int32 *src1);
Int32 *hh4DecodePointer(OsecpuJitc *jitc, Int32 opecode);
int jitcStepPointer(OsecpuJitc *jitc);
void execStepPointer(OsecpuVm *vm);

// float.c : ���������_����
void osecpuInitFloat();
int instrLengthFloat(const Int32 *src, const Int32 *src1);
Int32 *hh4DecodeFloat(OsecpuJitc *jitc, Int32 opecode);
int jitcStepFloat(OsecpuJitc *jitc);
void execStepFloat(OsecpuVm *vm);

// extend.c : �g�����ߊ֌W.
void osecpuInitExtend();
int instrLengthExtend(const Int32 *src, const Int32 *src1);
Int32 *hh4DecodeExtend(OsecpuJitc *jitc, Int32 opecode);
int jitcStepExtend(OsecpuJitc *jitc);
void execStepExtend(OsecpuVm *vm);

