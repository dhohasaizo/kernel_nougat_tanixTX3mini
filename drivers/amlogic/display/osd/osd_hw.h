/*
 * drivers/amlogic/display/osd/osd_hw.h
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

#ifndef _OSD_HW_H_
#define _OSD_HW_H_

#include "osd.h"
#include "osd_sync.h"

#define REG_OFFSET (0x20)
#define OSD_RELATIVE_BITS 0x33330
#ifdef CONFIG_FB_OSD_VSYNC_RDMA
#include "osd_rdma.h"
#endif

#ifdef CONFIG_HIBERNATION
extern void osd_freeze_hw(void);
extern void osd_thaw_hw(void);
extern void osd_restore_hw(void);
extern void osd_realdata_save_hw(void);
extern void osd_realdata_restore_hw(void);
#endif
extern struct hw_para_s osd_hw;
extern void osd_set_color_key_hw(u32 index, u32 bpp, u32 colorkey);
extern void osd_srckey_enable_hw(u32  index, u8 enable);
extern void osd_set_gbl_alpha_hw(u32 index, u32 gbl_alpha);
extern u32 osd_get_gbl_alpha_hw(u32  index);
extern void osd_set_color_mode(u32 index,
			       const struct color_bit_define_s *color);
extern void osd_update_disp_axis_hw(
	u32 index,
	u32 display_h_start,
	u32 display_h_end,
	u32 display_v_start,
	u32 display_v_end,
	u32 xoffset,
	u32 yoffset,
	u32 mode_change);
extern void osd_setup_hw(u32 index,
			 struct osd_ctl_s *osd_ctl,
			 u32 xoffset,
			 u32 yoffset,
			 u32 xres,
			 u32 yres,
			 u32 xres_virtual ,
			 u32 yres_virtual,
			 u32 disp_start_x,
			 u32 disp_start_y,
			 u32 disp_end_x,
			 u32 disp_end_y,
			 u32 fbmem,
			 phys_addr_t *afbc_fbmem,
			 const struct color_bit_define_s *color);
extern void osd_set_order_hw(u32 index, u32 order);
extern void osd_get_order_hw(u32 index, u32 *order);
extern void osd_set_free_scale_enable_hw(u32 index, u32 enable);
extern void osd_get_free_scale_enable_hw(u32 index, u32 *free_scale_enable);
extern void osd_set_free_scale_mode_hw(u32 index, u32 freescale_mode);
extern void osd_get_free_scale_mode_hw(u32 index, u32 *freescale_mode);
extern void osd_set_4k2k_fb_mode_hw(u32 fb_for_4k2k);
extern void osd_get_free_scale_mode_hw(u32 index, u32 *freescale_mode);
extern void osd_get_free_scale_width_hw(u32 index, u32 *free_scale_width);
extern void osd_get_free_scale_height_hw(u32 index, u32 *free_scale_height);
extern void osd_get_free_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
				       s32 *y1);
extern void osd_set_free_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1,
				       s32 y1);
extern void osd_get_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
				  s32 *y1);
extern void osd_get_window_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
				   s32 *y1);
extern void osd_set_window_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1);
extern void osd_set_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1);
extern void osd_get_block_windows_hw(u32 index, u32 *windows);
extern void osd_set_block_windows_hw(u32 index, u32 *windows);
extern void osd_get_block_mode_hw(u32 index, u32 *mode);
extern void osd_set_block_mode_hw(u32 index, u32 mode);
extern void osd_enable_3d_mode_hw(u32 index, u32 enable);
extern void osd_set_2x_scale_hw(u32 index, u16 h_scale_enable,
				u16 v_scale_enable);
extern void osd_get_flush_rate_hw(u32 *break_rate);
extern void osd_set_reverse_hw(u32 index, u32 reverse);
extern void osd_get_reverse_hw(u32 index, u32 *reverse);
extern void osd_set_antiflicker_hw(u32 index, u32 vmode, u32 yres);
extern void osd_get_antiflicker_hw(u32 index, u32 *on_off);
extern void osd_get_angle_hw(u32 index, u32 *angle);
extern void osd_set_angle_hw(u32 index, u32 angle, u32  virtual_osd1_yres,
			     u32 virtual_osd2_yres);
extern void osd_get_clone_hw(u32 index, u32 *clone);
extern void osd_set_clone_hw(u32 index, u32 clone);
extern void osd_set_update_pan_hw(u32 index);
extern void osd_setpal_hw(u32 index, unsigned regno, unsigned red,
			  unsigned green, unsigned blue, unsigned transp);
extern void osd_enable_hw(u32 index, u32 enable);
extern void osd_pan_display_hw(u32 index, unsigned int xoffset,
			       unsigned int yoffset);
extern int osd_sync_request(u32 index, u32 yres, u32 xoffset, u32 yoffset,
			    s32 in_fence_fd);
extern int osd_sync_request_render(u32 index, u32 yres,
	struct fb_sync_request_render_s *request,
	u32 phys_addr);
extern s32  osd_wait_vsync_event(void);
#if defined(CONFIG_FB_OSD2_CURSOR)
extern void osd_cursor_hw(u32 index, s16 x, s16 y, s16 xstart, s16 ystart,
			  u32 osd_w, u32 osd_h);
#endif
extern void osd_init_scan_mode(void);
extern void osd_suspend_hw(void);
extern void osd_resume_hw(void);
extern void osd_shutdown_hw(void);
extern void osd_init_hw(u32 logo_loaded);
extern void osd_init_scan_mode(void);
extern void osd_set_logo_index(int index);
extern int osd_get_logo_index(void);
extern int osd_get_init_hw_flag(void);
extern void osd_get_hw_para(struct hw_para_s **para);
extern int osd_set_debug_hw(const char *buf);
extern char *osd_get_debug_hw(void);
#ifdef CONFIG_VSYNC_RDMA
extern void enable_rdma(int enable_flag);
#endif

#ifdef CONFIG_AM_FB_EXT
extern void osd_ext_clone_pan(u32 index);
#endif
extern void osd_set_pxp_mode(u32 mode);
extern void osd_set_afbc(u32 enable);
extern u32 osd_get_afbc(void);
extern u32 osd_get_reset_status(void);
extern void osd_switch_free_scale(
	u32 pre_index, u32 pre_enable, u32 pre_scale,
	u32 next_index, u32 next_enable, u32 next_scale);
extern void osd_get_urgent(u32 index, u32 *urgent);
extern void osd_set_urgent(u32 index, u32 urgent);
void osd_get_deband(u32 *osd_deband_enable);
void osd_set_deband(u32 osd_deband_enable);
void osd_get_display_debug(u32 *osd_display_debug_enable);
void osd_set_display_debug(u32 osd_display_debug_enable);
void osd_backup_screen_info(
	u32 index,
	char __iomem *screen_base,
	u32 screen_size);
void osd_restore_screen_info(
	u32 index,
	char __iomem **screen_base,
	unsigned long *screen_size);
#endif
