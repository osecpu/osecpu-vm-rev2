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

int jitcAfterStepExtend(OsecpuJitc *jitc)
{
	return 0;
}

void execStepExtend(OsecpuVm *vm)
{
	return;
}
