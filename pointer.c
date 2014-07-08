#include "osecpu-vm.h"

// ポインタ関連命令: 01, 03, 2E

void jitcInitPointer(OsecpuJitc *jitc)
{
	return;
}

void jitcStep_checkPxx(int *pRC, int pxx)
{
	if (!(0x00 <= pxx && pxx <= 0x3f))
		jitcSetRetCode(pRC, JITC_BAD_PXX);
	return;
}

void getTypSize(int typ, int *typSize0, int *typSize1, int *typSign)
// typSize0: 入力バイナリ内でのビット数.
// typSize1: 出力バイナリ内でのバイト数.
{
	*typSize0 = *typSize1 = -1;
	if (2 <= typ && typ <= 21) {
		static unsigned char table[10] = { 8, 16, 32, 4, 2, 1, 12, 20, 24, 28 };
		int bytes;
		if ((typ & 1) == 0)
			*typSign = -1; // typが偶数なら符号あり.
		else
			*typSign = 0; // typが奇数なら符号なし.
		*typSize0 = table[(typ - 2) / 2];
		bytes = (*typSize0 + 7) / 8;
		if (bytes == 3) bytes = 4;
		*typSize1 = bytes;
	}
	return;
}

int jitcStepPointer(OsecpuJitc *jitc)
{
	Int32 *ip = jitc->hh4Buffer;
	Int32 opecode = ip[0], imm;
	int retcode = -1, *pRC = &retcode;
	int i, j, opt, p, typ, len, typSign, typSize0, typSize1;
	if (opecode == 0x01) { /* LB(opt, uimm); */
		jitcSetHh4BufferSimple(jitc, 3);
		i = ip[1]; opt = ip[2];
		if (jitc->phase > 0) goto fin;
		if (!(0 <= i && i < DEFINES_MAXLABELS)) {
			jitcSetRetCode(pRC, JITC_BAD_LABEL);
			goto fin;
		}
		if (jitc->defines->label[i].typ != 0) {
			jitcSetRetCode(pRC, JITC_LABEL_REDEFINED);
			goto fin;
		}
		if (!(0 <= opt && opt <= 1)) {
			jitcSetRetCode(pRC, JITC_BAD_LABEL_TYPE);
			goto fin;
		}
		jitc->defines->label[i].typ = -1; // とりあえずコードラベルにする.
		jitc->defines->label[i].opt = opt;
		jitc->defines->label[i].dst = jitc->dst;
		goto fin;
	}
	if (opecode == 0x03) { /* PLIMM(imm, Pxx); */
		jitcSetHh4BufferSimple(jitc, 3);
		i = ip[1]; p = ip[2];
		if (!(0 <= i && i < DEFINES_MAXLABELS)) {
			jitcSetRetCode(pRC, JITC_BAD_LABEL);
			goto fin;
		}
		jitcStep_checkPxx(pRC, p);
 		if (jitc->phase == 0) goto fin;
		if (jitc->defines->label[i].typ == 0)
			jitcSetRetCode(pRC, JITC_LABEL_UNDEFINED);
		if (p != 0x3f && jitc->defines->label[i].opt == 0)
			jitcSetRetCode(pRC, JITC_BAD_LABEL_TYPE);
		if (p == 0x3f && jitc->defines->label[i].typ != -1)
			jitcSetRetCode(pRC, JITC_BAD_LABEL_TYPE); // P3Fにデータラベルを代入できない.
		goto fin;
	}
	if (opecode == 0x2e) { // data
		BitReader br;
		typ = hh4ReaderGetUnsigned(&jitc->hh4r);
		len = hh4ReaderGetUnsigned(&jitc->hh4r);
		getTypSize(typ, &typSize0, &typSize1, &typSign);
		if (typSize0 < 0) {
			jitcSetRetCode(pRC, JITC_BAD_TYPE);
			goto fin;
		}
		jitc->instrLength = 0; // 自前で処理するので、この値は0にする.
		if (jitc->dst + 3 + (typSize1 * len + 3) / 4 > jitc->dst1) {
			jitcSetRetCode(pRC, JITC_DST_OVERRUN);
			goto fin;
		}
		ip = jitc->dst;
		ip[0] = 0x2e;
		ip[1] = typ;
		ip[2] = len;
		jitc->dst += 3;
		if (typ != 1) {
			bitReaderInit(&br, &jitc->hh4r);
			// 以下は構造体サポートに配慮できてない.
			if (typSize1 == 1 && typSign == 0) {
				unsigned char *puc = (unsigned char *) jitc->dst;
				for (i = 0; i < len; i++)
					puc[i] = bitReaderGetNbitUnsigned(&br, typSize0);
			}
			if (typSize1 == 1 && typSign != 0) {
				signed char *psc = (signed char *) jitc->dst;
				for (i = 0; i < len; i++)
					psc[i] = bitReaderGetNbitSigned(&br, typSize0);
			}
			if (typSize1 == 2 && typSign == 0) {
				unsigned short *pus = (unsigned short *) jitc->dst;
				for (i = 0; i < len; i++)
					pus[i] = bitReaderGetNbitUnsigned(&br, typSize0);
			}
			if (typSize1 == 2 && typSign != 0) {
				signed short *pss = (signed short *) jitc->dst;
				for (i = 0; i < len; i++)
					pss[i] = bitReaderGetNbitSigned(&br, typSize0);
			}
			if (typSize1 == 4 && typSign == 0) {
				unsigned int *pui = (unsigned int *) jitc->dst;
				for (i = 0; i < len; i++)
					pui[i] = bitReaderGetNbitUnsigned(&br, typSize0);
			}
			if (typSize1 == 4 && typSign != 0) {
				signed int *psi = (signed int *) jitc->dst;
				for (i = 0; i < len; i++)
					psi[i] = bitReaderGetNbitSigned(&br, typSize0);
			}
			jitc->dst += (typSize1 * len + 3) / 4;
			unsigned char *puc = (unsigned char *) jitc->dst;
			for (i = 0; i < len; i++)
				puc[i] = 0xff; // bits=255.
			jitc->dst += (len + 3) / 4;
		}
		for (j = 0; j < JITC_DSTLOG_SIZE; j++) {
			Int32 *dstLog = jitc->dstLog[jitc->dstLogIndex + JITC_DSTLOG_SIZE - 1 - j];
			if (dstLog == NULL) break;
			if (dstLog[0] != 0x01) break;
			i = dstLog[1];
			jitc->defines->label[i].typ = typ;
			jitc->defines->label[i].dst = ip;
		}
		goto fin;

	}
	goto fin1;
fin:
	if (retcode == -1)
		retcode = 0;
fin1:
	return retcode;
}

void jitcAfterStepPointer(OsecpuJitc *jitc)
{
	return;
}

void execStepPointer(OsecpuVm *vm)
{
	const Int32 *ip = vm->ip;
	Int32 opecode = ip[0];
	int i, p, typ, len, typSign, typSize0, typSize1;
	if (opecode == 0x01) { /* LB(opt, uimm); */
		ip += 3;
		goto fin;
	}
	if (opecode == 0x03) { /* PLIMM(imm, Pxx); */
		i = ip[1]; p = ip[2];
		if (p == 0x3f)
			ip = (const Int32 *) vm->defines->label[i].dst;
		else {
			typ = vm->defines->label[i].typ;
			vm->p[p].p = (unsigned char *) vm->defines->label[i].dst;
			vm->p[p].typ = typ;
			vm->p[p].p0 = vm->p[p].p;
			if (typ >= 2) {
				len = vm->defines->label[i].dst[2]; // 2e(data)のlenフィールド値.
				getTypSize(typ, NULL, &typSize1, NULL);
				vm->p[p].p1 = vm->p[p].p + typSize1 * len;
				vm->p[p].bit = vm->p[p].p + ((typSize1 * len + 3) / 4) * 4;
			}
			if (typ == -1) {	// コードラベル.
				vm->p[p].p1 = vm->p[p].p + 1;
			}
			if (typ == 1) {		// VPtr.
			}
			ip += 3;
		}
		goto fin;
	}
	if (opecode == 0x2e) {	// data
		typ = ip[1]; len = ip[2];
		getTypSize(typ, &typSize0, &typSize1, &typSign);
		ip += 3 + (typSize1 * len + 3) / 4 + (len + 3) / 4;
		goto fin;
	}
fin:
	vm->ip = ip;
	return;
}

