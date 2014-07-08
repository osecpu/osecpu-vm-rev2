#include "osecpu-vm.h"

int main(int argc, const char **argv)
{
	Defines defs;
	OsecpuJitc jitc;
	OsecpuVm vm;
	HH4Reader hh4r;
	unsigned char hh4src[64], *hh4src1;
	Int32 i32buf[64], j32buf[64];
	jitc.defines = &defs;
	vm.defines = &defs;
	jitc.hh4r = &hh4r;

	OsecpuInit();

	// hh4StrToBin()���g���āAhh4src[]��16�i����4bit�P�ʂŏ�������. 
	hh4src1 = 	hh4StrToBin(
					"2 d85 0 a0"					// LIMM32(R00, -123);
					"c40 0 1 0 c40"					// FLIMM64(F00, 1;
					"c40 1 40490fdb 1 c40"			// FLIMM64(F01, (float) PI);
					"c40 2 400921fb54442d18 2 c40", // FLIMM64(F02, PI);
					NULL, hh4src, &hh4src[64]
				);

	// hh4Decode()���g���āAhh4src[]���̃v���O������i32buf[]�ɓW�J����.
	hh4Init(&hh4r, hh4src, 0, hh4src1);
	jitc.hh4dst  =  i32buf;
	jitc.hh4dst1 = &i32buf[64];
	jitc.src     =  i32buf;
	jitc.src1    = hh4Decode(&jitc);
	// ���̎��_�ŁAhh4src[]�͔j�����Ă��ǂ�.

	// jitcAll()���g���āAi32buf[]���̃v���O������j32buf[]�ɕϊ�����(���ۂɂ͖��ϊ�).
	jitc.dst  =  j32buf;
	jitc.dst1 = &j32buf[64];
	printf("jitcAll()=%d\n", jitcAll(&jitc)); // 7�Ȃ琬��(JITC_SRC_OVERRUN).
	*jitc.dst = -1; // �f�o�b�O�p�̓���opecode.
	vm.ip  = j32buf;
	vm.ip1 = jitc.dst;
	// ���̎��_�ŁAi32buf[]�͔j�����Ă��ǂ�.

	// execAll()���g���āAj32buf[]���̒��ԃR�[�h�����s����.
	printf("execAll()=%d\n", execAll(&vm)); // 65535�Ȃ琬��(EXEC_ABORT_OPECODE_M1).
	printf("R00=%d\n", vm.r[0x00]);
	printf("F00=%f\n", vm.f[0x00]);
	printf("F01=%.15f\n", vm.f[0x01]);
	printf("F02=%.15f\n", vm.f[0x02]);
 
	return 0;
}

