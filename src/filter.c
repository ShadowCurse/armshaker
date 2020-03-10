/* Instruction printing code for the ARM
   Copyright (C) 1994-2020 Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
   Modification by James G. Smith (jsmith@cygnus.co.uk)
   Modification by Fredrik Strupe (fredrik@strupe.net)

   This file is part of libopcodes.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */


/*
 * More specifically, the following opcode tables are modified versions
 * of the ones found in binutils/opcodes/arm-dis.c and
 * binutils/opcodes/aarch64-opc.c
 *
 * Some of the disassembly code is also a modified version of what
 * is found in those files.
 */

#include "filter.h"
#include "util.h"

struct opcode
{
    uint32_t op_value;
    uint32_t op_mask;
    uint32_t sb_mask;
    const char *disassembly;
};

/*
 * The following opcode tables are modified version of the ones found
 * in libopcodes. Namely, some unneeded information like feature
 * versions are removed, and SBO/SBZ bit masks have been added.
 */
static const struct opcode base_opcodes[] =
{
#ifdef __aarch64__
    /*
     * The issue with including SBO/SBZ bits in the insn value
     * isn't as prevalent in aarch64, and the cases where it actually
     * happens (like ldar and ldarb), capstone manages to disassemble
     * them, thus masking over the issue.
     *
     * Would be nice to have the table here still for completeness,
     * but it's frankly too much work with little gain as it stands.
     */
    {0x00000000, 0x00000000, 0, 0}
#else
    {0xe1a00000, 0xffffffff, 0, "nop\t\t\t; (mov r0, r0)"},
    {0xe7f000f0, 0xfff000f0, 0, "udf\t#%e"},
    {0x012fff10, 0x0ffffff0, 0x000fff00, "bx%c\t%0-3r"},
    {0x00000090, 0x0fe000f0, 0x0000f000, "mul%20's%c\t%16-19R, %0-3R, %8-11R"},
    {0x00200090, 0x0fe000f0, 0, "mla%20's%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x01000090, 0x0fb00ff0, 0, "swp%22'b%c\t%12-15RU, %0-3Ru, [%16-19RuU]"},
    {0x00800090, 0x0fa000f0, 0, "%22?sumull%20's%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x00a00090, 0x0fa000f0, 0, "%22?sumlal%20's%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0xe320f010, 0xffffffff, 0x0000ff00, "esb"},
    {0x0320f005, 0x0fffffff, 0x0000ff00, "sevl"},
    {0xe1000070, 0xfff000f0, 0, "hlt\t0x%16-19X%12-15X%8-11X%0-3X"},
    {0x01800e90, 0x0ff00ff0, 0x00000c00, "stlex%c\t%12-15r, %0-3r, [%16-19R]"},
    {0x01900e9f, 0x0ff00fff, 0x00000c0f, "ldaex%c\t%12-15r, [%16-19R]"},
    {0x01a00e90, 0x0ff00ff0, 0x00000c00, "stlexd%c\t%12-15r, %0-3r, %0-3T, [%16-19R]"},
    {0x01b00e9f, 0x0ff00fff, 0x00000c0f, "ldaexd%c\t%12-15r, %12-15T, [%16-19R]"},
    {0x01c00e90, 0x0ff00ff0, 0x00000c00, "stlexb%c\t%12-15r, %0-3r, [%16-19R]"},
    {0x01d00e9f, 0x0ff00fff, 0x00000c0f, "ldaexb%c\t%12-15r, [%16-19R]"},
    {0x01e00e90, 0x0ff00ff0, 0x00000c00, "stlexh%c\t%12-15r, %0-3r, [%16-19R]"},
    {0x01f00e9f, 0x0ff00fff, 0x00000c0f, "ldaexh%c\t%12-15r, [%16-19R]"},
    {0x0180fc90, 0x0ff0fff0, 0x0000fc00, "stl%c\t%0-3r, [%16-19R]"},
    {0x01900c9f, 0x0ff00fff, 0x00000c0f, "lda%c\t%12-15r, [%16-19R]"},
    {0x01c0fc90, 0x0ff0fff0, 0x0000fc00, "stlb%c\t%0-3r, [%16-19R]"},
    {0x01d00c9f, 0x0ff00fff, 0x00000c0f, "ldab%c\t%12-15r, [%16-19R]"},
    {0x01e0fc90, 0x0ff0fff0, 0x0000fc00, "stlh%c\t%0-3r, [%16-19R]"},
    {0x01f00c9f, 0x0ff00fff, 0x00000c0f, "ldah%c\t%12-15r, [%16-19R]"},
    {0xe1000040, 0xfff00ff0, 0x00000d00, "crc32b\t%12-15R, %16-19R, %0-3R"},
    {0xe1200040, 0xfff00ff0, 0x00000d00, "crc32h\t%12-15R, %16-19R, %0-3R"},
    {0xe1400040, 0xfff00ff0, 0x00000d00, "crc32w\t%12-15R, %16-19R, %0-3R"},
    {0xe1000240, 0xfff00ff0, 0x00000d00, "crc32cb\t%12-15R, %16-19R, %0-3R"},
    {0xe1200240, 0xfff00ff0, 0x00000d00, "crc32ch\t%12-15R, %16-19R, %0-3R"},
    {0xe1400240, 0xfff00ff0, 0x00000d00, "crc32cw\t%12-15R, %16-19R, %0-3R"},
    {0xf1100000, 0xfffffdff, 0x000ffd0f, "setpan\t#%9-9d"},
    {0x0160006e, 0x0fffffff, 0x000fff0f, "eret%c"},
    {0x01400070, 0x0ff000f0, 0, "hvc%c\t%e"},
    {0x0710f010, 0x0ff0f0f0, 0x0000f000, "sdiv%c\t%16-19r, %0-3r, %8-11r"},
    {0x0730f010, 0x0ff0f0f0, 0x0000f000, "udiv%c\t%16-19r, %0-3r, %8-11r"},
    {0xf410f000, 0xfc70f000, 0x0000f000, "pldw\t%a"},
    {0xe320f014, 0xffffffff, 0x0000ff00, "csdb"},
    {0xf57ff040, 0xffffffff, 0x000fff00, "ssbb"},
    {0xf57ff044, 0xffffffff, 0x000fff00, "pssbb"},
    {0xf450f000, 0xfd70f000, 0x0000f000, "pli\t%P"},
    {0x0320f0f0, 0x0ffffff0, 0x0000ff00, "dbg%c\t#%0-3d"},
    {0xf57ff051, 0xfffffff3, 0x000fff00, "dmb\t%U"},
    {0xf57ff041, 0xfffffff3, 0x000fff00, "dsb\t%U"},
    {0xf57ff050, 0xfffffff0, 0x000fff00, "dmb\t%U"},
    {0xf57ff040, 0xfffffff0, 0x000fff00, "dsb\t%U"},
    {0xf57ff060, 0xfffffff0, 0x000fff00, "isb\t%U"},
    {0x0320f000, 0x0fffffff, 0x0000ff00, "nop%c\t{%0-7d}"},
    {0x07c0001f, 0x0fe0007f, 0, "bfc%c\t%12-15R, %E"},
    {0x07c00010, 0x0fe00070, 0, "bfi%c\t%12-15R, %0-3r, %E"},
    {0x00600090, 0x0ff000f0, 0, "mls%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x002000b0, 0x0f3000f0, 0, "strht%c\t%12-15R, %S"},
    {0x00300090, 0x0f3000f0, 0, "UNDEFINED"},
    {0x00300090, 0x0f300090, 0, "ldr%6's%5?hbt%c\t%12-15R, %S"},
    {0x03000000, 0x0ff00000, 0, "movw%c\t%12-15R, %V"},
    {0x03400000, 0x0ff00000, 0, "movt%c\t%12-15R, %V"},
    {0x06ff0f30, 0x0fff0ff0, 0x000f0f00, "rbit%c\t%12-15R, %0-3R"},
    {0x07a00050, 0x0fa00070, 0, "%22?usbfx%c\t%12-15r, %0-3r, #%7-11d, #%16-20W"},
    {0x01600070, 0x0ff000f0, 0x000fff00, "smc%c\t%e"},
    {0xf57ff01f, 0xffffffff, 0x000fff0f, "clrex"},
    {0x01d00f9f, 0x0ff00fff, 0x00000c0f, "ldrexb%c\t%12-15R, [%16-19R]"},
    {0x01b00f9f, 0x0ff00fff, 0x00000c0f, "ldrexd%c\t%12-15r, [%16-19R]"},
    {0x01f00f9f, 0x0ff00fff, 0x00000c0f, "ldrexh%c\t%12-15R, [%16-19R]"},
    {0x01c00f90, 0x0ff00ff0, 0x00000c00, "strexb%c\t%12-15R, %0-3R, [%16-19R]"},
    {0x01a00f90, 0x0ff00ff0, 0x00000c00, "strexd%c\t%12-15R, %0-3r, [%16-19R]"},
    {0x01e00f90, 0x0ff00ff0, 0x00000c00, "strexh%c\t%12-15R, %0-3R, [%16-19R]"},
    {0xf57ff070, 0xffffffff, 0x000fff0f, "sb"},
    {0x0320f001, 0x0fffffff, 0x0000ff00, "yield%c"},
    {0x0320f002, 0x0fffffff, 0x0000ff00, "wfe%c"},
    {0x0320f003, 0x0fffffff, 0x0000ff00, "wfi%c"},
    {0x0320f004, 0x0fffffff, 0x0000ff00, "sev%c"},
    {0x0320f000, 0x0fffff00, 0x0000ff00, "nop%c\t{%0-7d}"},
    {0xf1080000, 0xfffffe3f, 0x0000fe00, "cpsie\t%8'a%7'i%6'f"},
    {0xf10a0000, 0xfffffe20, 0x0000fe00, "cpsie\t%8'a%7'i%6'f,#%0-4d"},
    {0xf10c0000, 0xfffffe3f, 0x0000fe00, "cpsid\t%8'a%7'i%6'f"},
    {0xf10e0000, 0xfffffe20, 0x0000fe00, "cpsid\t%8'a%7'i%6'f,#%0-4d"},
    {0xf1000000, 0xfff1fe20, 0x0000fe00, "cps\t#%0-4d"},
    {0x06800010, 0x0ff00ff0, 0, "pkhbt%c\t%12-15R, %16-19R, %0-3R"},
    {0x06800010, 0x0ff00070, 0, "pkhbt%c\t%12-15R, %16-19R, %0-3R, lsl #%7-11d"},
    {0x06800050, 0x0ff00ff0, 0, "pkhtb%c\t%12-15R, %16-19R, %0-3R, asr #32"},
    {0x06800050, 0x0ff00070, 0, "pkhtb%c\t%12-15R, %16-19R, %0-3R, asr #%7-11d"},
    {0x01900f9f, 0x0ff00fff, 0x00000c0f, "ldrex%c\tr%12-15d, [%16-19R]"},
    {0x06200f10, 0x0ff00ff0, 0x00000f00, "qadd16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06200f90, 0x0ff00ff0, 0x00000f00, "qadd8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06200f30, 0x0ff00ff0, 0x00000f00, "qasx%c\t%12-15R, %16-19R, %0-3R"},
    {0x06200f70, 0x0ff00ff0, 0x00000f00, "qsub16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06200ff0, 0x0ff00ff0, 0x00000f00, "qsub8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06200f50, 0x0ff00ff0, 0x00000f00, "qsax%c\t%12-15R, %16-19R, %0-3R"},
    {0x06100f10, 0x0ff00ff0, 0x00000f00, "sadd16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06100f90, 0x0ff00ff0, 0x00000f00, "sadd8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06100f30, 0x0ff00ff0, 0x00000f00, "sasx%c\t%12-15R, %16-19R, %0-3R"},
    {0x06300f10, 0x0ff00ff0, 0x00000f00, "shadd16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06300f90, 0x0ff00ff0, 0x00000f00, "shadd8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06300f30, 0x0ff00ff0, 0x00000f00, "shasx%c\t%12-15R, %16-19R, %0-3R"},
    {0x06300f70, 0x0ff00ff0, 0x00000f00, "shsub16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06300ff0, 0x0ff00ff0, 0x00000f00, "shsub8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06300f50, 0x0ff00ff0, 0x00000f00, "shsax%c\t%12-15R, %16-19R, %0-3R"},
    {0x06100f70, 0x0ff00ff0, 0x00000f00, "ssub16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06100ff0, 0x0ff00ff0, 0x00000f00, "ssub8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06100f50, 0x0ff00ff0, 0x00000f00, "ssax%c\t%12-15R, %16-19R, %0-3R"},
    {0x06500f10, 0x0ff00ff0, 0x00000f00, "uadd16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06500f90, 0x0ff00ff0, 0x00000f00, "uadd8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06500f30, 0x0ff00ff0, 0x00000f00, "uasx%c\t%12-15R, %16-19R, %0-3R"},
    {0x06700f10, 0x0ff00ff0, 0x00000f00, "uhadd16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06700f90, 0x0ff00ff0, 0x00000f00, "uhadd8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06700f30, 0x0ff00ff0, 0x00000f00, "uhasx%c\t%12-15R, %16-19R, %0-3R"},
    {0x06700f70, 0x0ff00ff0, 0x00000f00, "uhsub16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06700ff0, 0x0ff00ff0, 0x00000f00, "uhsub8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06700f50, 0x0ff00ff0, 0x00000f00, "uhsax%c\t%12-15R, %16-19R, %0-3R"},
    {0x06600f10, 0x0ff00ff0, 0x00000f00, "uqadd16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06600f90, 0x0ff00ff0, 0x00000f00, "uqadd8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06600f30, 0x0ff00ff0, 0x00000f00, "uqasx%c\t%12-15R, %16-19R, %0-3R"},
    {0x06600f70, 0x0ff00ff0, 0x00000f00, "uqsub16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06600ff0, 0x0ff00ff0, 0x00000f00, "uqsub8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06600f50, 0x0ff00ff0, 0x00000f00, "uqsax%c\t%12-15R, %16-19R, %0-3R"},
    {0x06500f70, 0x0ff00ff0, 0x00000f00, "usub16%c\t%12-15R, %16-19R, %0-3R"},
    {0x06500ff0, 0x0ff00ff0, 0x00000f00, "usub8%c\t%12-15R, %16-19R, %0-3R"},
    {0x06500f50, 0x0ff00ff0, 0x00000f00, "usax%c\t%12-15R, %16-19R, %0-3R"},
    {0x06bf0f30, 0x0fff0ff0, 0x000f0f00, "rev%c\t%12-15R, %0-3R"},
    {0x06bf0fb0, 0x0fff0ff0, 0x000f0f00, "rev16%c\t%12-15R, %0-3R"},
    {0x06ff0fb0, 0x0fff0ff0, 0x000f0f00, "revsh%c\t%12-15R, %0-3R"},
    {0xf8100a00, 0xfe50ffff, 0x0000ffff, "rfe%23?id%24?ba\t%16-19r%21'!"},
    {0x06bf0070, 0x0fff0ff0, 0x00000300, "sxth%c\t%12-15R, %0-3R"},
    {0x06bf0470, 0x0fff0ff0, 0x00000300, "sxth%c\t%12-15R, %0-3R, ror #8"},
    {0x06bf0870, 0x0fff0ff0, 0x00000300, "sxth%c\t%12-15R, %0-3R, ror #16"},
    {0x06bf0c70, 0x0fff0ff0, 0x00000300, "sxth%c\t%12-15R, %0-3R, ror #24"},
    {0x068f0070, 0x0fff0ff0, 0x00000300, "sxtb16%c\t%12-15R, %0-3R"},
    {0x068f0470, 0x0fff0ff0, 0x00000300, "sxtb16%c\t%12-15R, %0-3R, ror #8"},
    {0x068f0870, 0x0fff0ff0, 0x00000300, "sxtb16%c\t%12-15R, %0-3R, ror #16"},
    {0x068f0c70, 0x0fff0ff0, 0x00000300, "sxtb16%c\t%12-15R, %0-3R, ror #24"},
    {0x06af0070, 0x0fff0ff0, 0x00000300, "sxtb%c\t%12-15R, %0-3R"},
    {0x06af0470, 0x0fff0ff0, 0x00000300, "sxtb%c\t%12-15R, %0-3R, ror #8"},
    {0x06af0870, 0x0fff0ff0, 0x00000300, "sxtb%c\t%12-15R, %0-3R, ror #16"},
    {0x06af0c70, 0x0fff0ff0, 0x00000300, "sxtb%c\t%12-15R, %0-3R, ror #24"},
    {0x06ff0070, 0x0fff0ff0, 0x00000300, "uxth%c\t%12-15R, %0-3R"},
    {0x06ff0470, 0x0fff0ff0, 0x00000300, "uxth%c\t%12-15R, %0-3R, ror #8"},
    {0x06ff0870, 0x0fff0ff0, 0x00000300, "uxth%c\t%12-15R, %0-3R, ror #16"},
    {0x06ff0c70, 0x0fff0ff0, 0x00000300, "uxth%c\t%12-15R, %0-3R, ror #24"},
    {0x06cf0070, 0x0fff0ff0, 0x00000300, "uxtb16%c\t%12-15R, %0-3R"},
    {0x06cf0470, 0x0fff0ff0, 0x00000300, "uxtb16%c\t%12-15R, %0-3R, ror #8"},
    {0x06cf0870, 0x0fff0ff0, 0x00000300, "uxtb16%c\t%12-15R, %0-3R, ror #16"},
    {0x06cf0c70, 0x0fff0ff0, 0x00000300, "uxtb16%c\t%12-15R, %0-3R, ror #24"},
    {0x06ef0070, 0x0fff0ff0, 0x00000300, "uxtb%c\t%12-15R, %0-3R"},
    {0x06ef0470, 0x0fff0ff0, 0x00000300, "uxtb%c\t%12-15R, %0-3R, ror #8"},
    {0x06ef0870, 0x0fff0ff0, 0x00000300, "uxtb%c\t%12-15R, %0-3R, ror #16"},
    {0x06ef0c70, 0x0fff0ff0, 0x00000300, "uxtb%c\t%12-15R, %0-3R, ror #24"},
    {0x06b00070, 0x0ff00ff0, 0x00000300, "sxtah%c\t%12-15R, %16-19r, %0-3R"},
    {0x06b00470, 0x0ff00ff0, 0x00000300, "sxtah%c\t%12-15R, %16-19r, %0-3R, ror #8"},
    {0x06b00870, 0x0ff00ff0, 0x00000300, "sxtah%c\t%12-15R, %16-19r, %0-3R, ror #16"},
    {0x06b00c70, 0x0ff00ff0, 0x00000300, "sxtah%c\t%12-15R, %16-19r, %0-3R, ror #24"},
    {0x06800070, 0x0ff00ff0, 0x00000300, "sxtab16%c\t%12-15R, %16-19r, %0-3R"},
    {0x06800470, 0x0ff00ff0, 0x00000300, "sxtab16%c\t%12-15R, %16-19r, %0-3R, ror #8"},
    {0x06800870, 0x0ff00ff0, 0x00000300, "sxtab16%c\t%12-15R, %16-19r, %0-3R, ror #16"},
    {0x06800c70, 0x0ff00ff0, 0x00000300, "sxtab16%c\t%12-15R, %16-19r, %0-3R, ror #24"},
    {0x06a00070, 0x0ff00ff0, 0x00000300, "sxtab%c\t%12-15R, %16-19r, %0-3R"},
    {0x06a00470, 0x0ff00ff0, 0x00000300, "sxtab%c\t%12-15R, %16-19r, %0-3R, ror #8"},
    {0x06a00870, 0x0ff00ff0, 0x00000300, "sxtab%c\t%12-15R, %16-19r, %0-3R, ror #16"},
    {0x06a00c70, 0x0ff00ff0, 0x00000300, "sxtab%c\t%12-15R, %16-19r, %0-3R, ror #24"},
    {0x06f00070, 0x0ff00ff0, 0x00000300, "uxtah%c\t%12-15R, %16-19r, %0-3R"},
    {0x06f00470, 0x0ff00ff0, 0x00000300, "uxtah%c\t%12-15R, %16-19r, %0-3R, ror #8"},
    {0x06f00870, 0x0ff00ff0, 0x00000300, "uxtah%c\t%12-15R, %16-19r, %0-3R, ror #16"},
    {0x06f00c70, 0x0ff00ff0, 0x00000300, "uxtah%c\t%12-15R, %16-19r, %0-3R, ror #24"},
    {0x06c00070, 0x0ff00ff0, 0x00000300, "uxtab16%c\t%12-15R, %16-19r, %0-3R"},
    {0x06c00470, 0x0ff00ff0, 0x00000300, "uxtab16%c\t%12-15R, %16-19r, %0-3R, ror #8"},
    {0x06c00870, 0x0ff00ff0, 0x00000300, "uxtab16%c\t%12-15R, %16-19r, %0-3R, ror #16"},
    {0x06c00c70, 0x0ff00ff0, 0x00000300, "uxtab16%c\t%12-15R, %16-19r, %0-3R, ROR #24"},
    {0x06e00070, 0x0ff00ff0, 0x00000300, "uxtab%c\t%12-15R, %16-19r, %0-3R"},
    {0x06e00470, 0x0ff00ff0, 0x00000300, "uxtab%c\t%12-15R, %16-19r, %0-3R, ror #8"},
    {0x06e00870, 0x0ff00ff0, 0x00000300, "uxtab%c\t%12-15R, %16-19r, %0-3R, ror #16"},
    {0x06e00c70, 0x0ff00ff0, 0x00000300, "uxtab%c\t%12-15R, %16-19r, %0-3R, ror #24"},
    {0x06800fb0, 0x0ff00ff0, 0x00000f00, "sel%c\t%12-15R, %16-19R, %0-3R"},
    {0xf1010000, 0xfffffc00, 0x000efd0f, "setend\t%9?ble"},
    {0x0700f010, 0x0ff0f0d0, 0, "smuad%5'x%c\t%16-19R, %0-3R, %8-11R"},
    {0x0700f050, 0x0ff0f0d0, 0, "smusd%5'x%c\t%16-19R, %0-3R, %8-11R"},
    {0x07000010, 0x0ff000d0, 0, "smlad%5'x%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x07400010, 0x0ff000d0, 0, "smlald%5'x%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x07000050, 0x0ff000d0, 0, "smlsd%5'x%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x07400050, 0x0ff000d0, 0, "smlsld%5'x%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x0750f010, 0x0ff0f0d0, 0, "smmul%5'r%c\t%16-19R, %0-3R, %8-11R"},
    {0x07500010, 0x0ff000d0, 0, "smmla%5'r%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x075000d0, 0x0ff000d0, 0, "smmls%5'r%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0xf84d0500, 0xfe5fffe0, 0x000fffe0, "srs%23?id%24?ba\t%16-19r%21'!, #%0-4d"},
    {0x06a00010, 0x0fe00ff0, 0, "ssat%c\t%12-15R, #%16-20W, %0-3R"},
    {0x06a00010, 0x0fe00070, 0, "ssat%c\t%12-15R, #%16-20W, %0-3R, lsl #%7-11d"},
    {0x06a00050, 0x0fe00070, 0, "ssat%c\t%12-15R, #%16-20W, %0-3R, asr #%7-11d"},
    {0x06a00f30, 0x0ff00ff0, 0x00000f00, "ssat16%c\t%12-15r, #%16-19W, %0-3r"},
    {0x01800f90, 0x0ff00ff0, 0x00000c00, "strex%c\t%12-15R, %0-3R, [%16-19R]"},
    {0x00400090, 0x0ff000f0, 0, "umaal%c\t%12-15R, %16-19R, %0-3R, %8-11R"},
    {0x0780f010, 0x0ff0f0f0, 0, "usad8%c\t%16-19R, %0-3R, %8-11R"},
    {0x07800010, 0x0ff000f0, 0, "usada8%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x06e00010, 0x0fe00ff0, 0, "usat%c\t%12-15R, #%16-20d, %0-3R"},
    {0x06e00010, 0x0fe00070, 0, "usat%c\t%12-15R, #%16-20d, %0-3R, lsl #%7-11d"},
    {0x06e00050, 0x0fe00070, 0, "usat%c\t%12-15R, #%16-20d, %0-3R, asr #%7-11d"},
    {0x06e00f30, 0x0ff00ff0, 0x00000f00, "usat16%c\t%12-15R, #%16-19d, %0-3R"},
    {0x012fff20, 0x0ffffff0, 0x000fff00, "bxj%c\t%0-3R"},
    {0xe1200070, 0xfff000f0, 0, "bkpt\t0x%16-19X%12-15X%8-11X%0-3X"},
    {0xfa000000, 0xfe000000, 0, "blx\t%B"},
    {0x012fff30, 0x0ffffff0, 0x000fff00, "blx%c\t%0-3R"},
    {0x016f0f10, 0x0fff0ff0, 0x000f0f00, "clz%c\t%12-15R, %0-3R"},
    {0x000000d0, 0x0e1000f0, 0x00000f00, "ldrd%c\t%12-15r, %s"},
    {0x000000f0, 0x0e1000f0, 0x00000f00, "strd%c\t%12-15r, %s"},
    {0xf450f000, 0xfc70f000, 0x0000f000, "pld\t%a"},
    {0x01000080, 0x0ff000f0, 0, "smlabb%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x010000a0, 0x0ff000f0, 0, "smlatb%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x010000c0, 0x0ff000f0, 0, "smlabt%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x010000e0, 0x0ff000f0, 0, "smlatt%c\t%16-19r, %0-3r, %8-11R, %12-15R"},
    {0x01200080, 0x0ff000f0, 0, "smlawb%c\t%16-19R, %0-3R, %8-11R, %12-15R"},
    {0x012000c0, 0x0ff000f0, 0, "smlawt%c\t%16-19R, %0-3r, %8-11R, %12-15R"},
    {0x01400080, 0x0ff000f0, 0, "smlalbb%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x014000a0, 0x0ff000f0, 0, "smlaltb%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x014000c0, 0x0ff000f0, 0, "smlalbt%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x014000e0, 0x0ff000f0, 0, "smlaltt%c\t%12-15Ru, %16-19Ru, %0-3R, %8-11R"},
    {0x01600080, 0x0ff0f0f0, 0x0000f000, "smulbb%c\t%16-19R, %0-3R, %8-11R"},
    {0x016000a0, 0x0ff0f0f0, 0x0000f000, "smultb%c\t%16-19R, %0-3R, %8-11R"},
    {0x016000c0, 0x0ff0f0f0, 0x0000f000, "smulbt%c\t%16-19R, %0-3R, %8-11R"},
    {0x016000e0, 0x0ff0f0f0, 0x0000f000, "smultt%c\t%16-19R, %0-3R, %8-11R"},
    {0x012000a0, 0x0ff0f0f0, 0x0000f000, "smulwb%c\t%16-19R, %0-3R, %8-11R"},
    {0x012000e0, 0x0ff0f0f0, 0x0000f000, "smulwt%c\t%16-19R, %0-3R, %8-11R"},
    {0x01000050, 0x0ff00ff0, 0x00000f00, "qadd%c\t%12-15R, %0-3R, %16-19R"},
    {0x01400050, 0x0ff00ff0, 0x00000f00, "qdadd%c\t%12-15R, %0-3R, %16-19R"},
    {0x01200050, 0x0ff00ff0, 0x00000f00, "qsub%c\t%12-15R, %0-3R, %16-19R"},
    {0x01600050, 0x0ff00ff0, 0x00000f00, "qdsub%c\t%12-15R, %0-3R, %16-19R"},
    {0x052d0004, 0x0fff0fff, 0, "push%c\t{%12-15r}\t\t; (str%c %12-15r, %a)"},
    {0x04400000, 0x0e500000, 0, "strb%t%c\t%12-15R, %a"},
    {0x04000000, 0x0e500000, 0, "str%t%c\t%12-15r, %a"},
    {0x06400000, 0x0e500ff0, 0, "strb%t%c\t%12-15R, %a"},
    {0x06000000, 0x0e500ff0, 0, "str%t%c\t%12-15r, %a"},
    {0x04400000, 0x0c500010, 0, "strb%t%c\t%12-15R, %a"},
    {0x04000000, 0x0c500010, 0, "str%t%c\t%12-15r, %a"},
    {0x04400000, 0x0e500000, 0, "strb%c\t%12-15R, %a"},
    {0x06400000, 0x0e500010, 0, "strb%c\t%12-15R, %a"},
    {0x004000b0, 0x0e5000f0, 0x00000f00, "strh%c\t%12-15R, %s"},
    {0x000000b0, 0x0e500ff0, 0x00000f00, "strh%c\t%12-15R, %s"},
    {0x00500090, 0x0e5000f0, 0, "UNDEFINED"},
    {0x00500090, 0x0e500090, 0, "ldr%6's%5?hb%c\t%12-15R, %s"},
    {0x00100090, 0x0e500ff0, 0, "UNDEFINED"},
    {0x00100090, 0x0e500f90, 0, "ldr%6's%5?hb%c\t%12-15R, %s"},
    {0x02000000, 0x0fe00000, 0, "and%20's%c\t%12-15r, %16-19r, %o"},
    {0x00000000, 0x0fe00010, 0, "and%20's%c\t%12-15r, %16-19r, %o"},
    {0x00000010, 0x0fe00090, 0, "and%20's%c\t%12-15R, %16-19R, %o"},
    {0x02200000, 0x0fe00000, 0, "eor%20's%c\t%12-15r, %16-19r, %o"},
    {0x00200000, 0x0fe00010, 0, "eor%20's%c\t%12-15r, %16-19r, %o"},
    {0x00200010, 0x0fe00090, 0, "eor%20's%c\t%12-15R, %16-19R, %o"},
    {0x02400000, 0x0fe00000, 0, "sub%20's%c\t%12-15r, %16-19r, %o"},
    {0x00400000, 0x0fe00010, 0, "sub%20's%c\t%12-15r, %16-19r, %o"},
    {0x00400010, 0x0fe00090, 0, "sub%20's%c\t%12-15R, %16-19R, %o"},
    {0x02600000, 0x0fe00000, 0, "rsb%20's%c\t%12-15r, %16-19r, %o"},
    {0x00600000, 0x0fe00010, 0, "rsb%20's%c\t%12-15r, %16-19r, %o"},
    {0x00600010, 0x0fe00090, 0, "rsb%20's%c\t%12-15R, %16-19R, %o"},
    {0x02800000, 0x0fe00000, 0, "add%20's%c\t%12-15r, %16-19r, %o"},
    {0x00800000, 0x0fe00010, 0, "add%20's%c\t%12-15r, %16-19r, %o"},
    {0x00800010, 0x0fe00090, 0, "add%20's%c\t%12-15R, %16-19R, %o"},
    {0x02a00000, 0x0fe00000, 0, "adc%20's%c\t%12-15r, %16-19r, %o"},
    {0x00a00000, 0x0fe00010, 0, "adc%20's%c\t%12-15r, %16-19r, %o"},
    {0x00a00010, 0x0fe00090, 0, "adc%20's%c\t%12-15R, %16-19R, %o"},
    {0x02c00000, 0x0fe00000, 0, "sbc%20's%c\t%12-15r, %16-19r, %o"},
    {0x00c00000, 0x0fe00010, 0, "sbc%20's%c\t%12-15r, %16-19r, %o"},
    {0x00c00010, 0x0fe00090, 0, "sbc%20's%c\t%12-15R, %16-19R, %o"},
    {0x02e00000, 0x0fe00000, 0, "rsc%20's%c\t%12-15r, %16-19r, %o"},
    {0x00e00000, 0x0fe00010, 0, "rsc%20's%c\t%12-15r, %16-19r, %o"},
    {0x00e00010, 0x0fe00090, 0, "rsc%20's%c\t%12-15R, %16-19R, %o"},
    {0x0120f200, 0x0fb0f200, 0x0000fc00, "msr%c\t%C, %0-3r"},
    {0x0120f000, 0x0db0f000, 0x0000f000, "msr%c\t%C, %o"},
    {0x01000000, 0x0fb00cff, 0x000f0d0f, "mrs%c\t%12-15R, %R"},
    {0x03000000, 0x0fe00000, 0x0000f000, "tst%p%c\t%16-19r, %o"},
    {0x01000000, 0x0fe00010, 0x0000f000, "tst%p%c\t%16-19r, %o"},
    {0x01000010, 0x0fe00090, 0x0000f000, "tst%p%c\t%16-19R, %o"},
    {0x03300000, 0x0ff00000, 0x0000f000, "teq%p%c\t%16-19r, %o"},
    {0x01300000, 0x0ff00010, 0x0000f000, "teq%p%c\t%16-19r, %o"},
    {0x01300010, 0x0ff00010, 0x0000f000, "teq%p%c\t%16-19R, %o"},
    {0x03400000, 0x0fe00000, 0x0000f000, "cmp%p%c\t%16-19r, %o"},
    {0x01400000, 0x0fe00010, 0x0000f000, "cmp%p%c\t%16-19r, %o"},
    {0x01400010, 0x0fe00090, 0x0000f000, "cmp%p%c\t%16-19R, %o"},
    {0x03600000, 0x0fe00000, 0x0000f000, "cmn%p%c\t%16-19r, %o"},
    {0x01600000, 0x0fe00010, 0x0000f000, "cmn%p%c\t%16-19r, %o"},
    {0x01600010, 0x0fe00090, 0x0000f000, "cmn%p%c\t%16-19R, %o"},
    {0x03800000, 0x0fe00000, 0, "orr%20's%c\t%12-15r, %16-19r, %o"},
    {0x01800000, 0x0fe00010, 0, "orr%20's%c\t%12-15r, %16-19r, %o"},
    {0x01800010, 0x0fe00090, 0, "orr%20's%c\t%12-15R, %16-19R, %o"},
    {0x03a00000, 0x0fef0000, 0x000f0000, "mov%20's%c\t%12-15r, %o"},
    {0x01a00000, 0x0def0ff0, 0x000f0000, "mov%20's%c\t%12-15r, %0-3r"},
    {0x01a00000, 0x0def0060, 0x000f0000, "lsl%20's%c\t%12-15R, %q"},
    {0x01a00020, 0x0def0060, 0x000f0000, "lsr%20's%c\t%12-15R, %q"},
    {0x01a00040, 0x0def0060, 0x000f0000, "asr%20's%c\t%12-15R, %q"},
    {0x01a00060, 0x0def0ff0, 0x000f0000, "rrx%20's%c\t%12-15r, %0-3r"},
    {0x01a00060, 0x0def0060, 0x000f0000, "ror%20's%c\t%12-15R, %q"},
    {0x03c00000, 0x0fe00000, 0, "bic%20's%c\t%12-15r, %16-19r, %o"},
    {0x01c00000, 0x0fe00010, 0, "bic%20's%c\t%12-15r, %16-19r, %o"},
    {0x01c00010, 0x0fe00090, 0, "bic%20's%c\t%12-15R, %16-19R, %o"},
    {0x03e00000, 0x0fe00000, 0x000f0000, "mvn%20's%c\t%12-15r, %o"},
    {0x01e00000, 0x0fe00010, 0x000f0000, "mvn%20's%c\t%12-15r, %o"},
    {0x01e00010, 0x0fe00090, 0x000f0000, "mvn%20's%c\t%12-15R, %o"},
    {0x06000010, 0x0e000010, 0, "UNDEFINED"},
    {0x049d0004, 0x0fff0fff, 0, "pop%c\t{%12-15r}\t\t; (ldr%c %12-15r, %a)"},
    {0x04500000, 0x0c500000, 0, "ldrb%t%c\t%12-15R, %a"},
    {0x04300000, 0x0d700000, 0, "ldrt%c\t%12-15R, %a"},
    {0x04100000, 0x0c500000, 0, "ldr%c\t%12-15r, %a"},
    {0x092d0001, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0002, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0004, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0008, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0010, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0020, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0040, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0080, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0100, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0200, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0400, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0800, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d1000, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d2000, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d4000, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d8000, 0x0fffffff, 0, "stmfd%c\t%16-19R!, %m"},
    {0x092d0000, 0x0fff0000, 0, "push%c\t%m"},
    {0x08800000, 0x0ff00000, 0, "stm%c\t%16-19R%21'!, %m%22'^"},
    {0x08000000, 0x0e100000, 0, "stm%23?id%24?ba%c\t%16-19R%21'!, %m%22'^"},
    {0x08bd0001, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0002, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0004, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0008, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0010, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0020, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0040, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0080, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0100, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0200, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0400, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0800, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd1000, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd2000, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd4000, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd8000, 0x0fffffff, 0, "ldmfd%c\t%16-19R!, %m"},
    {0x08bd0000, 0x0fff0000, 0, "pop%c\t%m"},
    {0x08900000, 0x0f900000, 0, "ldm%c\t%16-19R%21'!, %m%22'^"},
    {0x08100000, 0x0e100000, 0, "ldm%23?id%24?ba%c\t%16-19R%21'!, %m%22'^"},
    {0x0a000000, 0x0e000000, 0, "b%24'l%c\t%b"},
    {0x0f000000, 0x0f000000, 0, "svc%c\t%0-23x"},
    {0x03200000, 0x0fff00ff, 0x0000ff00, "nop%c\t{%0-7d}"},
    {0x00000000, 0x00000000, 0, "UNDEFINED"},
    {0x00000000, 0x00000000, 0, 0}
#endif
};

static const struct opcode coproc_opcodes[] =
{
#ifdef __aarch64__
    {0x00000000, 0x00000000, 0, 0}
#else
    /*
     * Most of the FPU and SIMD instructions don't have any SBO/SBZ bits,
     * so just include those who actually do (as opposed to the base
     * instructions, where all instructions are included).
     */

    // FPU
    {0x0c400b10, 0x0ff00fd0, 0, "vmov%c\t%0-3,5D, %12-15r, %16-19r"},
    {0x0c500b10, 0x0ff00fd0, 0, "vmov%c\t%12-15r, %16-19r, %0-3,5D"},
    {0x0e000b10, 0x0fd00f70, 0x0000000f, "vmov%c.32\t%16-19,7D[%21d], %12-15r"},
    {0x0e100b10, 0x0f500f70, 0x0000000f, "vmov%c.32\t%12-15r, %16-19,7D[%21d]"},
    {0x0e000b30, 0x0fd00f30, 0x0000000f, "vmov%c.16\t%16-19,7D[%6,21d], %12-15r"},
    {0x0e100b30, 0x0f500f30, 0x0000000f, "vmov%c.%23?us16\t%12-15r, %16-19,7D[%6,21d]"},
    {0x0e400b10, 0x0fd00f10, 0x0000000f, "vmov%c.8\t%16-19,7D[%5,6,21d], %12-15r"},
    {0x0e500b10, 0x0f500f10, 0x0000000f, "vmov%c.%23?us8\t%12-15r, %16-19,7D[%5,6,21d]"},
    {0x0ee00a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpsid, %12-15r"},
    {0x0ee10a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpscr, %12-15r"},
    {0x0ee20a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpscr_nzcvqc, %12-15r"},
    {0x0ee60a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tmvfr1, %12-15r"},
    {0x0ee70a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tmvfr0, %12-15r"},
    {0x0ee50a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tmvfr2, %12-15r"},
    {0x0ee80a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpexc, %12-15r"},
    {0x0ee90a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpinst, %12-15r\t@ Impl def"},
    {0x0eea0a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpinst2, %12-15r\t@ Impl def"},
    {0x0eec0a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tvpr, %12-15r"},
    {0x0eed0a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tp0, %12-15r"},
    {0x0eee0a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpcxt_ns, %12-15r"},
    {0x0eef0a10, 0x0fff0fff, 0x000000ef, "vmsr%c\tfpcxt_s, %12-15r"},
    {0x0ef00a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpsid"},
    {0x0ef1fa10, 0x0fffffff, 0x000000ef, "vmrs%c\tAPSR_nzcv, fpscr"},
    {0x0ef10a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpscr"},
    {0x0ef20a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpscr_nzcvqc"},
    {0x0ef50a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, mvfr2"},
    {0x0ef60a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, mvfr1"},
    {0x0ef70a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, mvfr0"},
    {0x0ef80a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpexc"},
    {0x0ef90a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpinst\t@ Impl def"},
    {0x0efa0a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpinst2\t@ Impl def"},
    {0x0efc0a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, vpr"},
    {0x0efd0a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, p0"},
    {0x0efe0a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpcxt_ns"},
    {0x0eff0a10, 0x0fff0fff, 0x000000ef, "vmrs%c\t%12-15r, fpcxt_s"},
    {0x0e000b10, 0x0fd00fff, 0x0000000f, "vmov%c.32\t%z2[%21d], %12-15r"},
    {0x0e100b10, 0x0fd00fff, 0x0000000f, "vmov%c.32\t%12-15r, %z2[%21d]"},
    {0x0ee00a10, 0x0ff00fff, 0x000000ef, "vmsr%c\t<impl def %16-19x>, %12-15r"},
    {0x0ef00a10, 0x0ff00fff, 0x000000ef, "vmrs%c\t%12-15r, <impl def %16-19x>"},
    {0x0e000a10, 0x0ff00f7f, 0x0000006f, "vmov%c\t%y2, %12-15r"},
    {0x0e100a10, 0x0ff00f7f, 0x0000006f, "vmov%c\t%12-15r, %y2"},
    {0x0eb50a40, 0x0fbf0f70, 0x0000002f, "vcmp%7'e%c.f32\t%y1, #0.0"},
    {0x0eb50b40, 0x0fbf0f70, 0x0000002f, "vcmp%7'e%c.f64\t%z1, #0.0"},
    {0x0eb00a40, 0x0fbf0fd0, 0, "vmov%c.f32\t%y1, %y0"},
    {0x0eb00b40, 0x0fbf0fd0, 0, "vmov%c.f64\t%z1, %z0"},
    {0x0eb40a40, 0x0fbf0f50, 0, "vcmp%7'e%c.f32\t%y1, %y0"},
    {0x0eb40b40, 0x0fbf0f50, 0, "vcmp%7'e%c.f64\t%z1, %z0"},
    {0x0c500b10, 0x0fb00ff0, 0, "vmov%c\t%12-15r, %16-19r, %z0"},
    {0x0eb00a00, 0x0fb00ff0, 0x000000a0, "vmov%c.f32\t%y1, #%0-3,16-19E"},
    {0x0eb00b00, 0x0fb00ff0, 0x000000a0, "vmov%c.f64\t%z1, #%0-3,16-19E"},
    {0x0c400a10, 0x0ff00fd0, 0, "vmov%c\t%y4, %12-15r, %16-19r"},
    {0x0c400b10, 0x0ff00fd0, 0, "vmov%c\t%z0, %12-15r, %16-19r"},
    {0x0c500a10, 0x0ff00fd0, 0, "vmov%c\t%12-15r, %16-19r, %y4"},
    {0x0eb40940, 0x0fbf0f50, 0, "vcmp%7'e%c.f16\t%y1, %y0"},
    {0x0eb50940, 0x0fbf0f70, 0x000002f0, "vcmp%7'e%c.f16\t%y1, #0.0"},
    {0x0e100910, 0x0ff00f7f, 0x0000006f, "vmov%c.f16\t%12-15r, %y2"},
    {0x0e000910, 0x0ff00f7f, 0x0000006f, "vmov%c.f16\t%y2, %12-15r"},
    {0x0eb00900, 0x0fb00ff0, 0x000000a0, "vmov%c.f16\t%y1, #%0-3,16-19E"},

    // SIMD
    {0x0e800b10, 0x0ff00f70, 0x0000000f, "vdup%c.32\t%16-19,7D, %12-15r"},
    {0x0e800b30, 0x0ff00f70, 0x0000000f, "vdup%c.16\t%16-19,7D, %12-15r"},
    {0x0ea00b10, 0x0ff00f70, 0x0000000f, "vdup%c.32\t%16-19,7Q, %12-15r"},
    {0x0ea00b30, 0x0ff00f70, 0x0000000f, "vdup%c.16\t%16-19,7Q, %12-15r"},
    {0x0ec00b10, 0x0ff00f70, 0x0000000f, "vdup%c.8\t%16-19,7D, %12-15r"},
    {0x0ee00b10, 0x0ff00f70, 0x0000000f, "vdup%c.8\t%16-19,7Q, %12-15r"},
    {0xf3b40c00, 0xffb70f90, 0, "vdup%c.32\t%12-15,22R, %0-3,5D[%19d]"},
    {0xf3b20c00, 0xffb30f90, 0, "vdup%c.16\t%12-15,22R, %0-3,5D[%18-19d]"},
    {0xf3b10c00, 0xffb10f90, 0, "vdup%c.8\t%12-15,22R, %0-3,5D[%17-19d]"},
    {0xf2800e10, 0xfeb80fb0, 0, "vmov%c.i8\t%12-15,22R, %E"},
    {0xf2800e30, 0xfeb80fb0, 0, "vmov%c.i64\t%12-15,22R, %E"},
    {0xf2800f10, 0xfeb80fb0, 0, "vmov%c.f32\t%12-15,22R, %E"},
    {0xf2800810, 0xfeb80db0, 0, "vmov%c.i16\t%12-15,22R, %E"},
    {0xf2800c10, 0xfeb80eb0, 0, "vmov%c.i32\t%12-15,22R, %E"},
    {0xf2800010, 0xfeb808b0, 0, "vmov%c.i32\t%12-15,22R, %E"},

    {0x00000000, 0x00000000, 0, 0}
#endif
};

static const struct opcode thumb16_opcodes[] =
{
    {0x4784, 0xff87, 0, "blxns\t%3-6r"},
    {0x4704, 0xff87, 0, "bxns\t%3-6r"},
    {0xbf50, 0xffff, 0, "sevl%c"},
    {0xba80, 0xffc0, 0, "hlt\t%0-5x"},
    {0xb610, 0xfff7, 0x0017, "setpan\t#%3-3d"},
    {0xbf00, 0xffff, 0, "nop%c"},
    {0xbf10, 0xffff, 0, "yield%c"},
    {0xbf20, 0xffff, 0, "wfe%c"},
    {0xbf30, 0xffff, 0, "wfi%c"},
    {0xbf40, 0xffff, 0, "sev%c"},
    {0xbf00, 0xff0f, 0, "nop%c\t{%4-7d}"},
    {0xb900, 0xfd00, 0, "cbnz\t%0-2r, %b%X"},
    {0xb100, 0xfd00, 0, "cbz\t%0-2r, %b%X"},
    {0xbf00, 0xff00, 0, "it%I%X"},
    {0xb660, 0xfff8, 0x0008, "cpsie\t%2'a%1'i%0'f%X"},
    {0xb670, 0xfff8, 0x0008, "cpsid\t%2'a%1'i%0'f%X"},
    {0x4600, 0xffc0, 0, "mov%c\t%0-2r, %3-5r"},
    {0xba00, 0xffc0, 0, "rev%c\t%0-2r, %3-5r"},
    {0xba40, 0xffc0, 0, "rev16%c\t%0-2r, %3-5r"},
    {0xbac0, 0xffc0, 0, "revsh%c\t%0-2r, %3-5r"},
    {0xb650, 0xfff7, 0x0017, "setend\t%3?ble%X"},
    {0xb200, 0xffc0, 0, "sxth%c\t%0-2r, %3-5r"},
    {0xb240, 0xffc0, 0, "sxtb%c\t%0-2r, %3-5r"},
    {0xb280, 0xffc0, 0, "uxth%c\t%0-2r, %3-5r"},
    {0xb2c0, 0xffc0, 0, "uxtb%c\t%0-2r, %3-5r"},
    {0xbe00, 0xff00, 0, "bkpt\t%0-7x"},
    {0x4780, 0xff87, 0x0007, "blx%c\t%3-6r%x"},
    {0x46c0, 0xffff, 0, "nop%c\t\t\t; (mov r8, r8)"},
    {0x4000, 0xffc0, 0, "and%C\t%0-2r, %3-5r"},
    {0x4040, 0xffc0, 0, "eor%C\t%0-2r, %3-5r"},
    {0x4080, 0xffc0, 0, "lsl%C\t%0-2r, %3-5r"},
    {0x40c0, 0xffc0, 0, "lsr%C\t%0-2r, %3-5r"},
    {0x4100, 0xffc0, 0, "asr%C\t%0-2r, %3-5r"},
    {0x4140, 0xffc0, 0, "adc%C\t%0-2r, %3-5r"},
    {0x4180, 0xffc0, 0, "sbc%C\t%0-2r, %3-5r"},
    {0x41c0, 0xffc0, 0, "ror%C\t%0-2r, %3-5r"},
    {0x4200, 0xffc0, 0, "tst%c\t%0-2r, %3-5r"},
    {0x4240, 0xffc0, 0, "neg%C\t%0-2r, %3-5r"},
    {0x4280, 0xffc0, 0, "cmp%c\t%0-2r, %3-5r"},
    {0x42c0, 0xffc0, 0, "cmn%c\t%0-2r, %3-5r"},
    {0x4300, 0xffc0, 0, "orr%C\t%0-2r, %3-5r"},
    {0x4340, 0xffc0, 0, "mul%C\t%0-2r, %3-5r"},
    {0x4380, 0xffc0, 0, "bic%C\t%0-2r, %3-5r"},
    {0x43c0, 0xffc0, 0, "mvn%C\t%0-2r, %3-5r"},
    {0xb000, 0xff80, 0, "add%c\tsp, #%0-6W"},
    {0xb080, 0xff80, 0, "sub%c\tsp, #%0-6W"},
    {0x4700, 0xff80, 0x0007, "bx%c\t%S%x"},
    {0x4400, 0xff00, 0, "add%c\t%D, %S"},
    {0x4500, 0xff00, 0, "cmp%c\t%D, %S"},
    {0x4600, 0xff00, 0, "mov%c\t%D, %S"},
    {0xb400, 0xfe00, 0, "push%c\t%N"},
    {0xbc00, 0xfe00, 0, "pop%c\t%O"},
    {0x1800, 0xfe00, 0, "add%C\t%0-2r, %3-5r, %6-8r"},
    {0x1a00, 0xfe00, 0, "sub%C\t%0-2r, %3-5r, %6-8r"},
    {0x1c00, 0xfe00, 0, "add%C\t%0-2r, %3-5r, #%6-8d"},
    {0x1e00, 0xfe00, 0, "sub%C\t%0-2r, %3-5r, #%6-8d"},
    {0x5200, 0xfe00, 0, "strh%c\t%0-2r, [%3-5r, %6-8r]"},
    {0x5a00, 0xfe00, 0, "ldrh%c\t%0-2r, [%3-5r, %6-8r]"},
    {0x5600, 0xf600, 0, "ldrs%11?hb%c\t%0-2r, [%3-5r, %6-8r]"},
    {0x5000, 0xfa00, 0, "str%10'b%c\t%0-2r, [%3-5r, %6-8r]"},
    {0x5800, 0xfa00, 0, "ldr%10'b%c\t%0-2r, [%3-5r, %6-8r]"},
    {0x0000, 0xffc0, 0, "mov%C\t%0-2r, %3-5r"},
    {0x0000, 0xf800, 0, "lsl%C\t%0-2r, %3-5r, #%6-10d"},
    {0x0800, 0xf800, 0, "lsr%C\t%0-2r, %3-5r, %s"},
    {0x1000, 0xf800, 0, "asr%C\t%0-2r, %3-5r, %s"},
    {0x2000, 0xf800, 0, "mov%C\t%8-10r, #%0-7d"},
    {0x2800, 0xf800, 0, "cmp%c\t%8-10r, #%0-7d"},
    {0x3000, 0xf800, 0, "add%C\t%8-10r, #%0-7d"},
    {0x3800, 0xf800, 0, "sub%C\t%8-10r, #%0-7d"},
    {0x4800, 0xf800, 0, "ldr%c\t%8-10r, [pc, #%0-7W]\t; (%0-7a)"},
    {0x6000, 0xf800, 0, "str%c\t%0-2r, [%3-5r, #%6-10W]"},
    {0x6800, 0xf800, 0, "ldr%c\t%0-2r, [%3-5r, #%6-10W]"},
    {0x7000, 0xf800, 0, "strb%c\t%0-2r, [%3-5r, #%6-10d]"},
    {0x7800, 0xf800, 0, "ldrb%c\t%0-2r, [%3-5r, #%6-10d]"},
    {0x8000, 0xf800, 0, "strh%c\t%0-2r, [%3-5r, #%6-10H]"},
    {0x8800, 0xf800, 0, "ldrh%c\t%0-2r, [%3-5r, #%6-10H]"},
    {0x9000, 0xf800, 0, "str%c\t%8-10r, [sp, #%0-7W]"},
    {0x9800, 0xf800, 0, "ldr%c\t%8-10r, [sp, #%0-7W]"},
    {0xa000, 0xf800, 0, "add%c\t%8-10r, pc, #%0-7W\t; (adr %8-10r, %0-7a)"},
    {0xa800, 0xf800, 0, "add%c\t%8-10r, sp, #%0-7W"},
    {0xc000, 0xf800, 0, "stmia%c\t%8-10r!, %M"},
    {0xc800, 0xf800, 0, "ldmia%c\t%8-10r%W, %M"},
    {0xdf00, 0xff00, 0, "svc%c\t%0-7d"},
    {0xde00, 0xff00, 0, "udf%c\t#%0-7d"},
    {0xde00, 0xfe00, 0, "UNDEFINED"},
    {0xd000, 0xf000, 0, "b%8-11c.n\t%0-7B%X"},
    {0xe000, 0xf800, 0, "b%c.n\t%0-10B%x"},
    {0x0000, 0x0000, 0, "UNDEFINED"},
    {0, 0, 0, 0}
};

static const struct opcode thumb32_opcodes[] =
{
    {0, 0, 0, 0}
};

/*
 * Checks whether insn is a legal/defined instruction that has
 * incorrect should-be-one/should-be-zero bits set. libopcodes
 * often recognizes such instructions as undefined, when they
 * should be constrained unpredictable according to the manual.
 *
 * Without this filter, these instructions will often be marked as
 * hidden, generating a lot of false positives.
 */
static bool has_incorrect_sb_bits(uint32_t insn, const struct opcode *opcodes, bool thumb16)
{
    const struct opcode *curr_op;
    for (curr_op = opcodes; curr_op->disassembly; ++curr_op) {

        uint32_t op_value = curr_op->op_value;
        uint32_t op_mask = curr_op->op_mask;
        uint32_t sb_mask = curr_op->sb_mask;

        if (thumb16) {
            /*
             * Since thumb16 instructions are encoded in the upper half of the
             * 32-bit insn variable, the table entries need to be shifted
             * one half-word left.
             */
            op_value <<= 16;
            op_mask <<= 16;
            sb_mask <<= 16;
        }

        uint32_t masked_insn = (insn & op_mask);
        uint32_t sb_masked_insn = masked_insn & ~sb_mask;
        uint32_t sb_masked_value = op_value & ~sb_mask;

        if (sb_masked_insn == sb_masked_value) {
            if (masked_insn != op_value) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

/*
 * Mostly taken from binutils/opcodes/aarch64-opc.c
 * In essence, this checks whether the ldpsw verifier in libopcodes
 * would (incorrectly) mark the instruction as undefined or not.
 */
static bool is_unpredictable_ldpsw(uint32_t insn)
{
#ifdef __aarch64__
#define BIT(INSN,BT)     (((INSN) >> (BT)) & 1)
#define BITS(INSN,HI,LO) (((INSN) >> (LO)) & ((1 << (((HI) - (LO)) + 1)) - 1))

    // Is an LDPSW insn?
    if ((insn & 0xfec00000) != 0x68c00000 && (insn & 0xffc00000) != 0x69400000) {
        return false;
    }

    // Is it unpredictable?
    uint32_t t = BITS(insn, 4, 0);
    uint32_t n = BITS(insn, 9, 5);
    uint32_t t2 = BITS(insn, 14, 10);

    if (BIT(insn, 23)) {
        // Writeback
        if ((t == n || t2 == n) && n != 31) {
            return true;
        }
    }

    if (BIT(insn, 22)) {
        // Load
        if (t == t2) {
            return true;
        }
    }

    return false;
#else
    // No ldpsw instruction in aarch32
    (void)insn;
    return false;
#endif
}

/*
 * Linux configures traps on certain undefined instructions, namely
 * 'udf #16' (e7f001f0), but without regarding the condition prefix.
 * (See Linux source, arch/arm/kernel/ptrace.c:213)
 *
 * This makes instructions [0-9a-f]7f001f0 raise SIGTRAP signals instead
 * of the normal SIGILL, which makes the fuzzer then (incorrectly) mark
 * them as hidden.
 *
 * I think that strictly speaking, Linux shouldn't hook on the instructions
 * with a prefix different than e, as the manual doesn't guarantee that
 * these instructions will be permanently undefined, but rather that
 * they are unallocated.
 *
 * Still, we're not interested in finding instructions 'trapped' by
 * Linux, so filter them away.
 */
static bool is_undef_breakpoint(uint32_t insn)
{
#ifdef __aarch64__
    // No undef hooks on breakpoints in aarch64
    (void)insn;
    return false;
#else
    // udf #16 with arbitrary cond prefix
    return ((insn & 0x0fffffff) == 0x07f001f0);
#endif
}

bool filter_instruction(uint32_t insn, bool thumb)
{
    if (is_unpredictable_ldpsw(insn))
        return true;

    if (is_undef_breakpoint(insn))
        return true;

    if (thumb) {
        if (is_thumb32(insn)) {
            return has_incorrect_sb_bits(insn, thumb32_opcodes, false);
        } else {
            return has_incorrect_sb_bits(insn, thumb16_opcodes, true);
        }
    } else {
        return (has_incorrect_sb_bits(insn, base_opcodes, false)
                || has_incorrect_sb_bits(insn, coproc_opcodes, false));
    }
    return false;
}
