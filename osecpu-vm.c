#include "osecpu-vm.h"

// init�֌W.

void osecpuInit()
{
	instrLengthSimpleInit();

	osecpuInitInteger();
	osecpuInitPointer();
	osecpuInitFloat();
	osecpuInitExtend();
	return;
}

void definesInit(Defines *def)
{
	int i;
	for (i = 0; i < DEFINES_MAXLABELS; i++)
		def->label[i].typ = 0; // ���g�p.
	return;
}

// instrLength�֌W.

#define INSTRLEN_TABLE_SIZE		0x100

static int *instrLengthSimpleTable; // �P���Œ蒷���ߗp�̃e�[�u��. �p�����[�^�͂��ׂ�unsigned.

int instrLengthSimple(Int32 opecode)
{
	int retcode = 0;
	if (0x00 <= opecode && opecode < INSTRLEN_TABLE_SIZE)
		retcode = instrLengthSimpleTable[opecode];
	return retcode;
}

void instrLengthSimpleInit()
{
	int i;
	instrLengthSimpleTable = malloc(INSTRLEN_TABLE_SIZE * sizeof (int));
	for (i = 0; i < INSTRLEN_TABLE_SIZE; i++)
		instrLengthSimpleTable[i] = 0;
	return;
}

void instrLengthSimpleInitTool(int *table, int ope0, int ope1)
{
	int i, len;
	if (ope1 >= INSTRLEN_TABLE_SIZE) {
		fprintf(stderr, "Error: instrLengthSimpleInitTool: ope1=0x%02X\n", ope1); // �����G���[.
		exit(1);
	}
	for (i = ope0; i <= ope1; i++) {
		len = table[i - ope0];
		if (len > 0)
			instrLengthSimpleTable[i] = len;
	}
	return;
}

int instrLength(const Int32 *src, const Int32 *src1)
{
	Int32 opecode = src[0];
	int retcode = instrLengthSimple(opecode);
	if (retcode > 0) goto fin;
	retcode = instrLengthInteger(src, src1);
	if (retcode > 0) goto fin;
	retcode = instrLengthPointer(src, src1);
	if (retcode > 0) goto fin;
	retcode = instrLengthFloat(src, src1);
	if (retcode > 0) goto fin;
	retcode = instrLengthExtend(src, src1);
	if (retcode > 0) goto fin;
	retcode = - JITC_BAD_OPECODE;
fin:
	if (src + retcode > src1)
		retcode = - JITC_SRC_OVERRUN;
	return retcode;
}

// jitc�֌W.

void jitcInitDstLogSetPhase(OsecpuJitc *jitc, int phase)
{
	int i;
	for (i = 0; i < JITC_DSTLOG_SIZE; i++)
		jitc->dstLog[i] = NULL;
	jitc->dstLogIndex = 0;
	jitc->phase = phase;
	return;
}

void jitcSetRetCode(int *pRC, int value)
{
	if (*pRC <= 0)
		*pRC = value; // ����0�ȉ��Ȃ�㏑������, ���Ȃ珑�������Ȃ�.
	return;
}

int jitcStep(OsecpuJitc *jitc)
{
	const Int32 *ip = jitc->src;
	Int32 opecode = ip[0], imm;
	int bit, bit0, bit1, r, r0, r1, r2;
	int retcode = -1, *pRC = &retcode, length = instrLength(ip, jitc->src1);
	int i;
	if (length < 0) {
		jitcSetRetCode(pRC, - length);
		goto fin;
	}
	retcode = jitcStepInteger(jitc);
	if (retcode >= 0) goto fin;
	retcode = jitcStepPointer(jitc);
	if (retcode >= 0) goto fin;
	retcode = jitcStepFloat(jitc);
	if (retcode >= 0) goto fin;
	retcode = jitcStepExtend(jitc);
	if (retcode >= 0) goto fin;
	retcode = JITC_BAD_OPECODE;
fin:
	if (retcode != 0)
		goto fin1;
	if (jitc->dst + length > jitc->dst1) {
		retcode = JITC_DST_OVERRUN;
		goto fin1;
	}
	for (i = 0; i < length; i++)
		jitc->dst[i] = ip[i];
	i = jitc->dstLogIndex;
	jitc->dstLog[i] = jitc->dst; // �G���[�̂Ȃ��������߂͋L�^����.
	jitc->dstLogIndex = (i + 1) % JITC_DSTLOG_SIZE;
	jitc->src += length;
	jitc->dst += length;
fin1:
	jitc->errorCode = retcode;
	return retcode;
}

int jitcAll(OsecpuJitc *jitc)
{
	const Int32 *src0 = jitc->src;
	Int32 *dst0 = jitc->dst;
	jitcInitDstLogSetPhase(jitc, 0);
	for (;;) {
		// ���z�͓K���ȏ�������߂āA�x�݋x�݂ł��ׂ���������Ȃ�.
		jitcStep(jitc);
		if (jitc->errorCode != 0) break;
	}
	if (jitc->errorCode != JITC_SRC_OVERRUN)
		goto fin;
	jitc->src = src0;
	jitc->dst = dst0;
	jitcInitDstLogSetPhase(jitc, 1);
	for (;;) {
		// ���z�͓K���ȏ�������߂āA�x�݋x�݂ł��ׂ���������Ȃ�.
		jitcStep(jitc);
		if (jitc->errorCode != 0) break;
	}
fin:
	return jitc->errorCode;
}

void jitcStep_checkBits32(int *pRC, int bits)
{
	if (!(0 <= bits && bits <= 32))
		jitcSetRetCode(pRC, JITC_BAD_BITS);
	return;
}

void jitcStep_checkRxx(int *pRC, int rxx)
{
	if (!(0x00 <= rxx && rxx <= 0x3f))
		jitcSetRetCode(pRC, JITC_BAD_RXX);
	return;
}

void jitcStep_checkRxxNotR3F(int *pRC, int rxx)
{
	if (!(0x00 <= rxx && rxx <= 0x3e))
		jitcSetRetCode(pRC, JITC_BAD_RXX);
	return;
}

// exec�֌W.

int execStep(OsecpuVm *vm)
{
	const Int32 *ip = vm->ip;
	vm->errorCode = 0;
	execStepInteger(vm);
	if (ip != vm->ip) goto fin;
	execStepPointer(vm);
	if (ip != vm->ip) goto fin;
	execStepFloat(vm);
	if (ip != vm->ip) goto fin;
	execStepExtend(vm);
	if (ip != vm->ip) goto fin;
	if (*ip == -1) {
		vm->errorCode = EXEC_ABORT_OPECODE_M1; // �f�o�b�O�p.
		goto fin;
	}

	fprintf(stderr, "Error: execStep: opecode=0x%02X\n", *ip); // �����G���[.
	exit(1);
fin:
	return vm->errorCode;
}

int execAll(OsecpuVm *vm)
{
	for (;;) {
		// ���z�͓K���ȏ�������߂āA�x�݋x�݂ł��ׂ���������Ȃ�.
		execStep(vm);
		if (vm->errorCode != 0) break;
	}
	return vm->errorCode;
}

void execStep_checkBitsRange(Int32 value, int bits, OsecpuVm *vm)
{
	int max, min;
	max = 1 << (bits - 1); // ��: bits=8����max=128�ɂȂ�.
	max--; // ��: bits=8����max=127�ɂȂ�.
	min = - max - 1; // ��: bits=8����min=-128�ɂȂ�B
	if (!(min <= value && value <= max))
		jitcSetRetCode(&vm->errorCode, EXEC_BITS_RANGE_OVER);
	return;
}

