#include "osecpu-vm.h"

// C0-DFに命令を拡張するためのもの.

void OsecpuInitExtend()
{
	return;
}

int instrLengthExtend(const Int32 *src, const Int32 *src1)
// instrLengthSimpleInitTool()で登録していないものだけに反応すればよい.
{
	return 0;
}

Int32 *hh4DecodeExtend(OsecpuJitc *jitc, Int32 opecode)
// instrLengthSimpleInitTool()で登録していないものだけに反応すればよい.
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
