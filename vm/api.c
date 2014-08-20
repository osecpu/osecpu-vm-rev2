#include "osecpu-vm.h"
#include <setjmp.h>
#include <time.h>

// OsecpuMain()はこのapi.cにあります！

typedef struct _ApiWork {
	char winClosed, autoSleep /* , col3bgr */;
//	jmp_buf setjmpEnv;
	jmp_buf setjmpErr;
	unsigned char lastConsoleChar;
	int argc;
	const unsigned char **argv;
	unsigned int xorShift[4];
} ApiWork;

static ApiWork apiWork;

const Int32 *apiEntry(OsecpuVm *vm);
void apiInit(OsecpuVm *vm, int argc, const unsigned char **argv);
void apiEnd(OsecpuVm *vm, Int32 retcode);
void apiXorShiftSetSeed(unsigned int s);
unsigned int apiXorShift();

#define BUFFER_SIZE		1024 * 1024	// 1M
#define KEYBUFSIZ		4096

int OsecpuMain(int argc, const unsigned char **argv)
{
	Defines defs;
	OsecpuJitc jitc;
	OsecpuVm vm;
	unsigned char *byteBuf0 = malloc(BUFFER_SIZE);
	Int32 *j32buf = malloc(BUFFER_SIZE * sizeof (Int32));
	int fileSize, rc, i;
	int stackSize = 1; /* メガバイト単位 */
	FILE *fp;
	char outputBackend = 0;
	jitc.defines = &defs;
	vm.defines = &defs;
	vm.toDebugMonitor = 0;
	vm.exitToDebug = 0;
	vm.debugAutoFlsh = 1;
	vm.debugBreakPointIndex = -1;
	vm.debugBreakPointValue = 0;
	osecpuVmStackInit(&vm, stackSize * (1024 * 1024));
	osecpuVmPtrCtrlInit(&vm, 4096); 
	for (i = 1; ; i++) {
		if (argc <= i) {
			fputs("usage>osecpu app.ose\n", stderr);
			exit(1);
		}
		if (argv[i][0] != '-') break;
		if (strcmp(argv[i], "-d") == 0) vm.toDebugMonitor = vm.exitToDebug = 1;
		if (strcmp(argv[i], "-b") == 0) outputBackend = 1;
	}
	fp = fopen(argv[i], "rb");
	if (fp == NULL) {
		fputs("fopen error.\n", stderr);
		exit(1);
	}
	fileSize = fread(byteBuf0, 1, BUFFER_SIZE, fp);
	fclose(fp);
	if (fileSize >= BUFFER_SIZE) {
		fputs("app-file too large.\n", stderr);
		exit(1);
	}
	if (byteBuf0[0] != 0x05 || byteBuf0[1] != 0xe2 || fileSize < 3) {
		fputs("app-file signature mismatch.\n", stderr);
		exit(1);
	}

	// フロントエンドコードからバックエンドコードを得るためのループ.
	for (;;) {
		if (fileSize < 0) break;
		if (byteBuf0[2] == 0x02) {
			fileSize = decode_tek5 (byteBuf0 + 3, byteBuf0 + fileSize, byteBuf0 + 2, byteBuf0 + BUFFER_SIZE);
			if (fileSize > 0) fileSize += 2;
			continue;
		}
		if (byteBuf0[2] == 0x01) {
			fileSize = decode_upx  (byteBuf0 + 3, byteBuf0 + fileSize, byteBuf0 + 2, byteBuf0 + BUFFER_SIZE);
			if (fileSize > 0) fileSize += 2;
			continue;
		}
		if (byteBuf0[2] >= 0x10) {
			fileSize = decode_fcode(byteBuf0 + 2, byteBuf0 + fileSize, byteBuf0 + 2, byteBuf0 + BUFFER_SIZE, 1);
			if (fileSize > 0) fileSize += 2;
			continue;
		}
		break;
	}
	if (fileSize <= 0 || byteBuf0[2] != 0x00) {
		fputs("app-file decode error.\n", stderr);
		exit(1);
	}
	if (outputBackend != 0) {
		fputs("output backend code.\n", stderr);
		fp = NULL;
		if (argc > i + 1)
			fp = fopen(argv[i + 1], "wb");
		if (fp == NULL) {
			fputs("fopen error.\nusage>osecpu -b app.ose app.org\n", stderr);
			exit(1);
		}
		fwrite(byteBuf0, 1, fileSize, fp);
		return 0;
	}

	hh4ReaderInit(&jitc.hh4r, byteBuf0 + 3, 0, byteBuf0 + fileSize, 0);
	jitc.dst  = j32buf;
	jitc.dst1 = j32buf + BUFFER_SIZE;
	rc = jitcAll(&jitc);
	if (rc != 0) {
		fprintf(stderr, "%s\n", debugJitcReport(&jitc, byteBuf0));
	//	fprintf(stderr, "jitcAll()=%d, DR0=%d\n", rc, jitc.dr[0]);
		exit(1);
	}
//	*jitc.dst = -1; // 終端のための特殊opecode.
	vm.ip  = j32buf;
	vm.ip1 = jitc.dst;

	apiInit(&vm, argc - i, &argv[i]);
//	for (i = 1; i < argc; i++) {
//		if (strcmp(argv[i], "-col3bgr") == 0)
//			apiWork.col3bgr = 1;
//	}
	rc = execAll(&vm);
	if (rc != EXEC_SRC_OVERRUN) {
		fprintf(stderr, "execAll()=%d, DR0=%d\n", rc, vm.dr[0]); // EXEC_SRC_OVERRUNなら成功.
		exit(1);
	}
	apiEnd(&vm, 0);
	return 0;
}

/* driver.c */
void *mallocRWE(int bytes); // 実行権付きメモリのmalloc.
void drv_openWin(int x, int y, unsigned char *buf, char *winClosed);
void drv_flshWin(int sx, int sy, int x0, int y0);
void drv_sleep(int msec);
extern int *vram, v_xsiz, v_ysiz;
extern int *keybuf, keybuf_r, keybuf_w, keybuf_c;
extern char *toDebugMonitor;

void apiInit(OsecpuVm *vm, int argc, const unsigned char **argv)
{
	int i, j;
	for (i = 0; i <= 0x3f; i++) {
		vm->r[i] = 0; vm->bit[i] = 32; // Rxx: すべて32ビットの0.
		vm->p[i].typ = PTR_TYP_INVALID; // Pxx: すべて不正.
	}
	j = 1;
	for (i = 0; i < DEFINES_MAXLABELS; i++) {
		if (j > 4) break;
		if (vm->defines->label[i].opt != 1) continue;
		if (vm->defines->label[i].typ == PTR_TYP_CODE) continue;
		execStep_plimm(vm, j, i);
		j++;
	}
	vm->p[0x2f].typ = PTR_TYP_NATIVECODE;
	vm->p[0x2f].p = (void *) &apiEntry;
	vm->disableDebug = 0;
	vm->debugWatchIndex[0] = 0;
	vm->debugWatchIndex[1] = 1;
	vm->debugWatchs = 0;
	apiWork.winClosed = 0;
	apiWork.autoSleep = 0;
//	apiWork.col3bgr = 0;
	apiWork.lastConsoleChar = '\n';
	apiWork.argc = argc;
	apiWork.argv = argv;
//	if (setjmp(apiWork.setjmpEnv) != 0)
//		apiEnd(vm);
	keybuf_r = keybuf_w = keybuf_c = 0;
	keybuf = malloc(KEYBUFSIZ * sizeof (int));
	toDebugMonitor = &(vm->toDebugMonitor);
	apiXorShiftSetSeed(time(NULL));
	vm->execSteps0 = vm->execSteps1 = 0;
	vm->execSteps0Limit = vm->execSteps1Limit = -1;
	vm->mallocTotal0 = 0; vm->mallocTotal0Limit = 0x7fffffff;
	vm->mallocTotal1 = 0; vm->mallocTotal1Limit = 0x7fffffff;
	vm->tallocTotal0 = 0; vm->tallocTotal0Limit = 0x7fffffff;
	vm->tallocTotal1 = 0; vm->tallocTotal1Limit = 0x7fffffff;
	return;
}

void api0001_putString(OsecpuVm *vm);
void api0002_drawPoint(OsecpuVm *vm);
void api0003_drawLine(OsecpuVm *vm);
void api0004_rect(OsecpuVm *vm);
void api0005_oval(OsecpuVm *vm);
void api0006_drawString(OsecpuVm *vm);
void api0008_exit(OsecpuVm *vm);
void api0009_sleep(OsecpuVm *vm);
void api000d_inkey(OsecpuVm *vm);
void api0010_openWin(OsecpuVm *vm);
void api0013_rand(OsecpuVm *vm);

void api07c0_fileRead(OsecpuVm *vm);
void api07c1_fileWrite(OsecpuVm *vm);

const Int32 *apiEntry(OsecpuVm *vm)
// VMの再開地点を返す.
{
	int func = execStep_getRxx(vm, 0x30, 16); // 下位16bitしかみない.
	int i;
	if (vm->errorCode != 0) goto fin;
	if (setjmp(apiWork.setjmpErr) != 0) goto fin;
	if (0x0002 <= func && func <= 0x0006) {
		if (vram == 0) {
			v_xsiz = 640;
			v_ysiz = 480;
			vram = malloc(v_xsiz * v_ysiz * 4);
			drv_openWin(v_xsiz, v_ysiz, (void *) vram, &apiWork.winClosed);
			apiWork.autoSleep = 1;
			for (i = 0; i < v_xsiz * v_ysiz; i++)
				vram[i] = 0;
		}
	}
	if (func == 0x0001) { api0001_putString(vm);	goto fin; }
	if (func == 0x0002) { api0002_drawPoint(vm);	goto fin; }
	if (func == 0x0003) { api0003_drawLine(vm);		goto fin; }
	if (func == 0x0004) { api0004_rect(vm);			goto fin; }
	if (func == 0x0005) { api0005_oval(vm);			goto fin; }
	if (func == 0x0006) { api0006_drawString(vm);	goto fin; }
	if (func == 0x0008) { api0008_exit(vm);			goto fin; }
	if (func == 0x0009) { api0009_sleep(vm);		goto fin; }
	if (func == 0x000d) { api000d_inkey(vm);		goto fin; }
	if (func == 0x0010) { api0010_openWin(vm);		goto fin; }
	if (func == 0x0013) { api0013_rand(vm);			goto fin; }
	if (func == 0x07c0) { api07c0_fileRead(vm);		goto fin; }
	if (func == 0x07c1) { api07c1_fileWrite(vm);	goto fin; }
	jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
fin: ;
	const Int32 *retcode = NULL;
	execStep_checkMemAccess(vm, 0x30, PTR_TYP_CODE, EXEC_CMA_FLAG_EXEC); // 主にliveSignのチェック.
	if (vm->errorCode == 0)
		retcode = (const Int32 *) vm->p[0x30].p;
	return retcode;
}

void apiEnd(OsecpuVm *vm, Int32 retcode)
{
	// 終了処理.
	if (apiWork.autoSleep != 0) {
		if (vram != 0)
			drv_flshWin(v_xsiz, v_ysiz, 0, 0);
		while (apiWork.winClosed == 0) {
			drv_sleep(100);
			if (vm->disableDebug == 0 && vm->toDebugMonitor == 1)
				execStepDebug(vm);
		}
	}
	if (apiWork.lastConsoleChar != '\n')
		putchar('\n');
	if (vm->disableDebug == 0 && vm->exitToDebug != 0) {
		jitcSetRetCode(&vm->errorCode, EXEC_EXIT);
		vm->toDebugMonitor = 1;
		execStepDebug(vm);
	}
	exit(retcode);
}

static int iColor1[] = {
	0x000000, 0xff0000, 0x00ff00, 0xffff00,
	0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
};

static unsigned char fontdata[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00,
	0x28, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x44, 0x44, 0x44, 0xfe, 0x44, 0x44, 0x44, 0x44, 0x44, 0xfe, 0x44, 0x44, 0x44, 0x00, 0x00,
	0x10, 0x3a, 0x56, 0x92, 0x92, 0x90, 0x50, 0x38, 0x14, 0x12, 0x92, 0x92, 0xd4, 0xb8, 0x10, 0x10,
	0x62, 0x92, 0x94, 0x94, 0x68, 0x08, 0x10, 0x10, 0x20, 0x2c, 0x52, 0x52, 0x92, 0x8c, 0x00, 0x00,
	0x00, 0x70, 0x88, 0x88, 0x88, 0x90, 0x60, 0x47, 0xa2, 0x92, 0x8a, 0x84, 0x46, 0x39, 0x00, 0x00,
	0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x04, 0x02, 0x00,
	0x80, 0x40, 0x20, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x20, 0x40, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x92, 0x54, 0x38, 0x54, 0x92, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0xfe, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08, 0x08, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
	0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x40, 0x80, 0x80,
	0x00, 0x18, 0x24, 0x24, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00,
	0x00, 0x18, 0x24, 0x42, 0x42, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x40, 0x40, 0x7e, 0x00, 0x00,
	0x00, 0x18, 0x24, 0x42, 0x02, 0x02, 0x04, 0x18, 0x04, 0x02, 0x02, 0x42, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x0c, 0x0c, 0x0c, 0x14, 0x14, 0x14, 0x24, 0x24, 0x44, 0x7e, 0x04, 0x04, 0x1e, 0x00, 0x00,
	0x00, 0x7c, 0x40, 0x40, 0x40, 0x58, 0x64, 0x02, 0x02, 0x02, 0x02, 0x42, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x18, 0x24, 0x42, 0x40, 0x58, 0x64, 0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x7e, 0x42, 0x42, 0x04, 0x04, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00,
	0x00, 0x18, 0x24, 0x42, 0x42, 0x42, 0x24, 0x18, 0x24, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x18, 0x24, 0x42, 0x42, 0x42, 0x42, 0x42, 0x26, 0x1a, 0x02, 0x42, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08, 0x08, 0x10,
	0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00,
	0x00, 0x38, 0x44, 0x82, 0x82, 0x82, 0x04, 0x08, 0x10, 0x10, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
	0x00, 0x38, 0x44, 0x82, 0x9a, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x9c, 0x80, 0x46, 0x38, 0x00, 0x00,
	0x00, 0x18, 0x18, 0x18, 0x18, 0x24, 0x24, 0x24, 0x24, 0x7e, 0x42, 0x42, 0x42, 0xe7, 0x00, 0x00,
	0x00, 0xf0, 0x48, 0x44, 0x44, 0x44, 0x48, 0x78, 0x44, 0x42, 0x42, 0x42, 0x44, 0xf8, 0x00, 0x00,
	0x00, 0x3a, 0x46, 0x42, 0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x82, 0x42, 0x44, 0x38, 0x00, 0x00,
	0x00, 0xf8, 0x44, 0x44, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x44, 0x44, 0xf8, 0x00, 0x00,
	0x00, 0xfe, 0x42, 0x42, 0x40, 0x40, 0x44, 0x7c, 0x44, 0x40, 0x40, 0x42, 0x42, 0xfe, 0x00, 0x00,
	0x00, 0xfe, 0x42, 0x42, 0x40, 0x40, 0x44, 0x7c, 0x44, 0x44, 0x40, 0x40, 0x40, 0xf0, 0x00, 0x00,
	0x00, 0x3a, 0x46, 0x42, 0x82, 0x80, 0x80, 0x9e, 0x82, 0x82, 0x82, 0x42, 0x46, 0x38, 0x00, 0x00,
	0x00, 0xe7, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x42, 0xe7, 0x00, 0x00,
	0x00, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00,
	0x00, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x84, 0x48, 0x30, 0x00,
	0x00, 0xe7, 0x42, 0x44, 0x48, 0x50, 0x50, 0x60, 0x50, 0x50, 0x48, 0x44, 0x42, 0xe7, 0x00, 0x00,
	0x00, 0xf0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x42, 0x42, 0xfe, 0x00, 0x00,
	0x00, 0xc3, 0x42, 0x66, 0x66, 0x66, 0x5a, 0x5a, 0x5a, 0x42, 0x42, 0x42, 0x42, 0xe7, 0x00, 0x00,
	0x00, 0xc7, 0x42, 0x62, 0x62, 0x52, 0x52, 0x52, 0x4a, 0x4a, 0x4a, 0x46, 0x46, 0xe2, 0x00, 0x00,
	0x00, 0x38, 0x44, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x44, 0x38, 0x00, 0x00,
	0x00, 0xf8, 0x44, 0x42, 0x42, 0x42, 0x44, 0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0xf0, 0x00, 0x00,
	0x00, 0x38, 0x44, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x92, 0x8a, 0x44, 0x3a, 0x00, 0x00,
	0x00, 0xfc, 0x42, 0x42, 0x42, 0x42, 0x7c, 0x44, 0x42, 0x42, 0x42, 0x42, 0x42, 0xe7, 0x00, 0x00,
	0x00, 0x3a, 0x46, 0x82, 0x82, 0x80, 0x40, 0x38, 0x04, 0x02, 0x82, 0x82, 0xc4, 0xb8, 0x00, 0x00,
	0x00, 0xfe, 0x92, 0x92, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00,
	0x00, 0xe7, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x3c, 0x00, 0x00,
	0x00, 0xe7, 0x42, 0x42, 0x42, 0x42, 0x24, 0x24, 0x24, 0x24, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
	0x00, 0xe7, 0x42, 0x42, 0x42, 0x5a, 0x5a, 0x5a, 0x5a, 0x24, 0x24, 0x24, 0x24, 0x24, 0x00, 0x00,
	0x00, 0xe7, 0x42, 0x42, 0x24, 0x24, 0x24, 0x18, 0x24, 0x24, 0x24, 0x42, 0x42, 0xe7, 0x00, 0x00,
	0x00, 0xee, 0x44, 0x44, 0x44, 0x28, 0x28, 0x28, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00,
	0x00, 0xfe, 0x84, 0x84, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x42, 0x82, 0xfe, 0x00, 0x00,
	0x00, 0x3e, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00,
	0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x04, 0x02, 0x02,
	0x00, 0x7c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x7c, 0x00,
	0x00, 0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00,
	0x10, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x08, 0x04, 0x3c, 0x44, 0x84, 0x84, 0x8c, 0x76, 0x00, 0x00,
	0xc0, 0x40, 0x40, 0x40, 0x40, 0x58, 0x64, 0x42, 0x42, 0x42, 0x42, 0x42, 0x64, 0x58, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x4c, 0x84, 0x84, 0x80, 0x80, 0x82, 0x44, 0x38, 0x00, 0x00,
	0x0c, 0x04, 0x04, 0x04, 0x04, 0x34, 0x4c, 0x84, 0x84, 0x84, 0x84, 0x84, 0x4c, 0x36, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x82, 0x82, 0xfc, 0x80, 0x82, 0x42, 0x3c, 0x00, 0x00,
	0x0e, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x4c, 0x84, 0x84, 0x84, 0x84, 0x4c, 0x34, 0x04, 0x04, 0x78,
	0xc0, 0x40, 0x40, 0x40, 0x40, 0x58, 0x64, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0xe3, 0x00, 0x00,
	0x00, 0x10, 0x10, 0x00, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00,
	0x00, 0x04, 0x04, 0x00, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x30,
	0xc0, 0x40, 0x40, 0x40, 0x40, 0x4e, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0xe6, 0x00, 0x00,
	0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0xdb, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x64, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0xe3, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x82, 0x82, 0x82, 0x82, 0x82, 0x44, 0x38, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x64, 0x42, 0x42, 0x42, 0x42, 0x42, 0x64, 0x58, 0x40, 0xe0,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x4c, 0x84, 0x84, 0x84, 0x84, 0x84, 0x4c, 0x34, 0x04, 0x0e,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x62, 0x42, 0x40, 0x40, 0x40, 0x40, 0x40, 0xe0, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x7a, 0x86, 0x82, 0xc0, 0x38, 0x06, 0x82, 0xc2, 0xbc, 0x00, 0x00,
	0x00, 0x00, 0x10, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x46, 0x3b, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xe7, 0x42, 0x42, 0x42, 0x24, 0x24, 0x24, 0x18, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xe7, 0x42, 0x42, 0x5a, 0x5a, 0x5a, 0x24, 0x24, 0x24, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x44, 0x28, 0x28, 0x10, 0x28, 0x28, 0x44, 0xc6, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xe7, 0x42, 0x42, 0x24, 0x24, 0x24, 0x18, 0x18, 0x10, 0x10, 0x60,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x82, 0x84, 0x08, 0x10, 0x20, 0x42, 0x82, 0xfe, 0x00, 0x00,
	0x00, 0x06, 0x08, 0x10, 0x10, 0x10, 0x10, 0x60, 0x10, 0x10, 0x10, 0x10, 0x08, 0x06, 0x00, 0x00,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x00, 0x60, 0x10, 0x08, 0x08, 0x08, 0x08, 0x06, 0x08, 0x08, 0x08, 0x08, 0x10, 0x60, 0x00, 0x00,
	0x00, 0x72, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x28, 0x44, 0x82, 0xfe, 0x82, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00
};

int apiLoadColor(OsecpuVm *vm, int rxx)
{
	int c = execStep_getRxx(vm, rxx, 16), m, mm, rr, gg, bb;
	mm = execStep_getRxx(vm, 0x31, 16);
    if ((mm & 0x103) == 0x100) mm |= 0x03;
	if ((mm & 0xf00) == 0x100) mm |= 0xe00;
	m = mm & 3;
	if (m == 0x00) {
	//	static col3_bgr_table[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
		if (c < -1 || c > 7)
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
	//	if (apiWork.col3bgr != 0)
	//		c = col3_bgr_table[c & 7];
		c = iColor1[c & 0x07];
	}
	if (m == 0x01) {
		// 00, 24, 48, 6d, 91, b6, da, ff
		if ((mm & 0x100) != 0)
			c *= 1 << 6 | 1 << 3 | 1;
		if (c < 0 || c >= (1 << 9))
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		rr = (c >>  6) & 0x07;
		gg = (c >>  3) & 0x07;
		bb =  c        & 0x07;
		rr = (rr * 255) / 7;
		gg = (gg * 255) / 7;
		bb = (bb * 255) / 7;
		c = rr << 16 | gg << 8 | bb;
	}
	if (m == 0x02) {
		// 00, 08, 10, 18, 20, 29, 31, 39,
		// 41, 4a, 52, 5a, 62, 6a, 73, 7b,
		// 83, 8b, 94, 9c, a4, ac, b4, bd,
		// c5, cd, d5, de, e6, ee, f6, ff
		if (c < 0 || c >= (1 << 15))
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		if ((mm & 0x100) != 0)
			c *= 1 << 10 | 1 << 5 | 1;
		rr = (c >> 10) & 0x1f;
		gg = (c >>  5) & 0x1f;
		bb =  c        & 0x1f;
		rr = (rr * 255) / 31;
		gg = (gg * 255) / 31;
		bb = (bb * 255) / 31;
		c = rr << 16 | gg << 8 | bb;
	}
	if (m == 0x03) {
		c = execStep_getRxx(vm, rxx, 32);
		if ((mm & 0x100) != 0)
			c *= 1 << 16 | 1 << 8 | 1;
	}
	if ((mm & 0x100) != 0)
		c &= iColor1[(mm >> 9) & 7];
	if (vm->errorCode > 0)
		longjmp(apiWork.setjmpErr, 1);
	return c;
}

void apiCheckPoint(OsecpuVm *vm, int x, int y)
{
	if (x < 0 || v_xsiz <= x || y < 0 || v_ysiz <= y) {
		jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		longjmp(apiWork.setjmpErr, 1);
	}
	return;
}

void apiFillRect(int modeC, int x0, int y0, int x1, int y1, int c)
{
	int x, y;
	if (modeC == 0x00) {
		for (y = y0; y <= y1; y++) {
			for (x = x0; x <= x1; x++)
				vram[x + y * v_xsiz] =  c;
		}
	} else {
		for (y = y0; y <= y1; y++) {
			for (x = x0; x <= x1; x++) {
				if (modeC == 0x04) vram[x + y * v_xsiz] |= c;
				if (modeC == 0x08) vram[x + y * v_xsiz] ^= c;
				if (modeC == 0x0c) vram[x + y * v_xsiz] &= c;
			}
		}
	}
	return;
}

int apiSprintf(int buflen, unsigned char *buf, unsigned char *p, unsigned char *p1, int charLen, Int32 *q, Int32 *q1, OsecpuVm *vm)
// %sや%fへの対応は将来の課題.
// b32の時のみcharLen!=1を許す(T_SINT32になるため)...だから常に任意の型を受け付けているわけではない.
{
	int i = 0, base, v, j, dis = 1, len;
	unsigned char c, sign;
	while (p < p1) {
		if (i >= buflen) {
err:
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
			return 0;
		}
		c = *p;
		p += charLen;
		if (c == 0x00 || c == 0x7f) continue;
		if (0x10 <= c && c <= 0x1f)
			c = "0123456789ABCDEF"[c & 0x0f];
		if (c >= 0x07) {
			buf[i++] = c;
			continue;
		}
		if (c == 0x01) {
			// q[2] = 1:桁可変, 2:スペース化しない, 4:符号なし, 8:プラスの付与.
			if (q + 4 > q1) goto err;
			base = q[0];
			sign = 0;
			if (base ==  0) base = 16;
			if (base == -1) base = 10;
			if (base < 0 || base > 16) goto err;
			if (i + q[1] > buflen) goto err;	// q[1]:桁の最大サイズ.
			v = q[3]; // q[3]: value.
			if ((q[2] & 4) == 0) {
				// vは符号付き整数.
				if ((q[2] & 8) != 0 && v > 0) sign = '+';
				if (v < 0) { sign = '-'; v *= -1; }
			} else {
				// vは符号無し整数.
				if ((q[2] & 8) != 0 && v != 0) sign = '+';
			}
			for (j = q[1] - 1; j >= 0; j--) {
				buf[i + j] = "0123456789ABCDEF"[v % base];
				v = ((unsigned) v) / base;
			}
			j = 0;
			if ((q[2] & 2) == 0 && v == 0) {
				for (j = 0; j < q[1] - 1; j++) {
					if (buf[i + j] != '0') break;
					buf[i + j] = ' ';
				}
			}
			if (sign != 0) {
				if (j > 0) j--;
				buf[i + j] = sign;
			}
			if ((q[2] & 1) != 0 && buf[i] == ' ') {
				for (v = 0; j + v < q[1]; v++)
					buf[i + v] = buf[i + j + v];
				i += v;
			} else
				i += q[1];
			q += 4;
			continue;
		}
		if (c == 0x03) {
			len = 2;
			goto lz;
		}
		if (c == 0x04) {
			len = 3;
			goto lz;
		}
		if (c == 0x05) {
			if (p >= p1) goto err;
			len = *p + 3; // 4-
			p += charLen;
			goto lz;
		}
		if (c == 0x06) {
			if (p >= p1) goto err;
			len = *p + 3; // 4-
			p += charLen;
			dis = *p;
			p += charLen;
lz:
			for (j = 0; j < len; j++) {
				if (i >= buflen) goto err;
				c = ' ';
				if (i >= dis)
					c = buf[i - dis];
				buf[i] = c;
				i++;
			}
			continue;
		}
		goto err;
	}
	return i;
}

void apiXorShiftSetSeed(unsigned int s)
{
	apiWork.xorShift[0] = 123456789;
	apiWork.xorShift[1] = 362436069;
	apiWork.xorShift[2] = 521288629;
	apiWork.xorShift[3] =  88675123;
	if (s != 0) {
		apiWork.xorShift[0] ^= s = 0x6C078965U * (s ^ (s >> 30)) + 2;
		apiWork.xorShift[1] ^= s = 0x6C078965U * (s ^ (s >> 30)) + 3;
		apiWork.xorShift[2] ^= s = 0x6C078965U * (s ^ (s >> 30)) + 5;
		apiWork.xorShift[3] ^= s = 0x6C078965U * (s ^ (s >> 30)) + 7;
	}
	return;
}

unsigned int apiXorShift()
{
	unsigned int *a = apiWork.xorShift;
	unsigned int t = (a[0] ^ (a[0] << 11));
	a[0] = a[1];
	a[1] = a[2];
	a[2] = a[3];
	a[3] = (a[3] ^ (a[3] >> 19)) ^ (t ^ (t >> 8));
	return a[3];
}

void api0001_putString(OsecpuVm *vm)
{
	int len = execStep_getRxx(vm, 0x31, 32), i, charLen = 0;
	int len32 = execStep_getRxx(vm, 0x32, 32);
//	int len33 = execStep_getRxx(vm, 0x33, 32);
//	int len34 = execStep_getRxx(vm, 0x34, 32);
	Int32 *q, tmp32;
	unsigned char *p = vm->p[0x31].p, buf[4096];
	if (len > 0) {
		if (vm->p[0x31].typ == 0x0003 /* T_UINT8 */) {
			charLen = sizeof (unsigned char);
			execStep_checkMemAccess(vm, 0x31, 0x03 /* T_UINT8 */, EXEC_CMA_FLAG_READ);
		}
		if (vm->p[0x31].typ == 0x0006 /* T_SINT32 */) {
			charLen = sizeof (Int32);
			execStep_checkMemAccess(vm, 0x31, 0x06 /* T_SINT32 */, EXEC_CMA_FLAG_READ);
		}
		if (len == 0 || p + len * charLen > vm->p[0x31].p1) {
err:
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
			longjmp(apiWork.setjmpErr, 1);
		}
	}
	q = &tmp32;
	if (len32 > 0) {
		execStep_checkMemAccess(vm, 0x32, 0x06 /* T_SINT32 */, EXEC_CMA_FLAG_READ);
		if (vm->p[0x32].p + len32 * sizeof (Int32) > vm->p[0x32].p1) goto err;
		q = (Int32 *) vm->p[0x32].p;
	}
	i = apiSprintf(sizeof buf, buf, p, p + len * charLen, charLen, q, q + len32, vm);
	if (i > 0) {
		fwrite(buf, 1, i, stdout);
		apiWork.lastConsoleChar = buf[i - 1];
	}
	return;
}

void api0002_drawPoint(OsecpuVm *vm)
// Point(mode:R31, c:R32, x:R33, y:R34)
{
	int c = apiLoadColor(vm, 0x32), modeC = execStep_getRxx(vm, 0x31, 4) & 0x0c;
	int x = execStep_getRxx(vm, 0x33, 16), y = execStep_getRxx(vm, 0x34, 16);
	apiCheckPoint(vm, x, y);
	if (modeC == 0x00) vram[x + y * v_xsiz]  = c;
	if (modeC == 0x04) vram[x + y * v_xsiz] |= c;
	if (modeC == 0x08) vram[x + y * v_xsiz] ^= c;
	if (modeC == 0x0c) vram[x + y * v_xsiz] &= c;
	return;
}

void api003_drawLine(OsecpuVm *vm)
/// 2014.08.13 ???さんの提案によりブレゼンハムのアルゴリズムに変更.
{
	int c = apiLoadColor(vm, 0x32), modeC = execStep_getRxx(vm, 0x31, 4) & 0x0c;
	int x0 = execStep_getRxx(vm, 0x33, 16), y0 = execStep_getRxx(vm, 0x34, 16);
	int x1 = execStep_getRxx(vm, 0x35, 16), y1 = execStep_getRxx(vm, 0x36, 16);
	int x, y, dx, dy, sx, sy, err, e2;
	if (1) { // クリッピングOFFの場合.
		if (x0 == -1) x0 = v_xsiz - 1;
		if (y0 == -1) y0 = v_ysiz - 1;
		if (x1 == -1) x1 = v_xsiz - 1;
		if (y1 == -1) y1 = v_ysiz - 1;
		apiCheckPoint(vm, x0, y0);
		apiCheckPoint(vm, x1, y1);
	}
	dx = x1 - x0;
	dy = y1 - y0;
	sx = sy = 1;
	if (dx < 0) { dx *= -1; sx = -1; }
	if (dy < 0) { dy *= -1; sy = -1; }
	err = dx - dy;
	x = x0;
	y = y0;
	for (;;) {
		if (modeC == 0x00) vram[x + y * v_xsiz]  = c;
		if (modeC == 0x04) vram[x + y * v_xsiz] |= c;
		if (modeC == 0x08) vram[x + y * v_xsiz] ^= c;
		if (modeC == 0x0c) vram[x + y * v_xsiz] &= c;
		if (x == x1 && y == y1) break;
		e2 = err * 2;
		if (e2 > - dy) {
			err -= dy;
			x += sx;
		}
		if (e2 <   dx) {
			err += dx;
			y += sy;
		}
	}
	return;
}

void api0004_rect(OsecpuVm *vm)
// Rect(mode:R31, c:R32, xsiz:R33, ysiz:R34, x0:R35, y0:R36)
{
	int c = apiLoadColor(vm, 0x32), mode = execStep_getRxx(vm, 0x31, 6);
	int xsiz = execStep_getRxx(vm, 0x33, 16), ysiz = execStep_getRxx(vm, 0x34, 16), x0, y0, x1, y1;
	if (xsiz == -1) { xsiz = v_xsiz; x0 = 0; } else { x0 = execStep_getRxx(vm, 0x35, 16); }
	if (ysiz == -1) { ysiz = v_ysiz; y0 = 0; } else { y0 = execStep_getRxx(vm, 0x36, 16); }
	if (ysiz == 0) ysiz = xsiz;
//printf("c=%06X %d %d %d %d", c, xsiz, ysiz, x0, y0);
	x1 = x0 + xsiz - 1;
	y1 = y0 + ysiz - 1;
	apiCheckPoint(vm, x0, y0);
	apiCheckPoint(vm, x1, y1);
	if ((mode & 0x20) == 0)
		apiFillRect(mode & 0x0c, x0, y0, x1, y1, c);
	else {
		apiFillRect(mode & 0x0c, x0, y0, x1, y0, c);
		apiFillRect(mode & 0x0c, x0, y1, x1, y1, c);
		apiFillRect(mode & 0x0c, x0, y0, x0, y1, c);
		apiFillRect(mode & 0x0c, x1, y0, x1, y1, c);
	}
	return;
}

void api0005_oval(OsecpuVm *vm)
// Oval(mode:R31, c:R32, xsiz:R33, ysiz:R34, x0:R35, y0:R36)
{
	// これの計算精度はアーキテクチャに依存する.
	int c = apiLoadColor(vm, 0x32), mode = execStep_getRxx(vm, 0x31, 6);
	int xsiz = execStep_getRxx(vm, 0x33, 16), ysiz = execStep_getRxx(vm, 0x34, 16), x0, y0, x1, y1;
	double dcx, dcy, dcxy, dtx, dty, dcx1, dcy1, dcxy1, dtx1, dty1;
	int x, y, modeC;
	if (xsiz == -1) { xsiz = v_xsiz; x0 = 0; } else { x0 = execStep_getRxx(vm, 0x35, 16); }
	if (ysiz == -1) { ysiz = v_ysiz; y0 = 0; } else { y0 = execStep_getRxx(vm, 0x36, 16); }
	if (ysiz == 0) ysiz = xsiz;
	x1 = x0 + xsiz - 1;
	y1 = y0 + ysiz - 1;
	apiCheckPoint(vm, x0, y0);
	apiCheckPoint(vm, x1, y1);
	dcx = 0.5 * (xsiz - 1);
	dcy = 0.5 * (ysiz - 1);
	dcxy = (dcx + 0.5) * (dcy + 0.5) - 0.1;
	dcxy *= dcxy;
	if ((mode & ~3) == 0) {
		for (y = 0; y < ysiz; y++) {
			dty = (y - dcy) * dcx;
			for (x = 0; x < xsiz; x++) {
				dtx = (x - dcx) * dcy;
				if (dtx * dtx + dty * dty > dcxy) continue;
				vram[(x + x0) + (y + y0) * v_xsiz] =  c;
			}
		}
	} else if ((mode & 0x20) == 0) {
		modeC = mode & 0x0c;
		for (y = 0; y < ysiz; y++) {
			dty = (y - dcy) * dcx;
			for (x = 0; x < xsiz; x++) {
				dtx = (x - dcx) * dcy;
				if (dtx * dtx + dty * dty > dcxy) continue;
				if (modeC == 0x04) vram[x + y * v_xsiz] |= c;
				if (modeC == 0x08) vram[x + y * v_xsiz] ^= c;
				if (modeC == 0x0c) vram[x + y * v_xsiz] &= c;
			}
		}
	} else {
		#define DRAWOVALPARAM	1
		modeC = mode & 0x0c;
		dcx1 = 0.5 * (xsiz - (1 + DRAWOVALPARAM * 2));
		dcy1 = 0.5 * (ysiz - (1 + DRAWOVALPARAM * 2));
		dcxy1 = (dcx1 + 0.5) * (dcy1 + 0.5) - 0.1;
		dcxy1 *= dcxy1;
		for (y = 0; y < ysiz; y++) {
			dty  = (y - dcy) * dcx;
			dty1 = (y - dcy) * dcx1;
			for (x = 0; x < xsiz; x++) {
				dtx = (x - dcx) * dcy;
				dtx1 = (x - dcx) * dcy1;
				if (dtx * dtx + dty * dty > dcxy) continue;
				if (DRAWOVALPARAM <= x && x < xsiz - DRAWOVALPARAM && DRAWOVALPARAM <= y && y < ysiz - DRAWOVALPARAM) {
					if (dtx1 * dtx1 + dty1 * dty1 < dcxy1) continue;
				}
				if (modeC == 0x00) vram[x + y * v_xsiz]  = c;
				if (modeC == 0x04) vram[x + y * v_xsiz] |= c;
				if (modeC == 0x08) vram[x + y * v_xsiz] ^= c;
				if (modeC == 0x0c) vram[x + y * v_xsiz] &= c;
			}
		}
	}
	return;
}

void api0006_drawString(OsecpuVm *vm)
{
	int len = execStep_getRxx(vm, 0x37, 32), charLen = 0;
	int len38 = execStep_getRxx(vm, 0x38, 32);
//	int len39 = execStep_getRxx(vm, 0x39, 32);
//	int len3a = execStep_getRxx(vm, 0x3a, 32);
	Int32 *q, tmp32;
	unsigned char *p = vm->p[0x31].p, buf[4096];
	int c = apiLoadColor(vm, 0x32), modeC = execStep_getRxx(vm, 0x31, 4) & 0xc;
	int sx = execStep_getRxx(vm, 0x33, 16), sy = execStep_getRxx(vm, 0x34, 16);
	int x = execStep_getRxx(vm, 0x35, 16), y = execStep_getRxx(vm, 0x36, 16);
	int x1, y1, i, ddx, ddy, j, ch, dx, dy;
	if (sy == 0) sy = sx;
	x1 = x + sx * 8;
	y1 = y + sy * 16;
	apiCheckPoint(vm, x, y);
	apiCheckPoint(vm, x1, y1);
	if (len > 0) {
		if (vm->p[0x31].typ == 0x0003 /* T_UINT8 */) {
			charLen = sizeof (unsigned char);
			execStep_checkMemAccess(vm, 0x31, 0x03 /* T_UINT8 */, EXEC_CMA_FLAG_READ);
		}
		if (vm->p[0x31].typ == 0x0006 /* T_SINT32 */) {
			charLen = sizeof (Int32);
			execStep_checkMemAccess(vm, 0x31, 0x06 /* T_SINT32 */, EXEC_CMA_FLAG_READ);
		}
		if (len == 0 || p + len * charLen > vm->p[0x31].p1) {
err:
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
			longjmp(apiWork.setjmpErr, 1);
		}
	}
	q = &tmp32;
	if (len38 > 0) {
		execStep_checkMemAccess(vm, 0x32, 0x06 /* T_SINT32 */, EXEC_CMA_FLAG_READ);
		if (vm->p[0x32].p + len38 * sizeof (Int32) > vm->p[0x32].p1) goto err;
		q = (Int32 *) vm->p[0x32].p;
	}
	len = apiSprintf(sizeof buf, buf, p, p + len * charLen, charLen, q, q + len38, vm);
	if (len <= 0) goto fin;

	if (modeC == 0x0 && sx == 1 && sy == 1) {
		// メジャーケースを高速化.
		for (i = 0; i < len; i++) {
			ch = buf[i];
			for (dy = 0; dy < 16; dy++) {
				j = fontdata[(ch - ' ') * 16 + dy];
				for (dx = 0; dx < 8; dx++) {
					if ((j & (0x80 >> dx)) != 0) vram[(x + dx) + (y + dy) * v_xsiz] = c;
				}
			}
			x += 8;
		}
		return;
	}
	for (i = 0; i < len; i++) {
		ch = buf[i];
		for (dy = 0; dy < 16; dy++) {
			j = fontdata[(ch - ' ') * 16 + dy];
			for (ddy = 0; ddy < sy; ddy++) {
				for (dx = 0; dx < 8; dx++) {
					if ((j & (0x80 >> dx)) != 0) {
						for (ddx = 0; ddx < sx; ddx++) {
							if (modeC == 0x0) vram[x + y * v_xsiz] =  c;
							if (modeC == 0x4) vram[x + y * v_xsiz] |= c;
							if (modeC == 0x8) vram[x + y * v_xsiz] ^= c;
							if (modeC == 0xc) vram[x + y * v_xsiz] &= c;
							x++;
						}
					} else
						x += sx;
				}
				x -= sx * 8;
				y++;
			}
		}
		x += sx * 8;
		y -= sy * 16;
	}
fin:
	return;
}

void api0008_exit(OsecpuVm *vm)
{
	apiWork.autoSleep = 0;
	apiEnd(vm, execStep_getRxx(vm, 0x31, 32));
}

void api0009_sleep(OsecpuVm *vm)
{
	int mod = execStep_getRxx(vm, 0x31, 16), msec = execStep_getRxx(vm, 0x32, 32);
	// 1:入力待ち.
	// 2:flshの抑制.
	if (msec == -1) {
	//	apiWork.autoSleep = 1;
		apiEnd(vm, 0);
	}
	if (msec < 0) {
		jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		longjmp(apiWork.setjmpErr, 1);
	}
//	apiWork.autoSleep = 0; // rev2ではこれをやらない.
		// すぐに終了したければ api0008_exit() を使う.
	if ((mod & 2) == 0 && vram != NULL)
		drv_flshWin(v_xsiz, v_ysiz, 0, 0);
	for (;;) {
		if (apiWork.winClosed != 0)
			apiEnd(vm, 1); // ユーザによる中断.
		drv_sleep(msec);
		if ((mod & 1) == 0 || keybuf_c > 0) break;
	}
	return;
}

void api000d_inkey(OsecpuVm *vm)
{
	int mod = execStep_getRxx(vm, 0x31, 16);
	//  1:get(0)/peek(1)
	//  2:window(0)/stdin(1)
	//	4: shift, lock系を有効化.
	//	8: 左右のshift系を区別する.
	vm->bit[0x30] = 32;
	vm->r[0x30] = -1;
	if (2 <= mod && mod <= 3) {
		vm->r[0x30] = fgetc(stdin);
		if (vm->r[0x30] == EOF)
			vm->r[0x30] = -1;
		if (mod == 3 && vm->r[0x30] != -1)
			ungetc(vm->r[0x30], stdin);
		goto fin;
	}
	if ((mod & 2) != 0) {
		jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		longjmp(apiWork.setjmpErr, 1);
	}
	if (keybuf_c > 0) {
		vm->r[0x30] = keybuf[keybuf_r];
		if ((mod & 4) == 0) vm->r[0x30] &= 0x3e3effff;
		if ((mod & 8) == 0) vm->r[0x30] |= (vm->r[0x30] >> 8) & 0xff0000;
		if ((mod & 1) == 0) {
			keybuf_c--;
			keybuf_r = (keybuf_r + 1) & (KEYBUFSIZ - 1);
		}
	}
	vm->bit[0x31] = vm->bit[0x32] = 32;
	vm->r[0x31] = vm->r[0x32] = 0;
	if (vm->r[0x30] == 4132) vm->r[0x31]--;
	if (vm->r[0x30] == 4133) vm->r[0x32]--;
	if (vm->r[0x30] == 4134) vm->r[0x31]++;
	if (vm->r[0x30] == 4135) vm->r[0x32]++;
fin:
	return;
}

void api0010_openWin(OsecpuVm *vm)
{
	int i, c, x, y;
	c = apiLoadColor(vm, 0x32);
	x = execStep_getRxx(vm, 0x33, 16);
	y = execStep_getRxx(vm, 0x34, 16);
	if (y == 0 && x > 0) y = x;
	if (vram == 0) {
		if (x <= 0 || 4096 < x || y < 0 || 4096 < y)
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		if (vm->errorCode > 0) goto fin;
		v_xsiz = x;
		v_ysiz = y;
		vram = malloc(x * y * sizeof (int));
		drv_openWin(x, y, (void *) vram, &apiWork.winClosed);
		apiWork.autoSleep = 1;
	} else {
		if (x != v_xsiz || y != v_ysiz)
			jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
	}
	for (i = 0; i < x * y; i++)
		vram[i] = c;
fin:
	return;
}

void api0013_rand(OsecpuVm *vm)
{
	int range = execStep_getRxx(vm, 0x31, 32);
	vm->bit[0x30] = 32;
	vm->r[0x30] = (apiXorShift() & 0x7fffffff) % (range + 1);
	return;
}

void api07c0_fileRead(OsecpuVm *vm)
{
	int i, fsiz;
	FILE *fp;
	unsigned char *fbuf = (unsigned char *) malloc(8 * 1024 * 1024 + 1); // 8MB
	unsigned char *bit;
	i = execStep_getRxx(vm, 0x31, 16);

	if (i <= 0) {
err:
		jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		longjmp(apiWork.setjmpErr, 1);
	}
	if (i >= apiWork.argc) {
		fprintf(stderr, "api07c0_fileRead: need more file-path: i=%d, argc=%d\n", i, apiWork.argc);
		goto err;
	}
	fp = fopen(apiWork.argv[i], "rb");
	if (fp == NULL) {
		fprintf(stderr, "api07c0_fileRead: fopen error: %s\n", apiWork.argv[i]);
		goto err;
	}
	fsiz = fread(fbuf, 1, 8 * 1024 * 1024 + 1, fp);
	fclose(fp);
	if (fsiz > 8 * 1024 * 1024) {
		fprintf(stderr, "api07c0_fileRead: too large file (max:8MB): %s\n", apiWork.argv[i]);
		goto err;
	}
	bit = (unsigned char *) malloc(fsiz);
	for (i = 0; i < fsiz; i++)
		bit[i] = 8;

	vm->bit[0x30] = 32;
	vm->r[0x30] = fsiz;
	vm->p[0x31].p = fbuf;
	vm->p[0x31].p0 = fbuf;
	vm->p[0x31].p1 = fbuf + fsiz;
	vm->p[0x31].typ = 3; // T_UINT8
	vm->p[0x31].flags = EXEC_CMA_FLAG_READ; // over-seek:ok, read:ok, write:err
	vm->p[0x31].bit = bit;
	return;
}

void api07c1_fileWrite(OsecpuVm *vm)
{
	int i, fsiz;
	FILE *fp;
	i = execStep_getRxx(vm, 0x31, 16);
	fsiz = execStep_getRxx(vm, 0x32, 32);
	if (i <= 0) {
err:
		jitcSetRetCode(&vm->errorCode, EXEC_API_ERROR);
		longjmp(apiWork.setjmpErr, 1);
	}
	if (i >= apiWork.argc) {
		fprintf(stderr, "api07c1_fileWrite: need more file-path: i=%d, argc=%d\n", i, apiWork.argc);
		goto err;
	}
	if (fsiz < 0) goto err;
	if (vm->p[0x31].p + fsiz > vm->p[0x31].p1) goto err;
	if (vm->p[0x31].p < vm->p[0x31].p0) goto err;
	fp = fopen(apiWork.argv[i], "wb");
	if (fp == NULL) {
		fprintf(stderr, "api07c1_fileWrite: fopen error: %s\n", apiWork.argv[i]);
		goto err;
	}
	fwrite(vm->p[0x31].p, 1, fsiz, fp);
	fclose(fp);
	return;
}

