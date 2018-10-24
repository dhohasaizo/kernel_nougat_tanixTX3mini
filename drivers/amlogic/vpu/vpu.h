/*
 * drivers/amlogic/vpu/vpu.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef __VPU_PARA_H__
#define __VPU_PARA_H__
#include <linux/clk.h>
#include <linux/clk-provider.h>

/*#define VPU_DEBUG_PRINT*/

#define VPUPR(fmt, args...)     pr_info("vpu: "fmt"", ## args)
#define VPUERR(fmt, args...)    pr_err("vpu: error: "fmt"", ## args)

enum vpu_chip_e {
	VPU_CHIP_M8 = 0,
	VPU_CHIP_M8B,
	VPU_CHIP_M8M2,
	VPU_CHIP_G9TV,
	VPU_CHIP_G9BB,
	VPU_CHIP_GXBB,
	VPU_CHIP_GXTVBB,
	VPU_CHIP_GXL,
	VPU_CHIP_GXM,
	VPU_CHIP_TXL,
	VPU_CHIP_TXLX,
	VPU_CHIP_GXLX,
	VPU_CHIP_MAX,
};

struct vpu_conf_s {
	unsigned int     clk_level_dft;
	unsigned int     clk_level_max;
	unsigned int     clk_level;
	unsigned int     fclk_type;
	unsigned int     mem_pd0;
	unsigned int     mem_pd1;
	struct clk       *clk_vpu_clk0;
	struct clk       *clk_vpu_clk1;
	struct clk       *clk_vpu_clk;
};

/* ************************************************ */


extern enum vpu_chip_e vpu_chip_type;
extern int vpu_debug_print_flag;

extern int vpu_ioremap(void);
extern int vpu_chip_valid_check(void);
extern enum vpu_mod_e get_vpu_mod(unsigned int vmod);
extern void vpu_ctrl_probe(void);

#endif
