#include "osecpu-vm.h"

// 0xc0�`0xdf�ɖ��߂��g�����邽�߂̂���.

Int32 *ext_hh4Decode(Jitc *jitc, Int32 opecode)
{
	return jitc->dst;
}

int ext_instrLength(const Int32 *src, const Int32 *src1)
{
	return 0;
}

int ext_jitcStep(Jitc *jitc)
{
	return JITC_BAD_OPECODE;
}

void ext_execStep(VM *vm)
{
	return;
}
