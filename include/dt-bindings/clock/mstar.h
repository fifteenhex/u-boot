/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _DT_BINDINGS_CLOCK_MSTAR_H
#define _DT_BINDINGS_CLOCK_MSTAR_H


#define MSTAR_CLKGEN_MUX_NULL		64

#define MSTAR_MPLL_432			0
#define MSTAR_MPLL_345			1
#define MSTAR_MPLL_288			2
#define MSTAR_MPLL_216			3
#define MSTAR_MPLL_172			4
#define MSTAR_MPLL_144			5
#define MSTAR_MPLL_123			6
#define MSTAR_MPLL_124			7
#define MSTAR_MPLL_86			8

#define MSTAR_MPLL_GATE_UPLL_384	0
#define MSTAR_MPLL_GATE_UPLL_320	1
#define MSTAR_MPLL_GATE_UTMI_160	2
#define MSTAR_MPLL_GATE_UTMI_192	3
#define MSTAR_MPLL_GATE_UTMI_240	4
#define MSTAR_MPLL_GATE_UTMI_480	5
#define MSTAR_MPLL_GATE_MPLL_432	6
#define MSTAR_MPLL_GATE_MPLL_345	7
#define MSTAR_MPLL_GATE_MPLL_288	8
#define MSTAR_MPLL_GATE_MPLL_216	9
#define MSTAR_MPLL_GATE_MPLL_172	10
#define MSTAR_MPLL_GATE_MPLL_144	11
#define MSTAR_MPLL_GATE_MPLL_123	12
#define MSTAR_MPLL_GATE_MPLL_124	13
#define MSTAR_MPLL_GATE_MPLL_86		14

#define MSTAR_CLKGEN_OUTPUTFLAG_CRITICAL 0x800

#define MSTAR_CLKGEN_MUX_OUTPUT_NAMES	"clock-output-names"
#define MSTAR_CLKGEN_MUX_GATE_SHIFTS	"shifts"
#define MSTAR_CLKGEN_MUX_MUX_SHIFTS	"mux-shifts"
#define MSTAR_CLKGEN_MUX_MUX_WIDTHS	"mux-widths"
#define MSTAR_CLKGEN_MUX_DEGLITCHES		"mstar,deglitches"

#endif