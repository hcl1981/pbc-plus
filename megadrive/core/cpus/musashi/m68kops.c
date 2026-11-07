/* m68kops.c -- written by tools/m68k_gen.py from m68k_in.c; do not edit.
 *
 * Musashi 68000 opcode handlers (MIT licence, see readme.txt) as the
 * 68000 needs them, and the packed dispatch table (see m68k_gen.py). */

#include "m68kcpu.h"

/* PBC: the most used handlers run from RAM (tools/m68k_hot.py) */
#define M68K_HOT1 __attribute__((section(".time_critical.m68k_hot")))
#if PBC_MULTI
#define M68K_HOT2 M68K_HOT1
#else
#define M68K_HOT2
#endif

static void m68k_op_1010(void)
{
	m68ki_exception_1010();
}


static void m68k_op_1111(void)
{
	m68ki_exception_1111();
}


static void m68k_op_abcd_8_rr(void)
{
	uint* r_dst = &DX;
	uint src = DY;
	uint dst = *r_dst;
	uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res += 6;
	res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res -= 0xa0;

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_abcd_8_mm_ax7(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res += 6;
	res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res -= 0xa0;

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_abcd_8_mm_ay7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res += 6;
	res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res -= 0xa0;

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_abcd_8_mm_axy7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res += 6;
	res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res -= 0xa0;

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_abcd_8_mm(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res += 6;
	res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res -= 0xa0;

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static M68K_HOT1 void m68k_op_add_8_er_d(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_8(DY);
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_8_er_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_8_er_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pi7(void)
{
	uint* r_dst = &DX;
	uint src = OPER_A7_PI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_8_er_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pd7(void)
{
	uint* r_dst = &DX;
	uint src = OPER_A7_PD_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_add_8_er_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_add_8_er_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_16_er_d(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(DY);
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_16_er_a(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(AY);
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_16_er_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_16_er_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_16_er_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_add_16_er_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_add_16_er_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_32_er_d(void)
{
	uint* r_dst = &DX;
	uint src = DY;
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_32_er_a(void)
{
	uint* r_dst = &DX;
	uint src = AY;
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_32_er_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_32_er_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_32_er_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT2 void m68k_op_add_32_er_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT2 void m68k_op_add_32_er_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_add_8_re_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_8_re_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_8_re_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_add_8_re_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_aw(void)
{
	uint ea = EA_AW_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_al(void)
{
	uint ea = EA_AL_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_16_re_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_16_re_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_16_re_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_add_16_re_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_aw(void)
{
	uint ea = EA_AW_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_al(void)
{
	uint ea = EA_AL_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_32_re_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_32_re_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_add_32_re_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_add_32_re_di(void)
{
	uint ea = EA_AY_DI_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_aw(void)
{
	uint ea = EA_AW_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_al(void)
{
	uint ea = EA_AL_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_adda_16_d(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + MAKE_INT_16(DY));
}


static M68K_HOT1 void m68k_op_adda_16_a(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + MAKE_INT_16(AY));
}


static M68K_HOT2 void m68k_op_adda_16_ai(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_AI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static M68K_HOT2 void m68k_op_adda_16_pi(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_PI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static M68K_HOT2 void m68k_op_adda_16_pd(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_PD_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_di(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_DI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_ix(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_IX_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_aw(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AW_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_al(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AL_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_pcdi(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_PCDI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_pcix(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_PCIX_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_i(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_I_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static M68K_HOT1 void m68k_op_adda_32_d(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + DY);
}


static M68K_HOT1 void m68k_op_adda_32_a(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + AY);
}


static M68K_HOT2 void m68k_op_adda_32_ai(void)
{
	uint src = OPER_AY_AI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static M68K_HOT2 void m68k_op_adda_32_pi(void)
{
	uint src = OPER_AY_PI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static M68K_HOT2 void m68k_op_adda_32_pd(void)
{
	uint src = OPER_AY_PD_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_di(void)
{
	uint src = OPER_AY_DI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_ix(void)
{
	uint src = OPER_AY_IX_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_aw(void)
{
	uint src = OPER_AW_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_al(void)
{
	uint src = OPER_AL_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_pcdi(void)
{
	uint src = OPER_PCDI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_pcix(void)
{
	uint src = OPER_PCIX_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_i(void)
{
	uint src = OPER_I_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_addi_8_d(void)
{
	uint* r_dst = &DY;
	uint src = OPER_I_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_addi_8_ai(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_AI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pi(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pi7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pd(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pd7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_di(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_DI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_ix(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_IX_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_aw(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AW_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_al(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AL_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_16_d(void)
{
	uint* r_dst = &DY;
	uint src = OPER_I_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_addi_16_ai(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_pi(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PI_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_pd(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PD_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_di(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_ix(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_aw(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AW_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_al(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AL_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_32_d(void)
{
	uint* r_dst = &DY;
	uint src = OPER_I_32();
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_addi_32_ai(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_AI_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_pi(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PI_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_pd(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PD_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_di(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_DI_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_ix(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_IX_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_aw(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AW_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_al(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AL_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_8_d(void)
{
	uint* r_dst = &DY;
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_addq_8_ai(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_AI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_8_pi(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_pi7(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_A7_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_8_pd(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_pd7(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_8_di(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_DI_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_ix(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_IX_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_addq_8_aw(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AW_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_addq_8_al(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AL_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_16_d(void)
{
	uint* r_dst = &DY;
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_addq_16_a(void)
{
	uint* r_dst = &AY;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + (((REG_IR >> 9) - 1) & 7) + 1);
}


static M68K_HOT1 void m68k_op_addq_16_ai(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_AI_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_16_pi(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PI_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_16_pd(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PD_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_16_di(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_DI_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_ix(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_IX_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_addq_16_aw(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AW_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_addq_16_al(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AL_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst;

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_32_d(void)
{
	uint* r_dst = &DY;
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint dst = *r_dst;
	uint res = src + dst;

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_addq_32_a(void)
{
	uint* r_dst = &AY;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst + (((REG_IR >> 9) - 1) & 7) + 1);
}


static M68K_HOT1 void m68k_op_addq_32_ai(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_AI_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_32_pi(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PI_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_32_pd(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PD_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_addq_32_di(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_DI_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_ix(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_IX_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_addq_32_aw(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AW_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_addq_32_al(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AL_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst;


	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addx_8_rr(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_8(DY);
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_addx_16_rr(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(DY);
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
}


static void m68k_op_addx_32_rr(void)
{
	uint* r_dst = &DX;
	uint src = DY;
	uint dst = *r_dst;
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	*r_dst = res;
}


static void m68k_op_addx_8_mm_ax7(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_addx_8_mm_ay7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_addx_8_mm_axy7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_addx_8_mm(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_V = VFLAG_ADD_8(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_8(res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_addx_16_mm(void)
{
	uint src = OPER_AY_PD_16();
	uint ea  = EA_AX_PD_16();
	uint dst = m68ki_read_16(ea);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_V = VFLAG_ADD_16(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_16(res);

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_addx_32_mm(void)
{
	uint src = OPER_AY_PD_32();
	uint ea  = EA_AX_PD_32();
	uint dst = m68ki_read_32(ea);
	uint res = src + dst + XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_ADD_32(src, dst, res);
	FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_and_8_er_d(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (DY | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_8_er_ai(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_AI_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pi(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_PI_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pi7(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_A7_PI_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pd(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_PD_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pd7(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_A7_PD_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_di(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_DI_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_ix(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_IX_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_aw(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AW_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_al(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AL_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pcdi(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_PCDI_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pcix(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_PCIX_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_i(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_I_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_and_16_er_d(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (DY | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_16_er_ai(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_AI_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_16_er_pi(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_PI_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_16_er_pd(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_PD_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_di(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_DI_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_ix(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_IX_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_aw(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AW_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_al(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AL_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_pcdi(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_PCDI_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_pcix(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_PCIX_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_i(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_I_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_and_32_er_d(void)
{
	FLAG_Z = DX &= DY;

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_32_er_ai(void)
{
	FLAG_Z = DX &= OPER_AY_AI_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_32_er_pi(void)
{
	FLAG_Z = DX &= OPER_AY_PI_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_and_32_er_pd(void)
{
	FLAG_Z = DX &= OPER_AY_PD_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_di(void)
{
	FLAG_Z = DX &= OPER_AY_DI_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_ix(void)
{
	FLAG_Z = DX &= OPER_AY_IX_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_aw(void)
{
	FLAG_Z = DX &= OPER_AW_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_al(void)
{
	FLAG_Z = DX &= OPER_AL_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_pcdi(void)
{
	FLAG_Z = DX &= OPER_PCDI_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_pcix(void)
{
	FLAG_Z = DX &= OPER_PCIX_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_i(void)
{
	FLAG_Z = DX &= OPER_I_32();

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_re_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_di(void)
{
	uint ea = EA_AY_DI_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_aw(void)
{
	uint ea = EA_AW_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_al(void)
{
	uint ea = EA_AL_8();
	uint res = DX & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_and_16_re_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_and_16_re_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_and_16_re_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_di(void)
{
	uint ea = EA_AY_DI_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_aw(void)
{
	uint ea = EA_AW_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_al(void)
{
	uint ea = EA_AL_16();
	uint res = DX & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_and_32_re_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT2 void m68k_op_and_32_re_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT2 void m68k_op_and_32_re_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_di(void)
{
	uint ea = EA_AY_DI_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_aw(void)
{
	uint ea = EA_AW_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_al(void)
{
	uint ea = EA_AL_32();
	uint res = DX & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_andi_8_d(void)
{
	FLAG_Z = MASK_OUT_ABOVE_8(DY &= (OPER_I_8() | 0xffffff00));

	FLAG_N = NFLAG_8(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_andi_8_ai(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_AI_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static M68K_HOT1 void m68k_op_andi_8_pi(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PI_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_pi7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PI_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static M68K_HOT1 void m68k_op_andi_8_pd(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PD_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_pd7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PD_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static M68K_HOT2 void m68k_op_andi_8_di(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_DI_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_ix(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_IX_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_aw(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AW_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_al(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AL_8();
	uint res = src & m68ki_read_8(ea);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_8(ea, res);
}


static M68K_HOT1 void m68k_op_andi_16_d(void)
{
	FLAG_Z = MASK_OUT_ABOVE_16(DY &= (OPER_I_16() | 0xffff0000));

	FLAG_N = NFLAG_16(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_andi_16_ai(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static M68K_HOT1 void m68k_op_andi_16_pi(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PI_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static M68K_HOT1 void m68k_op_andi_16_pd(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PD_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static M68K_HOT2 void m68k_op_andi_16_di(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_ix(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_aw(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AW_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_al(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AL_16();
	uint res = src & m68ki_read_16(ea);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_16(ea, res);
}


static M68K_HOT1 void m68k_op_andi_32_d(void)
{
	FLAG_Z = DY &= (OPER_I_32());

	FLAG_N = NFLAG_32(FLAG_Z);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_andi_32_ai(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_AI_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_andi_32_pi(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PI_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_andi_32_pd(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PD_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT2 void m68k_op_andi_32_di(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_DI_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_ix(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_IX_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_aw(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AW_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_al(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AL_32();
	uint res = src & m68ki_read_32(ea);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_andi_16_toc(void)
{
	m68ki_set_ccr(m68ki_get_ccr() & OPER_I_8());
}


static M68K_HOT1 void m68k_op_andi_16_tos(void)
{
	if(FLAG_S)
	{
		uint src = OPER_I_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(m68ki_get_sr() & src);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_asr_8_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src >> shift;

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	if(GET_MSB_8(src))
		res |= m68ki_shift_8_table[shift];

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_X = FLAG_C = src << (9-shift);
}


static M68K_HOT1 void m68k_op_asr_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src >> shift;

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	if(GET_MSB_16(src))
		res |= m68ki_shift_16_table[shift];

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_X = FLAG_C = src << (9-shift);
}


static M68K_HOT1 void m68k_op_asr_32_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = *r_dst;
	uint res = src >> shift;

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	if(GET_MSB_32(src))
		res |= m68ki_shift_32_table[shift];

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_X = FLAG_C = src << (9-shift);
}


static M68K_HOT1 void m68k_op_asr_8_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src >> shift;

	if(shift != 0 )
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 8)
		{
			if(GET_MSB_8(src))
				res |= m68ki_shift_8_table[shift];

			*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

			FLAG_X = FLAG_C = src << (9-shift);
			FLAG_N = NFLAG_8(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		if(GET_MSB_8(src))
		{
			*r_dst |= 0xff;
			FLAG_C = CFLAG_SET;
			FLAG_X = XFLAG_SET;
			FLAG_N = NFLAG_SET;
			FLAG_Z = ZFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst &= 0xffffff00;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_8(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_asr_16_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src >> shift;

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 16)
		{
			if(GET_MSB_16(src))
				res |= m68ki_shift_16_table[shift];

			*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

			FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
			FLAG_N = NFLAG_16(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		if(GET_MSB_16(src))
		{
			*r_dst |= 0xffff;
			FLAG_C = CFLAG_SET;
			FLAG_X = XFLAG_SET;
			FLAG_N = NFLAG_SET;
			FLAG_Z = ZFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst &= 0xffff0000;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_16(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_asr_32_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = *r_dst;
	uint res = src >> shift;

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 32)
		{
			if(GET_MSB_32(src))
				res |= m68ki_shift_32_table[shift];

			*r_dst = res;

			FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
			FLAG_N = NFLAG_32(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		if(GET_MSB_32(src))
		{
			*r_dst = 0xffffffff;
			FLAG_C = CFLAG_SET;
			FLAG_X = XFLAG_SET;
			FLAG_N = NFLAG_SET;
			FLAG_Z = ZFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst = 0;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_32(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_asr_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static M68K_HOT2 void m68k_op_asr_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static M68K_HOT2 void m68k_op_asr_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	if(GET_MSB_16(src))
		res |= 0x8000;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = FLAG_X = src << 8;
}


static M68K_HOT1 void m68k_op_asl_8_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = MASK_OUT_ABOVE_8(src << shift);

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_X = FLAG_C = src << shift;
	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	src &= m68ki_shift_8_table[shift + 1];
	FLAG_V = (!(src == 0 || (src == m68ki_shift_8_table[shift + 1] && shift < 8)))<<7;
}


static M68K_HOT1 void m68k_op_asl_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = MASK_OUT_ABOVE_16(src << shift);

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> (8-shift);
	src &= m68ki_shift_16_table[shift + 1];
	FLAG_V = (!(src == 0 || src == m68ki_shift_16_table[shift + 1]))<<7;
}


static M68K_HOT1 void m68k_op_asl_32_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32(src << shift);

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> (24-shift);
	src &= m68ki_shift_32_table[shift + 1];
	FLAG_V = (!(src == 0 || src == m68ki_shift_32_table[shift + 1]))<<7;
}


static M68K_HOT1 void m68k_op_asl_8_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = MASK_OUT_ABOVE_8(src << shift);

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 8)
		{
			*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
			FLAG_X = FLAG_C = src << shift;
			FLAG_N = NFLAG_8(res);
			FLAG_Z = res;
			src &= m68ki_shift_8_table[shift + 1];
			FLAG_V = (!(src == 0 || src == m68ki_shift_8_table[shift + 1]))<<7;
			return;
		}

		*r_dst &= 0xffffff00;
		FLAG_X = FLAG_C = ((shift == 8 ? src & 1 : 0))<<8;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = (!(src == 0))<<7;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_8(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_asl_16_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = MASK_OUT_ABOVE_16(src << shift);

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 16)
		{
			*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
			FLAG_X = FLAG_C = (src << shift) >> 8;
			FLAG_N = NFLAG_16(res);
			FLAG_Z = res;
			src &= m68ki_shift_16_table[shift + 1];
			FLAG_V = (!(src == 0 || src == m68ki_shift_16_table[shift + 1]))<<7;
			return;
		}

		*r_dst &= 0xffff0000;
		FLAG_X = FLAG_C = ((shift == 16 ? src & 1 : 0))<<8;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = (!(src == 0))<<7;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_16(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_asl_32_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32(src << shift);

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 32)
		{
			*r_dst = res;
			FLAG_X = FLAG_C = (src >> (32 - shift)) << 8;
			FLAG_N = NFLAG_32(res);
			FLAG_Z = res;
			src &= m68ki_shift_32_table[shift + 1];
			FLAG_V = (!(src == 0 || src == m68ki_shift_32_table[shift + 1]))<<7;
			return;
		}

		*r_dst = 0;
		FLAG_X = FLAG_C = ((shift == 32 ? src & 1 : 0))<<8;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = (!(src == 0))<<7;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_32(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_asl_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static M68K_HOT2 void m68k_op_asl_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static M68K_HOT2 void m68k_op_asl_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	src &= 0xc000;
	FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static M68K_HOT1 void m68k_op_bhi_8(void)
{
	if(COND_HI())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bls_8(void)
{
	if(COND_LS())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bcc_8(void)
{
	if(COND_CC())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bcs_8(void)
{
	if(COND_CS())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bne_8(void)
{
	if(COND_NE())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_beq_8(void)
{
	if(COND_EQ())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bvc_8(void)
{
	if(COND_VC())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bvs_8(void)
{
	if(COND_VS())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bpl_8(void)
{
	if(COND_PL())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bmi_8(void)
{
	if(COND_MI())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bge_8(void)
{
	if(COND_GE())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_blt_8(void)
{
	if(COND_LT())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bgt_8(void)
{
	if(COND_GT())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_ble_8(void)
{
	if(COND_LE())
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		/* PBC: back by 6 or 8 bytes, maybe a loop polling work RAM */
		if((uint8)(REG_IR + 8) <= 2)
			m68ki_poll_skip();
		return;
	}
	USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static M68K_HOT1 void m68k_op_bhi_16(void)
{
	if(COND_HI())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bls_16(void)
{
	if(COND_LS())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bcc_16(void)
{
	if(COND_CC())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bcs_16(void)
{
	if(COND_CS())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bne_16(void)
{
	if(COND_NE())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_beq_16(void)
{
	if(COND_EQ())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bvc_16(void)
{
	if(COND_VC())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bvs_16(void)
{
	if(COND_VS())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bpl_16(void)
{
	if(COND_PL())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bmi_16(void)
{
	if(COND_MI())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bge_16(void)
{
	if(COND_GE())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_blt_16(void)
{
	if(COND_LT())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bgt_16(void)
{
	if(COND_GT())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_ble_16(void)
{
	if(COND_LE())
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static M68K_HOT1 void m68k_op_bhi_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_HI())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_HI())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bls_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_LS())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_LS())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bcc_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_CC())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_CC())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bcs_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_CS())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_CS())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bne_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_NE())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_NE())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_beq_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_EQ())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_EQ())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bvc_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_VC())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_VC())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bvs_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_VS())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_VS())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bpl_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_PL())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_PL())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bmi_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_MI())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_MI())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bge_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_GE())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_GE())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_blt_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_LT())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_LT())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_bgt_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_GT())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_GT())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT1 void m68k_op_ble_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		if(COND_LE())
		{
			uint offset = OPER_I_32();
			REG_PC -= 4;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_32(offset);
			return;
		}
		REG_PC += 4;
		return;
	}
	else
	{
		if(COND_LE())
		{
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
			return;
		}
		USE_CYCLES(CYC_BCC_NOTAKE_B);
	}
}


static M68K_HOT2 void m68k_op_bchg_32_r_d(void)
{
	uint* r_dst = &DY;
	uint mask = 1 << (DX & 0x1f);

	FLAG_Z = *r_dst & mask;
	*r_dst ^= mask;
}


static void m68k_op_bchg_8_r_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_aw(void)
{
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_al(void)
{
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static M68K_HOT2 void m68k_op_bchg_32_s_d(void)
{
	uint* r_dst = &DY;
	uint mask = 1 << (OPER_I_8() & 0x1f);

	FLAG_Z = *r_dst & mask;
	*r_dst ^= mask;
}


static void m68k_op_bchg_8_s_ai(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pi(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pi7(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pd(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pd7(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_di(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_ix(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_aw(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_al(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src ^ mask);
}


static M68K_HOT2 void m68k_op_bclr_32_r_d(void)
{
	uint* r_dst = &DY;
	uint mask = 1 << (DX & 0x1f);

	FLAG_Z = *r_dst & mask;
	*r_dst &= ~mask;
}


static void m68k_op_bclr_8_r_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_aw(void)
{
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_al(void)
{
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static M68K_HOT2 void m68k_op_bclr_32_s_d(void)
{
	uint* r_dst = &DY;
	uint mask = 1 << (OPER_I_8() & 0x1f);

	FLAG_Z = *r_dst & mask;
	*r_dst &= ~mask;
}


static void m68k_op_bclr_8_s_ai(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pi(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pi7(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pd(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pd7(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_di(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_ix(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_aw(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_al(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src & ~mask);
}


static M68K_HOT1 void m68k_op_bra_8(void)
{
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static M68K_HOT1 void m68k_op_bra_16(void)
{
	uint offset = OPER_I_16();
	REG_PC -= 2;
	m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
	m68ki_branch_16(offset);
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static M68K_HOT1 void m68k_op_bra_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		uint offset = OPER_I_32();
		REG_PC -= 4;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_32(offset);
		if(REG_PC == REG_PPC)
			USE_ALL_CYCLES();
		return;
	}
	else
	{
		m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
		if(REG_PC == REG_PPC)
			USE_ALL_CYCLES();
	}
}


static M68K_HOT2 void m68k_op_bset_32_r_d(void)
{
	uint* r_dst = &DY;
	uint mask = 1 << (DX & 0x1f);

	FLAG_Z = *r_dst & mask;
	*r_dst |= mask;
}


static void m68k_op_bset_8_r_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_aw(void)
{
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_al(void)
{
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);
	uint mask = 1 << (DX & 7);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static M68K_HOT2 void m68k_op_bset_32_s_d(void)
{
	uint* r_dst = &DY;
	uint mask = 1 << (OPER_I_8() & 0x1f);

	FLAG_Z = *r_dst & mask;
	*r_dst |= mask;
}


static void m68k_op_bset_8_s_ai(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pi(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pi7(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pd(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pd7(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_di(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_ix(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_aw(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_al(void)
{
	uint mask = 1 << (OPER_I_8() & 7);
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);

	FLAG_Z = src & mask;
	m68ki_write_8(ea, src | mask);
}


static M68K_HOT1 void m68k_op_bsr_8(void)
{
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
}


static M68K_HOT1 void m68k_op_bsr_16(void)
{
	uint offset = OPER_I_16();
	m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	REG_PC -= 2;
	m68ki_branch_16(offset);
}


static M68K_HOT1 void m68k_op_bsr_32(void)
{
	if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		uint offset = OPER_I_32();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_push_32(REG_PC);
		REG_PC -= 4;
		m68ki_branch_32(offset);
		return;
	}
	else
	{
		m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
		m68ki_push_32(REG_PC);
		m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
	}
}


static M68K_HOT1 void m68k_op_btst_32_r_d(void)
{
	FLAG_Z = DY & (1 << (DX & 0x1f));
}


static M68K_HOT1 void m68k_op_btst_8_r_ai(void)
{
	FLAG_Z = OPER_AY_AI_8() & (1 << (DX & 7));
}


static M68K_HOT1 void m68k_op_btst_8_r_pi(void)
{
	FLAG_Z = OPER_AY_PI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pi7(void)
{
	FLAG_Z = OPER_A7_PI_8() & (1 << (DX & 7));
}


static M68K_HOT1 void m68k_op_btst_8_r_pd(void)
{
	FLAG_Z = OPER_AY_PD_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pd7(void)
{
	FLAG_Z = OPER_A7_PD_8() & (1 << (DX & 7));
}


static M68K_HOT1 void m68k_op_btst_8_r_di(void)
{
	FLAG_Z = OPER_AY_DI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_ix(void)
{
	FLAG_Z = OPER_AY_IX_8() & (1 << (DX & 7));
}


static M68K_HOT2 void m68k_op_btst_8_r_aw(void)
{
	FLAG_Z = OPER_AW_8() & (1 << (DX & 7));
}


static M68K_HOT2 void m68k_op_btst_8_r_al(void)
{
	FLAG_Z = OPER_AL_8() & (1 << (DX & 7));
}


static M68K_HOT2 void m68k_op_btst_8_r_pcdi(void)
{
	FLAG_Z = OPER_PCDI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pcix(void)
{
	FLAG_Z = OPER_PCIX_8() & (1 << (DX & 7));
}


static M68K_HOT1 void m68k_op_btst_8_r_i(void)
{
	FLAG_Z = OPER_I_8() & (1 << (DX & 7));
}


static M68K_HOT1 void m68k_op_btst_32_s_d(void)
{
	FLAG_Z = DY & (1 << (OPER_I_8() & 0x1f));
}


static M68K_HOT1 void m68k_op_btst_8_s_ai(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AY_AI_8() & (1 << bit);
}


static M68K_HOT1 void m68k_op_btst_8_s_pi(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AY_PI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pi7(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_A7_PI_8() & (1 << bit);
}


static M68K_HOT1 void m68k_op_btst_8_s_pd(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AY_PD_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pd7(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_A7_PD_8() & (1 << bit);
}


static M68K_HOT1 void m68k_op_btst_8_s_di(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AY_DI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_ix(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AY_IX_8() & (1 << bit);
}


static M68K_HOT2 void m68k_op_btst_8_s_aw(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AW_8() & (1 << bit);
}


static M68K_HOT2 void m68k_op_btst_8_s_al(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_AL_8() & (1 << bit);
}


static M68K_HOT2 void m68k_op_btst_8_s_pcdi(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_PCDI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pcix(void)
{
	uint bit = OPER_I_8() & 7;

	FLAG_Z = OPER_PCIX_8() & (1 << bit);
}


static void m68k_op_chk_16_d(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(DY);

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_ai(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AY_AI_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pi(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AY_PI_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pd(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AY_PD_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_di(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AY_DI_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_ix(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AY_IX_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_aw(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AW_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_al(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_AL_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pcdi(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_PCDI_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pcix(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_PCIX_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_i(void)
{
	sint src = MAKE_INT_16(DX);
	sint bound = MAKE_INT_16(OPER_I_16());

	FLAG_Z = ZFLAG_16(src); /* Undocumented */
	FLAG_V = VFLAG_CLEAR;   /* Undocumented */
	FLAG_C = CFLAG_CLEAR;   /* Undocumented */

	if(src >= 0 && src <= bound)
	{
		return;
	}
	FLAG_N = (src < 0)<<7;
	m68ki_exception_trap(EXCEPTION_CHK);
}


static M68K_HOT1 void m68k_op_clr_8_d(void)
{
	DY &= 0xffffff00;

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT2 void m68k_op_clr_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_al(void)
{
	m68ki_write_8(EA_AL_8(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_16_d(void)
{
	DY &= 0xffff0000;

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_16_ai(void)
{
	m68ki_write_16(EA_AY_AI_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_16_pi(void)
{
	m68ki_write_16(EA_AY_PI_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_16_pd(void)
{
	m68ki_write_16(EA_AY_PD_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT2 void m68k_op_clr_16_di(void)
{
	m68ki_write_16(EA_AY_DI_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_ix(void)
{
	m68ki_write_16(EA_AY_IX_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_aw(void)
{
	m68ki_write_16(EA_AW_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_al(void)
{
	m68ki_write_16(EA_AL_16(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_32_d(void)
{
	DY = 0;

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_32_ai(void)
{
	m68ki_write_32(EA_AY_AI_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_32_pi(void)
{
	m68ki_write_32(EA_AY_PI_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_clr_32_pd(void)
{
	m68ki_write_32(EA_AY_PD_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT2 void m68k_op_clr_32_di(void)
{
	m68ki_write_32(EA_AY_DI_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_ix(void)
{
	m68ki_write_32(EA_AY_IX_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_aw(void)
{
	m68ki_write_32(EA_AW_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_al(void)
{
	m68ki_write_32(EA_AL_32(), 0);

	FLAG_N = NFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	FLAG_Z = ZFLAG_SET;
}


static M68K_HOT1 void m68k_op_cmp_8_d(void)
{
	uint src = MASK_OUT_ABOVE_8(DY);
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmp_8_ai(void)
{
	uint src = OPER_AY_AI_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmp_8_pi(void)
{
	uint src = OPER_AY_PI_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pi7(void)
{
	uint src = OPER_A7_PI_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmp_8_pd(void)
{
	uint src = OPER_AY_PD_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pd7(void)
{
	uint src = OPER_A7_PD_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmp_8_di(void)
{
	uint src = OPER_AY_DI_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_ix(void)
{
	uint src = OPER_AY_IX_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT2 void m68k_op_cmp_8_aw(void)
{
	uint src = OPER_AW_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT2 void m68k_op_cmp_8_al(void)
{
	uint src = OPER_AL_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT2 void m68k_op_cmp_8_pcdi(void)
{
	uint src = OPER_PCDI_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pcix(void)
{
	uint src = OPER_PCIX_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmp_8_i(void)
{
	uint src = OPER_I_8();
	uint dst = MASK_OUT_ABOVE_8(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmp_16_d(void)
{
	uint src = MASK_OUT_ABOVE_16(DY);
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_16_a(void)
{
	uint src = MASK_OUT_ABOVE_16(AY);
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_16_ai(void)
{
	uint src = OPER_AY_AI_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_16_pi(void)
{
	uint src = OPER_AY_PI_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_16_pd(void)
{
	uint src = OPER_AY_PD_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_16_di(void)
{
	uint src = OPER_AY_DI_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_ix(void)
{
	uint src = OPER_AY_IX_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT2 void m68k_op_cmp_16_aw(void)
{
	uint src = OPER_AW_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT2 void m68k_op_cmp_16_al(void)
{
	uint src = OPER_AL_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT2 void m68k_op_cmp_16_pcdi(void)
{
	uint src = OPER_PCDI_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_pcix(void)
{
	uint src = OPER_PCIX_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_16_i(void)
{
	uint src = OPER_I_16();
	uint dst = MASK_OUT_ABOVE_16(DX);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmp_32_d(void)
{
	uint src = DY;
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmp_32_a(void)
{
	uint src = AY;
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmp_32_ai(void)
{
	uint src = OPER_AY_AI_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmp_32_pi(void)
{
	uint src = OPER_AY_PI_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmp_32_pd(void)
{
	uint src = OPER_AY_PD_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmp_32_di(void)
{
	uint src = OPER_AY_DI_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_ix(void)
{
	uint src = OPER_AY_IX_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmp_32_aw(void)
{
	uint src = OPER_AW_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmp_32_al(void)
{
	uint src = OPER_AL_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmp_32_pcdi(void)
{
	uint src = OPER_PCDI_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_pcix(void)
{
	uint src = OPER_PCIX_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmp_32_i(void)
{
	uint src = OPER_I_32();
	uint dst = DX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpa_16_d(void)
{
	uint src = MAKE_INT_16(DY);
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpa_16_a(void)
{
	uint src = MAKE_INT_16(AY);
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpa_16_ai(void)
{
	uint src = MAKE_INT_16(OPER_AY_AI_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpa_16_pi(void)
{
	uint src = MAKE_INT_16(OPER_AY_PI_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpa_16_pd(void)
{
	uint src = MAKE_INT_16(OPER_AY_PD_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_di(void)
{
	uint src = MAKE_INT_16(OPER_AY_DI_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_ix(void)
{
	uint src = MAKE_INT_16(OPER_AY_IX_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_aw(void)
{
	uint src = MAKE_INT_16(OPER_AW_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_al(void)
{
	uint src = MAKE_INT_16(OPER_AL_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_pcdi(void)
{
	uint src = MAKE_INT_16(OPER_PCDI_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_pcix(void)
{
	uint src = MAKE_INT_16(OPER_PCIX_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_i(void)
{
	uint src = MAKE_INT_16(OPER_I_16());
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpa_32_d(void)
{
	uint src = DY;
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpa_32_a(void)
{
	uint src = AY;
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpa_32_ai(void)
{
	uint src = OPER_AY_AI_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpa_32_pi(void)
{
	uint src = OPER_AY_PI_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpa_32_pd(void)
{
	uint src = OPER_AY_PD_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_di(void)
{
	uint src = OPER_AY_DI_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_ix(void)
{
	uint src = OPER_AY_IX_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_aw(void)
{
	uint src = OPER_AW_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_al(void)
{
	uint src = OPER_AL_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_pcdi(void)
{
	uint src = OPER_PCDI_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_pcix(void)
{
	uint src = OPER_PCIX_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_i(void)
{
	uint src = OPER_I_32();
	uint dst = AX;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpi_8_d(void)
{
	uint src = OPER_I_8();
	uint dst = MASK_OUT_ABOVE_8(DY);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmpi_8_ai(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AY_AI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmpi_8_pi(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AY_PI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_pi7(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_A7_PI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmpi_8_pd(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AY_PD_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_pd7(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_A7_PD_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmpi_8_di(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AY_DI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_ix(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AY_IX_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT2 void m68k_op_cmpi_8_aw(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AW_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT2 void m68k_op_cmpi_8_al(void)
{
	uint src = OPER_I_8();
	uint dst = OPER_AL_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static M68K_HOT1 void m68k_op_cmpi_16_d(void)
{
	uint src = OPER_I_16();
	uint dst = MASK_OUT_ABOVE_16(DY);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmpi_16_ai(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AY_AI_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmpi_16_pi(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AY_PI_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmpi_16_pd(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AY_PD_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmpi_16_di(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AY_DI_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_ix(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AY_IX_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT2 void m68k_op_cmpi_16_aw(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AW_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT2 void m68k_op_cmpi_16_al(void)
{
	uint src = OPER_I_16();
	uint dst = OPER_AL_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static M68K_HOT1 void m68k_op_cmpi_32_d(void)
{
	uint src = OPER_I_32();
	uint dst = DY;
	uint res = dst - src;

	m68ki_cmpild_callback(src, REG_IR & 7);		   /* auto-disable (see m68kcpu.h) */
	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpi_32_ai(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AY_AI_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpi_32_pi(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AY_PI_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpi_32_pd(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AY_PD_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT1 void m68k_op_cmpi_32_di(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AY_DI_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_ix(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AY_IX_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpi_32_aw(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AW_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static M68K_HOT2 void m68k_op_cmpi_32_al(void)
{
	uint src = OPER_I_32();
	uint dst = OPER_AL_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpm_8_ax7(void)
{
	uint src = OPER_AY_PI_8();
	uint dst = OPER_A7_PI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_8_ay7(void)
{
	uint src = OPER_A7_PI_8();
	uint dst = OPER_AX_PI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_8_axy7(void)
{
	uint src = OPER_A7_PI_8();
	uint dst = OPER_A7_PI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_8(void)
{
	uint src = OPER_AY_PI_8();
	uint dst = OPER_AX_PI_8();
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_16(void)
{
	uint src = OPER_AY_PI_16();
	uint dst = OPER_AX_PI_16();
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpm_32(void)
{
	uint src = OPER_AY_PI_32();
	uint dst = OPER_AX_PI_32();
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_dbt_16(void)
{
	REG_PC += 2;
}


static M68K_HOT1 void m68k_op_dbf_16(void)
{
	uint* r_dst = &DY;
	uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
	if(res != 0xffff)
	{
		uint offset = OPER_I_16();
		REG_PC -= 2;
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset);
		USE_CYCLES(CYC_DBCC_F_NOEXP);
		/* PBC: DBF to itself, a delay loop */
		if(REG_PC == REG_PPC)
			m68ki_dbf_skip(r_dst);
		return;
	}
	REG_PC += 2;
	USE_CYCLES(CYC_DBCC_F_EXP);
}


static void m68k_op_dbhi_16(void)
{
	if(COND_NOT_HI())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbls_16(void)
{
	if(COND_NOT_LS())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static M68K_HOT1 void m68k_op_dbcc_16(void)
{
	if(COND_NOT_CC())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbcs_16(void)
{
	if(COND_NOT_CS())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static M68K_HOT1 void m68k_op_dbne_16(void)
{
	if(COND_NOT_NE())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static M68K_HOT1 void m68k_op_dbeq_16(void)
{
	if(COND_NOT_EQ())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbvc_16(void)
{
	if(COND_NOT_VC())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbvs_16(void)
{
	if(COND_NOT_VS())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbpl_16(void)
{
	if(COND_NOT_PL())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbmi_16(void)
{
	if(COND_NOT_MI())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbge_16(void)
{
	if(COND_NOT_GE())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dblt_16(void)
{
	if(COND_NOT_LT())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dbgt_16(void)
{
	if(COND_NOT_GT())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_dble_16(void)
{
	if(COND_NOT_LE())
	{
		uint* r_dst = &DY;
		uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		if(res != 0xffff)
		{
			uint offset = OPER_I_16();
			REG_PC -= 2;
			m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
			m68ki_branch_16(offset);
			USE_CYCLES(CYC_DBCC_F_NOEXP);
			return;
		}
		REG_PC += 2;
		USE_CYCLES(CYC_DBCC_F_EXP);
		return;
	}
	REG_PC += 2;
}


static void m68k_op_divs_16_d(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(DY);
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_ai(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AY_AI_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pi(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AY_PI_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pd(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AY_PD_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_di(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AY_DI_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_ix(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AY_IX_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_aw(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AW_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_al(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_AL_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pcdi(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_PCDI_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pcix(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_PCIX_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_i(void)
{
	uint* r_dst = &DX;
	sint src = MAKE_INT_16(OPER_I_16());
	sint quotient;
	sint remainder;

	if(src != 0)
	{
		USE_CYCLES(m68ki_divs_cycles(*r_dst, src) - 158); /* PBC: exact time */

		if((uint32)*r_dst == 0x80000000 && src == -1)
		{
			FLAG_Z = 0;
			FLAG_N = NFLAG_CLEAR;
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = 0;
			return;
		}

		quotient = MAKE_INT_32(*r_dst) / src;
		remainder = MAKE_INT_32(*r_dst) % src;

		if(quotient == MAKE_INT_16(quotient))
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_d(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(DY);

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_16();

	if(src != 0)
	{
		uint quotient = *r_dst / src;
		uint remainder = *r_dst % src;

		USE_CYCLES(m68ki_divu_cycles(*r_dst, src) - 140); /* PBC: exact time */

		if(quotient < 0x10000)
		{
			FLAG_Z = quotient;
			FLAG_N = NFLAG_16(quotient);
			FLAG_V = VFLAG_CLEAR;
			FLAG_C = CFLAG_CLEAR;
			*r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
			return;
		}
		FLAG_V = VFLAG_SET;
		FLAG_N = NFLAG_SET;		/* PBC: as the 68000 does (Blood Shot) */
		FLAG_C = CFLAG_CLEAR;
		return;
	}
	FLAG_C = CFLAG_CLEAR;		/* PBC: as the 68000 does */
	m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static M68K_HOT1 void m68k_op_eor_8_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY ^= MASK_OUT_ABOVE_8(DX));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_di(void)
{
	uint ea = EA_AY_DI_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_aw(void)
{
	uint ea = EA_AW_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_al(void)
{
	uint ea = EA_AL_8();
	uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_eor_16_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY ^= MASK_OUT_ABOVE_16(DX));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_aw(void)
{
	uint ea = EA_AW_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_al(void)
{
	uint ea = EA_AL_16();
	uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_eor_32_d(void)
{
	uint res = DY ^= DX;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_di(void)
{
	uint ea = EA_AY_DI_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_aw(void)
{
	uint ea = EA_AW_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_al(void)
{
	uint ea = EA_AL_32();
	uint res = DX ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_eori_8_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY ^= OPER_I_8());

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_ai(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_AI_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pi(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PI_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pi7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PI_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pd(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PD_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pd7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PD_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_di(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_DI_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_ix(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_IX_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_aw(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AW_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_al(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AL_8();
	uint res = src ^ m68ki_read_8(ea);

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_eori_16_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY ^= OPER_I_16());

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_ai(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_pi(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PI_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_pd(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PD_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_di(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_ix(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_aw(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AW_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_al(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AL_16();
	uint res = src ^ m68ki_read_16(ea);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_eori_32_d(void)
{
	uint res = DY ^= OPER_I_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_ai(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_AI_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_pi(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PI_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_pd(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PD_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_di(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_DI_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_ix(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_IX_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_aw(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AW_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_al(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AL_32();
	uint res = src ^ m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_eori_16_toc(void)
{
	m68ki_set_ccr(m68ki_get_ccr() ^ OPER_I_8());
}


static M68K_HOT1 void m68k_op_eori_16_tos(void)
{
	if(FLAG_S)
	{
		uint src = OPER_I_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(m68ki_get_sr() ^ src);
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_exg_32_dd(void)
{
	uint* reg_a = &DX;
	uint* reg_b = &DY;
	uint tmp = *reg_a;
	*reg_a = *reg_b;
	*reg_b = tmp;
}


static void m68k_op_exg_32_aa(void)
{
	uint* reg_a = &AX;
	uint* reg_b = &AY;
	uint tmp = *reg_a;
	*reg_a = *reg_b;
	*reg_b = tmp;
}


static void m68k_op_exg_32_da(void)
{
	uint* reg_a = &DX;
	uint* reg_b = &AY;
	uint tmp = *reg_a;
	*reg_a = *reg_b;
	*reg_b = tmp;
}


static M68K_HOT1 void m68k_op_ext_16(void)
{
	uint* r_dst = &DY;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | MASK_OUT_ABOVE_8(*r_dst) | (GET_MSB_8(*r_dst) ? 0xff00 : 0);

	FLAG_N = NFLAG_16(*r_dst);
	FLAG_Z = MASK_OUT_ABOVE_16(*r_dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ext_32(void)
{
	uint* r_dst = &DY;

	*r_dst = MASK_OUT_ABOVE_16(*r_dst) | (GET_MSB_16(*r_dst) ? 0xffff0000 : 0);

	FLAG_N = NFLAG_32(*r_dst);
	FLAG_Z = *r_dst;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_illegal(void)
{
	m68ki_exception_illegal();
}


static M68K_HOT2 void m68k_op_jmp_32_ai(void)
{
	m68ki_jump(EA_AY_AI_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static void m68k_op_jmp_32_di(void)
{
	m68ki_jump(EA_AY_DI_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static void m68k_op_jmp_32_ix(void)
{
	m68ki_jump(EA_AY_IX_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static void m68k_op_jmp_32_aw(void)
{
	m68ki_jump(EA_AW_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static void m68k_op_jmp_32_al(void)
{
	m68ki_jump(EA_AL_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static void m68k_op_jmp_32_pcdi(void)
{
	m68ki_jump(EA_PCDI_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static void m68k_op_jmp_32_pcix(void)
{
	m68ki_jump(EA_PCIX_32());
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	if(REG_PC == REG_PPC)
		USE_ALL_CYCLES();
}


static M68K_HOT1 void m68k_op_jsr_32_ai(void)
{
	uint ea = EA_AY_AI_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static M68K_HOT1 void m68k_op_jsr_32_di(void)
{
	uint ea = EA_AY_DI_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static void m68k_op_jsr_32_ix(void)
{
	uint ea = EA_AY_IX_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static M68K_HOT2 void m68k_op_jsr_32_aw(void)
{
	uint ea = EA_AW_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static M68K_HOT2 void m68k_op_jsr_32_al(void)
{
	uint ea = EA_AL_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static M68K_HOT2 void m68k_op_jsr_32_pcdi(void)
{
	uint ea = EA_PCDI_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static void m68k_op_jsr_32_pcix(void)
{
	uint ea = EA_PCIX_32();
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_push_32(REG_PC);
	m68ki_jump(ea);
}


static M68K_HOT1 void m68k_op_lea_32_ai(void)
{
	AX = EA_AY_AI_32();
}


static M68K_HOT1 void m68k_op_lea_32_di(void)
{
	AX = EA_AY_DI_32();
}


static void m68k_op_lea_32_ix(void)
{
	AX = EA_AY_IX_32();
}


static M68K_HOT2 void m68k_op_lea_32_aw(void)
{
	AX = EA_AW_32();
}


static M68K_HOT2 void m68k_op_lea_32_al(void)
{
	AX = EA_AL_32();
}


static M68K_HOT2 void m68k_op_lea_32_pcdi(void)
{
	AX = EA_PCDI_32();
}


static void m68k_op_lea_32_pcix(void)
{
	AX = EA_PCIX_32();
}


static void m68k_op_link_16_a7(void)
{
	REG_A[7] -= 4;
	m68ki_write_32(REG_A[7], REG_A[7]);
	REG_A[7] = MASK_OUT_ABOVE_32(REG_A[7] + MAKE_INT_16(OPER_I_16()));
}


static void m68k_op_link_16(void)
{
	uint* r_dst = &AY;

	m68ki_push_32(*r_dst);
	*r_dst = REG_A[7];
	REG_A[7] = MASK_OUT_ABOVE_32(REG_A[7] + MAKE_INT_16(OPER_I_16()));
}


static M68K_HOT1 void m68k_op_lsr_8_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src >> shift;

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_X = FLAG_C = src << (9-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsr_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src >> shift;

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_X = FLAG_C = src << (9-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsr_32_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = *r_dst;
	uint res = src >> shift;

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_X = FLAG_C = src << (9-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsr_8_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = src >> shift;

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift <= 8)
		{
			*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
			FLAG_X = FLAG_C = src << (9-shift);
			FLAG_N = NFLAG_CLEAR;
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst &= 0xffffff00;
		FLAG_X = XFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_8(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsr_16_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = src >> shift;

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift <= 16)
		{
			*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
			FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
			FLAG_N = NFLAG_CLEAR;
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst &= 0xffff0000;
		FLAG_X = XFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_16(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsr_32_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = *r_dst;
	uint res = src >> shift;

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 32)
		{
			*r_dst = res;
			FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
			FLAG_N = NFLAG_CLEAR;
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst = 0;
		FLAG_X = FLAG_C = (shift == 32 ? GET_MSB_32(src)>>23 : 0);
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_32(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_lsr_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_lsr_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_lsr_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = src >> 1;

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_CLEAR;
	FLAG_Z = res;
	FLAG_C = FLAG_X = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsl_8_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = MASK_OUT_ABOVE_8(src << shift);

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src << shift;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsl_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = MASK_OUT_ABOVE_16(src << shift);

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> (8-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsl_32_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32(src << shift);

	if(shift != 0 && CPU_TYPE_IS_010_LESS(CPU_TYPE))
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> (24-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsl_8_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = MASK_OUT_ABOVE_8(src << shift);

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift <= 8)
		{
			*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
			FLAG_X = FLAG_C = src << shift;
			FLAG_N = NFLAG_8(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst &= 0xffffff00;
		FLAG_X = XFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_8(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsl_16_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = MASK_OUT_ABOVE_16(src << shift);

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
			USE_CYCLES(shift<<CYC_SHIFT);

		if(shift <= 16)
		{
			*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
			FLAG_X = FLAG_C = (src << shift) >> 8;
			FLAG_N = NFLAG_16(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst &= 0xffff0000;
		FLAG_X = XFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_16(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_lsl_32_r(void)
{
	uint* r_dst = &DY;
	uint shift = DX & 0x3f;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32(src << shift);

	if(shift != 0)
	{
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE))
				USE_CYCLES(shift<<CYC_SHIFT);

		if(shift < 32)
		{
			*r_dst = res;
			FLAG_X = FLAG_C = (src >> (32 - shift)) << 8;
			FLAG_N = NFLAG_32(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}

		*r_dst = 0;
		FLAG_X = FLAG_C = ((shift == 32 ? src & 1 : 0))<<8;
		FLAG_N = NFLAG_CLEAR;
		FLAG_Z = ZFLAG_SET;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_32(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_lsl_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_lsl_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_lsl_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(src << 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_X = FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_d_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_d_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_d_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_d_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_d_di(void)
{
	uint res = OPER_AY_DI_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_d_aw(void)
{
	uint res = OPER_AW_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_d_al(void)
{
	uint res = OPER_AL_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_d_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_d_i(void)
{
	uint res = OPER_I_8();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_ai_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_ai_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_ai_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ai_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_ai_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ai_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_ai_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ai_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_ai_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_ai_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_ai_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ai_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_ai_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AX_AI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pi_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi7_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_A7_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pi_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pi_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pi_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pi_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_pi_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_pi_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_pi_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pi_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pi_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AX_PI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pd_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd7_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_A7_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pd_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pd_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pd_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pd_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_pd_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_pd_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_pd_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_pd_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_pd_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AX_PD_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_di_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_di_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_di_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_di_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_di_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_di_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_di_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_di_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_di_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_di_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_di_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_di_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_8_di_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AX_DI_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ix_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AX_IX_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_aw_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_aw_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_aw_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_aw_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_aw_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AW_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_ai(void)
{
	uint res = OPER_AY_AI_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_pi(void)
{
	uint res = OPER_AY_PI_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_al_pi7(void)
{
	uint res = OPER_A7_PI_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_pd(void)
{
	uint res = OPER_AY_PD_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_al_pd7(void)
{
	uint res = OPER_A7_PD_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_di(void)
{
	uint res = OPER_AY_DI_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_al_ix(void)
{
	uint res = OPER_AY_IX_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_aw(void)
{
	uint res = OPER_AW_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_al(void)
{
	uint res = OPER_AL_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_pcdi(void)
{
	uint res = OPER_PCDI_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_al_pcix(void)
{
	uint res = OPER_PCIX_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_8_al_i(void)
{
	uint res = OPER_I_8();
	uint ea = EA_AL_8();

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_di(void)
{
	uint res = OPER_AY_DI_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_d_aw(void)
{
	uint res = OPER_AW_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_d_al(void)
{
	uint res = OPER_AL_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_d_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_d_i(void)
{
	uint res = OPER_I_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ai_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ai_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_ai_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AX_AI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_pi_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_pi_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_pi_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_pi_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_pi_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pi_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AX_PI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_pd_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_pd_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_pd_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_pd_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_pd_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_pd_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AX_PD_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_di_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_di_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_di_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_16_di_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_di_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_di_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AX_DI_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ix_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AX_IX_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_aw_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_aw_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_aw_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AW_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_a(void)
{
	uint res = MASK_OUT_ABOVE_16(AY);
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_ai(void)
{
	uint res = OPER_AY_AI_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_pi(void)
{
	uint res = OPER_AY_PI_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_pd(void)
{
	uint res = OPER_AY_PD_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_di(void)
{
	uint res = OPER_AY_DI_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_al_ix(void)
{
	uint res = OPER_AY_IX_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_aw(void)
{
	uint res = OPER_AW_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_al(void)
{
	uint res = OPER_AL_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_pcdi(void)
{
	uint res = OPER_PCDI_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_al_pcix(void)
{
	uint res = OPER_PCIX_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_16_al_i(void)
{
	uint res = OPER_I_16();
	uint ea = EA_AL_16();

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_d(void)
{
	uint res = DY;
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_a(void)
{
	uint res = AY;
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_di(void)
{
	uint res = OPER_AY_DI_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_d_aw(void)
{
	uint res = OPER_AW_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_d_al(void)
{
	uint res = OPER_AL_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_d_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_d_i(void)
{
	uint res = OPER_I_32();
	uint* r_dst = &DX;

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_d(void)
{
	uint res = DY;
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_a(void)
{
	uint res = AY;
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ai_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_ai_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_ai_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_ai_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ai_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_ai_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AX_AI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_d(void)
{
	uint res = DY;
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_a(void)
{
	uint res = AY;
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_pi_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_pi_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_pi_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_pi_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_pi_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pi_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AX_PI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_d(void)
{
	uint res = DY;
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_a(void)
{
	uint res = AY;
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_pd_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_pd_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_pd_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_pd_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_pd_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_pd_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AX_PD_32();

	m68ki_write_16(ea+2, res & 0xFFFF );
	m68ki_write_16(ea, (res >> 16) & 0xFFFF );

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_d(void)
{
	uint res = DY;
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_a(void)
{
	uint res = AY;
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_di_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_di_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_di_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_di_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_di_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_move_32_di_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AX_DI_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_d(void)
{
	uint res = DY;
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_a(void)
{
	uint res = AY;
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ix_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AX_IX_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_d(void)
{
	uint res = DY;
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_a(void)
{
	uint res = AY;
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_aw_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_aw_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_aw_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AW_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_d(void)
{
	uint res = DY;
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_a(void)
{
	uint res = AY;
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_ai(void)
{
	uint res = OPER_AY_AI_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_pi(void)
{
	uint res = OPER_AY_PI_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_pd(void)
{
	uint res = OPER_AY_PD_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_di(void)
{
	uint res = OPER_AY_DI_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_al_ix(void)
{
	uint res = OPER_AY_IX_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_aw(void)
{
	uint res = OPER_AW_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_al(void)
{
	uint res = OPER_AL_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_pcdi(void)
{
	uint res = OPER_PCDI_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_al_pcix(void)
{
	uint res = OPER_PCIX_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_move_32_al_i(void)
{
	uint res = OPER_I_32();
	uint ea = EA_AL_32();

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_movea_16_d(void)
{
	AX = MAKE_INT_16(DY);
}


static M68K_HOT1 void m68k_op_movea_16_a(void)
{
	AX = MAKE_INT_16(AY);
}


static M68K_HOT1 void m68k_op_movea_16_ai(void)
{
	AX = MAKE_INT_16(OPER_AY_AI_16());
}


static M68K_HOT1 void m68k_op_movea_16_pi(void)
{
	AX = MAKE_INT_16(OPER_AY_PI_16());
}


static M68K_HOT1 void m68k_op_movea_16_pd(void)
{
	AX = MAKE_INT_16(OPER_AY_PD_16());
}


static M68K_HOT1 void m68k_op_movea_16_di(void)
{
	AX = MAKE_INT_16(OPER_AY_DI_16());
}


static void m68k_op_movea_16_ix(void)
{
	AX = MAKE_INT_16(OPER_AY_IX_16());
}


static M68K_HOT2 void m68k_op_movea_16_aw(void)
{
	AX = MAKE_INT_16(OPER_AW_16());
}


static M68K_HOT2 void m68k_op_movea_16_al(void)
{
	AX = MAKE_INT_16(OPER_AL_16());
}


static M68K_HOT2 void m68k_op_movea_16_pcdi(void)
{
	AX = MAKE_INT_16(OPER_PCDI_16());
}


static void m68k_op_movea_16_pcix(void)
{
	AX = MAKE_INT_16(OPER_PCIX_16());
}


static M68K_HOT1 void m68k_op_movea_16_i(void)
{
	AX = MAKE_INT_16(OPER_I_16());
}


static M68K_HOT1 void m68k_op_movea_32_d(void)
{
	AX = DY;
}


static M68K_HOT1 void m68k_op_movea_32_a(void)
{
	AX = AY;
}


static M68K_HOT1 void m68k_op_movea_32_ai(void)
{
	AX = OPER_AY_AI_32();
}


static M68K_HOT1 void m68k_op_movea_32_pi(void)
{
	AX = OPER_AY_PI_32();
}


static M68K_HOT1 void m68k_op_movea_32_pd(void)
{
	AX = OPER_AY_PD_32();
}


static M68K_HOT1 void m68k_op_movea_32_di(void)
{
	AX = OPER_AY_DI_32();
}


static void m68k_op_movea_32_ix(void)
{
	AX = OPER_AY_IX_32();
}


static M68K_HOT2 void m68k_op_movea_32_aw(void)
{
	AX = OPER_AW_32();
}


static M68K_HOT2 void m68k_op_movea_32_al(void)
{
	AX = OPER_AL_32();
}


static M68K_HOT2 void m68k_op_movea_32_pcdi(void)
{
	AX = OPER_PCDI_32();
}


static void m68k_op_movea_32_pcix(void)
{
	AX = OPER_PCIX_32();
}


static M68K_HOT1 void m68k_op_movea_32_i(void)
{
	AX = OPER_I_32();
}


static M68K_HOT1 void m68k_op_move_16_toc_d(void)
{
	m68ki_set_ccr(DY);
}


static M68K_HOT1 void m68k_op_move_16_toc_ai(void)
{
	m68ki_set_ccr(OPER_AY_AI_16());
}


static M68K_HOT1 void m68k_op_move_16_toc_pi(void)
{
	m68ki_set_ccr(OPER_AY_PI_16());
}


static M68K_HOT1 void m68k_op_move_16_toc_pd(void)
{
	m68ki_set_ccr(OPER_AY_PD_16());
}


static M68K_HOT1 void m68k_op_move_16_toc_di(void)
{
	m68ki_set_ccr(OPER_AY_DI_16());
}


static void m68k_op_move_16_toc_ix(void)
{
	m68ki_set_ccr(OPER_AY_IX_16());
}


static M68K_HOT2 void m68k_op_move_16_toc_aw(void)
{
	m68ki_set_ccr(OPER_AW_16());
}


static M68K_HOT2 void m68k_op_move_16_toc_al(void)
{
	m68ki_set_ccr(OPER_AL_16());
}


static M68K_HOT2 void m68k_op_move_16_toc_pcdi(void)
{
	m68ki_set_ccr(OPER_PCDI_16());
}


static void m68k_op_move_16_toc_pcix(void)
{
	m68ki_set_ccr(OPER_PCIX_16());
}


static M68K_HOT1 void m68k_op_move_16_toc_i(void)
{
	m68ki_set_ccr(OPER_I_16());
}


static M68K_HOT1 void m68k_op_move_16_frs_d(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		DY = MASK_OUT_BELOW_16(DY) | m68ki_get_sr();
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_frs_ai(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AY_AI_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_frs_pi(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AY_PI_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_frs_pd(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AY_PD_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_frs_di(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AY_DI_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_frs_ix(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AY_IX_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT2 void m68k_op_move_16_frs_aw(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AW_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT2 void m68k_op_move_16_frs_al(void)
{
	if(CPU_TYPE_IS_000(CPU_TYPE) || FLAG_S)	/* NS990408 */
	{
		uint ea = EA_AL_16();
		m68ki_write_16(ea, m68ki_get_sr());
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_tos_d(void)
{
	if(FLAG_S)
	{
		m68ki_set_sr(DY);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_tos_ai(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AY_AI_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_tos_pi(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AY_PI_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_tos_pd(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AY_PD_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_tos_di(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AY_DI_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_ix(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AY_IX_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT2 void m68k_op_move_16_tos_aw(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AW_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT2 void m68k_op_move_16_tos_al(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_AL_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT2 void m68k_op_move_16_tos_pcdi(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_PCDI_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_pcix(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_PCIX_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_16_tos_i(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_I_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(new_sr);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_32_fru(void)
{
	if(FLAG_S)
	{
		AY = REG_USP;
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_move_32_tou(void)
{
	if(FLAG_S)
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		REG_USP = AY;
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_movem_16_re_pd(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = AY;
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			ea -= 2;
			m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[15-i]));
			count++;
		}
	AY = ea;

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_ai(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_di(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_ix(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_aw(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AW_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_al(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AL_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_32_re_pd(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = AY;
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			ea -= 4;
			m68ki_write_16(ea+2, REG_DA[15-i] & 0xFFFF );
			m68ki_write_16(ea, (REG_DA[15-i] >> 16) & 0xFFFF );
			count++;
		}
	AY = ea;

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_re_ai(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_AI_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_32(ea, REG_DA[i]);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_re_di(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_DI_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_32(ea, REG_DA[i]);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_re_ix(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_IX_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_32(ea, REG_DA[i]);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_re_aw(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AW_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_32(ea, REG_DA[i]);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_re_al(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AL_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			m68ki_write_32(ea, REG_DA[i]);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_16_er_pi(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = AY;
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
			ea += 2;
			count++;
		}
	AY = ea;

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_pcdi(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_PCDI_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_pcrel_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_pcix(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_PCIX_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_pcrel_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_ai(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_di(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_ix(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_aw(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AW_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_al(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AL_16();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
			ea += 2;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_W);
}


static void m68k_op_movem_32_er_pi(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = AY;
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_32(ea);
			ea += 4;
			count++;
		}
	AY = ea;

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_pcdi(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_PCDI_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_pcrel_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_pcix(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_PCIX_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_pcrel_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_ai(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_AI_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_di(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_DI_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_ix(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AY_IX_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_aw(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AW_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movem_32_er_al(void)
{
	uint i = 0;
	uint register_list = OPER_I_16();
	uint ea = EA_AL_32();
	uint count = 0;

	for(; i < 16; i++)
		if(register_list & (1 << i))
		{
			REG_DA[i] = m68ki_read_32(ea);
			ea += 4;
			count++;
		}

	USE_CYCLES(count<<CYC_MOVEM_L);
}


static void m68k_op_movep_16_re(void)
{
	uint ea = EA_AY_DI_16();
	uint src = DX;

	m68ki_write_8(ea, MASK_OUT_ABOVE_8(src >> 8));
	m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src));
}


static void m68k_op_movep_32_re(void)
{
	uint ea = EA_AY_DI_32();
	uint src = DX;

	m68ki_write_8(ea, MASK_OUT_ABOVE_8(src >> 24));
	m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src >> 16));
	m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src >> 8));
	m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src));
}


static void m68k_op_movep_16_er(void)
{
	uint ea = EA_AY_DI_16();
	uint* r_dst = &DX;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | ((m68ki_read_8(ea) << 8) + m68ki_read_8(ea + 2));
}


static void m68k_op_movep_32_er(void)
{
	uint ea = EA_AY_DI_32();

	DX = (m68ki_read_8(ea) << 24) + (m68ki_read_8(ea + 2) << 16)
		+ (m68ki_read_8(ea + 4) << 8) + m68ki_read_8(ea + 6);
}


static M68K_HOT1 void m68k_op_moveq_32(void)
{
	uint res = DX = MAKE_INT_8(MASK_OUT_ABOVE_8(REG_IR));

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_muls_16_d(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(DY);
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */

	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_ai(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AY_AI_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pi(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AY_PI_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pd(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AY_PD_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_di(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AY_DI_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_ix(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AY_IX_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_aw(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AW_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_al(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_AL_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pcdi(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_PCDI_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pcix(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_PCIX_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_i(void)
{
	uint* r_dst = &DX;
	uint x = MAKE_INT_16(OPER_I_16());
	USE_CYCLES(2 * __builtin_popcount((x ^ (x << 1)) & 0xffff));	/* PBC: 38 + 2n, n = 01/10 pairs in x<<1 */
	uint res = MASK_OUT_ABOVE_32(x * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_mulu_16_d(void)
{
	uint* r_dst = &DX;
	uint x = MASK_OUT_ABOVE_16(DY);
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */
	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_ai(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AY_AI_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pi(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AY_PI_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pd(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AY_PD_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_di(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AY_DI_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_ix(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AY_IX_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_aw(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AW_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_al(void)
{
	uint* r_dst = &DX;
	uint x = OPER_AL_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pcdi(void)
{
	uint* r_dst = &DX;
	uint x = OPER_PCDI_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pcix(void)
{
	uint* r_dst = &DX;
	uint x = OPER_PCIX_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_i(void)
{
	uint* r_dst = &DX;
	uint x = OPER_I_16();
	USE_CYCLES(2 * __builtin_popcount(x));	/* PBC: 38 + 2n, n = bits set */

	uint res = x * MASK_OUT_ABOVE_16(*r_dst);

	*r_dst = res;

	FLAG_Z = res;
	FLAG_N = NFLAG_32(res);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_nbcd_8_d(void)
{
	uint* r_dst = &DY;
	uint dst = *r_dst;
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_di(void)
{
	uint ea = EA_AY_DI_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_aw(void)
{
	uint ea = EA_AW_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static void m68k_op_nbcd_8_al(void)
{
	uint ea = EA_AL_8();
	uint dst = m68ki_read_8(ea);
	uint res = MASK_OUT_ABOVE_8(0x9a - dst - XFLAG_AS_1());

	if(res != 0x9a)
	{
		FLAG_V = ~res; /* Undefined V behavior */

		if((res & 0x0f) == 0xa)
			res = (res & 0xf0) + 0x10;

		res = MASK_OUT_ABOVE_8(res);

		FLAG_V &= res; /* Undefined V behavior part II */

		m68ki_write_8(ea, MASK_OUT_ABOVE_8(res));

		FLAG_Z |= res;
		FLAG_C = CFLAG_SET;
		FLAG_X = XFLAG_SET;
	}
	else
	{
		FLAG_V = VFLAG_CLEAR;
		FLAG_C = CFLAG_CLEAR;
		FLAG_X = XFLAG_CLEAR;
	}
	FLAG_N = NFLAG_8(res);	/* Undefined N behavior */
}


static M68K_HOT2 void m68k_op_neg_8_d(void)
{
	uint* r_dst = &DY;
	uint res = 0 - MASK_OUT_ABOVE_8(*r_dst);

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = *r_dst & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_neg_8_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_aw(void)
{
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_al(void)
{
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_8(res);
	FLAG_C = FLAG_X = CFLAG_8(res);
	FLAG_V = src & res;
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_neg_16_d(void)
{
	uint* r_dst = &DY;
	uint res = 0 - MASK_OUT_ABOVE_16(*r_dst);

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (*r_dst & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_neg_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_16(res);
	FLAG_C = FLAG_X = CFLAG_16(res);
	FLAG_V = (src & res)>>8;
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_neg_32_d(void)
{
	uint* r_dst = &DY;
	uint res = 0 - *r_dst;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(*r_dst, 0, res);
	FLAG_V = (*r_dst & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_neg_32_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_di(void)
{
	uint ea = EA_AY_DI_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_aw(void)
{
	uint ea = EA_AW_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_al(void)
{
	uint ea = EA_AL_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - src;

	FLAG_N = NFLAG_32(res);
	FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_negx_8_d(void)
{
	uint* r_dst = &DY;
	uint res = 0 - MASK_OUT_ABOVE_8(*r_dst) - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = *r_dst & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_negx_8_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_aw(void)
{
	uint ea = EA_AW_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_al(void)
{
	uint ea = EA_AL_8();
	uint src = m68ki_read_8(ea);
	uint res = 0 - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = src & res;

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_negx_16_d(void)
{
	uint* r_dst = &DY;
	uint res = 0 - MASK_OUT_ABOVE_16(*r_dst) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (*r_dst & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
}


static void m68k_op_negx_16_ai(void)
{
	uint ea  = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_pi(void)
{
	uint ea  = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_pd(void)
{
	uint ea  = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_di(void)
{
	uint ea  = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_ix(void)
{
	uint ea  = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_aw(void)
{
	uint ea  = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_al(void)
{
	uint ea  = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = (src & res)>>8;

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_negx_32_d(void)
{
	uint* r_dst = &DY;
	uint res = 0 - MASK_OUT_ABOVE_32(*r_dst) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(*r_dst, 0, res);
	FLAG_V = (*r_dst & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	*r_dst = res;
}


static void m68k_op_negx_32_ai(void)
{
	uint ea  = EA_AY_AI_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_pi(void)
{
	uint ea  = EA_AY_PI_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_pd(void)
{
	uint ea  = EA_AY_PD_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_di(void)
{
	uint ea  = EA_AY_DI_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_ix(void)
{
	uint ea  = EA_AY_IX_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_aw(void)
{
	uint ea  = EA_AW_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_al(void)
{
	uint ea  = EA_AL_32();
	uint src = m68ki_read_32(ea);
	uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
	FLAG_V = (src & res)>>24;

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_nop(void)
{
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
}


static M68K_HOT2 void m68k_op_not_8_d(void)
{
	uint* r_dst = &DY;
	uint res = MASK_OUT_ABOVE_8(~*r_dst);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_di(void)
{
	uint ea = EA_AY_DI_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_aw(void)
{
	uint ea = EA_AW_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_al(void)
{
	uint ea = EA_AL_8();
	uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_not_16_d(void)
{
	uint* r_dst = &DY;
	uint res = MASK_OUT_ABOVE_16(~*r_dst);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_aw(void)
{
	uint ea = EA_AW_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_al(void)
{
	uint ea = EA_AL_16();
	uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_not_32_d(void)
{
	uint* r_dst = &DY;
	uint res = *r_dst = MASK_OUT_ABOVE_32(~*r_dst);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_di(void)
{
	uint ea = EA_AY_DI_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_aw(void)
{
	uint ea = EA_AW_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_al(void)
{
	uint ea = EA_AL_32();
	uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_or_8_er_d(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= MASK_OUT_ABOVE_8(DY)));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_ai(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_AI_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pi(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_PI_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pi7(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_A7_PI_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pd(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_PD_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pd7(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_A7_PD_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_di(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_DI_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_ix(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_IX_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_aw(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AW_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_al(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_AL_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pcdi(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_PCDI_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pcix(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_PCIX_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_i(void)
{
	uint res = MASK_OUT_ABOVE_8((DX |= OPER_I_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_or_16_er_d(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= MASK_OUT_ABOVE_16(DY)));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_ai(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_AI_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pi(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_PI_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pd(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_PD_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_di(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_DI_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_ix(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_IX_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_aw(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AW_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_al(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_AL_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pcdi(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_PCDI_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pcix(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_PCIX_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_i(void)
{
	uint res = MASK_OUT_ABOVE_16((DX |= OPER_I_16()));

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_or_32_er_d(void)
{
	uint res = DX |= DY;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_ai(void)
{
	uint res = DX |= OPER_AY_AI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pi(void)
{
	uint res = DX |= OPER_AY_PI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pd(void)
{
	uint res = DX |= OPER_AY_PD_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_di(void)
{
	uint res = DX |= OPER_AY_DI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_ix(void)
{
	uint res = DX |= OPER_AY_IX_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_aw(void)
{
	uint res = DX |= OPER_AW_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_al(void)
{
	uint res = DX |= OPER_AL_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pcdi(void)
{
	uint res = DX |= OPER_PCDI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pcix(void)
{
	uint res = DX |= OPER_PCIX_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_i(void)
{
	uint res = DX |= OPER_I_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_di(void)
{
	uint ea = EA_AY_DI_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_aw(void)
{
	uint ea = EA_AW_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_al(void)
{
	uint ea = EA_AL_8();
	uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_di(void)
{
	uint ea = EA_AY_DI_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_aw(void)
{
	uint ea = EA_AW_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_al(void)
{
	uint ea = EA_AL_16();
	uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_di(void)
{
	uint ea = EA_AY_DI_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_aw(void)
{
	uint ea = EA_AW_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_al(void)
{
	uint ea = EA_AL_32();
	uint res = DX | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_8_d(void)
{
	uint res = MASK_OUT_ABOVE_8((DY |= OPER_I_8()));

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_8_ai(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_AI_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_8_pi(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PI_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_pi7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PI_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_8_pd(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PD_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_pd7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PD_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ori_8_di(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_DI_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_ix(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_IX_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_aw(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AW_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_al(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AL_8();
	uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

	m68ki_write_8(ea, res);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_16_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY |= OPER_I_16());

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_16_ai(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_16_pi(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PI_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_16_pd(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PD_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ori_16_di(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_ix(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_aw(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AW_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_al(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AL_16();
	uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_32_d(void)
{
	uint res = DY |= OPER_I_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_32_ai(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_AI_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_32_pi(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PI_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_32_pd(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PD_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ori_32_di(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_DI_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_ix(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_IX_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_aw(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AW_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_al(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AL_32();
	uint res = src | m68ki_read_32(ea);

	m68ki_write_32(ea, res);

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_ori_16_toc(void)
{
	m68ki_set_ccr(m68ki_get_ccr() | OPER_I_8());
}


static M68K_HOT1 void m68k_op_ori_16_tos(void)
{
	if(FLAG_S)
	{
		uint src = OPER_I_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_set_sr(m68ki_get_sr() | src);
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_pea_32_ai(void)
{
	uint ea = EA_AY_AI_32();

	m68ki_push_32(ea);
}


static void m68k_op_pea_32_di(void)
{
	uint ea = EA_AY_DI_32();

	m68ki_push_32(ea);
}


static void m68k_op_pea_32_ix(void)
{
	uint ea = EA_AY_IX_32();

	m68ki_push_32(ea);
}


static void m68k_op_pea_32_aw(void)
{
	uint ea = EA_AW_32();

	m68ki_push_32(ea);
}


static void m68k_op_pea_32_al(void)
{
	uint ea = EA_AL_32();

	m68ki_push_32(ea);
}


static void m68k_op_pea_32_pcdi(void)
{
	uint ea = EA_PCDI_32();

	m68ki_push_32(ea);
}


static void m68k_op_pea_32_pcix(void)
{
	uint ea = EA_PCIX_32();

	m68ki_push_32(ea);
}


static void m68k_op_reset(void)
{
	if(FLAG_S)
	{
		m68ki_output_reset();		   /* auto-disable (see m68kcpu.h) */
		USE_CYCLES(CYC_RESET);
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT2 void m68k_op_ror_8_s(void)
{
	uint* r_dst = &DY;
	uint orig_shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint shift = orig_shift & 7;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = ROR_8(src, shift);

	if(orig_shift != 0)
		USE_CYCLES(orig_shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = src << (9-orig_shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ror_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = ROR_16(src, shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << (9-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ror_32_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint64 src = *r_dst;
	uint res = ROR_32(src, shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = src << (9-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ror_8_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift & 7;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = ROR_8(src, shift);

	if(orig_shift != 0)
	{
		USE_CYCLES(orig_shift<<CYC_SHIFT);

		*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
		FLAG_C = src << (8-((shift-1)&7));
		FLAG_N = NFLAG_8(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_8(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ror_16_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift & 15;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = ROR_16(src, shift);

	if(orig_shift != 0)
	{
		USE_CYCLES(orig_shift<<CYC_SHIFT);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		FLAG_C = (src >> ((shift - 1) & 15)) << 8;
		FLAG_N = NFLAG_16(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_16(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_ror_32_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift & 31;
	uint64 src = *r_dst;
	uint res = ROR_32(src, shift);

	if(orig_shift != 0)
	{
		USE_CYCLES(orig_shift<<CYC_SHIFT);

		*r_dst = res;
		FLAG_C = (src >> ((shift - 1) & 31)) << 8;
		FLAG_N = NFLAG_32(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_32(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_16(src, 1);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src << 8;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_rol_8_s(void)
{
	uint* r_dst = &DY;
	uint orig_shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint shift = orig_shift & 7;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = ROL_8(src, shift);

	if(orig_shift != 0)
		USE_CYCLES(orig_shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_C = src << orig_shift;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_rol_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = ROL_16(src, shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> (8-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_rol_32_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint64 src = *r_dst;
	uint res = ROL_32(src, shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_C = src >> (24-shift);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_rol_8_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift & 7;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = ROL_8(src, shift);

	if(orig_shift != 0)
	{
		USE_CYCLES(orig_shift<<CYC_SHIFT);

		if(shift != 0)
		{
			*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
			FLAG_C = src << shift;
			FLAG_N = NFLAG_8(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}
		FLAG_C = (src & 1)<<8;
		FLAG_N = NFLAG_8(src);
		FLAG_Z = src;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_8(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_rol_16_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift & 15;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, shift));

	if(orig_shift != 0)
	{
		USE_CYCLES(orig_shift<<CYC_SHIFT);

		if(shift != 0)
		{
			*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
			FLAG_C = (src << shift) >> 8;
			FLAG_N = NFLAG_16(res);
			FLAG_Z = res;
			FLAG_V = VFLAG_CLEAR;
			return;
		}
		FLAG_C = (src & 1)<<8;
		FLAG_N = NFLAG_16(src);
		FLAG_Z = src;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_16(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_rol_32_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift & 31;
	uint64 src = *r_dst;
	uint res = ROL_32(src, shift);

	if(orig_shift != 0)
	{
		USE_CYCLES(orig_shift<<CYC_SHIFT);

		*r_dst = res;

		FLAG_C = (src >> ((32 - shift) & 0x1f)) << 8;
		FLAG_N = NFLAG_32(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = CFLAG_CLEAR;
	FLAG_N = NFLAG_32(src);
	FLAG_Z = src;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_C = src >> 7;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxr_8_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = ROR_9(src | (XFLAG_AS_1() << 8), shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	FLAG_C = FLAG_X = res;
	res = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxr_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxr_32_s(void)
{
#if M68K_USE_64_BIT

	uint*  r_dst = &DY;
	uint   shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint64 src   = *r_dst;
	uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	res = ROR_33_64(res, shift);

	FLAG_C = FLAG_X = res >> 24;
	res = MASK_OUT_ABOVE_32(res);

	*r_dst =  res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;

#else

	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32((ROR_33(src, shift) & ~(1 << (32 - shift))) | (XFLAG_AS_1() << (32 - shift)));
	uint new_x_flag = src & (1 << (shift - 1));

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_C = FLAG_X = (new_x_flag != 0)<<8;
	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;

#endif
}


static M68K_HOT2 void m68k_op_roxr_8_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;

	if(orig_shift != 0)
	{
		uint shift = orig_shift % 9;
		uint src   = MASK_OUT_ABOVE_8(*r_dst);
		uint res   = ROR_9(src | (XFLAG_AS_1() << 8), shift);

		USE_CYCLES(orig_shift<<CYC_SHIFT);

		FLAG_C = FLAG_X = res;
		res = MASK_OUT_ABOVE_8(res);

		*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
		FLAG_N = NFLAG_8(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_8(*r_dst);
	FLAG_Z = MASK_OUT_ABOVE_8(*r_dst);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxr_16_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;

	if(orig_shift != 0)
	{
		uint shift = orig_shift % 17;
		uint src   = MASK_OUT_ABOVE_16(*r_dst);
		uint res   = ROR_17(src | (XFLAG_AS_1() << 16), shift);

		USE_CYCLES(orig_shift<<CYC_SHIFT);

		FLAG_C = FLAG_X = res >> 8;
		res = MASK_OUT_ABOVE_16(res);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		FLAG_N = NFLAG_16(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_16(*r_dst);
	FLAG_Z = MASK_OUT_ABOVE_16(*r_dst);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxr_32_r(void)
{
#if M68K_USE_64_BIT

	uint*  r_dst = &DY;
	uint   orig_shift = DX & 0x3f;

	if(orig_shift != 0)
	{
		uint   shift = orig_shift % 33;
		uint64 src   = *r_dst;
		uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

		res = ROR_33_64(res, shift);

		USE_CYCLES(orig_shift<<CYC_SHIFT);

		FLAG_C = FLAG_X = res >> 24;
		res = MASK_OUT_ABOVE_32(res);

		*r_dst = res;
		FLAG_N = NFLAG_32(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_32(*r_dst);
	FLAG_Z = *r_dst;
	FLAG_V = VFLAG_CLEAR;

#else

	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift % 33;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32((ROR_33(src, shift) & ~(1 << (32 - shift))) | (XFLAG_AS_1() << (32 - shift)));
	uint new_x_flag = src & (1 << (shift - 1));

	if(orig_shift != 0)
		USE_CYCLES(orig_shift<<CYC_SHIFT);

	if(shift != 0)
	{
		*r_dst = res;
		FLAG_X = (new_x_flag != 0)<<8;
	}
	else
		res = src;
	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;

#endif
}


static void m68k_op_roxr_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxl_8_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_8(*r_dst);
	uint res = ROL_9(src | (XFLAG_AS_1() << 8), shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	FLAG_C = FLAG_X = res;
	res = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxl_16_s(void)
{
	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = MASK_OUT_ABOVE_16(*r_dst);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), shift);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxl_32_s(void)
{
#if M68K_USE_64_BIT

	uint*  r_dst = &DY;
	uint   shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint64 src   = *r_dst;
	uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	res = ROL_33_64(res, shift);

	FLAG_C = FLAG_X = res >> 24;
	res = MASK_OUT_ABOVE_32(res);

	*r_dst = res;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;

#else

	uint* r_dst = &DY;
	uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32((ROL_33(src, shift) & ~(1 << (shift - 1))) | (XFLAG_AS_1() << (shift - 1)));
	uint new_x_flag = src & (1 << (32 - shift));

	if(shift != 0)
		USE_CYCLES(shift<<CYC_SHIFT);

	*r_dst = res;

	FLAG_C = FLAG_X = (new_x_flag != 0)<<8;
	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;

#endif
}


static M68K_HOT2 void m68k_op_roxl_8_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;


	if(orig_shift != 0)
	{
		uint shift = orig_shift % 9;
		uint src   = MASK_OUT_ABOVE_8(*r_dst);
		uint res   = ROL_9(src | (XFLAG_AS_1() << 8), shift);

		USE_CYCLES(orig_shift<<CYC_SHIFT);

		FLAG_C = FLAG_X = res;
		res = MASK_OUT_ABOVE_8(res);

		*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
		FLAG_N = NFLAG_8(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_8(*r_dst);
	FLAG_Z = MASK_OUT_ABOVE_8(*r_dst);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxl_16_r(void)
{
	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;

	if(orig_shift != 0)
	{
		uint shift = orig_shift % 17;
		uint src   = MASK_OUT_ABOVE_16(*r_dst);
		uint res   = ROL_17(src | (XFLAG_AS_1() << 16), shift);

		USE_CYCLES(orig_shift<<CYC_SHIFT);

		FLAG_C = FLAG_X = res >> 8;
		res = MASK_OUT_ABOVE_16(res);

		*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
		FLAG_N = NFLAG_16(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_16(*r_dst);
	FLAG_Z = MASK_OUT_ABOVE_16(*r_dst);
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_roxl_32_r(void)
{
#if M68K_USE_64_BIT

	uint*  r_dst = &DY;
	uint   orig_shift = DX & 0x3f;

	if(orig_shift != 0)
	{
		uint   shift = orig_shift % 33;
		uint64 src   = *r_dst;
		uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

		res = ROL_33_64(res, shift);

		USE_CYCLES(orig_shift<<CYC_SHIFT);

		FLAG_C = FLAG_X = res >> 24;
		res = MASK_OUT_ABOVE_32(res);

		*r_dst = res;
		FLAG_N = NFLAG_32(res);
		FLAG_Z = res;
		FLAG_V = VFLAG_CLEAR;
		return;
	}

	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_32(*r_dst);
	FLAG_Z = *r_dst;
	FLAG_V = VFLAG_CLEAR;

#else

	uint* r_dst = &DY;
	uint orig_shift = DX & 0x3f;
	uint shift = orig_shift % 33;
	uint src = *r_dst;
	uint res = MASK_OUT_ABOVE_32((ROL_33(src, shift) & ~(1 << (shift - 1))) | (XFLAG_AS_1() << (shift - 1)));
	uint new_x_flag = src & (1 << (32 - shift));

	if(orig_shift != 0)
		USE_CYCLES(orig_shift<<CYC_SHIFT);

	if(shift != 0)
	{
		*r_dst = res;
		FLAG_X = (new_x_flag != 0)<<8;
	}
	else
		res = src;
	FLAG_C = FLAG_X;
	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;

#endif
}


static void m68k_op_roxl_16_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_aw(void)
{
	uint ea = EA_AW_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_al(void)
{
	uint ea = EA_AL_16();
	uint src = m68ki_read_16(ea);
	uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

	FLAG_C = FLAG_X = res >> 8;
	res = MASK_OUT_ABOVE_16(res);

	m68ki_write_16(ea, res);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_rte_32(void)
{
	if(FLAG_S)
	{
		uint new_sr;
		uint new_pc;
		uint format_word;

		m68ki_rte_callback();		   /* auto-disable (see m68kcpu.h) */
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */

		if(CPU_TYPE_IS_000(CPU_TYPE))
		{
			new_sr = m68ki_pull_16();
			new_pc = m68ki_pull_32();
			m68ki_jump(new_pc);
			m68ki_set_sr(new_sr);

#if M68K_EMULATE_ADDRESS_ERROR
			CPU_INSTR_MODE = INSTRUCTION_YES;
			CPU_RUN_MODE = RUN_MODE_NORMAL;
#endif

			return;
		}

		if(CPU_TYPE_IS_010(CPU_TYPE))
		{
			format_word = m68ki_read_16(REG_A[7]+6) >> 12;
			if(format_word == 0)
			{
				new_sr = m68ki_pull_16();
				new_pc = m68ki_pull_32();
				m68ki_fake_pull_16();	/* format word */
				m68ki_jump(new_pc);
				m68ki_set_sr(new_sr);
				CPU_INSTR_MODE = INSTRUCTION_YES;
				CPU_RUN_MODE = RUN_MODE_NORMAL;
				return;
			} else if (format_word == 8) {
				/* Format 8 stack frame -- 68010 only. 29 word bus/address error */
				new_sr = m68ki_pull_16();
				new_pc = m68ki_pull_32();
				m68ki_fake_pull_16();	/* format word */
				m68ki_fake_pull_16();	/* special status word */
				m68ki_fake_pull_32();	/* fault address */
				m68ki_fake_pull_16();	/* unused/reserved */
				m68ki_fake_pull_16();	/* data output buffer */
				m68ki_fake_pull_16();	/* unused/reserved */
				m68ki_fake_pull_16();	/* data input buffer */
				m68ki_fake_pull_16();	/* unused/reserved */
				m68ki_fake_pull_16();	/* instruction input buffer */
				m68ki_fake_pull_32();	/* internal information, 16 words */
				m68ki_fake_pull_32();	/* (actually, we use 8 DWORDs) */
				m68ki_fake_pull_32();
				m68ki_fake_pull_32();
				m68ki_fake_pull_32();
				m68ki_fake_pull_32();
				m68ki_fake_pull_32();
				m68ki_fake_pull_32();
				m68ki_jump(new_pc);
				m68ki_set_sr(new_sr);
				CPU_INSTR_MODE = INSTRUCTION_YES;
				CPU_RUN_MODE = RUN_MODE_NORMAL;
				return;
			}
			CPU_INSTR_MODE = INSTRUCTION_YES;
			CPU_RUN_MODE = RUN_MODE_NORMAL;
			/* Not handling other exception types (9) */
			m68ki_exception_format_error();
			return;
		}

		/* Otherwise it's 020 */
rte_loop:
		format_word = m68ki_read_16(REG_A[7]+6) >> 12;
		switch(format_word)
		{
			case 0: /* Normal */
				new_sr = m68ki_pull_16();
				new_pc = m68ki_pull_32();
				m68ki_fake_pull_16();	/* format word */
				m68ki_jump(new_pc);
				m68ki_set_sr(new_sr);
				CPU_INSTR_MODE = INSTRUCTION_YES;
				CPU_RUN_MODE = RUN_MODE_NORMAL;
				return;
			case 1: /* Throwaway */
				new_sr = m68ki_pull_16();
				m68ki_fake_pull_32();	/* program counter */
				m68ki_fake_pull_16();	/* format word */
				m68ki_set_sr_noint(new_sr);
				goto rte_loop;
			case 2: /* Trap */
				new_sr = m68ki_pull_16();
				new_pc = m68ki_pull_32();
				m68ki_fake_pull_16();	/* format word */
				m68ki_fake_pull_32();	/* address */
				m68ki_jump(new_pc);
				m68ki_set_sr(new_sr);
				CPU_INSTR_MODE = INSTRUCTION_YES;
				CPU_RUN_MODE = RUN_MODE_NORMAL;
				return;
		}
		/* Not handling long or short bus fault */
		CPU_INSTR_MODE = INSTRUCTION_YES;
		CPU_RUN_MODE = RUN_MODE_NORMAL;
		m68ki_exception_format_error();
		return;
	}
	m68ki_exception_privilege_violation();
}


static void m68k_op_rtr_32(void)
{
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_set_ccr(m68ki_pull_16());
	m68ki_jump(m68ki_pull_32());
}


static M68K_HOT1 void m68k_op_rts_32(void)
{
	m68ki_trace_t0();				   /* auto-disable (see m68kcpu.h) */
	m68ki_jump(m68ki_pull_32());
}


static void m68k_op_sbcd_8_rr(void)
{
	uint* r_dst = &DX;
	uint src = DY;
	uint dst = *r_dst;
	uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res -= 6;
	res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res += 0xa0;

	res = MASK_OUT_ABOVE_8(res);

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_sbcd_8_mm_ax7(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res -= 6;
	res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res += 0xa0;

	res = MASK_OUT_ABOVE_8(res);

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_sbcd_8_mm_ay7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res -= 6;
	res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res += 0xa0;

	res = MASK_OUT_ABOVE_8(res);

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_sbcd_8_mm_axy7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res -= 6;
	res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res += 0xa0;

	res = MASK_OUT_ABOVE_8(res);

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_sbcd_8_mm(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();

	FLAG_V = ~res; /* Undefined V behavior */

	if(res > 9)
		res -= 6;
	res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
	FLAG_X = FLAG_C = (res > 0x99) << 8;
	if(FLAG_C)
		res += 0xa0;

	res = MASK_OUT_ABOVE_8(res);

	FLAG_V &= res; /* Undefined V behavior part II */
	FLAG_N = NFLAG_8(res); /* Undefined N behavior */
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_st_8_d(void)
{
	DY |= 0xff;
}


static void m68k_op_st_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), 0xff);
}


static void m68k_op_st_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), 0xff);
}


static void m68k_op_st_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), 0xff);
}


static void m68k_op_st_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), 0xff);
}


static void m68k_op_st_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), 0xff);
}


static void m68k_op_st_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), 0xff);
}


static void m68k_op_st_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), 0xff);
}


static void m68k_op_st_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), 0xff);
}


static void m68k_op_st_8_al(void)
{
	m68ki_write_8(EA_AL_8(), 0xff);
}


static void m68k_op_sf_8_d(void)
{
	DY &= 0xffffff00;
}


static void m68k_op_sf_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), 0);
}


static void m68k_op_sf_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), 0);
}


static void m68k_op_sf_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), 0);
}


static void m68k_op_sf_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), 0);
}


static void m68k_op_sf_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), 0);
}


static void m68k_op_sf_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), 0);
}


static void m68k_op_sf_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), 0);
}


static void m68k_op_sf_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), 0);
}


static void m68k_op_sf_8_al(void)
{
	m68ki_write_8(EA_AL_8(), 0);
}


static void m68k_op_shi_8_d(void)
{
	if(COND_HI())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_sls_8_d(void)
{
	if(COND_LS())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_scc_8_d(void)
{
	if(COND_CC())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_scs_8_d(void)
{
	if(COND_CS())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_sne_8_d(void)
{
	if(COND_NE())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_seq_8_d(void)
{
	if(COND_EQ())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_svc_8_d(void)
{
	if(COND_VC())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_svs_8_d(void)
{
	if(COND_VS())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_spl_8_d(void)
{
	if(COND_PL())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_smi_8_d(void)
{
	if(COND_MI())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_sge_8_d(void)
{
	if(COND_GE())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_slt_8_d(void)
{
	if(COND_LT())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_sgt_8_d(void)
{
	if(COND_GT())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_sle_8_d(void)
{
	if(COND_LE())
	{
		DY |= 0xff;
		USE_CYCLES(CYC_SCC_R_TRUE);
		return;
	}
	DY &= 0xffffff00;
}


static void m68k_op_shi_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_sls_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_scc_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scs_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_sne_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_seq_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_svc_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svs_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_spl_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_smi_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_sge_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_slt_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_sgt_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sle_8_ai(void)
{
	m68ki_write_8(EA_AY_AI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pi(void)
{
	m68ki_write_8(EA_AY_PI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pi7(void)
{
	m68ki_write_8(EA_A7_PI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pd(void)
{
	m68ki_write_8(EA_AY_PD_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pd7(void)
{
	m68ki_write_8(EA_A7_PD_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_di(void)
{
	m68ki_write_8(EA_AY_DI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_ix(void)
{
	m68ki_write_8(EA_AY_IX_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_aw(void)
{
	m68ki_write_8(EA_AW_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_al(void)
{
	m68ki_write_8(EA_AL_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_stop(void)
{
	if(FLAG_S)
	{
		uint new_sr = OPER_I_16();
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		CPU_STOPPED |= STOP_LEVEL_STOP;
		m68ki_set_sr(new_sr);
		/* PBC: still stopped (no interrupt taken): skip to the end of the slice */
		if(CPU_STOPPED)
			USE_ALL_CYCLES();
		return;
	}
	m68ki_exception_privilege_violation();
}


static M68K_HOT1 void m68k_op_sub_8_er_d(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_8(DY);
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_8_er_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_8_er_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pi7(void)
{
	uint* r_dst = &DX;
	uint src = OPER_A7_PI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_8_er_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pd7(void)
{
	uint* r_dst = &DX;
	uint src = OPER_A7_PD_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_sub_8_er_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_sub_8_er_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_16_er_d(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(DY);
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_16_er_a(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(AY);
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_16_er_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_16_er_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_16_er_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_sub_16_er_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT2 void m68k_op_sub_16_er_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_32_er_d(void)
{
	uint* r_dst = &DX;
	uint src = DY;
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_32_er_a(void)
{
	uint* r_dst = &DX;
	uint src = AY;
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_32_er_ai(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_AI_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_32_er_pi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PI_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_32_er_pd(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_PD_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT2 void m68k_op_sub_32_er_di(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_DI_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_ix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AY_IX_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_aw(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AW_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_al(void)
{
	uint* r_dst = &DX;
	uint src = OPER_AL_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_pcdi(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCDI_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_pcix(void)
{
	uint* r_dst = &DX;
	uint src = OPER_PCIX_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT2 void m68k_op_sub_32_er_i(void)
{
	uint* r_dst = &DX;
	uint src = OPER_I_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_sub_8_re_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_8_re_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_8_re_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_sub_8_re_di(void)
{
	uint ea = EA_AY_DI_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_aw(void)
{
	uint ea = EA_AW_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_al(void)
{
	uint ea = EA_AL_8();
	uint src = MASK_OUT_ABOVE_8(DX);
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_16_re_ai(void)
{
	uint ea = EA_AY_AI_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_16_re_pi(void)
{
	uint ea = EA_AY_PI_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_16_re_pd(void)
{
	uint ea = EA_AY_PD_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_sub_16_re_di(void)
{
	uint ea = EA_AY_DI_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_ix(void)
{
	uint ea = EA_AY_IX_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_aw(void)
{
	uint ea = EA_AW_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_al(void)
{
	uint ea = EA_AL_16();
	uint src = MASK_OUT_ABOVE_16(DX);
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_32_re_ai(void)
{
	uint ea = EA_AY_AI_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_32_re_pi(void)
{
	uint ea = EA_AY_PI_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_sub_32_re_pd(void)
{
	uint ea = EA_AY_PD_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_sub_32_re_di(void)
{
	uint ea = EA_AY_DI_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_ix(void)
{
	uint ea = EA_AY_IX_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_aw(void)
{
	uint ea = EA_AW_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_al(void)
{
	uint ea = EA_AL_32();
	uint src = DX;
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_suba_16_d(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - MAKE_INT_16(DY));
}


static M68K_HOT1 void m68k_op_suba_16_a(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - MAKE_INT_16(AY));
}


static M68K_HOT2 void m68k_op_suba_16_ai(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_AI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static M68K_HOT2 void m68k_op_suba_16_pi(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_PI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static M68K_HOT2 void m68k_op_suba_16_pd(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_PD_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_di(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_DI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_ix(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AY_IX_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_aw(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AW_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_al(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_AL_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_pcdi(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_PCDI_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_pcix(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_PCIX_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_i(void)
{
	uint* r_dst = &AX;
	uint src = MAKE_INT_16(OPER_I_16());

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static M68K_HOT1 void m68k_op_suba_32_d(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - DY);
}


static M68K_HOT1 void m68k_op_suba_32_a(void)
{
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - AY);
}


static M68K_HOT2 void m68k_op_suba_32_ai(void)
{
	uint src = OPER_AY_AI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static M68K_HOT2 void m68k_op_suba_32_pi(void)
{
	uint src = OPER_AY_PI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static M68K_HOT2 void m68k_op_suba_32_pd(void)
{
	uint src = OPER_AY_PD_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_di(void)
{
	uint src = OPER_AY_DI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_ix(void)
{
	uint src = OPER_AY_IX_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_aw(void)
{
	uint src = OPER_AW_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_al(void)
{
	uint src = OPER_AL_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_pcdi(void)
{
	uint src = OPER_PCDI_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_pcix(void)
{
	uint src = OPER_PCIX_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_i(void)
{
	uint src = OPER_I_32();
	uint* r_dst = &AX;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_subi_8_d(void)
{
	uint* r_dst = &DY;
	uint src = OPER_I_8();
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_subi_8_ai(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_AI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pi(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pi7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pd(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pd7(void)
{
	uint src = OPER_I_8();
	uint ea = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_di(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_DI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_ix(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AY_IX_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_aw(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AW_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_al(void)
{
	uint src = OPER_I_8();
	uint ea = EA_AL_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_16_d(void)
{
	uint* r_dst = &DY;
	uint src = OPER_I_16();
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_subi_16_ai(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_AI_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_pi(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PI_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_pd(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_PD_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_di(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_DI_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_ix(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AY_IX_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_aw(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AW_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_al(void)
{
	uint src = OPER_I_16();
	uint ea = EA_AL_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_32_d(void)
{
	uint* r_dst = &DY;
	uint src = OPER_I_32();
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	*r_dst = FLAG_Z;
}


static void m68k_op_subi_32_ai(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_AI_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_pi(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PI_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_pd(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_PD_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_di(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_DI_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_ix(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AY_IX_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_aw(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AW_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_al(void)
{
	uint src = OPER_I_32();
	uint ea = EA_AL_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_8_d(void)
{
	uint* r_dst = &DY;
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_subq_8_ai(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_AI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_8_pi(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_pi7(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_A7_PI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_8_pd(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_pd7(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_8_di(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_DI_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_ix(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_IX_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_subq_8_aw(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AW_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_subq_8_al(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AL_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_8(res);
	FLAG_Z = MASK_OUT_ABOVE_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	m68ki_write_8(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_16_d(void)
{
	uint* r_dst = &DY;
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static M68K_HOT1 void m68k_op_subq_16_a(void)
{
	uint* r_dst = &AY;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - ((((REG_IR >> 9) - 1) & 7) + 1));
}


static M68K_HOT1 void m68k_op_subq_16_ai(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_AI_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_16_pi(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PI_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_16_pd(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PD_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_16_di(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_DI_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_ix(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_IX_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_subq_16_aw(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AW_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_subq_16_al(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AL_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_16(res);
	FLAG_Z = MASK_OUT_ABOVE_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	m68ki_write_16(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_32_d(void)
{
	uint* r_dst = &DY;
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint dst = *r_dst;
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	*r_dst = FLAG_Z;
}


static M68K_HOT1 void m68k_op_subq_32_a(void)
{
	uint* r_dst = &AY;

	*r_dst = MASK_OUT_ABOVE_32(*r_dst - ((((REG_IR >> 9) - 1) & 7) + 1));
}


static M68K_HOT1 void m68k_op_subq_32_ai(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_AI_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_32_pi(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PI_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_32_pd(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_PD_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT1 void m68k_op_subq_32_di(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_DI_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_ix(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AY_IX_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_subq_32_aw(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AW_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static M68K_HOT2 void m68k_op_subq_32_al(void)
{
	uint src = (((REG_IR >> 9) - 1) & 7) + 1;
	uint ea = EA_AL_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = MASK_OUT_ABOVE_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subx_8_rr(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_8(DY);
	uint dst = MASK_OUT_ABOVE_8(*r_dst);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_subx_16_rr(void)
{
	uint* r_dst = &DX;
	uint src = MASK_OUT_ABOVE_16(DY);
	uint dst = MASK_OUT_ABOVE_16(*r_dst);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	*r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
}


static void m68k_op_subx_32_rr(void)
{
	uint* r_dst = &DX;
	uint src = DY;
	uint dst = *r_dst;
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	*r_dst = res;
}


static void m68k_op_subx_8_mm_ax7(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_subx_8_mm_ay7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_subx_8_mm_axy7(void)
{
	uint src = OPER_A7_PD_8();
	uint ea  = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_subx_8_mm(void)
{
	uint src = OPER_AY_PD_8();
	uint ea  = EA_AX_PD_8();
	uint dst = m68ki_read_8(ea);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_8(res);
	FLAG_X = FLAG_C = CFLAG_8(res);
	FLAG_V = VFLAG_SUB_8(src, dst, res);

	res = MASK_OUT_ABOVE_8(res);
	FLAG_Z |= res;

	m68ki_write_8(ea, res);
}


static void m68k_op_subx_16_mm(void)
{
	uint src = OPER_AY_PD_16();
	uint ea  = EA_AX_PD_16();
	uint dst = m68ki_read_16(ea);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_16(res);
	FLAG_X = FLAG_C = CFLAG_16(res);
	FLAG_V = VFLAG_SUB_16(src, dst, res);

	res = MASK_OUT_ABOVE_16(res);
	FLAG_Z |= res;

	m68ki_write_16(ea, res);
}


static void m68k_op_subx_32_mm(void)
{
	uint src = OPER_AY_PD_32();
	uint ea  = EA_AX_PD_32();
	uint dst = m68ki_read_32(ea);
	uint res = dst - src - XFLAG_AS_1();

	FLAG_N = NFLAG_32(res);
	FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
	FLAG_V = VFLAG_SUB_32(src, dst, res);

	res = MASK_OUT_ABOVE_32(res);
	FLAG_Z |= res;

	m68ki_write_32(ea, res);
}


static M68K_HOT1 void m68k_op_swap_32(void)
{
	uint* r_dst = &DY;

	FLAG_Z = MASK_OUT_ABOVE_32(*r_dst<<16);
	*r_dst = (*r_dst>>16) | FLAG_Z;

	FLAG_Z = *r_dst;
	FLAG_N = NFLAG_32(*r_dst);
	FLAG_C = CFLAG_CLEAR;
	FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_tas_8_d(void)
{
	uint* r_dst = &DY;

	FLAG_Z = MASK_OUT_ABOVE_8(*r_dst);
	FLAG_N = NFLAG_8(*r_dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
	*r_dst |= 0x80;
}


static void m68k_op_tas_8_ai(void)
{
	uint ea = EA_AY_AI_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pi(void)
{
	uint ea = EA_AY_PI_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pi7(void)
{
	uint ea = EA_A7_PI_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pd(void)
{
	uint ea = EA_AY_PD_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pd7(void)
{
	uint ea = EA_A7_PD_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_di(void)
{
	uint ea = EA_AY_DI_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_ix(void)
{
	uint ea = EA_AY_IX_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_aw(void)
{
	uint ea = EA_AW_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_al(void)
{
	uint ea = EA_AL_8();
	uint dst = m68ki_read_8(ea);
	uint allow_writeback;

	FLAG_Z = dst;
	FLAG_N = NFLAG_8(dst);
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;

	/* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
	allow_writeback = m68ki_tas_callback();

	if (allow_writeback==1) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_trap(void)
{
	/* Trap#n stacks exception frame type 0 */
	m68ki_exception_trapN(EXCEPTION_TRAP_BASE + (REG_IR & 0xf));	/* HJB 990403 */
}


static void m68k_op_trapv(void)
{
	if(COND_VC())
	{
		return;
	}
	m68ki_exception_trap(EXCEPTION_TRAPV);  /* HJB 990403 */
}


static M68K_HOT1 void m68k_op_tst_8_d(void)
{
	uint res = MASK_OUT_ABOVE_8(DY);

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_8_ai(void)
{
	uint res = OPER_AY_AI_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_8_pi(void)
{
	uint res = OPER_AY_PI_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_pi7(void)
{
	uint res = OPER_A7_PI_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_8_pd(void)
{
	uint res = OPER_AY_PD_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_pd7(void)
{
	uint res = OPER_A7_PD_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_8_di(void)
{
	uint res = OPER_AY_DI_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_ix(void)
{
	uint res = OPER_AY_IX_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_tst_8_aw(void)
{
	uint res = OPER_AW_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_tst_8_al(void)
{
	uint res = OPER_AL_8();

	FLAG_N = NFLAG_8(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_16_d(void)
{
	uint res = MASK_OUT_ABOVE_16(DY);

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_16_ai(void)
{
	uint res = OPER_AY_AI_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_16_pi(void)
{
	uint res = OPER_AY_PI_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_16_pd(void)
{
	uint res = OPER_AY_PD_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_16_di(void)
{
	uint res = OPER_AY_DI_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_ix(void)
{
	uint res = OPER_AY_IX_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_tst_16_aw(void)
{
	uint res = OPER_AW_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_tst_16_al(void)
{
	uint res = OPER_AL_16();

	FLAG_N = NFLAG_16(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_32_d(void)
{
	uint res = DY;

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_32_ai(void)
{
	uint res = OPER_AY_AI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_32_pi(void)
{
	uint res = OPER_AY_PI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_32_pd(void)
{
	uint res = OPER_AY_PD_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT1 void m68k_op_tst_32_di(void)
{
	uint res = OPER_AY_DI_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_ix(void)
{
	uint res = OPER_AY_IX_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_tst_32_aw(void)
{
	uint res = OPER_AW_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static M68K_HOT2 void m68k_op_tst_32_al(void)
{
	uint res = OPER_AL_32();

	FLAG_N = NFLAG_32(res);
	FLAG_Z = res;
	FLAG_V = VFLAG_CLEAR;
	FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_unlk_32_a7(void)
{
	REG_A[7] = m68ki_read_32(REG_A[7]);
}


static void m68k_op_unlk_32(void)
{
	uint* r_dst = &AY;

	REG_A[7] = *r_dst;
	*r_dst = m68ki_pull_32();
}


/* 1701 handlers; 1703 handler/cycle pairs, 379 runs of
 * 32 opcodes, 41976 bytes in RAM. Cycles are master clocks. */

#define M68KI_IN_RAM __attribute__((section(".data.m68ki_dispatch")))

M68KI_IN_RAM const m68ki_op_t m68ki_ops[1703] = {
  {m68k_op_ori_8_d, 56},
  {m68k_op_illegal, 0},
  {m68k_op_ori_8_ai, 112},
  {m68k_op_ori_8_pi, 112},
  {m68k_op_ori_8_pi7, 112},
  {m68k_op_ori_8_pd, 126},
  {m68k_op_ori_8_pd7, 126},
  {m68k_op_ori_8_di, 140},
  {m68k_op_ori_8_ix, 154},
  {m68k_op_ori_8_aw, 140},
  {m68k_op_ori_8_al, 168},
  {m68k_op_ori_16_toc, 140},
  {m68k_op_ori_16_d, 56},
  {m68k_op_ori_16_ai, 112},
  {m68k_op_ori_16_pi, 112},
  {m68k_op_ori_16_pd, 126},
  {m68k_op_ori_16_di, 140},
  {m68k_op_ori_16_ix, 154},
  {m68k_op_ori_16_aw, 140},
  {m68k_op_ori_16_al, 168},
  {m68k_op_ori_16_tos, 140},
  {m68k_op_ori_32_d, 112},
  {m68k_op_ori_32_ai, 196},
  {m68k_op_ori_32_pi, 196},
  {m68k_op_ori_32_pd, 210},
  {m68k_op_ori_32_di, 224},
  {m68k_op_ori_32_ix, 238},
  {m68k_op_ori_32_aw, 224},
  {m68k_op_ori_32_al, 252},
  {m68k_op_btst_32_r_d, 42},
  {m68k_op_movep_16_er, 112},
  {m68k_op_btst_8_r_ai, 56},
  {m68k_op_btst_8_r_pi, 56},
  {m68k_op_btst_8_r_pi7, 56},
  {m68k_op_btst_8_r_pd, 70},
  {m68k_op_btst_8_r_pd7, 70},
  {m68k_op_btst_8_r_di, 84},
  {m68k_op_btst_8_r_ix, 98},
  {m68k_op_btst_8_r_aw, 84},
  {m68k_op_btst_8_r_al, 112},
  {m68k_op_btst_8_r_pcdi, 84},
  {m68k_op_btst_8_r_pcix, 98},
  {m68k_op_btst_8_r_i, 56},
  {m68k_op_bchg_32_r_d, 56},
  {m68k_op_movep_32_er, 168},
  {m68k_op_bchg_8_r_ai, 84},
  {m68k_op_bchg_8_r_pi, 84},
  {m68k_op_bchg_8_r_pi7, 84},
  {m68k_op_bchg_8_r_pd, 98},
  {m68k_op_bchg_8_r_pd7, 98},
  {m68k_op_bchg_8_r_di, 112},
  {m68k_op_bchg_8_r_ix, 126},
  {m68k_op_bchg_8_r_aw, 112},
  {m68k_op_bchg_8_r_al, 140},
  {m68k_op_bclr_32_r_d, 70},
  {m68k_op_movep_16_re, 112},
  {m68k_op_bclr_8_r_ai, 84},
  {m68k_op_bclr_8_r_pi, 84},
  {m68k_op_bclr_8_r_pi7, 84},
  {m68k_op_bclr_8_r_pd, 98},
  {m68k_op_bclr_8_r_pd7, 98},
  {m68k_op_bclr_8_r_di, 112},
  {m68k_op_bclr_8_r_ix, 126},
  {m68k_op_bclr_8_r_aw, 112},
  {m68k_op_bclr_8_r_al, 140},
  {m68k_op_bset_32_r_d, 56},
  {m68k_op_movep_32_re, 168},
  {m68k_op_bset_8_r_ai, 84},
  {m68k_op_bset_8_r_pi, 84},
  {m68k_op_bset_8_r_pi7, 84},
  {m68k_op_bset_8_r_pd, 98},
  {m68k_op_bset_8_r_pd7, 98},
  {m68k_op_bset_8_r_di, 112},
  {m68k_op_bset_8_r_ix, 126},
  {m68k_op_bset_8_r_aw, 112},
  {m68k_op_bset_8_r_al, 140},
  {m68k_op_andi_8_d, 56},
  {m68k_op_andi_8_ai, 112},
  {m68k_op_andi_8_pi, 112},
  {m68k_op_andi_8_pi7, 112},
  {m68k_op_andi_8_pd, 126},
  {m68k_op_andi_8_pd7, 126},
  {m68k_op_andi_8_di, 140},
  {m68k_op_andi_8_ix, 154},
  {m68k_op_andi_8_aw, 140},
  {m68k_op_andi_8_al, 168},
  {m68k_op_andi_16_toc, 140},
  {m68k_op_andi_16_d, 56},
  {m68k_op_andi_16_ai, 112},
  {m68k_op_andi_16_pi, 112},
  {m68k_op_andi_16_pd, 126},
  {m68k_op_andi_16_di, 140},
  {m68k_op_andi_16_ix, 154},
  {m68k_op_andi_16_aw, 140},
  {m68k_op_andi_16_al, 168},
  {m68k_op_andi_16_tos, 140},
  {m68k_op_andi_32_d, 112},
  {m68k_op_andi_32_ai, 196},
  {m68k_op_andi_32_pi, 196},
  {m68k_op_andi_32_pd, 210},
  {m68k_op_andi_32_di, 224},
  {m68k_op_andi_32_ix, 238},
  {m68k_op_andi_32_aw, 224},
  {m68k_op_andi_32_al, 252},
  {m68k_op_subi_8_d, 56},
  {m68k_op_subi_8_ai, 112},
  {m68k_op_subi_8_pi, 112},
  {m68k_op_subi_8_pi7, 112},
  {m68k_op_subi_8_pd, 126},
  {m68k_op_subi_8_pd7, 126},
  {m68k_op_subi_8_di, 140},
  {m68k_op_subi_8_ix, 154},
  {m68k_op_subi_8_aw, 140},
  {m68k_op_subi_8_al, 168},
  {m68k_op_subi_16_d, 56},
  {m68k_op_subi_16_ai, 112},
  {m68k_op_subi_16_pi, 112},
  {m68k_op_subi_16_pd, 126},
  {m68k_op_subi_16_di, 140},
  {m68k_op_subi_16_ix, 154},
  {m68k_op_subi_16_aw, 140},
  {m68k_op_subi_16_al, 168},
  {m68k_op_subi_32_d, 112},
  {m68k_op_subi_32_ai, 196},
  {m68k_op_subi_32_pi, 196},
  {m68k_op_subi_32_pd, 210},
  {m68k_op_subi_32_di, 224},
  {m68k_op_subi_32_ix, 238},
  {m68k_op_subi_32_aw, 224},
  {m68k_op_subi_32_al, 252},
  {m68k_op_addi_8_d, 56},
  {m68k_op_addi_8_ai, 112},
  {m68k_op_addi_8_pi, 112},
  {m68k_op_addi_8_pi7, 112},
  {m68k_op_addi_8_pd, 126},
  {m68k_op_addi_8_pd7, 126},
  {m68k_op_addi_8_di, 140},
  {m68k_op_addi_8_ix, 154},
  {m68k_op_addi_8_aw, 140},
  {m68k_op_addi_8_al, 168},
  {m68k_op_addi_16_d, 56},
  {m68k_op_addi_16_ai, 112},
  {m68k_op_addi_16_pi, 112},
  {m68k_op_addi_16_pd, 126},
  {m68k_op_addi_16_di, 140},
  {m68k_op_addi_16_ix, 154},
  {m68k_op_addi_16_aw, 140},
  {m68k_op_addi_16_al, 168},
  {m68k_op_addi_32_d, 112},
  {m68k_op_addi_32_ai, 196},
  {m68k_op_addi_32_pi, 196},
  {m68k_op_addi_32_pd, 210},
  {m68k_op_addi_32_di, 224},
  {m68k_op_addi_32_ix, 238},
  {m68k_op_addi_32_aw, 224},
  {m68k_op_addi_32_al, 252},
  {m68k_op_btst_32_s_d, 70},
  {m68k_op_btst_8_s_ai, 84},
  {m68k_op_btst_8_s_pi, 84},
  {m68k_op_btst_8_s_pi7, 84},
  {m68k_op_btst_8_s_pd, 98},
  {m68k_op_btst_8_s_pd7, 98},
  {m68k_op_btst_8_s_di, 112},
  {m68k_op_btst_8_s_ix, 126},
  {m68k_op_btst_8_s_aw, 112},
  {m68k_op_btst_8_s_al, 140},
  {m68k_op_btst_8_s_pcdi, 112},
  {m68k_op_btst_8_s_pcix, 126},
  {m68k_op_bchg_32_s_d, 84},
  {m68k_op_bchg_8_s_ai, 112},
  {m68k_op_bchg_8_s_pi, 112},
  {m68k_op_bchg_8_s_pi7, 112},
  {m68k_op_bchg_8_s_pd, 126},
  {m68k_op_bchg_8_s_pd7, 126},
  {m68k_op_bchg_8_s_di, 140},
  {m68k_op_bchg_8_s_ix, 154},
  {m68k_op_bchg_8_s_aw, 140},
  {m68k_op_bchg_8_s_al, 168},
  {m68k_op_bclr_32_s_d, 98},
  {m68k_op_bclr_8_s_ai, 112},
  {m68k_op_bclr_8_s_pi, 112},
  {m68k_op_bclr_8_s_pi7, 112},
  {m68k_op_bclr_8_s_pd, 126},
  {m68k_op_bclr_8_s_pd7, 126},
  {m68k_op_bclr_8_s_di, 140},
  {m68k_op_bclr_8_s_ix, 154},
  {m68k_op_bclr_8_s_aw, 140},
  {m68k_op_bclr_8_s_al, 168},
  {m68k_op_bset_32_s_d, 84},
  {m68k_op_bset_8_s_ai, 112},
  {m68k_op_bset_8_s_pi, 112},
  {m68k_op_bset_8_s_pi7, 112},
  {m68k_op_bset_8_s_pd, 126},
  {m68k_op_bset_8_s_pd7, 126},
  {m68k_op_bset_8_s_di, 140},
  {m68k_op_bset_8_s_ix, 154},
  {m68k_op_bset_8_s_aw, 140},
  {m68k_op_bset_8_s_al, 168},
  {m68k_op_eori_8_d, 56},
  {m68k_op_eori_8_ai, 112},
  {m68k_op_eori_8_pi, 112},
  {m68k_op_eori_8_pi7, 112},
  {m68k_op_eori_8_pd, 126},
  {m68k_op_eori_8_pd7, 126},
  {m68k_op_eori_8_di, 140},
  {m68k_op_eori_8_ix, 154},
  {m68k_op_eori_8_aw, 140},
  {m68k_op_eori_8_al, 168},
  {m68k_op_eori_16_toc, 140},
  {m68k_op_eori_16_d, 56},
  {m68k_op_eori_16_ai, 112},
  {m68k_op_eori_16_pi, 112},
  {m68k_op_eori_16_pd, 126},
  {m68k_op_eori_16_di, 140},
  {m68k_op_eori_16_ix, 154},
  {m68k_op_eori_16_aw, 140},
  {m68k_op_eori_16_al, 168},
  {m68k_op_eori_16_tos, 140},
  {m68k_op_eori_32_d, 112},
  {m68k_op_eori_32_ai, 196},
  {m68k_op_eori_32_pi, 196},
  {m68k_op_eori_32_pd, 210},
  {m68k_op_eori_32_di, 224},
  {m68k_op_eori_32_ix, 238},
  {m68k_op_eori_32_aw, 224},
  {m68k_op_eori_32_al, 252},
  {m68k_op_cmpi_8_d, 56},
  {m68k_op_cmpi_8_ai, 84},
  {m68k_op_cmpi_8_pi, 84},
  {m68k_op_cmpi_8_pi7, 84},
  {m68k_op_cmpi_8_pd, 98},
  {m68k_op_cmpi_8_pd7, 98},
  {m68k_op_cmpi_8_di, 112},
  {m68k_op_cmpi_8_ix, 126},
  {m68k_op_cmpi_8_aw, 112},
  {m68k_op_cmpi_8_al, 140},
  {m68k_op_cmpi_16_d, 56},
  {m68k_op_cmpi_16_ai, 84},
  {m68k_op_cmpi_16_pi, 84},
  {m68k_op_cmpi_16_pd, 98},
  {m68k_op_cmpi_16_di, 112},
  {m68k_op_cmpi_16_ix, 126},
  {m68k_op_cmpi_16_aw, 112},
  {m68k_op_cmpi_16_al, 140},
  {m68k_op_cmpi_32_d, 98},
  {m68k_op_cmpi_32_ai, 140},
  {m68k_op_cmpi_32_pi, 140},
  {m68k_op_cmpi_32_pd, 154},
  {m68k_op_cmpi_32_di, 168},
  {m68k_op_cmpi_32_ix, 182},
  {m68k_op_cmpi_32_aw, 168},
  {m68k_op_cmpi_32_al, 196},
  {m68k_op_move_8_d_d, 28},
  {m68k_op_move_8_d_ai, 56},
  {m68k_op_move_8_d_pi, 56},
  {m68k_op_move_8_d_pi7, 56},
  {m68k_op_move_8_d_pd, 70},
  {m68k_op_move_8_d_pd7, 70},
  {m68k_op_move_8_d_di, 84},
  {m68k_op_move_8_d_ix, 98},
  {m68k_op_move_8_d_aw, 84},
  {m68k_op_move_8_d_al, 112},
  {m68k_op_move_8_d_pcdi, 84},
  {m68k_op_move_8_d_pcix, 98},
  {m68k_op_move_8_d_i, 56},
  {m68k_op_move_8_ai_d, 56},
  {m68k_op_move_8_ai_ai, 84},
  {m68k_op_move_8_ai_pi, 84},
  {m68k_op_move_8_ai_pi7, 84},
  {m68k_op_move_8_ai_pd, 98},
  {m68k_op_move_8_ai_pd7, 98},
  {m68k_op_move_8_ai_di, 112},
  {m68k_op_move_8_ai_ix, 126},
  {m68k_op_move_8_ai_aw, 112},
  {m68k_op_move_8_ai_al, 140},
  {m68k_op_move_8_ai_pcdi, 112},
  {m68k_op_move_8_ai_pcix, 126},
  {m68k_op_move_8_ai_i, 84},
  {m68k_op_move_8_pi_d, 56},
  {m68k_op_move_8_pi_ai, 84},
  {m68k_op_move_8_pi_pi, 84},
  {m68k_op_move_8_pi_pi7, 84},
  {m68k_op_move_8_pi_pd, 98},
  {m68k_op_move_8_pi_pd7, 98},
  {m68k_op_move_8_pi_di, 112},
  {m68k_op_move_8_pi_ix, 126},
  {m68k_op_move_8_pi_aw, 112},
  {m68k_op_move_8_pi_al, 140},
  {m68k_op_move_8_pi_pcdi, 112},
  {m68k_op_move_8_pi_pcix, 126},
  {m68k_op_move_8_pi_i, 84},
  {m68k_op_move_8_pd_d, 56},
  {m68k_op_move_8_pd_ai, 84},
  {m68k_op_move_8_pd_pi, 84},
  {m68k_op_move_8_pd_pi7, 84},
  {m68k_op_move_8_pd_pd, 98},
  {m68k_op_move_8_pd_pd7, 98},
  {m68k_op_move_8_pd_di, 112},
  {m68k_op_move_8_pd_ix, 126},
  {m68k_op_move_8_pd_aw, 112},
  {m68k_op_move_8_pd_al, 140},
  {m68k_op_move_8_pd_pcdi, 112},
  {m68k_op_move_8_pd_pcix, 126},
  {m68k_op_move_8_pd_i, 84},
  {m68k_op_move_8_di_d, 84},
  {m68k_op_move_8_di_ai, 112},
  {m68k_op_move_8_di_pi, 112},
  {m68k_op_move_8_di_pi7, 112},
  {m68k_op_move_8_di_pd, 126},
  {m68k_op_move_8_di_pd7, 126},
  {m68k_op_move_8_di_di, 140},
  {m68k_op_move_8_di_ix, 154},
  {m68k_op_move_8_di_aw, 140},
  {m68k_op_move_8_di_al, 168},
  {m68k_op_move_8_di_pcdi, 140},
  {m68k_op_move_8_di_pcix, 154},
  {m68k_op_move_8_di_i, 112},
  {m68k_op_move_8_ix_d, 98},
  {m68k_op_move_8_ix_ai, 126},
  {m68k_op_move_8_ix_pi, 126},
  {m68k_op_move_8_ix_pi7, 126},
  {m68k_op_move_8_ix_pd, 140},
  {m68k_op_move_8_ix_pd7, 140},
  {m68k_op_move_8_ix_di, 154},
  {m68k_op_move_8_ix_ix, 168},
  {m68k_op_move_8_ix_aw, 154},
  {m68k_op_move_8_ix_al, 182},
  {m68k_op_move_8_ix_pcdi, 154},
  {m68k_op_move_8_ix_pcix, 168},
  {m68k_op_move_8_ix_i, 126},
  {m68k_op_move_8_aw_d, 84},
  {m68k_op_move_8_aw_ai, 112},
  {m68k_op_move_8_aw_pi, 112},
  {m68k_op_move_8_aw_pi7, 112},
  {m68k_op_move_8_aw_pd, 126},
  {m68k_op_move_8_aw_pd7, 126},
  {m68k_op_move_8_aw_di, 140},
  {m68k_op_move_8_aw_ix, 154},
  {m68k_op_move_8_aw_aw, 140},
  {m68k_op_move_8_aw_al, 168},
  {m68k_op_move_8_aw_pcdi, 140},
  {m68k_op_move_8_aw_pcix, 154},
  {m68k_op_move_8_aw_i, 112},
  {m68k_op_move_8_al_d, 112},
  {m68k_op_move_8_al_ai, 140},
  {m68k_op_move_8_al_pi, 140},
  {m68k_op_move_8_al_pi7, 140},
  {m68k_op_move_8_al_pd, 154},
  {m68k_op_move_8_al_pd7, 154},
  {m68k_op_move_8_al_di, 168},
  {m68k_op_move_8_al_ix, 182},
  {m68k_op_move_8_al_aw, 168},
  {m68k_op_move_8_al_al, 196},
  {m68k_op_move_8_al_pcdi, 168},
  {m68k_op_move_8_al_pcix, 182},
  {m68k_op_move_8_al_i, 140},
  {m68k_op_move_8_pi7_d, 56},
  {m68k_op_move_8_pi7_ai, 84},
  {m68k_op_move_8_pi7_pi, 84},
  {m68k_op_move_8_pi7_pi7, 84},
  {m68k_op_move_8_pi7_pd, 98},
  {m68k_op_move_8_pi7_pd7, 98},
  {m68k_op_move_8_pi7_di, 112},
  {m68k_op_move_8_pi7_ix, 126},
  {m68k_op_move_8_pi7_aw, 112},
  {m68k_op_move_8_pi7_al, 140},
  {m68k_op_move_8_pi7_pcdi, 112},
  {m68k_op_move_8_pi7_pcix, 126},
  {m68k_op_move_8_pi7_i, 84},
  {m68k_op_move_8_pd7_d, 56},
  {m68k_op_move_8_pd7_ai, 84},
  {m68k_op_move_8_pd7_pi, 84},
  {m68k_op_move_8_pd7_pi7, 84},
  {m68k_op_move_8_pd7_pd, 98},
  {m68k_op_move_8_pd7_pd7, 98},
  {m68k_op_move_8_pd7_di, 112},
  {m68k_op_move_8_pd7_ix, 126},
  {m68k_op_move_8_pd7_aw, 112},
  {m68k_op_move_8_pd7_al, 140},
  {m68k_op_move_8_pd7_pcdi, 112},
  {m68k_op_move_8_pd7_pcix, 126},
  {m68k_op_move_8_pd7_i, 84},
  {m68k_op_move_32_d_d, 28},
  {m68k_op_move_32_d_a, 28},
  {m68k_op_move_32_d_ai, 84},
  {m68k_op_move_32_d_pi, 84},
  {m68k_op_move_32_d_pd, 98},
  {m68k_op_move_32_d_di, 112},
  {m68k_op_move_32_d_ix, 126},
  {m68k_op_move_32_d_aw, 112},
  {m68k_op_move_32_d_al, 140},
  {m68k_op_move_32_d_pcdi, 112},
  {m68k_op_move_32_d_pcix, 126},
  {m68k_op_move_32_d_i, 84},
  {m68k_op_movea_32_d, 28},
  {m68k_op_movea_32_a, 28},
  {m68k_op_movea_32_ai, 84},
  {m68k_op_movea_32_pi, 84},
  {m68k_op_movea_32_pd, 98},
  {m68k_op_movea_32_di, 112},
  {m68k_op_movea_32_ix, 126},
  {m68k_op_movea_32_aw, 112},
  {m68k_op_movea_32_al, 140},
  {m68k_op_movea_32_pcdi, 112},
  {m68k_op_movea_32_pcix, 126},
  {m68k_op_movea_32_i, 84},
  {m68k_op_move_32_ai_d, 84},
  {m68k_op_move_32_ai_a, 84},
  {m68k_op_move_32_ai_ai, 140},
  {m68k_op_move_32_ai_pi, 140},
  {m68k_op_move_32_ai_pd, 154},
  {m68k_op_move_32_ai_di, 168},
  {m68k_op_move_32_ai_ix, 182},
  {m68k_op_move_32_ai_aw, 168},
  {m68k_op_move_32_ai_al, 196},
  {m68k_op_move_32_ai_pcdi, 168},
  {m68k_op_move_32_ai_pcix, 182},
  {m68k_op_move_32_ai_i, 140},
  {m68k_op_move_32_pi_d, 84},
  {m68k_op_move_32_pi_a, 84},
  {m68k_op_move_32_pi_ai, 140},
  {m68k_op_move_32_pi_pi, 140},
  {m68k_op_move_32_pi_pd, 154},
  {m68k_op_move_32_pi_di, 168},
  {m68k_op_move_32_pi_ix, 182},
  {m68k_op_move_32_pi_aw, 168},
  {m68k_op_move_32_pi_al, 196},
  {m68k_op_move_32_pi_pcdi, 168},
  {m68k_op_move_32_pi_pcix, 182},
  {m68k_op_move_32_pi_i, 140},
  {m68k_op_move_32_pd_d, 84},
  {m68k_op_move_32_pd_a, 84},
  {m68k_op_move_32_pd_ai, 140},
  {m68k_op_move_32_pd_pi, 140},
  {m68k_op_move_32_pd_pd, 154},
  {m68k_op_move_32_pd_di, 168},
  {m68k_op_move_32_pd_ix, 182},
  {m68k_op_move_32_pd_aw, 168},
  {m68k_op_move_32_pd_al, 196},
  {m68k_op_move_32_pd_pcdi, 168},
  {m68k_op_move_32_pd_pcix, 182},
  {m68k_op_move_32_pd_i, 140},
  {m68k_op_move_32_di_d, 112},
  {m68k_op_move_32_di_a, 112},
  {m68k_op_move_32_di_ai, 168},
  {m68k_op_move_32_di_pi, 168},
  {m68k_op_move_32_di_pd, 182},
  {m68k_op_move_32_di_di, 196},
  {m68k_op_move_32_di_ix, 210},
  {m68k_op_move_32_di_aw, 196},
  {m68k_op_move_32_di_al, 224},
  {m68k_op_move_32_di_pcdi, 196},
  {m68k_op_move_32_di_pcix, 210},
  {m68k_op_move_32_di_i, 168},
  {m68k_op_move_32_ix_d, 126},
  {m68k_op_move_32_ix_a, 126},
  {m68k_op_move_32_ix_ai, 182},
  {m68k_op_move_32_ix_pi, 182},
  {m68k_op_move_32_ix_pd, 196},
  {m68k_op_move_32_ix_di, 210},
  {m68k_op_move_32_ix_ix, 224},
  {m68k_op_move_32_ix_aw, 210},
  {m68k_op_move_32_ix_al, 238},
  {m68k_op_move_32_ix_pcdi, 210},
  {m68k_op_move_32_ix_pcix, 224},
  {m68k_op_move_32_ix_i, 182},
  {m68k_op_move_32_aw_d, 112},
  {m68k_op_move_32_aw_a, 112},
  {m68k_op_move_32_aw_ai, 168},
  {m68k_op_move_32_aw_pi, 168},
  {m68k_op_move_32_aw_pd, 182},
  {m68k_op_move_32_aw_di, 196},
  {m68k_op_move_32_aw_ix, 210},
  {m68k_op_move_32_aw_aw, 196},
  {m68k_op_move_32_aw_al, 224},
  {m68k_op_move_32_aw_pcdi, 196},
  {m68k_op_move_32_aw_pcix, 210},
  {m68k_op_move_32_aw_i, 168},
  {m68k_op_move_32_al_d, 140},
  {m68k_op_move_32_al_a, 140},
  {m68k_op_move_32_al_ai, 196},
  {m68k_op_move_32_al_pi, 196},
  {m68k_op_move_32_al_pd, 210},
  {m68k_op_move_32_al_di, 224},
  {m68k_op_move_32_al_ix, 238},
  {m68k_op_move_32_al_aw, 224},
  {m68k_op_move_32_al_al, 252},
  {m68k_op_move_32_al_pcdi, 224},
  {m68k_op_move_32_al_pcix, 238},
  {m68k_op_move_32_al_i, 196},
  {m68k_op_move_16_d_d, 28},
  {m68k_op_move_16_d_a, 28},
  {m68k_op_move_16_d_ai, 56},
  {m68k_op_move_16_d_pi, 56},
  {m68k_op_move_16_d_pd, 70},
  {m68k_op_move_16_d_di, 84},
  {m68k_op_move_16_d_ix, 98},
  {m68k_op_move_16_d_aw, 84},
  {m68k_op_move_16_d_al, 112},
  {m68k_op_move_16_d_pcdi, 84},
  {m68k_op_move_16_d_pcix, 98},
  {m68k_op_move_16_d_i, 56},
  {m68k_op_movea_16_d, 28},
  {m68k_op_movea_16_a, 28},
  {m68k_op_movea_16_ai, 56},
  {m68k_op_movea_16_pi, 56},
  {m68k_op_movea_16_pd, 70},
  {m68k_op_movea_16_di, 84},
  {m68k_op_movea_16_ix, 98},
  {m68k_op_movea_16_aw, 84},
  {m68k_op_movea_16_al, 112},
  {m68k_op_movea_16_pcdi, 84},
  {m68k_op_movea_16_pcix, 98},
  {m68k_op_movea_16_i, 56},
  {m68k_op_move_16_ai_d, 56},
  {m68k_op_move_16_ai_a, 56},
  {m68k_op_move_16_ai_ai, 84},
  {m68k_op_move_16_ai_pi, 84},
  {m68k_op_move_16_ai_pd, 98},
  {m68k_op_move_16_ai_di, 112},
  {m68k_op_move_16_ai_ix, 126},
  {m68k_op_move_16_ai_aw, 112},
  {m68k_op_move_16_ai_al, 140},
  {m68k_op_move_16_ai_pcdi, 112},
  {m68k_op_move_16_ai_pcix, 126},
  {m68k_op_move_16_ai_i, 84},
  {m68k_op_move_16_pi_d, 56},
  {m68k_op_move_16_pi_a, 56},
  {m68k_op_move_16_pi_ai, 84},
  {m68k_op_move_16_pi_pi, 84},
  {m68k_op_move_16_pi_pd, 98},
  {m68k_op_move_16_pi_di, 112},
  {m68k_op_move_16_pi_ix, 126},
  {m68k_op_move_16_pi_aw, 112},
  {m68k_op_move_16_pi_al, 140},
  {m68k_op_move_16_pi_pcdi, 112},
  {m68k_op_move_16_pi_pcix, 126},
  {m68k_op_move_16_pi_i, 84},
  {m68k_op_move_16_pd_d, 56},
  {m68k_op_move_16_pd_a, 56},
  {m68k_op_move_16_pd_ai, 84},
  {m68k_op_move_16_pd_pi, 84},
  {m68k_op_move_16_pd_pd, 98},
  {m68k_op_move_16_pd_di, 112},
  {m68k_op_move_16_pd_ix, 126},
  {m68k_op_move_16_pd_aw, 112},
  {m68k_op_move_16_pd_al, 140},
  {m68k_op_move_16_pd_pcdi, 112},
  {m68k_op_move_16_pd_pcix, 126},
  {m68k_op_move_16_pd_i, 84},
  {m68k_op_move_16_di_d, 84},
  {m68k_op_move_16_di_a, 84},
  {m68k_op_move_16_di_ai, 112},
  {m68k_op_move_16_di_pi, 112},
  {m68k_op_move_16_di_pd, 126},
  {m68k_op_move_16_di_di, 140},
  {m68k_op_move_16_di_ix, 154},
  {m68k_op_move_16_di_aw, 140},
  {m68k_op_move_16_di_al, 168},
  {m68k_op_move_16_di_pcdi, 140},
  {m68k_op_move_16_di_pcix, 154},
  {m68k_op_move_16_di_i, 112},
  {m68k_op_move_16_ix_d, 98},
  {m68k_op_move_16_ix_a, 98},
  {m68k_op_move_16_ix_ai, 126},
  {m68k_op_move_16_ix_pi, 126},
  {m68k_op_move_16_ix_pd, 140},
  {m68k_op_move_16_ix_di, 154},
  {m68k_op_move_16_ix_ix, 168},
  {m68k_op_move_16_ix_aw, 154},
  {m68k_op_move_16_ix_al, 182},
  {m68k_op_move_16_ix_pcdi, 154},
  {m68k_op_move_16_ix_pcix, 168},
  {m68k_op_move_16_ix_i, 126},
  {m68k_op_move_16_aw_d, 84},
  {m68k_op_move_16_aw_a, 84},
  {m68k_op_move_16_aw_ai, 112},
  {m68k_op_move_16_aw_pi, 112},
  {m68k_op_move_16_aw_pd, 126},
  {m68k_op_move_16_aw_di, 140},
  {m68k_op_move_16_aw_ix, 154},
  {m68k_op_move_16_aw_aw, 140},
  {m68k_op_move_16_aw_al, 168},
  {m68k_op_move_16_aw_pcdi, 140},
  {m68k_op_move_16_aw_pcix, 154},
  {m68k_op_move_16_aw_i, 112},
  {m68k_op_move_16_al_d, 112},
  {m68k_op_move_16_al_a, 112},
  {m68k_op_move_16_al_ai, 140},
  {m68k_op_move_16_al_pi, 140},
  {m68k_op_move_16_al_pd, 154},
  {m68k_op_move_16_al_di, 168},
  {m68k_op_move_16_al_ix, 182},
  {m68k_op_move_16_al_aw, 168},
  {m68k_op_move_16_al_al, 196},
  {m68k_op_move_16_al_pcdi, 168},
  {m68k_op_move_16_al_pcix, 182},
  {m68k_op_move_16_al_i, 140},
  {m68k_op_negx_8_d, 28},
  {m68k_op_negx_8_ai, 84},
  {m68k_op_negx_8_pi, 84},
  {m68k_op_negx_8_pi7, 84},
  {m68k_op_negx_8_pd, 98},
  {m68k_op_negx_8_pd7, 98},
  {m68k_op_negx_8_di, 112},
  {m68k_op_negx_8_ix, 126},
  {m68k_op_negx_8_aw, 112},
  {m68k_op_negx_8_al, 140},
  {m68k_op_negx_16_d, 28},
  {m68k_op_negx_16_ai, 84},
  {m68k_op_negx_16_pi, 84},
  {m68k_op_negx_16_pd, 98},
  {m68k_op_negx_16_di, 112},
  {m68k_op_negx_16_ix, 126},
  {m68k_op_negx_16_aw, 112},
  {m68k_op_negx_16_al, 140},
  {m68k_op_negx_32_d, 42},
  {m68k_op_negx_32_ai, 140},
  {m68k_op_negx_32_pi, 140},
  {m68k_op_negx_32_pd, 154},
  {m68k_op_negx_32_di, 168},
  {m68k_op_negx_32_ix, 182},
  {m68k_op_negx_32_aw, 168},
  {m68k_op_negx_32_al, 196},
  {m68k_op_move_16_frs_d, 42},
  {m68k_op_move_16_frs_ai, 84},
  {m68k_op_move_16_frs_pi, 84},
  {m68k_op_move_16_frs_pd, 98},
  {m68k_op_move_16_frs_di, 112},
  {m68k_op_move_16_frs_ix, 126},
  {m68k_op_move_16_frs_aw, 112},
  {m68k_op_move_16_frs_al, 140},
  {m68k_op_chk_16_d, 70},
  {m68k_op_chk_16_ai, 98},
  {m68k_op_chk_16_pi, 98},
  {m68k_op_chk_16_pd, 112},
  {m68k_op_chk_16_di, 126},
  {m68k_op_chk_16_ix, 140},
  {m68k_op_chk_16_aw, 126},
  {m68k_op_chk_16_al, 154},
  {m68k_op_chk_16_pcdi, 126},
  {m68k_op_chk_16_pcix, 140},
  {m68k_op_chk_16_i, 98},
  {m68k_op_lea_32_ai, 28},
  {m68k_op_lea_32_di, 56},
  {m68k_op_lea_32_ix, 84},
  {m68k_op_lea_32_aw, 56},
  {m68k_op_lea_32_al, 84},
  {m68k_op_lea_32_pcdi, 56},
  {m68k_op_lea_32_pcix, 84},
  {m68k_op_clr_8_d, 28},
  {m68k_op_clr_8_ai, 84},
  {m68k_op_clr_8_pi, 84},
  {m68k_op_clr_8_pi7, 84},
  {m68k_op_clr_8_pd, 98},
  {m68k_op_clr_8_pd7, 98},
  {m68k_op_clr_8_di, 112},
  {m68k_op_clr_8_ix, 126},
  {m68k_op_clr_8_aw, 112},
  {m68k_op_clr_8_al, 140},
  {m68k_op_clr_16_d, 28},
  {m68k_op_clr_16_ai, 84},
  {m68k_op_clr_16_pi, 84},
  {m68k_op_clr_16_pd, 98},
  {m68k_op_clr_16_di, 112},
  {m68k_op_clr_16_ix, 126},
  {m68k_op_clr_16_aw, 112},
  {m68k_op_clr_16_al, 140},
  {m68k_op_clr_32_d, 42},
  {m68k_op_clr_32_ai, 140},
  {m68k_op_clr_32_pi, 140},
  {m68k_op_clr_32_pd, 154},
  {m68k_op_clr_32_di, 168},
  {m68k_op_clr_32_ix, 182},
  {m68k_op_clr_32_aw, 168},
  {m68k_op_clr_32_al, 196},
  {m68k_op_neg_8_d, 28},
  {m68k_op_neg_8_ai, 84},
  {m68k_op_neg_8_pi, 84},
  {m68k_op_neg_8_pi7, 84},
  {m68k_op_neg_8_pd, 98},
  {m68k_op_neg_8_pd7, 98},
  {m68k_op_neg_8_di, 112},
  {m68k_op_neg_8_ix, 126},
  {m68k_op_neg_8_aw, 112},
  {m68k_op_neg_8_al, 140},
  {m68k_op_neg_16_d, 28},
  {m68k_op_neg_16_ai, 84},
  {m68k_op_neg_16_pi, 84},
  {m68k_op_neg_16_pd, 98},
  {m68k_op_neg_16_di, 112},
  {m68k_op_neg_16_ix, 126},
  {m68k_op_neg_16_aw, 112},
  {m68k_op_neg_16_al, 140},
  {m68k_op_neg_32_d, 42},
  {m68k_op_neg_32_ai, 140},
  {m68k_op_neg_32_pi, 140},
  {m68k_op_neg_32_pd, 154},
  {m68k_op_neg_32_di, 168},
  {m68k_op_neg_32_ix, 182},
  {m68k_op_neg_32_aw, 168},
  {m68k_op_neg_32_al, 196},
  {m68k_op_move_16_toc_d, 84},
  {m68k_op_move_16_toc_ai, 112},
  {m68k_op_move_16_toc_pi, 112},
  {m68k_op_move_16_toc_pd, 126},
  {m68k_op_move_16_toc_di, 140},
  {m68k_op_move_16_toc_ix, 154},
  {m68k_op_move_16_toc_aw, 140},
  {m68k_op_move_16_toc_al, 168},
  {m68k_op_move_16_toc_pcdi, 140},
  {m68k_op_move_16_toc_pcix, 154},
  {m68k_op_move_16_toc_i, 112},
  {m68k_op_not_8_d, 28},
  {m68k_op_not_8_ai, 84},
  {m68k_op_not_8_pi, 84},
  {m68k_op_not_8_pi7, 84},
  {m68k_op_not_8_pd, 98},
  {m68k_op_not_8_pd7, 98},
  {m68k_op_not_8_di, 112},
  {m68k_op_not_8_ix, 126},
  {m68k_op_not_8_aw, 112},
  {m68k_op_not_8_al, 140},
  {m68k_op_not_16_d, 28},
  {m68k_op_not_16_ai, 84},
  {m68k_op_not_16_pi, 84},
  {m68k_op_not_16_pd, 98},
  {m68k_op_not_16_di, 112},
  {m68k_op_not_16_ix, 126},
  {m68k_op_not_16_aw, 112},
  {m68k_op_not_16_al, 140},
  {m68k_op_not_32_d, 42},
  {m68k_op_not_32_ai, 140},
  {m68k_op_not_32_pi, 140},
  {m68k_op_not_32_pd, 154},
  {m68k_op_not_32_di, 168},
  {m68k_op_not_32_ix, 182},
  {m68k_op_not_32_aw, 168},
  {m68k_op_not_32_al, 196},
  {m68k_op_move_16_tos_d, 84},
  {m68k_op_move_16_tos_ai, 112},
  {m68k_op_move_16_tos_pi, 112},
  {m68k_op_move_16_tos_pd, 126},
  {m68k_op_move_16_tos_di, 140},
  {m68k_op_move_16_tos_ix, 154},
  {m68k_op_move_16_tos_aw, 140},
  {m68k_op_move_16_tos_al, 168},
  {m68k_op_move_16_tos_pcdi, 140},
  {m68k_op_move_16_tos_pcix, 154},
  {m68k_op_move_16_tos_i, 112},
  {m68k_op_nbcd_8_d, 42},
  {m68k_op_nbcd_8_ai, 84},
  {m68k_op_nbcd_8_pi, 84},
  {m68k_op_nbcd_8_pi7, 84},
  {m68k_op_nbcd_8_pd, 98},
  {m68k_op_nbcd_8_pd7, 98},
  {m68k_op_nbcd_8_di, 112},
  {m68k_op_nbcd_8_ix, 126},
  {m68k_op_nbcd_8_aw, 112},
  {m68k_op_nbcd_8_al, 140},
  {m68k_op_swap_32, 28},
  {m68k_op_pea_32_ai, 84},
  {m68k_op_pea_32_di, 112},
  {m68k_op_pea_32_ix, 140},
  {m68k_op_pea_32_aw, 112},
  {m68k_op_pea_32_al, 140},
  {m68k_op_pea_32_pcdi, 112},
  {m68k_op_pea_32_pcix, 140},
  {m68k_op_ext_16, 28},
  {m68k_op_movem_16_re_ai, 56},
  {m68k_op_movem_16_re_pd, 56},
  {m68k_op_movem_16_re_di, 84},
  {m68k_op_movem_16_re_ix, 98},
  {m68k_op_movem_16_re_aw, 84},
  {m68k_op_movem_16_re_al, 112},
  {m68k_op_ext_32, 28},
  {m68k_op_movem_32_re_ai, 56},
  {m68k_op_movem_32_re_pd, 56},
  {m68k_op_movem_32_re_di, 84},
  {m68k_op_movem_32_re_ix, 98},
  {m68k_op_movem_32_re_aw, 84},
  {m68k_op_movem_32_re_al, 112},
  {m68k_op_tst_8_d, 28},
  {m68k_op_tst_8_ai, 56},
  {m68k_op_tst_8_pi, 56},
  {m68k_op_tst_8_pi7, 56},
  {m68k_op_tst_8_pd, 70},
  {m68k_op_tst_8_pd7, 70},
  {m68k_op_tst_8_di, 84},
  {m68k_op_tst_8_ix, 98},
  {m68k_op_tst_8_aw, 84},
  {m68k_op_tst_8_al, 112},
  {m68k_op_tst_16_d, 28},
  {m68k_op_tst_16_ai, 56},
  {m68k_op_tst_16_pi, 56},
  {m68k_op_tst_16_pd, 70},
  {m68k_op_tst_16_di, 84},
  {m68k_op_tst_16_ix, 98},
  {m68k_op_tst_16_aw, 84},
  {m68k_op_tst_16_al, 112},
  {m68k_op_tst_32_d, 28},
  {m68k_op_tst_32_ai, 84},
  {m68k_op_tst_32_pi, 84},
  {m68k_op_tst_32_pd, 98},
  {m68k_op_tst_32_di, 112},
  {m68k_op_tst_32_ix, 126},
  {m68k_op_tst_32_aw, 112},
  {m68k_op_tst_32_al, 140},
  {m68k_op_tas_8_d, 28},
  {m68k_op_tas_8_ai, 126},
  {m68k_op_tas_8_pi, 126},
  {m68k_op_tas_8_pi7, 126},
  {m68k_op_tas_8_pd, 140},
  {m68k_op_tas_8_pd7, 140},
  {m68k_op_tas_8_di, 154},
  {m68k_op_tas_8_ix, 168},
  {m68k_op_tas_8_aw, 154},
  {m68k_op_tas_8_al, 182},
  {m68k_op_illegal, 28},
  {m68k_op_movem_16_er_ai, 84},
  {m68k_op_movem_16_er_pi, 84},
  {m68k_op_movem_16_er_di, 112},
  {m68k_op_movem_16_er_ix, 126},
  {m68k_op_movem_16_er_aw, 112},
  {m68k_op_movem_16_er_al, 140},
  {m68k_op_movem_16_er_pcdi, 112},
  {m68k_op_movem_16_er_pcix, 126},
  {m68k_op_movem_32_er_ai, 84},
  {m68k_op_movem_32_er_pi, 84},
  {m68k_op_movem_32_er_di, 112},
  {m68k_op_movem_32_er_ix, 126},
  {m68k_op_movem_32_er_aw, 112},
  {m68k_op_movem_32_er_al, 140},
  {m68k_op_movem_32_er_pcdi, 112},
  {m68k_op_movem_32_er_pcix, 126},
  {m68k_op_trap, 28},
  {m68k_op_link_16, 112},
  {m68k_op_link_16_a7, 112},
  {m68k_op_unlk_32, 84},
  {m68k_op_unlk_32_a7, 84},
  {m68k_op_move_32_tou, 28},
  {m68k_op_move_32_fru, 28},
  {m68k_op_reset, 0},
  {m68k_op_nop, 28},
  {m68k_op_stop, 28},
  {m68k_op_rte_32, 140},
  {m68k_op_rts_32, 112},
  {m68k_op_trapv, 28},
  {m68k_op_rtr_32, 140},
  {m68k_op_jsr_32_ai, 112},
  {m68k_op_jsr_32_di, 126},
  {m68k_op_jsr_32_ix, 154},
  {m68k_op_jsr_32_aw, 126},
  {m68k_op_jsr_32_al, 140},
  {m68k_op_jsr_32_pcdi, 126},
  {m68k_op_jsr_32_pcix, 154},
  {m68k_op_jmp_32_ai, 56},
  {m68k_op_jmp_32_di, 70},
  {m68k_op_jmp_32_ix, 98},
  {m68k_op_jmp_32_aw, 70},
  {m68k_op_jmp_32_al, 84},
  {m68k_op_jmp_32_pcdi, 70},
  {m68k_op_jmp_32_pcix, 98},
  {m68k_op_addq_8_d, 28},
  {m68k_op_addq_8_ai, 84},
  {m68k_op_addq_8_pi, 84},
  {m68k_op_addq_8_pi7, 84},
  {m68k_op_addq_8_pd, 98},
  {m68k_op_addq_8_pd7, 98},
  {m68k_op_addq_8_di, 112},
  {m68k_op_addq_8_ix, 126},
  {m68k_op_addq_8_aw, 112},
  {m68k_op_addq_8_al, 140},
  {m68k_op_addq_16_d, 28},
  {m68k_op_addq_16_a, 56},
  {m68k_op_addq_16_ai, 84},
  {m68k_op_addq_16_pi, 84},
  {m68k_op_addq_16_pd, 98},
  {m68k_op_addq_16_di, 112},
  {m68k_op_addq_16_ix, 126},
  {m68k_op_addq_16_aw, 112},
  {m68k_op_addq_16_al, 140},
  {m68k_op_addq_32_d, 56},
  {m68k_op_addq_32_a, 56},
  {m68k_op_addq_32_ai, 140},
  {m68k_op_addq_32_pi, 140},
  {m68k_op_addq_32_pd, 154},
  {m68k_op_addq_32_di, 168},
  {m68k_op_addq_32_ix, 182},
  {m68k_op_addq_32_aw, 168},
  {m68k_op_addq_32_al, 196},
  {m68k_op_st_8_d, 42},
  {m68k_op_dbt_16, 84},
  {m68k_op_st_8_ai, 84},
  {m68k_op_st_8_pi, 84},
  {m68k_op_st_8_pi7, 84},
  {m68k_op_st_8_pd, 98},
  {m68k_op_st_8_pd7, 98},
  {m68k_op_st_8_di, 112},
  {m68k_op_st_8_ix, 126},
  {m68k_op_st_8_aw, 112},
  {m68k_op_st_8_al, 140},
  {m68k_op_subq_8_d, 28},
  {m68k_op_subq_8_ai, 84},
  {m68k_op_subq_8_pi, 84},
  {m68k_op_subq_8_pi7, 84},
  {m68k_op_subq_8_pd, 98},
  {m68k_op_subq_8_pd7, 98},
  {m68k_op_subq_8_di, 112},
  {m68k_op_subq_8_ix, 126},
  {m68k_op_subq_8_aw, 112},
  {m68k_op_subq_8_al, 140},
  {m68k_op_subq_16_d, 28},
  {m68k_op_subq_16_a, 56},
  {m68k_op_subq_16_ai, 84},
  {m68k_op_subq_16_pi, 84},
  {m68k_op_subq_16_pd, 98},
  {m68k_op_subq_16_di, 112},
  {m68k_op_subq_16_ix, 126},
  {m68k_op_subq_16_aw, 112},
  {m68k_op_subq_16_al, 140},
  {m68k_op_subq_32_d, 56},
  {m68k_op_subq_32_a, 56},
  {m68k_op_subq_32_ai, 140},
  {m68k_op_subq_32_pi, 140},
  {m68k_op_subq_32_pd, 154},
  {m68k_op_subq_32_di, 168},
  {m68k_op_subq_32_ix, 182},
  {m68k_op_subq_32_aw, 168},
  {m68k_op_subq_32_al, 196},
  {m68k_op_sf_8_d, 28},
  {m68k_op_dbf_16, 84},
  {m68k_op_sf_8_ai, 84},
  {m68k_op_sf_8_pi, 84},
  {m68k_op_sf_8_pi7, 84},
  {m68k_op_sf_8_pd, 98},
  {m68k_op_sf_8_pd7, 98},
  {m68k_op_sf_8_di, 112},
  {m68k_op_sf_8_ix, 126},
  {m68k_op_sf_8_aw, 112},
  {m68k_op_sf_8_al, 140},
  {m68k_op_shi_8_d, 28},
  {m68k_op_dbhi_16, 84},
  {m68k_op_shi_8_ai, 84},
  {m68k_op_shi_8_pi, 84},
  {m68k_op_shi_8_pi7, 84},
  {m68k_op_shi_8_pd, 98},
  {m68k_op_shi_8_pd7, 98},
  {m68k_op_shi_8_di, 112},
  {m68k_op_shi_8_ix, 126},
  {m68k_op_shi_8_aw, 112},
  {m68k_op_shi_8_al, 140},
  {m68k_op_sls_8_d, 28},
  {m68k_op_dbls_16, 84},
  {m68k_op_sls_8_ai, 84},
  {m68k_op_sls_8_pi, 84},
  {m68k_op_sls_8_pi7, 84},
  {m68k_op_sls_8_pd, 98},
  {m68k_op_sls_8_pd7, 98},
  {m68k_op_sls_8_di, 112},
  {m68k_op_sls_8_ix, 126},
  {m68k_op_sls_8_aw, 112},
  {m68k_op_sls_8_al, 140},
  {m68k_op_scc_8_d, 28},
  {m68k_op_dbcc_16, 84},
  {m68k_op_scc_8_ai, 84},
  {m68k_op_scc_8_pi, 84},
  {m68k_op_scc_8_pi7, 84},
  {m68k_op_scc_8_pd, 98},
  {m68k_op_scc_8_pd7, 98},
  {m68k_op_scc_8_di, 112},
  {m68k_op_scc_8_ix, 126},
  {m68k_op_scc_8_aw, 112},
  {m68k_op_scc_8_al, 140},
  {m68k_op_scs_8_d, 28},
  {m68k_op_dbcs_16, 84},
  {m68k_op_scs_8_ai, 84},
  {m68k_op_scs_8_pi, 84},
  {m68k_op_scs_8_pi7, 84},
  {m68k_op_scs_8_pd, 98},
  {m68k_op_scs_8_pd7, 98},
  {m68k_op_scs_8_di, 112},
  {m68k_op_scs_8_ix, 126},
  {m68k_op_scs_8_aw, 112},
  {m68k_op_scs_8_al, 140},
  {m68k_op_sne_8_d, 28},
  {m68k_op_dbne_16, 84},
  {m68k_op_sne_8_ai, 84},
  {m68k_op_sne_8_pi, 84},
  {m68k_op_sne_8_pi7, 84},
  {m68k_op_sne_8_pd, 98},
  {m68k_op_sne_8_pd7, 98},
  {m68k_op_sne_8_di, 112},
  {m68k_op_sne_8_ix, 126},
  {m68k_op_sne_8_aw, 112},
  {m68k_op_sne_8_al, 140},
  {m68k_op_seq_8_d, 28},
  {m68k_op_dbeq_16, 84},
  {m68k_op_seq_8_ai, 84},
  {m68k_op_seq_8_pi, 84},
  {m68k_op_seq_8_pi7, 84},
  {m68k_op_seq_8_pd, 98},
  {m68k_op_seq_8_pd7, 98},
  {m68k_op_seq_8_di, 112},
  {m68k_op_seq_8_ix, 126},
  {m68k_op_seq_8_aw, 112},
  {m68k_op_seq_8_al, 140},
  {m68k_op_svc_8_d, 28},
  {m68k_op_dbvc_16, 84},
  {m68k_op_svc_8_ai, 84},
  {m68k_op_svc_8_pi, 84},
  {m68k_op_svc_8_pi7, 84},
  {m68k_op_svc_8_pd, 98},
  {m68k_op_svc_8_pd7, 98},
  {m68k_op_svc_8_di, 112},
  {m68k_op_svc_8_ix, 126},
  {m68k_op_svc_8_aw, 112},
  {m68k_op_svc_8_al, 140},
  {m68k_op_svs_8_d, 28},
  {m68k_op_dbvs_16, 84},
  {m68k_op_svs_8_ai, 84},
  {m68k_op_svs_8_pi, 84},
  {m68k_op_svs_8_pi7, 84},
  {m68k_op_svs_8_pd, 98},
  {m68k_op_svs_8_pd7, 98},
  {m68k_op_svs_8_di, 112},
  {m68k_op_svs_8_ix, 126},
  {m68k_op_svs_8_aw, 112},
  {m68k_op_svs_8_al, 140},
  {m68k_op_spl_8_d, 28},
  {m68k_op_dbpl_16, 84},
  {m68k_op_spl_8_ai, 84},
  {m68k_op_spl_8_pi, 84},
  {m68k_op_spl_8_pi7, 84},
  {m68k_op_spl_8_pd, 98},
  {m68k_op_spl_8_pd7, 98},
  {m68k_op_spl_8_di, 112},
  {m68k_op_spl_8_ix, 126},
  {m68k_op_spl_8_aw, 112},
  {m68k_op_spl_8_al, 140},
  {m68k_op_smi_8_d, 28},
  {m68k_op_dbmi_16, 84},
  {m68k_op_smi_8_ai, 84},
  {m68k_op_smi_8_pi, 84},
  {m68k_op_smi_8_pi7, 84},
  {m68k_op_smi_8_pd, 98},
  {m68k_op_smi_8_pd7, 98},
  {m68k_op_smi_8_di, 112},
  {m68k_op_smi_8_ix, 126},
  {m68k_op_smi_8_aw, 112},
  {m68k_op_smi_8_al, 140},
  {m68k_op_sge_8_d, 28},
  {m68k_op_dbge_16, 84},
  {m68k_op_sge_8_ai, 84},
  {m68k_op_sge_8_pi, 84},
  {m68k_op_sge_8_pi7, 84},
  {m68k_op_sge_8_pd, 98},
  {m68k_op_sge_8_pd7, 98},
  {m68k_op_sge_8_di, 112},
  {m68k_op_sge_8_ix, 126},
  {m68k_op_sge_8_aw, 112},
  {m68k_op_sge_8_al, 140},
  {m68k_op_slt_8_d, 28},
  {m68k_op_dblt_16, 84},
  {m68k_op_slt_8_ai, 84},
  {m68k_op_slt_8_pi, 84},
  {m68k_op_slt_8_pi7, 84},
  {m68k_op_slt_8_pd, 98},
  {m68k_op_slt_8_pd7, 98},
  {m68k_op_slt_8_di, 112},
  {m68k_op_slt_8_ix, 126},
  {m68k_op_slt_8_aw, 112},
  {m68k_op_slt_8_al, 140},
  {m68k_op_sgt_8_d, 28},
  {m68k_op_dbgt_16, 84},
  {m68k_op_sgt_8_ai, 84},
  {m68k_op_sgt_8_pi, 84},
  {m68k_op_sgt_8_pi7, 84},
  {m68k_op_sgt_8_pd, 98},
  {m68k_op_sgt_8_pd7, 98},
  {m68k_op_sgt_8_di, 112},
  {m68k_op_sgt_8_ix, 126},
  {m68k_op_sgt_8_aw, 112},
  {m68k_op_sgt_8_al, 140},
  {m68k_op_sle_8_d, 28},
  {m68k_op_dble_16, 84},
  {m68k_op_sle_8_ai, 84},
  {m68k_op_sle_8_pi, 84},
  {m68k_op_sle_8_pi7, 84},
  {m68k_op_sle_8_pd, 98},
  {m68k_op_sle_8_pd7, 98},
  {m68k_op_sle_8_di, 112},
  {m68k_op_sle_8_ix, 126},
  {m68k_op_sle_8_aw, 112},
  {m68k_op_sle_8_al, 140},
  {m68k_op_bra_16, 70},
  {m68k_op_bra_8, 70},
  {m68k_op_bra_32, 70},
  {m68k_op_bsr_16, 126},
  {m68k_op_bsr_8, 126},
  {m68k_op_bsr_32, 126},
  {m68k_op_bhi_16, 70},
  {m68k_op_bhi_8, 70},
  {m68k_op_bhi_32, 70},
  {m68k_op_bls_16, 70},
  {m68k_op_bls_8, 70},
  {m68k_op_bls_32, 70},
  {m68k_op_bcc_16, 70},
  {m68k_op_bcc_8, 70},
  {m68k_op_bcc_32, 70},
  {m68k_op_bcs_16, 70},
  {m68k_op_bcs_8, 70},
  {m68k_op_bcs_32, 70},
  {m68k_op_bne_16, 70},
  {m68k_op_bne_8, 70},
  {m68k_op_bne_32, 70},
  {m68k_op_beq_16, 70},
  {m68k_op_beq_8, 70},
  {m68k_op_beq_32, 70},
  {m68k_op_bvc_16, 70},
  {m68k_op_bvc_8, 70},
  {m68k_op_bvc_32, 70},
  {m68k_op_bvs_16, 70},
  {m68k_op_bvs_8, 70},
  {m68k_op_bvs_32, 70},
  {m68k_op_bpl_16, 70},
  {m68k_op_bpl_8, 70},
  {m68k_op_bpl_32, 70},
  {m68k_op_bmi_16, 70},
  {m68k_op_bmi_8, 70},
  {m68k_op_bmi_32, 70},
  {m68k_op_bge_16, 70},
  {m68k_op_bge_8, 70},
  {m68k_op_bge_32, 70},
  {m68k_op_blt_16, 70},
  {m68k_op_blt_8, 70},
  {m68k_op_blt_32, 70},
  {m68k_op_bgt_16, 70},
  {m68k_op_bgt_8, 70},
  {m68k_op_bgt_32, 70},
  {m68k_op_ble_16, 70},
  {m68k_op_ble_8, 70},
  {m68k_op_ble_32, 70},
  {m68k_op_moveq_32, 28},
  {m68k_op_or_8_er_d, 28},
  {m68k_op_or_8_er_ai, 56},
  {m68k_op_or_8_er_pi, 56},
  {m68k_op_or_8_er_pi7, 56},
  {m68k_op_or_8_er_pd, 70},
  {m68k_op_or_8_er_pd7, 70},
  {m68k_op_or_8_er_di, 84},
  {m68k_op_or_8_er_ix, 98},
  {m68k_op_or_8_er_aw, 84},
  {m68k_op_or_8_er_al, 112},
  {m68k_op_or_8_er_pcdi, 84},
  {m68k_op_or_8_er_pcix, 98},
  {m68k_op_or_8_er_i, 56},
  {m68k_op_or_16_er_d, 28},
  {m68k_op_or_16_er_ai, 56},
  {m68k_op_or_16_er_pi, 56},
  {m68k_op_or_16_er_pd, 70},
  {m68k_op_or_16_er_di, 84},
  {m68k_op_or_16_er_ix, 98},
  {m68k_op_or_16_er_aw, 84},
  {m68k_op_or_16_er_al, 112},
  {m68k_op_or_16_er_pcdi, 84},
  {m68k_op_or_16_er_pcix, 98},
  {m68k_op_or_16_er_i, 56},
  {m68k_op_or_32_er_d, 56},
  {m68k_op_or_32_er_ai, 98},
  {m68k_op_or_32_er_pi, 98},
  {m68k_op_or_32_er_pd, 112},
  {m68k_op_or_32_er_di, 126},
  {m68k_op_or_32_er_ix, 140},
  {m68k_op_or_32_er_aw, 126},
  {m68k_op_or_32_er_al, 154},
  {m68k_op_or_32_er_pcdi, 126},
  {m68k_op_or_32_er_pcix, 140},
  {m68k_op_or_32_er_i, 112},
  {m68k_op_divu_16_d, 980},
  {m68k_op_divu_16_ai, 1008},
  {m68k_op_divu_16_pi, 1008},
  {m68k_op_divu_16_pd, 1022},
  {m68k_op_divu_16_di, 1036},
  {m68k_op_divu_16_ix, 1050},
  {m68k_op_divu_16_aw, 1036},
  {m68k_op_divu_16_al, 1064},
  {m68k_op_divu_16_pcdi, 1036},
  {m68k_op_divu_16_pcix, 1050},
  {m68k_op_divu_16_i, 1008},
  {m68k_op_sbcd_8_rr, 42},
  {m68k_op_sbcd_8_mm, 126},
  {m68k_op_sbcd_8_mm_ay7, 126},
  {m68k_op_or_8_re_ai, 84},
  {m68k_op_or_8_re_pi, 84},
  {m68k_op_or_8_re_pi7, 84},
  {m68k_op_or_8_re_pd, 98},
  {m68k_op_or_8_re_pd7, 98},
  {m68k_op_or_8_re_di, 112},
  {m68k_op_or_8_re_ix, 126},
  {m68k_op_or_8_re_aw, 112},
  {m68k_op_or_8_re_al, 140},
  {m68k_op_or_16_re_ai, 84},
  {m68k_op_or_16_re_pi, 84},
  {m68k_op_or_16_re_pd, 98},
  {m68k_op_or_16_re_di, 112},
  {m68k_op_or_16_re_ix, 126},
  {m68k_op_or_16_re_aw, 112},
  {m68k_op_or_16_re_al, 140},
  {m68k_op_or_32_re_ai, 140},
  {m68k_op_or_32_re_pi, 140},
  {m68k_op_or_32_re_pd, 154},
  {m68k_op_or_32_re_di, 168},
  {m68k_op_or_32_re_ix, 182},
  {m68k_op_or_32_re_aw, 168},
  {m68k_op_or_32_re_al, 196},
  {m68k_op_divs_16_d, 1106},
  {m68k_op_divs_16_ai, 1134},
  {m68k_op_divs_16_pi, 1134},
  {m68k_op_divs_16_pd, 1148},
  {m68k_op_divs_16_di, 1162},
  {m68k_op_divs_16_ix, 1176},
  {m68k_op_divs_16_aw, 1162},
  {m68k_op_divs_16_al, 1190},
  {m68k_op_divs_16_pcdi, 1162},
  {m68k_op_divs_16_pcix, 1176},
  {m68k_op_divs_16_i, 1134},
  {m68k_op_sbcd_8_mm_ax7, 126},
  {m68k_op_sbcd_8_mm_axy7, 126},
  {m68k_op_sub_8_er_d, 28},
  {m68k_op_sub_8_er_ai, 56},
  {m68k_op_sub_8_er_pi, 56},
  {m68k_op_sub_8_er_pi7, 56},
  {m68k_op_sub_8_er_pd, 70},
  {m68k_op_sub_8_er_pd7, 70},
  {m68k_op_sub_8_er_di, 84},
  {m68k_op_sub_8_er_ix, 98},
  {m68k_op_sub_8_er_aw, 84},
  {m68k_op_sub_8_er_al, 112},
  {m68k_op_sub_8_er_pcdi, 84},
  {m68k_op_sub_8_er_pcix, 98},
  {m68k_op_sub_8_er_i, 56},
  {m68k_op_sub_16_er_d, 28},
  {m68k_op_sub_16_er_a, 28},
  {m68k_op_sub_16_er_ai, 56},
  {m68k_op_sub_16_er_pi, 56},
  {m68k_op_sub_16_er_pd, 70},
  {m68k_op_sub_16_er_di, 84},
  {m68k_op_sub_16_er_ix, 98},
  {m68k_op_sub_16_er_aw, 84},
  {m68k_op_sub_16_er_al, 112},
  {m68k_op_sub_16_er_pcdi, 84},
  {m68k_op_sub_16_er_pcix, 98},
  {m68k_op_sub_16_er_i, 56},
  {m68k_op_sub_32_er_d, 56},
  {m68k_op_sub_32_er_a, 56},
  {m68k_op_sub_32_er_ai, 98},
  {m68k_op_sub_32_er_pi, 98},
  {m68k_op_sub_32_er_pd, 112},
  {m68k_op_sub_32_er_di, 126},
  {m68k_op_sub_32_er_ix, 140},
  {m68k_op_sub_32_er_aw, 126},
  {m68k_op_sub_32_er_al, 154},
  {m68k_op_sub_32_er_pcdi, 126},
  {m68k_op_sub_32_er_pcix, 140},
  {m68k_op_sub_32_er_i, 112},
  {m68k_op_suba_16_d, 56},
  {m68k_op_suba_16_a, 56},
  {m68k_op_suba_16_ai, 84},
  {m68k_op_suba_16_pi, 84},
  {m68k_op_suba_16_pd, 98},
  {m68k_op_suba_16_di, 112},
  {m68k_op_suba_16_ix, 126},
  {m68k_op_suba_16_aw, 112},
  {m68k_op_suba_16_al, 140},
  {m68k_op_suba_16_pcdi, 112},
  {m68k_op_suba_16_pcix, 126},
  {m68k_op_suba_16_i, 84},
  {m68k_op_subx_8_rr, 28},
  {m68k_op_subx_8_mm, 126},
  {m68k_op_subx_8_mm_ay7, 126},
  {m68k_op_sub_8_re_ai, 84},
  {m68k_op_sub_8_re_pi, 84},
  {m68k_op_sub_8_re_pi7, 84},
  {m68k_op_sub_8_re_pd, 98},
  {m68k_op_sub_8_re_pd7, 98},
  {m68k_op_sub_8_re_di, 112},
  {m68k_op_sub_8_re_ix, 126},
  {m68k_op_sub_8_re_aw, 112},
  {m68k_op_sub_8_re_al, 140},
  {m68k_op_subx_16_rr, 28},
  {m68k_op_subx_16_mm, 126},
  {m68k_op_sub_16_re_ai, 84},
  {m68k_op_sub_16_re_pi, 84},
  {m68k_op_sub_16_re_pd, 98},
  {m68k_op_sub_16_re_di, 112},
  {m68k_op_sub_16_re_ix, 126},
  {m68k_op_sub_16_re_aw, 112},
  {m68k_op_sub_16_re_al, 140},
  {m68k_op_subx_32_rr, 56},
  {m68k_op_subx_32_mm, 210},
  {m68k_op_sub_32_re_ai, 140},
  {m68k_op_sub_32_re_pi, 140},
  {m68k_op_sub_32_re_pd, 154},
  {m68k_op_sub_32_re_di, 168},
  {m68k_op_sub_32_re_ix, 182},
  {m68k_op_sub_32_re_aw, 168},
  {m68k_op_sub_32_re_al, 196},
  {m68k_op_suba_32_d, 56},
  {m68k_op_suba_32_a, 56},
  {m68k_op_suba_32_ai, 98},
  {m68k_op_suba_32_pi, 98},
  {m68k_op_suba_32_pd, 112},
  {m68k_op_suba_32_di, 126},
  {m68k_op_suba_32_ix, 140},
  {m68k_op_suba_32_aw, 126},
  {m68k_op_suba_32_al, 154},
  {m68k_op_suba_32_pcdi, 126},
  {m68k_op_suba_32_pcix, 140},
  {m68k_op_suba_32_i, 112},
  {m68k_op_subx_8_mm_ax7, 126},
  {m68k_op_subx_8_mm_axy7, 126},
  {m68k_op_1010, 28},
  {m68k_op_cmp_8_d, 28},
  {m68k_op_cmp_8_ai, 56},
  {m68k_op_cmp_8_pi, 56},
  {m68k_op_cmp_8_pi7, 56},
  {m68k_op_cmp_8_pd, 70},
  {m68k_op_cmp_8_pd7, 70},
  {m68k_op_cmp_8_di, 84},
  {m68k_op_cmp_8_ix, 98},
  {m68k_op_cmp_8_aw, 84},
  {m68k_op_cmp_8_al, 112},
  {m68k_op_cmp_8_pcdi, 84},
  {m68k_op_cmp_8_pcix, 98},
  {m68k_op_cmp_8_i, 56},
  {m68k_op_cmp_16_d, 28},
  {m68k_op_cmp_16_a, 28},
  {m68k_op_cmp_16_ai, 56},
  {m68k_op_cmp_16_pi, 56},
  {m68k_op_cmp_16_pd, 70},
  {m68k_op_cmp_16_di, 84},
  {m68k_op_cmp_16_ix, 98},
  {m68k_op_cmp_16_aw, 84},
  {m68k_op_cmp_16_al, 112},
  {m68k_op_cmp_16_pcdi, 84},
  {m68k_op_cmp_16_pcix, 98},
  {m68k_op_cmp_16_i, 56},
  {m68k_op_cmp_32_d, 42},
  {m68k_op_cmp_32_a, 42},
  {m68k_op_cmp_32_ai, 98},
  {m68k_op_cmp_32_pi, 98},
  {m68k_op_cmp_32_pd, 112},
  {m68k_op_cmp_32_di, 126},
  {m68k_op_cmp_32_ix, 140},
  {m68k_op_cmp_32_aw, 126},
  {m68k_op_cmp_32_al, 154},
  {m68k_op_cmp_32_pcdi, 126},
  {m68k_op_cmp_32_pcix, 140},
  {m68k_op_cmp_32_i, 98},
  {m68k_op_cmpa_16_d, 42},
  {m68k_op_cmpa_16_a, 42},
  {m68k_op_cmpa_16_ai, 70},
  {m68k_op_cmpa_16_pi, 70},
  {m68k_op_cmpa_16_pd, 84},
  {m68k_op_cmpa_16_di, 98},
  {m68k_op_cmpa_16_ix, 112},
  {m68k_op_cmpa_16_aw, 98},
  {m68k_op_cmpa_16_al, 126},
  {m68k_op_cmpa_16_pcdi, 98},
  {m68k_op_cmpa_16_pcix, 112},
  {m68k_op_cmpa_16_i, 70},
  {m68k_op_eor_8_d, 28},
  {m68k_op_cmpm_8, 84},
  {m68k_op_cmpm_8_ay7, 84},
  {m68k_op_eor_8_ai, 84},
  {m68k_op_eor_8_pi, 84},
  {m68k_op_eor_8_pi7, 84},
  {m68k_op_eor_8_pd, 98},
  {m68k_op_eor_8_pd7, 98},
  {m68k_op_eor_8_di, 112},
  {m68k_op_eor_8_ix, 126},
  {m68k_op_eor_8_aw, 112},
  {m68k_op_eor_8_al, 140},
  {m68k_op_eor_16_d, 28},
  {m68k_op_cmpm_16, 84},
  {m68k_op_eor_16_ai, 84},
  {m68k_op_eor_16_pi, 84},
  {m68k_op_eor_16_pd, 98},
  {m68k_op_eor_16_di, 112},
  {m68k_op_eor_16_ix, 126},
  {m68k_op_eor_16_aw, 112},
  {m68k_op_eor_16_al, 140},
  {m68k_op_eor_32_d, 56},
  {m68k_op_cmpm_32, 140},
  {m68k_op_eor_32_ai, 140},
  {m68k_op_eor_32_pi, 140},
  {m68k_op_eor_32_pd, 154},
  {m68k_op_eor_32_di, 168},
  {m68k_op_eor_32_ix, 182},
  {m68k_op_eor_32_aw, 168},
  {m68k_op_eor_32_al, 196},
  {m68k_op_cmpa_32_d, 42},
  {m68k_op_cmpa_32_a, 42},
  {m68k_op_cmpa_32_ai, 98},
  {m68k_op_cmpa_32_pi, 98},
  {m68k_op_cmpa_32_pd, 112},
  {m68k_op_cmpa_32_di, 126},
  {m68k_op_cmpa_32_ix, 140},
  {m68k_op_cmpa_32_aw, 126},
  {m68k_op_cmpa_32_al, 154},
  {m68k_op_cmpa_32_pcdi, 126},
  {m68k_op_cmpa_32_pcix, 140},
  {m68k_op_cmpa_32_i, 98},
  {m68k_op_cmpm_8_ax7, 84},
  {m68k_op_cmpm_8_axy7, 84},
  {m68k_op_and_8_er_d, 28},
  {m68k_op_and_8_er_ai, 56},
  {m68k_op_and_8_er_pi, 56},
  {m68k_op_and_8_er_pi7, 56},
  {m68k_op_and_8_er_pd, 70},
  {m68k_op_and_8_er_pd7, 70},
  {m68k_op_and_8_er_di, 84},
  {m68k_op_and_8_er_ix, 98},
  {m68k_op_and_8_er_aw, 84},
  {m68k_op_and_8_er_al, 112},
  {m68k_op_and_8_er_pcdi, 84},
  {m68k_op_and_8_er_pcix, 98},
  {m68k_op_and_8_er_i, 56},
  {m68k_op_and_16_er_d, 28},
  {m68k_op_and_16_er_ai, 56},
  {m68k_op_and_16_er_pi, 56},
  {m68k_op_and_16_er_pd, 70},
  {m68k_op_and_16_er_di, 84},
  {m68k_op_and_16_er_ix, 98},
  {m68k_op_and_16_er_aw, 84},
  {m68k_op_and_16_er_al, 112},
  {m68k_op_and_16_er_pcdi, 84},
  {m68k_op_and_16_er_pcix, 98},
  {m68k_op_and_16_er_i, 56},
  {m68k_op_and_32_er_d, 56},
  {m68k_op_and_32_er_ai, 98},
  {m68k_op_and_32_er_pi, 98},
  {m68k_op_and_32_er_pd, 112},
  {m68k_op_and_32_er_di, 126},
  {m68k_op_and_32_er_ix, 140},
  {m68k_op_and_32_er_aw, 126},
  {m68k_op_and_32_er_al, 154},
  {m68k_op_and_32_er_pcdi, 126},
  {m68k_op_and_32_er_pcix, 140},
  {m68k_op_and_32_er_i, 112},
  {m68k_op_mulu_16_d, 266},
  {m68k_op_mulu_16_ai, 294},
  {m68k_op_mulu_16_pi, 294},
  {m68k_op_mulu_16_pd, 308},
  {m68k_op_mulu_16_di, 322},
  {m68k_op_mulu_16_ix, 336},
  {m68k_op_mulu_16_aw, 322},
  {m68k_op_mulu_16_al, 350},
  {m68k_op_mulu_16_pcdi, 322},
  {m68k_op_mulu_16_pcix, 336},
  {m68k_op_mulu_16_i, 294},
  {m68k_op_abcd_8_rr, 42},
  {m68k_op_abcd_8_mm, 126},
  {m68k_op_abcd_8_mm_ay7, 126},
  {m68k_op_and_8_re_ai, 84},
  {m68k_op_and_8_re_pi, 84},
  {m68k_op_and_8_re_pi7, 84},
  {m68k_op_and_8_re_pd, 98},
  {m68k_op_and_8_re_pd7, 98},
  {m68k_op_and_8_re_di, 112},
  {m68k_op_and_8_re_ix, 126},
  {m68k_op_and_8_re_aw, 112},
  {m68k_op_and_8_re_al, 140},
  {m68k_op_exg_32_dd, 42},
  {m68k_op_exg_32_aa, 42},
  {m68k_op_and_16_re_ai, 84},
  {m68k_op_and_16_re_pi, 84},
  {m68k_op_and_16_re_pd, 98},
  {m68k_op_and_16_re_di, 112},
  {m68k_op_and_16_re_ix, 126},
  {m68k_op_and_16_re_aw, 112},
  {m68k_op_and_16_re_al, 140},
  {m68k_op_exg_32_da, 42},
  {m68k_op_and_32_re_ai, 140},
  {m68k_op_and_32_re_pi, 140},
  {m68k_op_and_32_re_pd, 154},
  {m68k_op_and_32_re_di, 168},
  {m68k_op_and_32_re_ix, 182},
  {m68k_op_and_32_re_aw, 168},
  {m68k_op_and_32_re_al, 196},
  {m68k_op_muls_16_d, 266},
  {m68k_op_muls_16_ai, 294},
  {m68k_op_muls_16_pi, 294},
  {m68k_op_muls_16_pd, 308},
  {m68k_op_muls_16_di, 322},
  {m68k_op_muls_16_ix, 336},
  {m68k_op_muls_16_aw, 322},
  {m68k_op_muls_16_al, 350},
  {m68k_op_muls_16_pcdi, 322},
  {m68k_op_muls_16_pcix, 336},
  {m68k_op_muls_16_i, 294},
  {m68k_op_abcd_8_mm_ax7, 126},
  {m68k_op_abcd_8_mm_axy7, 126},
  {m68k_op_add_8_er_d, 28},
  {m68k_op_add_8_er_ai, 56},
  {m68k_op_add_8_er_pi, 56},
  {m68k_op_add_8_er_pi7, 56},
  {m68k_op_add_8_er_pd, 70},
  {m68k_op_add_8_er_pd7, 70},
  {m68k_op_add_8_er_di, 84},
  {m68k_op_add_8_er_ix, 98},
  {m68k_op_add_8_er_aw, 84},
  {m68k_op_add_8_er_al, 112},
  {m68k_op_add_8_er_pcdi, 84},
  {m68k_op_add_8_er_pcix, 98},
  {m68k_op_add_8_er_i, 56},
  {m68k_op_add_16_er_d, 28},
  {m68k_op_add_16_er_a, 28},
  {m68k_op_add_16_er_ai, 56},
  {m68k_op_add_16_er_pi, 56},
  {m68k_op_add_16_er_pd, 70},
  {m68k_op_add_16_er_di, 84},
  {m68k_op_add_16_er_ix, 98},
  {m68k_op_add_16_er_aw, 84},
  {m68k_op_add_16_er_al, 112},
  {m68k_op_add_16_er_pcdi, 84},
  {m68k_op_add_16_er_pcix, 98},
  {m68k_op_add_16_er_i, 56},
  {m68k_op_add_32_er_d, 56},
  {m68k_op_add_32_er_a, 56},
  {m68k_op_add_32_er_ai, 98},
  {m68k_op_add_32_er_pi, 98},
  {m68k_op_add_32_er_pd, 112},
  {m68k_op_add_32_er_di, 126},
  {m68k_op_add_32_er_ix, 140},
  {m68k_op_add_32_er_aw, 126},
  {m68k_op_add_32_er_al, 154},
  {m68k_op_add_32_er_pcdi, 126},
  {m68k_op_add_32_er_pcix, 140},
  {m68k_op_add_32_er_i, 112},
  {m68k_op_adda_16_d, 56},
  {m68k_op_adda_16_a, 56},
  {m68k_op_adda_16_ai, 84},
  {m68k_op_adda_16_pi, 84},
  {m68k_op_adda_16_pd, 98},
  {m68k_op_adda_16_di, 112},
  {m68k_op_adda_16_ix, 126},
  {m68k_op_adda_16_aw, 112},
  {m68k_op_adda_16_al, 140},
  {m68k_op_adda_16_pcdi, 112},
  {m68k_op_adda_16_pcix, 126},
  {m68k_op_adda_16_i, 84},
  {m68k_op_addx_8_rr, 28},
  {m68k_op_addx_8_mm, 126},
  {m68k_op_addx_8_mm_ay7, 126},
  {m68k_op_add_8_re_ai, 84},
  {m68k_op_add_8_re_pi, 84},
  {m68k_op_add_8_re_pi7, 84},
  {m68k_op_add_8_re_pd, 98},
  {m68k_op_add_8_re_pd7, 98},
  {m68k_op_add_8_re_di, 112},
  {m68k_op_add_8_re_ix, 126},
  {m68k_op_add_8_re_aw, 112},
  {m68k_op_add_8_re_al, 140},
  {m68k_op_addx_16_rr, 28},
  {m68k_op_addx_16_mm, 126},
  {m68k_op_add_16_re_ai, 84},
  {m68k_op_add_16_re_pi, 84},
  {m68k_op_add_16_re_pd, 98},
  {m68k_op_add_16_re_di, 112},
  {m68k_op_add_16_re_ix, 126},
  {m68k_op_add_16_re_aw, 112},
  {m68k_op_add_16_re_al, 140},
  {m68k_op_addx_32_rr, 56},
  {m68k_op_addx_32_mm, 210},
  {m68k_op_add_32_re_ai, 140},
  {m68k_op_add_32_re_pi, 140},
  {m68k_op_add_32_re_pd, 154},
  {m68k_op_add_32_re_di, 168},
  {m68k_op_add_32_re_ix, 182},
  {m68k_op_add_32_re_aw, 168},
  {m68k_op_add_32_re_al, 196},
  {m68k_op_adda_32_d, 56},
  {m68k_op_adda_32_a, 56},
  {m68k_op_adda_32_ai, 98},
  {m68k_op_adda_32_pi, 98},
  {m68k_op_adda_32_pd, 112},
  {m68k_op_adda_32_di, 126},
  {m68k_op_adda_32_ix, 140},
  {m68k_op_adda_32_aw, 126},
  {m68k_op_adda_32_al, 154},
  {m68k_op_adda_32_pcdi, 126},
  {m68k_op_adda_32_pcix, 140},
  {m68k_op_adda_32_i, 112},
  {m68k_op_addx_8_mm_ax7, 126},
  {m68k_op_addx_8_mm_axy7, 126},
  {m68k_op_asr_8_s, 42},
  {m68k_op_lsr_8_s, 42},
  {m68k_op_roxr_8_s, 42},
  {m68k_op_ror_8_s, 42},
  {m68k_op_asr_8_r, 42},
  {m68k_op_lsr_8_r, 42},
  {m68k_op_roxr_8_r, 42},
  {m68k_op_ror_8_r, 42},
  {m68k_op_asr_16_s, 42},
  {m68k_op_lsr_16_s, 42},
  {m68k_op_roxr_16_s, 42},
  {m68k_op_ror_16_s, 42},
  {m68k_op_asr_16_r, 42},
  {m68k_op_lsr_16_r, 42},
  {m68k_op_roxr_16_r, 42},
  {m68k_op_ror_16_r, 42},
  {m68k_op_asr_32_s, 56},
  {m68k_op_lsr_32_s, 56},
  {m68k_op_roxr_32_s, 56},
  {m68k_op_ror_32_s, 56},
  {m68k_op_asr_32_r, 56},
  {m68k_op_lsr_32_r, 56},
  {m68k_op_roxr_32_r, 56},
  {m68k_op_ror_32_r, 56},
  {m68k_op_asr_16_ai, 84},
  {m68k_op_asr_16_pi, 84},
  {m68k_op_asr_16_pd, 98},
  {m68k_op_asr_16_di, 112},
  {m68k_op_asr_16_ix, 126},
  {m68k_op_asr_16_aw, 112},
  {m68k_op_asr_16_al, 140},
  {m68k_op_asl_8_s, 42},
  {m68k_op_lsl_8_s, 42},
  {m68k_op_roxl_8_s, 42},
  {m68k_op_rol_8_s, 42},
  {m68k_op_asl_8_r, 42},
  {m68k_op_lsl_8_r, 42},
  {m68k_op_roxl_8_r, 42},
  {m68k_op_rol_8_r, 42},
  {m68k_op_asl_16_s, 42},
  {m68k_op_lsl_16_s, 42},
  {m68k_op_roxl_16_s, 42},
  {m68k_op_rol_16_s, 42},
  {m68k_op_asl_16_r, 42},
  {m68k_op_lsl_16_r, 42},
  {m68k_op_roxl_16_r, 42},
  {m68k_op_rol_16_r, 42},
  {m68k_op_asl_32_s, 56},
  {m68k_op_lsl_32_s, 56},
  {m68k_op_roxl_32_s, 56},
  {m68k_op_rol_32_s, 56},
  {m68k_op_asl_32_r, 56},
  {m68k_op_lsl_32_r, 56},
  {m68k_op_roxl_32_r, 56},
  {m68k_op_rol_32_r, 56},
  {m68k_op_asl_16_ai, 84},
  {m68k_op_asl_16_pi, 84},
  {m68k_op_asl_16_pd, 98},
  {m68k_op_asl_16_di, 112},
  {m68k_op_asl_16_ix, 126},
  {m68k_op_asl_16_aw, 112},
  {m68k_op_asl_16_al, 140},
  {m68k_op_lsr_16_ai, 84},
  {m68k_op_lsr_16_pi, 84},
  {m68k_op_lsr_16_pd, 98},
  {m68k_op_lsr_16_di, 112},
  {m68k_op_lsr_16_ix, 126},
  {m68k_op_lsr_16_aw, 112},
  {m68k_op_lsr_16_al, 140},
  {m68k_op_lsl_16_ai, 84},
  {m68k_op_lsl_16_pi, 84},
  {m68k_op_lsl_16_pd, 98},
  {m68k_op_lsl_16_di, 112},
  {m68k_op_lsl_16_ix, 126},
  {m68k_op_lsl_16_aw, 112},
  {m68k_op_lsl_16_al, 140},
  {m68k_op_roxr_16_ai, 84},
  {m68k_op_roxr_16_pi, 84},
  {m68k_op_roxr_16_pd, 98},
  {m68k_op_roxr_16_di, 112},
  {m68k_op_roxr_16_ix, 126},
  {m68k_op_roxr_16_aw, 112},
  {m68k_op_roxr_16_al, 140},
  {m68k_op_roxl_16_ai, 84},
  {m68k_op_roxl_16_pi, 84},
  {m68k_op_roxl_16_pd, 98},
  {m68k_op_roxl_16_di, 112},
  {m68k_op_roxl_16_ix, 126},
  {m68k_op_roxl_16_aw, 112},
  {m68k_op_roxl_16_al, 140},
  {m68k_op_ror_16_ai, 84},
  {m68k_op_ror_16_pi, 84},
  {m68k_op_ror_16_pd, 98},
  {m68k_op_ror_16_di, 112},
  {m68k_op_ror_16_ix, 126},
  {m68k_op_ror_16_aw, 112},
  {m68k_op_ror_16_al, 140},
  {m68k_op_rol_16_ai, 84},
  {m68k_op_rol_16_pi, 84},
  {m68k_op_rol_16_pd, 98},
  {m68k_op_rol_16_di, 112},
  {m68k_op_rol_16_ix, 126},
  {m68k_op_rol_16_aw, 112},
  {m68k_op_rol_16_al, 140},
  {m68k_op_1111, 0},
  {m68k_op_1111, 28},
};

M68KI_IN_RAM const unsigned short m68ki_blk[379][32] = {
  {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,4},
  {5,5,5,5,5,5,5,6,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,9,10,1,1,11,1,1,1},
  {12,12,12,12,12,12,12,12,1,1,1,1,1,1,1,1,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14},
  {15,15,15,15,15,15,15,15,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,18,19,1,1,20,1,1,1},
  {21,21,21,21,21,21,21,21,1,1,1,1,1,1,1,1,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,23},
  {24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,28,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,31,31,31,31,31,31,31,31,32,32,32,32,32,32,32,33},
  {34,34,34,34,34,34,34,35,36,36,36,36,36,36,36,36,37,37,37,37,37,37,37,37,38,39,40,41,42,1,1,1},
  {43,43,43,43,43,43,43,43,44,44,44,44,44,44,44,44,45,45,45,45,45,45,45,45,46,46,46,46,46,46,46,47},
  {48,48,48,48,48,48,48,49,50,50,50,50,50,50,50,50,51,51,51,51,51,51,51,51,52,53,1,1,1,1,1,1},
  {54,54,54,54,54,54,54,54,55,55,55,55,55,55,55,55,56,56,56,56,56,56,56,56,57,57,57,57,57,57,57,58},
  {59,59,59,59,59,59,59,60,61,61,61,61,61,61,61,61,62,62,62,62,62,62,62,62,63,64,1,1,1,1,1,1},
  {65,65,65,65,65,65,65,65,66,66,66,66,66,66,66,66,67,67,67,67,67,67,67,67,68,68,68,68,68,68,68,69},
  {70,70,70,70,70,70,70,71,72,72,72,72,72,72,72,72,73,73,73,73,73,73,73,73,74,75,1,1,1,1,1,1},
  {76,76,76,76,76,76,76,76,1,1,1,1,1,1,1,1,77,77,77,77,77,77,77,77,78,78,78,78,78,78,78,79},
  {80,80,80,80,80,80,80,81,82,82,82,82,82,82,82,82,83,83,83,83,83,83,83,83,84,85,1,1,86,1,1,1},
  {87,87,87,87,87,87,87,87,1,1,1,1,1,1,1,1,88,88,88,88,88,88,88,88,89,89,89,89,89,89,89,89},
  {90,90,90,90,90,90,90,90,91,91,91,91,91,91,91,91,92,92,92,92,92,92,92,92,93,94,1,1,95,1,1,1},
  {96,96,96,96,96,96,96,96,1,1,1,1,1,1,1,1,97,97,97,97,97,97,97,97,98,98,98,98,98,98,98,98},
  {99,99,99,99,99,99,99,99,100,100,100,100,100,100,100,100,101,101,101,101,101,101,101,101,102,103,1,1,1,1,1,1},
  {104,104,104,104,104,104,104,104,1,1,1,1,1,1,1,1,105,105,105,105,105,105,105,105,106,106,106,106,106,106,106,107},
  {108,108,108,108,108,108,108,109,110,110,110,110,110,110,110,110,111,111,111,111,111,111,111,111,112,113,1,1,1,1,1,1},
  {114,114,114,114,114,114,114,114,1,1,1,1,1,1,1,1,115,115,115,115,115,115,115,115,116,116,116,116,116,116,116,116},
  {117,117,117,117,117,117,117,117,118,118,118,118,118,118,118,118,119,119,119,119,119,119,119,119,120,121,1,1,1,1,1,1},
  {122,122,122,122,122,122,122,122,1,1,1,1,1,1,1,1,123,123,123,123,123,123,123,123,124,124,124,124,124,124,124,124},
  {125,125,125,125,125,125,125,125,126,126,126,126,126,126,126,126,127,127,127,127,127,127,127,127,128,129,1,1,1,1,1,1},
  {130,130,130,130,130,130,130,130,1,1,1,1,1,1,1,1,131,131,131,131,131,131,131,131,132,132,132,132,132,132,132,133},
  {134,134,134,134,134,134,134,135,136,136,136,136,136,136,136,136,137,137,137,137,137,137,137,137,138,139,1,1,1,1,1,1},
  {140,140,140,140,140,140,140,140,1,1,1,1,1,1,1,1,141,141,141,141,141,141,141,141,142,142,142,142,142,142,142,142},
  {143,143,143,143,143,143,143,143,144,144,144,144,144,144,144,144,145,145,145,145,145,145,145,145,146,147,1,1,1,1,1,1},
  {148,148,148,148,148,148,148,148,1,1,1,1,1,1,1,1,149,149,149,149,149,149,149,149,150,150,150,150,150,150,150,150},
  {151,151,151,151,151,151,151,151,152,152,152,152,152,152,152,152,153,153,153,153,153,153,153,153,154,155,1,1,1,1,1,1},
  {156,156,156,156,156,156,156,156,1,1,1,1,1,1,1,1,157,157,157,157,157,157,157,157,158,158,158,158,158,158,158,159},
  {160,160,160,160,160,160,160,161,162,162,162,162,162,162,162,162,163,163,163,163,163,163,163,163,164,165,166,167,1,1,1,1},
  {168,168,168,168,168,168,168,168,1,1,1,1,1,1,1,1,169,169,169,169,169,169,169,169,170,170,170,170,170,170,170,171},
  {172,172,172,172,172,172,172,173,174,174,174,174,174,174,174,174,175,175,175,175,175,175,175,175,176,177,1,1,1,1,1,1},
  {178,178,178,178,178,178,178,178,1,1,1,1,1,1,1,1,179,179,179,179,179,179,179,179,180,180,180,180,180,180,180,181},
  {182,182,182,182,182,182,182,183,184,184,184,184,184,184,184,184,185,185,185,185,185,185,185,185,186,187,1,1,1,1,1,1},
  {188,188,188,188,188,188,188,188,1,1,1,1,1,1,1,1,189,189,189,189,189,189,189,189,190,190,190,190,190,190,190,191},
  {192,192,192,192,192,192,192,193,194,194,194,194,194,194,194,194,195,195,195,195,195,195,195,195,196,197,1,1,1,1,1,1},
  {198,198,198,198,198,198,198,198,1,1,1,1,1,1,1,1,199,199,199,199,199,199,199,199,200,200,200,200,200,200,200,201},
  {202,202,202,202,202,202,202,203,204,204,204,204,204,204,204,204,205,205,205,205,205,205,205,205,206,207,1,1,208,1,1,1},
  {209,209,209,209,209,209,209,209,1,1,1,1,1,1,1,1,210,210,210,210,210,210,210,210,211,211,211,211,211,211,211,211},
  {212,212,212,212,212,212,212,212,213,213,213,213,213,213,213,213,214,214,214,214,214,214,214,214,215,216,1,1,217,1,1,1},
  {218,218,218,218,218,218,218,218,1,1,1,1,1,1,1,1,219,219,219,219,219,219,219,219,220,220,220,220,220,220,220,220},
  {221,221,221,221,221,221,221,221,222,222,222,222,222,222,222,222,223,223,223,223,223,223,223,223,224,225,1,1,1,1,1,1},
  {226,226,226,226,226,226,226,226,1,1,1,1,1,1,1,1,227,227,227,227,227,227,227,227,228,228,228,228,228,228,228,229},
  {230,230,230,230,230,230,230,231,232,232,232,232,232,232,232,232,233,233,233,233,233,233,233,233,234,235,1,1,1,1,1,1},
  {236,236,236,236,236,236,236,236,1,1,1,1,1,1,1,1,237,237,237,237,237,237,237,237,238,238,238,238,238,238,238,238},
  {239,239,239,239,239,239,239,239,240,240,240,240,240,240,240,240,241,241,241,241,241,241,241,241,242,243,1,1,1,1,1,1},
  {244,244,244,244,244,244,244,244,1,1,1,1,1,1,1,1,245,245,245,245,245,245,245,245,246,246,246,246,246,246,246,246},
  {247,247,247,247,247,247,247,247,248,248,248,248,248,248,248,248,249,249,249,249,249,249,249,249,250,251,1,1,1,1,1,1},
  {252,252,252,252,252,252,252,252,1,1,1,1,1,1,1,1,253,253,253,253,253,253,253,253,254,254,254,254,254,254,254,255},
  {256,256,256,256,256,256,256,257,258,258,258,258,258,258,258,258,259,259,259,259,259,259,259,259,260,261,262,263,264,1,1,1},
  {265,265,265,265,265,265,265,265,1,1,1,1,1,1,1,1,266,266,266,266,266,266,266,266,267,267,267,267,267,267,267,268},
  {269,269,269,269,269,269,269,270,271,271,271,271,271,271,271,271,272,272,272,272,272,272,272,272,273,274,275,276,277,1,1,1},
  {278,278,278,278,278,278,278,278,1,1,1,1,1,1,1,1,279,279,279,279,279,279,279,279,280,280,280,280,280,280,280,281},
  {282,282,282,282,282,282,282,283,284,284,284,284,284,284,284,284,285,285,285,285,285,285,285,285,286,287,288,289,290,1,1,1},
  {291,291,291,291,291,291,291,291,1,1,1,1,1,1,1,1,292,292,292,292,292,292,292,292,293,293,293,293,293,293,293,294},
  {295,295,295,295,295,295,295,296,297,297,297,297,297,297,297,297,298,298,298,298,298,298,298,298,299,300,301,302,303,1,1,1},
  {304,304,304,304,304,304,304,304,1,1,1,1,1,1,1,1,305,305,305,305,305,305,305,305,306,306,306,306,306,306,306,307},
  {308,308,308,308,308,308,308,309,310,310,310,310,310,310,310,310,311,311,311,311,311,311,311,311,312,313,314,315,316,1,1,1},
  {317,317,317,317,317,317,317,317,1,1,1,1,1,1,1,1,318,318,318,318,318,318,318,318,319,319,319,319,319,319,319,320},
  {321,321,321,321,321,321,321,322,323,323,323,323,323,323,323,323,324,324,324,324,324,324,324,324,325,326,327,328,329,1,1,1},
  {330,330,330,330,330,330,330,330,1,1,1,1,1,1,1,1,331,331,331,331,331,331,331,331,332,332,332,332,332,332,332,333},
  {334,334,334,334,334,334,334,335,336,336,336,336,336,336,336,336,337,337,337,337,337,337,337,337,338,339,340,341,342,1,1,1},
  {343,343,343,343,343,343,343,343,1,1,1,1,1,1,1,1,344,344,344,344,344,344,344,344,345,345,345,345,345,345,345,346},
  {347,347,347,347,347,347,347,348,349,349,349,349,349,349,349,349,350,350,350,350,350,350,350,350,351,352,353,354,355,1,1,1},
  {356,356,356,356,356,356,356,356,1,1,1,1,1,1,1,1,357,357,357,357,357,357,357,357,358,358,358,358,358,358,358,359},
  {360,360,360,360,360,360,360,361,362,362,362,362,362,362,362,362,363,363,363,363,363,363,363,363,364,365,366,367,368,1,1,1},
  {369,369,369,369,369,369,369,369,1,1,1,1,1,1,1,1,370,370,370,370,370,370,370,370,371,371,371,371,371,371,371,372},
  {373,373,373,373,373,373,373,374,375,375,375,375,375,375,375,375,376,376,376,376,376,376,376,376,377,378,379,380,381,1,1,1},
  {382,382,382,382,382,382,382,382,383,383,383,383,383,383,383,383,384,384,384,384,384,384,384,384,385,385,385,385,385,385,385,385},
  {386,386,386,386,386,386,386,386,387,387,387,387,387,387,387,387,388,388,388,388,388,388,388,388,389,390,391,392,393,1,1,1},
  {394,394,394,394,394,394,394,394,395,395,395,395,395,395,395,395,396,396,396,396,396,396,396,396,397,397,397,397,397,397,397,397},
  {398,398,398,398,398,398,398,398,399,399,399,399,399,399,399,399,400,400,400,400,400,400,400,400,401,402,403,404,405,1,1,1},
  {406,406,406,406,406,406,406,406,407,407,407,407,407,407,407,407,408,408,408,408,408,408,408,408,409,409,409,409,409,409,409,409},
  {410,410,410,410,410,410,410,410,411,411,411,411,411,411,411,411,412,412,412,412,412,412,412,412,413,414,415,416,417,1,1,1},
  {418,418,418,418,418,418,418,418,419,419,419,419,419,419,419,419,420,420,420,420,420,420,420,420,421,421,421,421,421,421,421,421},
  {422,422,422,422,422,422,422,422,423,423,423,423,423,423,423,423,424,424,424,424,424,424,424,424,425,426,427,428,429,1,1,1},
  {430,430,430,430,430,430,430,430,431,431,431,431,431,431,431,431,432,432,432,432,432,432,432,432,433,433,433,433,433,433,433,433},
  {434,434,434,434,434,434,434,434,435,435,435,435,435,435,435,435,436,436,436,436,436,436,436,436,437,438,439,440,441,1,1,1},
  {442,442,442,442,442,442,442,442,443,443,443,443,443,443,443,443,444,444,444,444,444,444,444,444,445,445,445,445,445,445,445,445},
  {446,446,446,446,446,446,446,446,447,447,447,447,447,447,447,447,448,448,448,448,448,448,448,448,449,450,451,452,453,1,1,1},
  {454,454,454,454,454,454,454,454,455,455,455,455,455,455,455,455,456,456,456,456,456,456,456,456,457,457,457,457,457,457,457,457},
  {458,458,458,458,458,458,458,458,459,459,459,459,459,459,459,459,460,460,460,460,460,460,460,460,461,462,463,464,465,1,1,1},
  {466,466,466,466,466,466,466,466,467,467,467,467,467,467,467,467,468,468,468,468,468,468,468,468,469,469,469,469,469,469,469,469},
  {470,470,470,470,470,470,470,470,471,471,471,471,471,471,471,471,472,472,472,472,472,472,472,472,473,474,475,476,477,1,1,1},
  {478,478,478,478,478,478,478,478,479,479,479,479,479,479,479,479,480,480,480,480,480,480,480,480,481,481,481,481,481,481,481,481},
  {482,482,482,482,482,482,482,482,483,483,483,483,483,483,483,483,484,484,484,484,484,484,484,484,485,486,487,488,489,1,1,1},
  {490,490,490,490,490,490,490,490,491,491,491,491,491,491,491,491,492,492,492,492,492,492,492,492,493,493,493,493,493,493,493,493},
  {494,494,494,494,494,494,494,494,495,495,495,495,495,495,495,495,496,496,496,496,496,496,496,496,497,498,499,500,501,1,1,1},
  {502,502,502,502,502,502,502,502,503,503,503,503,503,503,503,503,504,504,504,504,504,504,504,504,505,505,505,505,505,505,505,505},
  {506,506,506,506,506,506,506,506,507,507,507,507,507,507,507,507,508,508,508,508,508,508,508,508,509,510,511,512,513,1,1,1},
  {514,514,514,514,514,514,514,514,515,515,515,515,515,515,515,515,516,516,516,516,516,516,516,516,517,517,517,517,517,517,517,517},
  {518,518,518,518,518,518,518,518,519,519,519,519,519,519,519,519,520,520,520,520,520,520,520,520,521,522,523,524,525,1,1,1},
  {526,526,526,526,526,526,526,526,527,527,527,527,527,527,527,527,528,528,528,528,528,528,528,528,529,529,529,529,529,529,529,529},
  {530,530,530,530,530,530,530,530,531,531,531,531,531,531,531,531,532,532,532,532,532,532,532,532,533,534,535,536,537,1,1,1},
  {538,538,538,538,538,538,538,538,539,539,539,539,539,539,539,539,540,540,540,540,540,540,540,540,541,541,541,541,541,541,541,541},
  {542,542,542,542,542,542,542,542,543,543,543,543,543,543,543,543,544,544,544,544,544,544,544,544,545,546,547,548,549,1,1,1},
  {550,550,550,550,550,550,550,550,551,551,551,551,551,551,551,551,552,552,552,552,552,552,552,552,553,553,553,553,553,553,553,553},
  {554,554,554,554,554,554,554,554,555,555,555,555,555,555,555,555,556,556,556,556,556,556,556,556,557,558,559,560,561,1,1,1},
  {562,562,562,562,562,562,562,562,563,563,563,563,563,563,563,563,564,564,564,564,564,564,564,564,565,565,565,565,565,565,565,565},
  {566,566,566,566,566,566,566,566,567,567,567,567,567,567,567,567,568,568,568,568,568,568,568,568,569,570,571,572,573,1,1,1},
  {574,574,574,574,574,574,574,574,575,575,575,575,575,575,575,575,576,576,576,576,576,576,576,576,577,577,577,577,577,577,577,577},
  {578,578,578,578,578,578,578,578,579,579,579,579,579,579,579,579,580,580,580,580,580,580,580,580,581,582,583,584,585,1,1,1},
  {586,586,586,586,586,586,586,586,587,587,587,587,587,587,587,587,588,588,588,588,588,588,588,588,589,589,589,589,589,589,589,589},
  {590,590,590,590,590,590,590,590,591,591,591,591,591,591,591,591,592,592,592,592,592,592,592,592,593,594,595,596,597,1,1,1},
  {598,598,598,598,598,598,598,598,1,1,1,1,1,1,1,1,599,599,599,599,599,599,599,599,600,600,600,600,600,600,600,601},
  {602,602,602,602,602,602,602,603,604,604,604,604,604,604,604,604,605,605,605,605,605,605,605,605,606,607,1,1,1,1,1,1},
  {608,608,608,608,608,608,608,608,1,1,1,1,1,1,1,1,609,609,609,609,609,609,609,609,610,610,610,610,610,610,610,610},
  {611,611,611,611,611,611,611,611,612,612,612,612,612,612,612,612,613,613,613,613,613,613,613,613,614,615,1,1,1,1,1,1},
  {616,616,616,616,616,616,616,616,1,1,1,1,1,1,1,1,617,617,617,617,617,617,617,617,618,618,618,618,618,618,618,618},
  {619,619,619,619,619,619,619,619,620,620,620,620,620,620,620,620,621,621,621,621,621,621,621,621,622,623,1,1,1,1,1,1},
  {624,624,624,624,624,624,624,624,1,1,1,1,1,1,1,1,625,625,625,625,625,625,625,625,626,626,626,626,626,626,626,626},
  {627,627,627,627,627,627,627,627,628,628,628,628,628,628,628,628,629,629,629,629,629,629,629,629,630,631,1,1,1,1,1,1},
  {632,632,632,632,632,632,632,632,1,1,1,1,1,1,1,1,633,633,633,633,633,633,633,633,634,634,634,634,634,634,634,634},
  {635,635,635,635,635,635,635,635,636,636,636,636,636,636,636,636,637,637,637,637,637,637,637,637,638,639,640,641,642,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,643,643,643,643,643,643,643,643,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,644,644,644,644,644,644,644,644,645,645,645,645,645,645,645,645,646,647,648,649,1,1,1,1},
  {650,650,650,650,650,650,650,650,1,1,1,1,1,1,1,1,651,651,651,651,651,651,651,651,652,652,652,652,652,652,652,653},
  {654,654,654,654,654,654,654,655,656,656,656,656,656,656,656,656,657,657,657,657,657,657,657,657,658,659,1,1,1,1,1,1},
  {660,660,660,660,660,660,660,660,1,1,1,1,1,1,1,1,661,661,661,661,661,661,661,661,662,662,662,662,662,662,662,662},
  {663,663,663,663,663,663,663,663,664,664,664,664,664,664,664,664,665,665,665,665,665,665,665,665,666,667,1,1,1,1,1,1},
  {668,668,668,668,668,668,668,668,1,1,1,1,1,1,1,1,669,669,669,669,669,669,669,669,670,670,670,670,670,670,670,670},
  {671,671,671,671,671,671,671,671,672,672,672,672,672,672,672,672,673,673,673,673,673,673,673,673,674,675,1,1,1,1,1,1},
  {676,676,676,676,676,676,676,676,1,1,1,1,1,1,1,1,677,677,677,677,677,677,677,677,678,678,678,678,678,678,678,679},
  {680,680,680,680,680,680,680,681,682,682,682,682,682,682,682,682,683,683,683,683,683,683,683,683,684,685,1,1,1,1,1,1},
  {686,686,686,686,686,686,686,686,1,1,1,1,1,1,1,1,687,687,687,687,687,687,687,687,688,688,688,688,688,688,688,688},
  {689,689,689,689,689,689,689,689,690,690,690,690,690,690,690,690,691,691,691,691,691,691,691,691,692,693,1,1,1,1,1,1},
  {694,694,694,694,694,694,694,694,1,1,1,1,1,1,1,1,695,695,695,695,695,695,695,695,696,696,696,696,696,696,696,696},
  {697,697,697,697,697,697,697,697,698,698,698,698,698,698,698,698,699,699,699,699,699,699,699,699,700,701,1,1,1,1,1,1},
  {702,702,702,702,702,702,702,702,1,1,1,1,1,1,1,1,703,703,703,703,703,703,703,703,704,704,704,704,704,704,704,704},
  {705,705,705,705,705,705,705,705,706,706,706,706,706,706,706,706,707,707,707,707,707,707,707,707,708,709,710,711,712,1,1,1},
  {713,713,713,713,713,713,713,713,1,1,1,1,1,1,1,1,714,714,714,714,714,714,714,714,715,715,715,715,715,715,715,716},
  {717,717,717,717,717,717,717,718,719,719,719,719,719,719,719,719,720,720,720,720,720,720,720,720,721,722,1,1,1,1,1,1},
  {723,723,723,723,723,723,723,723,1,1,1,1,1,1,1,1,724,724,724,724,724,724,724,724,725,725,725,725,725,725,725,725},
  {726,726,726,726,726,726,726,726,727,727,727,727,727,727,727,727,728,728,728,728,728,728,728,728,729,730,1,1,1,1,1,1},
  {731,731,731,731,731,731,731,731,1,1,1,1,1,1,1,1,732,732,732,732,732,732,732,732,733,733,733,733,733,733,733,733},
  {734,734,734,734,734,734,734,734,735,735,735,735,735,735,735,735,736,736,736,736,736,736,736,736,737,738,1,1,1,1,1,1},
  {739,739,739,739,739,739,739,739,1,1,1,1,1,1,1,1,740,740,740,740,740,740,740,740,741,741,741,741,741,741,741,741},
  {742,742,742,742,742,742,742,742,743,743,743,743,743,743,743,743,744,744,744,744,744,744,744,744,745,746,747,748,749,1,1,1},
  {750,750,750,750,750,750,750,750,1,1,1,1,1,1,1,1,751,751,751,751,751,751,751,751,752,752,752,752,752,752,752,753},
  {754,754,754,754,754,754,754,755,756,756,756,756,756,756,756,756,757,757,757,757,757,757,757,757,758,759,1,1,1,1,1,1},
  {760,760,760,760,760,760,760,760,1,1,1,1,1,1,1,1,761,761,761,761,761,761,761,761,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,762,762,762,762,762,762,762,762,763,763,763,763,763,763,763,763,764,765,766,767,1,1,1,1},
  {768,768,768,768,768,768,768,768,1,1,1,1,1,1,1,1,769,769,769,769,769,769,769,769,1,1,1,1,1,1,1,1},
  {770,770,770,770,770,770,770,770,771,771,771,771,771,771,771,771,772,772,772,772,772,772,772,772,773,774,1,1,1,1,1,1},
  {775,775,775,775,775,775,775,775,1,1,1,1,1,1,1,1,776,776,776,776,776,776,776,776,1,1,1,1,1,1,1,1},
  {777,777,777,777,777,777,777,777,778,778,778,778,778,778,778,778,779,779,779,779,779,779,779,779,780,781,1,1,1,1,1,1},
  {782,782,782,782,782,782,782,782,1,1,1,1,1,1,1,1,783,783,783,783,783,783,783,783,784,784,784,784,784,784,784,785},
  {786,786,786,786,786,786,786,787,788,788,788,788,788,788,788,788,789,789,789,789,789,789,789,789,790,791,1,1,1,1,1,1},
  {792,792,792,792,792,792,792,792,1,1,1,1,1,1,1,1,793,793,793,793,793,793,793,793,794,794,794,794,794,794,794,794},
  {795,795,795,795,795,795,795,795,796,796,796,796,796,796,796,796,797,797,797,797,797,797,797,797,798,799,1,1,1,1,1,1},
  {800,800,800,800,800,800,800,800,1,1,1,1,1,1,1,1,801,801,801,801,801,801,801,801,802,802,802,802,802,802,802,802},
  {803,803,803,803,803,803,803,803,804,804,804,804,804,804,804,804,805,805,805,805,805,805,805,805,806,807,1,1,1,1,1,1},
  {808,808,808,808,808,808,808,808,1,1,1,1,1,1,1,1,809,809,809,809,809,809,809,809,810,810,810,810,810,810,810,811},
  {812,812,812,812,812,812,812,813,814,814,814,814,814,814,814,814,815,815,815,815,815,815,815,815,816,817,1,1,818,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,819,819,819,819,819,819,819,819,820,820,820,820,820,820,820,820},
  {1,1,1,1,1,1,1,1,821,821,821,821,821,821,821,821,822,822,822,822,822,822,822,822,823,824,825,826,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,827,827,827,827,827,827,827,827,828,828,828,828,828,828,828,828},
  {1,1,1,1,1,1,1,1,829,829,829,829,829,829,829,829,830,830,830,830,830,830,830,830,831,832,833,834,1,1,1,1},
  {835,835,835,835,835,835,835,835,835,835,835,835,835,835,835,835,836,836,836,836,836,836,836,837,838,838,838,838,838,838,838,839},
  {840,840,840,840,840,840,840,840,841,841,841,841,841,841,841,841,842,843,844,845,1,846,847,848,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,849,849,849,849,849,849,849,849,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,850,850,850,850,850,850,850,850,851,851,851,851,851,851,851,851,852,853,854,855,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,856,856,856,856,856,856,856,856,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,857,857,857,857,857,857,857,857,858,858,858,858,858,858,858,858,859,860,861,862,1,1,1,1},
  {863,863,863,863,863,863,863,863,1,1,1,1,1,1,1,1,864,864,864,864,864,864,864,864,865,865,865,865,865,865,865,866},
  {867,867,867,867,867,867,867,868,869,869,869,869,869,869,869,869,870,870,870,870,870,870,870,870,871,872,1,1,1,1,1,1},
  {873,873,873,873,873,873,873,873,874,874,874,874,874,874,874,874,875,875,875,875,875,875,875,875,876,876,876,876,876,876,876,876},
  {877,877,877,877,877,877,877,877,878,878,878,878,878,878,878,878,879,879,879,879,879,879,879,879,880,881,1,1,1,1,1,1},
  {882,882,882,882,882,882,882,882,883,883,883,883,883,883,883,883,884,884,884,884,884,884,884,884,885,885,885,885,885,885,885,885},
  {886,886,886,886,886,886,886,886,887,887,887,887,887,887,887,887,888,888,888,888,888,888,888,888,889,890,1,1,1,1,1,1},
  {891,891,891,891,891,891,891,891,892,892,892,892,892,892,892,892,893,893,893,893,893,893,893,893,894,894,894,894,894,894,894,895},
  {896,896,896,896,896,896,896,897,898,898,898,898,898,898,898,898,899,899,899,899,899,899,899,899,900,901,1,1,1,1,1,1},
  {902,902,902,902,902,902,902,902,1,1,1,1,1,1,1,1,903,903,903,903,903,903,903,903,904,904,904,904,904,904,904,905},
  {906,906,906,906,906,906,906,907,908,908,908,908,908,908,908,908,909,909,909,909,909,909,909,909,910,911,1,1,1,1,1,1},
  {912,912,912,912,912,912,912,912,913,913,913,913,913,913,913,913,914,914,914,914,914,914,914,914,915,915,915,915,915,915,915,915},
  {916,916,916,916,916,916,916,916,917,917,917,917,917,917,917,917,918,918,918,918,918,918,918,918,919,920,1,1,1,1,1,1},
  {921,921,921,921,921,921,921,921,922,922,922,922,922,922,922,922,923,923,923,923,923,923,923,923,924,924,924,924,924,924,924,924},
  {925,925,925,925,925,925,925,925,926,926,926,926,926,926,926,926,927,927,927,927,927,927,927,927,928,929,1,1,1,1,1,1},
  {930,930,930,930,930,930,930,930,931,931,931,931,931,931,931,931,932,932,932,932,932,932,932,932,933,933,933,933,933,933,933,934},
  {935,935,935,935,935,935,935,936,937,937,937,937,937,937,937,937,938,938,938,938,938,938,938,938,939,940,1,1,1,1,1,1},
  {941,941,941,941,941,941,941,941,942,942,942,942,942,942,942,942,943,943,943,943,943,943,943,943,944,944,944,944,944,944,944,945},
  {946,946,946,946,946,946,946,947,948,948,948,948,948,948,948,948,949,949,949,949,949,949,949,949,950,951,1,1,1,1,1,1},
  {952,952,952,952,952,952,952,952,953,953,953,953,953,953,953,953,954,954,954,954,954,954,954,954,955,955,955,955,955,955,955,956},
  {957,957,957,957,957,957,957,958,959,959,959,959,959,959,959,959,960,960,960,960,960,960,960,960,961,962,1,1,1,1,1,1},
  {963,963,963,963,963,963,963,963,964,964,964,964,964,964,964,964,965,965,965,965,965,965,965,965,966,966,966,966,966,966,966,967},
  {968,968,968,968,968,968,968,969,970,970,970,970,970,970,970,970,971,971,971,971,971,971,971,971,972,973,1,1,1,1,1,1},
  {974,974,974,974,974,974,974,974,975,975,975,975,975,975,975,975,976,976,976,976,976,976,976,976,977,977,977,977,977,977,977,978},
  {979,979,979,979,979,979,979,980,981,981,981,981,981,981,981,981,982,982,982,982,982,982,982,982,983,984,1,1,1,1,1,1},
  {985,985,985,985,985,985,985,985,986,986,986,986,986,986,986,986,987,987,987,987,987,987,987,987,988,988,988,988,988,988,988,989},
  {990,990,990,990,990,990,990,991,992,992,992,992,992,992,992,992,993,993,993,993,993,993,993,993,994,995,1,1,1,1,1,1},
  {996,996,996,996,996,996,996,996,997,997,997,997,997,997,997,997,998,998,998,998,998,998,998,998,999,999,999,999,999,999,999,1000},
  {1001,1001,1001,1001,1001,1001,1001,1002,1003,1003,1003,1003,1003,1003,1003,1003,1004,1004,1004,1004,1004,1004,1004,1004,1005,1006,1,1,1,1,1,1},
  {1007,1007,1007,1007,1007,1007,1007,1007,1008,1008,1008,1008,1008,1008,1008,1008,1009,1009,1009,1009,1009,1009,1009,1009,1010,1010,1010,1010,1010,1010,1010,1011},
  {1012,1012,1012,1012,1012,1012,1012,1013,1014,1014,1014,1014,1014,1014,1014,1014,1015,1015,1015,1015,1015,1015,1015,1015,1016,1017,1,1,1,1,1,1},
  {1018,1018,1018,1018,1018,1018,1018,1018,1019,1019,1019,1019,1019,1019,1019,1019,1020,1020,1020,1020,1020,1020,1020,1020,1021,1021,1021,1021,1021,1021,1021,1022},
  {1023,1023,1023,1023,1023,1023,1023,1024,1025,1025,1025,1025,1025,1025,1025,1025,1026,1026,1026,1026,1026,1026,1026,1026,1027,1028,1,1,1,1,1,1},
  {1029,1029,1029,1029,1029,1029,1029,1029,1030,1030,1030,1030,1030,1030,1030,1030,1031,1031,1031,1031,1031,1031,1031,1031,1032,1032,1032,1032,1032,1032,1032,1033},
  {1034,1034,1034,1034,1034,1034,1034,1035,1036,1036,1036,1036,1036,1036,1036,1036,1037,1037,1037,1037,1037,1037,1037,1037,1038,1039,1,1,1,1,1,1},
  {1040,1040,1040,1040,1040,1040,1040,1040,1041,1041,1041,1041,1041,1041,1041,1041,1042,1042,1042,1042,1042,1042,1042,1042,1043,1043,1043,1043,1043,1043,1043,1044},
  {1045,1045,1045,1045,1045,1045,1045,1046,1047,1047,1047,1047,1047,1047,1047,1047,1048,1048,1048,1048,1048,1048,1048,1048,1049,1050,1,1,1,1,1,1},
  {1051,1051,1051,1051,1051,1051,1051,1051,1052,1052,1052,1052,1052,1052,1052,1052,1053,1053,1053,1053,1053,1053,1053,1053,1054,1054,1054,1054,1054,1054,1054,1055},
  {1056,1056,1056,1056,1056,1056,1056,1057,1058,1058,1058,1058,1058,1058,1058,1058,1059,1059,1059,1059,1059,1059,1059,1059,1060,1061,1,1,1,1,1,1},
  {1062,1062,1062,1062,1062,1062,1062,1062,1063,1063,1063,1063,1063,1063,1063,1063,1064,1064,1064,1064,1064,1064,1064,1064,1065,1065,1065,1065,1065,1065,1065,1066},
  {1067,1067,1067,1067,1067,1067,1067,1068,1069,1069,1069,1069,1069,1069,1069,1069,1070,1070,1070,1070,1070,1070,1070,1070,1071,1072,1,1,1,1,1,1},
  {1073,1073,1073,1073,1073,1073,1073,1073,1074,1074,1074,1074,1074,1074,1074,1074,1075,1075,1075,1075,1075,1075,1075,1075,1076,1076,1076,1076,1076,1076,1076,1077},
  {1078,1078,1078,1078,1078,1078,1078,1079,1080,1080,1080,1080,1080,1080,1080,1080,1081,1081,1081,1081,1081,1081,1081,1081,1082,1083,1,1,1,1,1,1},
  {1084,1084,1084,1084,1084,1084,1084,1084,1085,1085,1085,1085,1085,1085,1085,1085,1086,1086,1086,1086,1086,1086,1086,1086,1087,1087,1087,1087,1087,1087,1087,1088},
  {1089,1089,1089,1089,1089,1089,1089,1090,1091,1091,1091,1091,1091,1091,1091,1091,1092,1092,1092,1092,1092,1092,1092,1092,1093,1094,1,1,1,1,1,1},
  {1095,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096},
  {1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096},
  {1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1096,1097},
  {1098,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099},
  {1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099},
  {1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1099,1100},
  {1101,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102},
  {1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102},
  {1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1102,1103},
  {1104,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105},
  {1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105},
  {1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1105,1106},
  {1107,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108},
  {1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108},
  {1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1108,1109},
  {1110,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111},
  {1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111},
  {1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1112},
  {1113,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114},
  {1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114},
  {1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1114,1115},
  {1116,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117},
  {1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117},
  {1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1117,1118},
  {1119,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120},
  {1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120},
  {1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1120,1121},
  {1122,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123},
  {1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123},
  {1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1123,1124},
  {1125,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126},
  {1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126},
  {1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1126,1127},
  {1128,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129},
  {1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129},
  {1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1129,1130},
  {1131,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132},
  {1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132},
  {1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1132,1133},
  {1134,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135},
  {1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135},
  {1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1135,1136},
  {1137,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138},
  {1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138},
  {1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1138,1139},
  {1140,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141},
  {1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141},
  {1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1141,1142},
  {1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143,1143},
  {1144,1144,1144,1144,1144,1144,1144,1144,1,1,1,1,1,1,1,1,1145,1145,1145,1145,1145,1145,1145,1145,1146,1146,1146,1146,1146,1146,1146,1147},
  {1148,1148,1148,1148,1148,1148,1148,1149,1150,1150,1150,1150,1150,1150,1150,1150,1151,1151,1151,1151,1151,1151,1151,1151,1152,1153,1154,1155,1156,1,1,1},
  {1157,1157,1157,1157,1157,1157,1157,1157,1,1,1,1,1,1,1,1,1158,1158,1158,1158,1158,1158,1158,1158,1159,1159,1159,1159,1159,1159,1159,1159},
  {1160,1160,1160,1160,1160,1160,1160,1160,1161,1161,1161,1161,1161,1161,1161,1161,1162,1162,1162,1162,1162,1162,1162,1162,1163,1164,1165,1166,1167,1,1,1},
  {1168,1168,1168,1168,1168,1168,1168,1168,1,1,1,1,1,1,1,1,1169,1169,1169,1169,1169,1169,1169,1169,1170,1170,1170,1170,1170,1170,1170,1170},
  {1171,1171,1171,1171,1171,1171,1171,1171,1172,1172,1172,1172,1172,1172,1172,1172,1173,1173,1173,1173,1173,1173,1173,1173,1174,1175,1176,1177,1178,1,1,1},
  {1179,1179,1179,1179,1179,1179,1179,1179,1,1,1,1,1,1,1,1,1180,1180,1180,1180,1180,1180,1180,1180,1181,1181,1181,1181,1181,1181,1181,1181},
  {1182,1182,1182,1182,1182,1182,1182,1182,1183,1183,1183,1183,1183,1183,1183,1183,1184,1184,1184,1184,1184,1184,1184,1184,1185,1186,1187,1188,1189,1,1,1},
  {1190,1190,1190,1190,1190,1190,1190,1190,1191,1191,1191,1191,1191,1191,1191,1192,1193,1193,1193,1193,1193,1193,1193,1193,1194,1194,1194,1194,1194,1194,1194,1195},
  {1196,1196,1196,1196,1196,1196,1196,1197,1198,1198,1198,1198,1198,1198,1198,1198,1199,1199,1199,1199,1199,1199,1199,1199,1200,1201,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1202,1202,1202,1202,1202,1202,1202,1202,1203,1203,1203,1203,1203,1203,1203,1203},
  {1204,1204,1204,1204,1204,1204,1204,1204,1205,1205,1205,1205,1205,1205,1205,1205,1206,1206,1206,1206,1206,1206,1206,1206,1207,1208,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1209,1209,1209,1209,1209,1209,1209,1209,1210,1210,1210,1210,1210,1210,1210,1210},
  {1211,1211,1211,1211,1211,1211,1211,1211,1212,1212,1212,1212,1212,1212,1212,1212,1213,1213,1213,1213,1213,1213,1213,1213,1214,1215,1,1,1,1,1,1},
  {1216,1216,1216,1216,1216,1216,1216,1216,1,1,1,1,1,1,1,1,1217,1217,1217,1217,1217,1217,1217,1217,1218,1218,1218,1218,1218,1218,1218,1218},
  {1219,1219,1219,1219,1219,1219,1219,1219,1220,1220,1220,1220,1220,1220,1220,1220,1221,1221,1221,1221,1221,1221,1221,1221,1222,1223,1224,1225,1226,1,1,1},
  {1190,1190,1190,1190,1190,1190,1190,1190,1227,1227,1227,1227,1227,1227,1227,1228,1193,1193,1193,1193,1193,1193,1193,1193,1194,1194,1194,1194,1194,1194,1194,1195},
  {1229,1229,1229,1229,1229,1229,1229,1229,1,1,1,1,1,1,1,1,1230,1230,1230,1230,1230,1230,1230,1230,1231,1231,1231,1231,1231,1231,1231,1232},
  {1233,1233,1233,1233,1233,1233,1233,1234,1235,1235,1235,1235,1235,1235,1235,1235,1236,1236,1236,1236,1236,1236,1236,1236,1237,1238,1239,1240,1241,1,1,1},
  {1242,1242,1242,1242,1242,1242,1242,1242,1243,1243,1243,1243,1243,1243,1243,1243,1244,1244,1244,1244,1244,1244,1244,1244,1245,1245,1245,1245,1245,1245,1245,1245},
  {1246,1246,1246,1246,1246,1246,1246,1246,1247,1247,1247,1247,1247,1247,1247,1247,1248,1248,1248,1248,1248,1248,1248,1248,1249,1250,1251,1252,1253,1,1,1},
  {1254,1254,1254,1254,1254,1254,1254,1254,1255,1255,1255,1255,1255,1255,1255,1255,1256,1256,1256,1256,1256,1256,1256,1256,1257,1257,1257,1257,1257,1257,1257,1257},
  {1258,1258,1258,1258,1258,1258,1258,1258,1259,1259,1259,1259,1259,1259,1259,1259,1260,1260,1260,1260,1260,1260,1260,1260,1261,1262,1263,1264,1265,1,1,1},
  {1266,1266,1266,1266,1266,1266,1266,1266,1267,1267,1267,1267,1267,1267,1267,1267,1268,1268,1268,1268,1268,1268,1268,1268,1269,1269,1269,1269,1269,1269,1269,1269},
  {1270,1270,1270,1270,1270,1270,1270,1270,1271,1271,1271,1271,1271,1271,1271,1271,1272,1272,1272,1272,1272,1272,1272,1272,1273,1274,1275,1276,1277,1,1,1},
  {1278,1278,1278,1278,1278,1278,1278,1278,1279,1279,1279,1279,1279,1279,1279,1280,1281,1281,1281,1281,1281,1281,1281,1281,1282,1282,1282,1282,1282,1282,1282,1283},
  {1284,1284,1284,1284,1284,1284,1284,1285,1286,1286,1286,1286,1286,1286,1286,1286,1287,1287,1287,1287,1287,1287,1287,1287,1288,1289,1,1,1,1,1,1},
  {1290,1290,1290,1290,1290,1290,1290,1290,1291,1291,1291,1291,1291,1291,1291,1291,1292,1292,1292,1292,1292,1292,1292,1292,1293,1293,1293,1293,1293,1293,1293,1293},
  {1294,1294,1294,1294,1294,1294,1294,1294,1295,1295,1295,1295,1295,1295,1295,1295,1296,1296,1296,1296,1296,1296,1296,1296,1297,1298,1,1,1,1,1,1},
  {1299,1299,1299,1299,1299,1299,1299,1299,1300,1300,1300,1300,1300,1300,1300,1300,1301,1301,1301,1301,1301,1301,1301,1301,1302,1302,1302,1302,1302,1302,1302,1302},
  {1303,1303,1303,1303,1303,1303,1303,1303,1304,1304,1304,1304,1304,1304,1304,1304,1305,1305,1305,1305,1305,1305,1305,1305,1306,1307,1,1,1,1,1,1},
  {1308,1308,1308,1308,1308,1308,1308,1308,1309,1309,1309,1309,1309,1309,1309,1309,1310,1310,1310,1310,1310,1310,1310,1310,1311,1311,1311,1311,1311,1311,1311,1311},
  {1312,1312,1312,1312,1312,1312,1312,1312,1313,1313,1313,1313,1313,1313,1313,1313,1314,1314,1314,1314,1314,1314,1314,1314,1315,1316,1317,1318,1319,1,1,1},
  {1278,1278,1278,1278,1278,1278,1278,1278,1320,1320,1320,1320,1320,1320,1320,1321,1281,1281,1281,1281,1281,1281,1281,1281,1282,1282,1282,1282,1282,1282,1282,1283},
  {1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322,1322},
  {1323,1323,1323,1323,1323,1323,1323,1323,1,1,1,1,1,1,1,1,1324,1324,1324,1324,1324,1324,1324,1324,1325,1325,1325,1325,1325,1325,1325,1326},
  {1327,1327,1327,1327,1327,1327,1327,1328,1329,1329,1329,1329,1329,1329,1329,1329,1330,1330,1330,1330,1330,1330,1330,1330,1331,1332,1333,1334,1335,1,1,1},
  {1336,1336,1336,1336,1336,1336,1336,1336,1337,1337,1337,1337,1337,1337,1337,1337,1338,1338,1338,1338,1338,1338,1338,1338,1339,1339,1339,1339,1339,1339,1339,1339},
  {1340,1340,1340,1340,1340,1340,1340,1340,1341,1341,1341,1341,1341,1341,1341,1341,1342,1342,1342,1342,1342,1342,1342,1342,1343,1344,1345,1346,1347,1,1,1},
  {1348,1348,1348,1348,1348,1348,1348,1348,1349,1349,1349,1349,1349,1349,1349,1349,1350,1350,1350,1350,1350,1350,1350,1350,1351,1351,1351,1351,1351,1351,1351,1351},
  {1352,1352,1352,1352,1352,1352,1352,1352,1353,1353,1353,1353,1353,1353,1353,1353,1354,1354,1354,1354,1354,1354,1354,1354,1355,1356,1357,1358,1359,1,1,1},
  {1360,1360,1360,1360,1360,1360,1360,1360,1361,1361,1361,1361,1361,1361,1361,1361,1362,1362,1362,1362,1362,1362,1362,1362,1363,1363,1363,1363,1363,1363,1363,1363},
  {1364,1364,1364,1364,1364,1364,1364,1364,1365,1365,1365,1365,1365,1365,1365,1365,1366,1366,1366,1366,1366,1366,1366,1366,1367,1368,1369,1370,1371,1,1,1},
  {1372,1372,1372,1372,1372,1372,1372,1372,1373,1373,1373,1373,1373,1373,1373,1374,1375,1375,1375,1375,1375,1375,1375,1375,1376,1376,1376,1376,1376,1376,1376,1377},
  {1378,1378,1378,1378,1378,1378,1378,1379,1380,1380,1380,1380,1380,1380,1380,1380,1381,1381,1381,1381,1381,1381,1381,1381,1382,1383,1,1,1,1,1,1},
  {1384,1384,1384,1384,1384,1384,1384,1384,1385,1385,1385,1385,1385,1385,1385,1385,1386,1386,1386,1386,1386,1386,1386,1386,1387,1387,1387,1387,1387,1387,1387,1387},
  {1388,1388,1388,1388,1388,1388,1388,1388,1389,1389,1389,1389,1389,1389,1389,1389,1390,1390,1390,1390,1390,1390,1390,1390,1391,1392,1,1,1,1,1,1},
  {1393,1393,1393,1393,1393,1393,1393,1393,1394,1394,1394,1394,1394,1394,1394,1394,1395,1395,1395,1395,1395,1395,1395,1395,1396,1396,1396,1396,1396,1396,1396,1396},
  {1397,1397,1397,1397,1397,1397,1397,1397,1398,1398,1398,1398,1398,1398,1398,1398,1399,1399,1399,1399,1399,1399,1399,1399,1400,1401,1,1,1,1,1,1},
  {1402,1402,1402,1402,1402,1402,1402,1402,1403,1403,1403,1403,1403,1403,1403,1403,1404,1404,1404,1404,1404,1404,1404,1404,1405,1405,1405,1405,1405,1405,1405,1405},
  {1406,1406,1406,1406,1406,1406,1406,1406,1407,1407,1407,1407,1407,1407,1407,1407,1408,1408,1408,1408,1408,1408,1408,1408,1409,1410,1411,1412,1413,1,1,1},
  {1372,1372,1372,1372,1372,1372,1372,1372,1414,1414,1414,1414,1414,1414,1414,1415,1375,1375,1375,1375,1375,1375,1375,1375,1376,1376,1376,1376,1376,1376,1376,1377},
  {1416,1416,1416,1416,1416,1416,1416,1416,1,1,1,1,1,1,1,1,1417,1417,1417,1417,1417,1417,1417,1417,1418,1418,1418,1418,1418,1418,1418,1419},
  {1420,1420,1420,1420,1420,1420,1420,1421,1422,1422,1422,1422,1422,1422,1422,1422,1423,1423,1423,1423,1423,1423,1423,1423,1424,1425,1426,1427,1428,1,1,1},
  {1429,1429,1429,1429,1429,1429,1429,1429,1,1,1,1,1,1,1,1,1430,1430,1430,1430,1430,1430,1430,1430,1431,1431,1431,1431,1431,1431,1431,1431},
  {1432,1432,1432,1432,1432,1432,1432,1432,1433,1433,1433,1433,1433,1433,1433,1433,1434,1434,1434,1434,1434,1434,1434,1434,1435,1436,1437,1438,1439,1,1,1},
  {1440,1440,1440,1440,1440,1440,1440,1440,1,1,1,1,1,1,1,1,1441,1441,1441,1441,1441,1441,1441,1441,1442,1442,1442,1442,1442,1442,1442,1442},
  {1443,1443,1443,1443,1443,1443,1443,1443,1444,1444,1444,1444,1444,1444,1444,1444,1445,1445,1445,1445,1445,1445,1445,1445,1446,1447,1448,1449,1450,1,1,1},
  {1451,1451,1451,1451,1451,1451,1451,1451,1,1,1,1,1,1,1,1,1452,1452,1452,1452,1452,1452,1452,1452,1453,1453,1453,1453,1453,1453,1453,1453},
  {1454,1454,1454,1454,1454,1454,1454,1454,1455,1455,1455,1455,1455,1455,1455,1455,1456,1456,1456,1456,1456,1456,1456,1456,1457,1458,1459,1460,1461,1,1,1},
  {1462,1462,1462,1462,1462,1462,1462,1462,1463,1463,1463,1463,1463,1463,1463,1464,1465,1465,1465,1465,1465,1465,1465,1465,1466,1466,1466,1466,1466,1466,1466,1467},
  {1468,1468,1468,1468,1468,1468,1468,1469,1470,1470,1470,1470,1470,1470,1470,1470,1471,1471,1471,1471,1471,1471,1471,1471,1472,1473,1,1,1,1,1,1},
  {1474,1474,1474,1474,1474,1474,1474,1474,1475,1475,1475,1475,1475,1475,1475,1475,1476,1476,1476,1476,1476,1476,1476,1476,1477,1477,1477,1477,1477,1477,1477,1477},
  {1478,1478,1478,1478,1478,1478,1478,1478,1479,1479,1479,1479,1479,1479,1479,1479,1480,1480,1480,1480,1480,1480,1480,1480,1481,1482,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1483,1483,1483,1483,1483,1483,1483,1483,1484,1484,1484,1484,1484,1484,1484,1484,1485,1485,1485,1485,1485,1485,1485,1485},
  {1486,1486,1486,1486,1486,1486,1486,1486,1487,1487,1487,1487,1487,1487,1487,1487,1488,1488,1488,1488,1488,1488,1488,1488,1489,1490,1,1,1,1,1,1},
  {1491,1491,1491,1491,1491,1491,1491,1491,1,1,1,1,1,1,1,1,1492,1492,1492,1492,1492,1492,1492,1492,1493,1493,1493,1493,1493,1493,1493,1493},
  {1494,1494,1494,1494,1494,1494,1494,1494,1495,1495,1495,1495,1495,1495,1495,1495,1496,1496,1496,1496,1496,1496,1496,1496,1497,1498,1499,1500,1501,1,1,1},
  {1462,1462,1462,1462,1462,1462,1462,1462,1502,1502,1502,1502,1502,1502,1502,1503,1465,1465,1465,1465,1465,1465,1465,1465,1466,1466,1466,1466,1466,1466,1466,1467},
  {1504,1504,1504,1504,1504,1504,1504,1504,1,1,1,1,1,1,1,1,1505,1505,1505,1505,1505,1505,1505,1505,1506,1506,1506,1506,1506,1506,1506,1507},
  {1508,1508,1508,1508,1508,1508,1508,1509,1510,1510,1510,1510,1510,1510,1510,1510,1511,1511,1511,1511,1511,1511,1511,1511,1512,1513,1514,1515,1516,1,1,1},
  {1517,1517,1517,1517,1517,1517,1517,1517,1518,1518,1518,1518,1518,1518,1518,1518,1519,1519,1519,1519,1519,1519,1519,1519,1520,1520,1520,1520,1520,1520,1520,1520},
  {1521,1521,1521,1521,1521,1521,1521,1521,1522,1522,1522,1522,1522,1522,1522,1522,1523,1523,1523,1523,1523,1523,1523,1523,1524,1525,1526,1527,1528,1,1,1},
  {1529,1529,1529,1529,1529,1529,1529,1529,1530,1530,1530,1530,1530,1530,1530,1530,1531,1531,1531,1531,1531,1531,1531,1531,1532,1532,1532,1532,1532,1532,1532,1532},
  {1533,1533,1533,1533,1533,1533,1533,1533,1534,1534,1534,1534,1534,1534,1534,1534,1535,1535,1535,1535,1535,1535,1535,1535,1536,1537,1538,1539,1540,1,1,1},
  {1541,1541,1541,1541,1541,1541,1541,1541,1542,1542,1542,1542,1542,1542,1542,1542,1543,1543,1543,1543,1543,1543,1543,1543,1544,1544,1544,1544,1544,1544,1544,1544},
  {1545,1545,1545,1545,1545,1545,1545,1545,1546,1546,1546,1546,1546,1546,1546,1546,1547,1547,1547,1547,1547,1547,1547,1547,1548,1549,1550,1551,1552,1,1,1},
  {1553,1553,1553,1553,1553,1553,1553,1553,1554,1554,1554,1554,1554,1554,1554,1555,1556,1556,1556,1556,1556,1556,1556,1556,1557,1557,1557,1557,1557,1557,1557,1558},
  {1559,1559,1559,1559,1559,1559,1559,1560,1561,1561,1561,1561,1561,1561,1561,1561,1562,1562,1562,1562,1562,1562,1562,1562,1563,1564,1,1,1,1,1,1},
  {1565,1565,1565,1565,1565,1565,1565,1565,1566,1566,1566,1566,1566,1566,1566,1566,1567,1567,1567,1567,1567,1567,1567,1567,1568,1568,1568,1568,1568,1568,1568,1568},
  {1569,1569,1569,1569,1569,1569,1569,1569,1570,1570,1570,1570,1570,1570,1570,1570,1571,1571,1571,1571,1571,1571,1571,1571,1572,1573,1,1,1,1,1,1},
  {1574,1574,1574,1574,1574,1574,1574,1574,1575,1575,1575,1575,1575,1575,1575,1575,1576,1576,1576,1576,1576,1576,1576,1576,1577,1577,1577,1577,1577,1577,1577,1577},
  {1578,1578,1578,1578,1578,1578,1578,1578,1579,1579,1579,1579,1579,1579,1579,1579,1580,1580,1580,1580,1580,1580,1580,1580,1581,1582,1,1,1,1,1,1},
  {1583,1583,1583,1583,1583,1583,1583,1583,1584,1584,1584,1584,1584,1584,1584,1584,1585,1585,1585,1585,1585,1585,1585,1585,1586,1586,1586,1586,1586,1586,1586,1586},
  {1587,1587,1587,1587,1587,1587,1587,1587,1588,1588,1588,1588,1588,1588,1588,1588,1589,1589,1589,1589,1589,1589,1589,1589,1590,1591,1592,1593,1594,1,1,1},
  {1553,1553,1553,1553,1553,1553,1553,1553,1595,1595,1595,1595,1595,1595,1595,1596,1556,1556,1556,1556,1556,1556,1556,1556,1557,1557,1557,1557,1557,1557,1557,1558},
  {1597,1597,1597,1597,1597,1597,1597,1597,1598,1598,1598,1598,1598,1598,1598,1598,1599,1599,1599,1599,1599,1599,1599,1599,1600,1600,1600,1600,1600,1600,1600,1600},
  {1601,1601,1601,1601,1601,1601,1601,1601,1602,1602,1602,1602,1602,1602,1602,1602,1603,1603,1603,1603,1603,1603,1603,1603,1604,1604,1604,1604,1604,1604,1604,1604},
  {1605,1605,1605,1605,1605,1605,1605,1605,1606,1606,1606,1606,1606,1606,1606,1606,1607,1607,1607,1607,1607,1607,1607,1607,1608,1608,1608,1608,1608,1608,1608,1608},
  {1609,1609,1609,1609,1609,1609,1609,1609,1610,1610,1610,1610,1610,1610,1610,1610,1611,1611,1611,1611,1611,1611,1611,1611,1612,1612,1612,1612,1612,1612,1612,1612},
  {1613,1613,1613,1613,1613,1613,1613,1613,1614,1614,1614,1614,1614,1614,1614,1614,1615,1615,1615,1615,1615,1615,1615,1615,1616,1616,1616,1616,1616,1616,1616,1616},
  {1617,1617,1617,1617,1617,1617,1617,1617,1618,1618,1618,1618,1618,1618,1618,1618,1619,1619,1619,1619,1619,1619,1619,1619,1620,1620,1620,1620,1620,1620,1620,1620},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1621,1621,1621,1621,1621,1621,1621,1621,1622,1622,1622,1622,1622,1622,1622,1622},
  {1623,1623,1623,1623,1623,1623,1623,1623,1624,1624,1624,1624,1624,1624,1624,1624,1625,1625,1625,1625,1625,1625,1625,1625,1626,1627,1,1,1,1,1,1},
  {1628,1628,1628,1628,1628,1628,1628,1628,1629,1629,1629,1629,1629,1629,1629,1629,1630,1630,1630,1630,1630,1630,1630,1630,1631,1631,1631,1631,1631,1631,1631,1631},
  {1632,1632,1632,1632,1632,1632,1632,1632,1633,1633,1633,1633,1633,1633,1633,1633,1634,1634,1634,1634,1634,1634,1634,1634,1635,1635,1635,1635,1635,1635,1635,1635},
  {1636,1636,1636,1636,1636,1636,1636,1636,1637,1637,1637,1637,1637,1637,1637,1637,1638,1638,1638,1638,1638,1638,1638,1638,1639,1639,1639,1639,1639,1639,1639,1639},
  {1640,1640,1640,1640,1640,1640,1640,1640,1641,1641,1641,1641,1641,1641,1641,1641,1642,1642,1642,1642,1642,1642,1642,1642,1643,1643,1643,1643,1643,1643,1643,1643},
  {1644,1644,1644,1644,1644,1644,1644,1644,1645,1645,1645,1645,1645,1645,1645,1645,1646,1646,1646,1646,1646,1646,1646,1646,1647,1647,1647,1647,1647,1647,1647,1647},
  {1648,1648,1648,1648,1648,1648,1648,1648,1649,1649,1649,1649,1649,1649,1649,1649,1650,1650,1650,1650,1650,1650,1650,1650,1651,1651,1651,1651,1651,1651,1651,1651},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1652,1652,1652,1652,1652,1652,1652,1652,1653,1653,1653,1653,1653,1653,1653,1653},
  {1654,1654,1654,1654,1654,1654,1654,1654,1655,1655,1655,1655,1655,1655,1655,1655,1656,1656,1656,1656,1656,1656,1656,1656,1657,1658,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1659,1659,1659,1659,1659,1659,1659,1659,1660,1660,1660,1660,1660,1660,1660,1660},
  {1661,1661,1661,1661,1661,1661,1661,1661,1662,1662,1662,1662,1662,1662,1662,1662,1663,1663,1663,1663,1663,1663,1663,1663,1664,1665,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1666,1666,1666,1666,1666,1666,1666,1666,1667,1667,1667,1667,1667,1667,1667,1667},
  {1668,1668,1668,1668,1668,1668,1668,1668,1669,1669,1669,1669,1669,1669,1669,1669,1670,1670,1670,1670,1670,1670,1670,1670,1671,1672,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1673,1673,1673,1673,1673,1673,1673,1673,1674,1674,1674,1674,1674,1674,1674,1674},
  {1675,1675,1675,1675,1675,1675,1675,1675,1676,1676,1676,1676,1676,1676,1676,1676,1677,1677,1677,1677,1677,1677,1677,1677,1678,1679,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1680,1680,1680,1680,1680,1680,1680,1680,1681,1681,1681,1681,1681,1681,1681,1681},
  {1682,1682,1682,1682,1682,1682,1682,1682,1683,1683,1683,1683,1683,1683,1683,1683,1684,1684,1684,1684,1684,1684,1684,1684,1685,1686,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1687,1687,1687,1687,1687,1687,1687,1687,1688,1688,1688,1688,1688,1688,1688,1688},
  {1689,1689,1689,1689,1689,1689,1689,1689,1690,1690,1690,1690,1690,1690,1690,1690,1691,1691,1691,1691,1691,1691,1691,1691,1692,1693,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1694,1694,1694,1694,1694,1694,1694,1694,1695,1695,1695,1695,1695,1695,1695,1695},
  {1696,1696,1696,1696,1696,1696,1696,1696,1697,1697,1697,1697,1697,1697,1697,1697,1698,1698,1698,1698,1698,1698,1698,1698,1699,1700,1,1,1,1,1,1},
  {1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701,1701},
  {1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1701,1702,1702,1702,1702,1702,1702,1702},
  {1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702,1702},
};

M68KI_IN_RAM const unsigned short m68ki_top[2048] = {
  0,1,2,3,4,5,6,6,7,8,9,10,11,12,13,14,
  15,16,17,18,19,20,6,6,7,8,9,10,11,12,13,14,
  21,22,23,24,25,26,6,6,7,8,9,10,11,12,13,14,
  27,28,29,30,31,32,6,6,7,8,9,10,11,12,13,14,
  33,34,35,36,37,38,39,40,7,8,9,10,11,12,13,14,
  41,42,43,44,45,46,6,6,7,8,9,10,11,12,13,14,
  47,48,49,50,51,52,6,6,7,8,9,10,11,12,13,14,
  6,6,6,6,6,6,6,6,7,8,9,10,11,12,13,14,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,65,66,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,67,68,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,6,6,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,6,6,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,6,6,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,6,6,
  53,54,6,6,55,56,57,58,59,60,61,62,63,64,6,6,
  53,54,6,6,55,56,69,70,71,72,61,62,63,64,6,6,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,89,90,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,6,6,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,6,6,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,6,6,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,6,6,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,6,6,
  73,74,75,76,77,78,79,80,81,82,83,84,85,86,6,6,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,107,108,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,6,6,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,6,6,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,6,6,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,6,6,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,6,6,
  91,92,93,94,95,96,97,98,99,100,101,102,103,104,6,6,
  109,110,111,112,113,114,115,116,6,6,6,6,117,118,119,120,
  121,122,123,124,125,126,6,6,6,6,6,6,117,118,119,120,
  127,128,129,130,131,132,133,134,6,6,6,6,117,118,119,120,
  135,136,137,138,139,140,141,142,6,6,6,6,117,118,119,120,
  143,144,145,146,147,148,149,150,6,6,6,6,117,118,119,120,
  151,152,153,154,155,156,157,158,6,6,6,6,117,118,119,120,
  6,6,6,6,159,160,161,162,6,6,6,6,117,118,119,120,
  6,6,163,164,165,166,167,168,6,6,6,6,117,118,119,120,
  169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,
  169,170,171,172,173,174,185,186,177,178,179,180,181,182,187,188,
  169,170,171,172,173,174,189,190,177,178,179,180,181,182,191,192,
  169,170,171,172,173,174,193,194,177,178,179,180,181,182,195,196,
  169,170,171,172,173,174,197,198,177,178,179,180,181,182,199,200,
  169,170,171,172,173,174,201,202,177,178,179,180,181,182,203,204,
  169,170,171,172,173,174,205,206,177,178,179,180,181,182,207,208,
  169,170,171,172,173,174,209,210,177,178,179,180,181,182,211,212,
  213,214,214,214,214,214,214,215,216,217,217,217,217,217,217,218,
  219,220,220,220,220,220,220,221,222,223,223,223,223,223,223,224,
  225,226,226,226,226,226,226,227,228,229,229,229,229,229,229,230,
  231,232,232,232,232,232,232,233,234,235,235,235,235,235,235,236,
  237,238,238,238,238,238,238,239,240,241,241,241,241,241,241,242,
  243,244,244,244,244,244,244,245,246,247,247,247,247,247,247,248,
  249,250,250,250,250,250,250,251,252,253,253,253,253,253,253,254,
  255,256,256,256,256,256,256,257,258,259,259,259,259,259,259,260,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  261,261,261,261,261,261,261,261,6,6,6,6,6,6,6,6,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,
  262,263,264,265,266,267,268,269,278,271,272,273,274,275,276,277,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,
  279,280,281,282,283,284,285,286,295,288,289,290,291,292,293,294,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,296,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
  297,298,299,300,301,302,303,304,313,306,307,308,309,310,311,312,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,
  314,315,316,317,318,319,320,321,330,323,324,325,326,327,328,329,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,
  331,332,333,334,335,336,337,338,347,340,341,342,343,344,345,346,
  348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,
  348,349,350,351,352,353,364,365,356,357,358,359,360,361,366,367,
  348,349,350,351,352,353,368,369,356,357,358,359,360,361,370,371,
  348,349,350,351,352,353,372,373,356,357,358,359,360,361,374,375,
  348,349,350,351,352,353,6,6,356,357,358,359,360,361,6,6,
  348,349,350,351,352,353,6,6,356,357,358,359,360,361,6,6,
  348,349,350,351,352,353,6,6,356,357,358,359,360,361,6,6,
  348,349,350,351,352,353,6,6,356,357,358,359,360,361,6,6,
  376,376,376,376,376,376,376,376,376,376,376,376,376,376,376,376,
  376,376,376,376,376,376,376,376,376,376,376,376,376,376,376,376,
  376,376,376,376,376,376,376,376,377,378,378,378,378,378,378,378,
  376,376,376,376,376,376,376,376,378,378,378,378,378,378,378,378,
  376,376,376,376,376,376,376,376,378,378,378,378,378,378,378,378,
  376,376,376,376,376,376,376,376,378,378,378,378,378,378,378,378,
  376,376,376,376,376,376,376,376,378,378,378,378,378,378,378,378,
  376,376,376,376,376,376,376,376,378,378,378,378,378,378,378,378,
};
