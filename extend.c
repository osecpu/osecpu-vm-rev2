#include "osecpu-vm.h"

// C0-DF�ɖ��߂��g�����邽�߂̂���.

void osecpuInitExtend()
{
	return;
}

int instrLengthExtend(const Int32 *src, const Int32 *src1)
// instrLengthSimpleInitTool()�œo�^���Ă��Ȃ����̂����ɔ�������΂悢.
{
	return 0;
}

Int32 *hh4DecodeExtend(OsecpuJitc *jitc, Int32 opecode)
// instrLengthSimpleInitTool()�œo�^���Ă��Ȃ����̂����ɔ�������΂悢.
{
	return jitc->dst;
}

int jitcStepExtend(OsecpuJitc *jitc)
{
	return -1;
}

void execStepExtend(OsecpuVm *vm)
{
	return;
}
