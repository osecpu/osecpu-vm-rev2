#include "osecpu-vm.h"

// C0-DFに命令を拡張するためのもの.

void jitcInitExtend(OsecpuJitc *jitc)
{
	return;
}

int jitcStepExtend(OsecpuJitc *jitc)
{
	return -1;
}

void jitcAfterStepExtend(OsecpuJitc *jitc)
{
	return;
}

void execStepExtend(OsecpuVm *vm)
{
	return;
}
