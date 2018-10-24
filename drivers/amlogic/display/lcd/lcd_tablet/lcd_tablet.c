
/*
 * drivers/amlogic/display/lcd/lcd_tablet/lcd_tablet.c
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

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#ifdef CONFIG_AML_VPU
#include <linux/amlogic/vpu.h>
#endif
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include <linux/amlogic/vout/lcd_unifykey.h>
#include <linux/amlogic/vout/lcd_notify.h>
#include "lcd_tablet.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

#define PANEL_NAME    "panel"

/* ************************************************** *
   vout server api
 * ************************************************** */
static enum vmode_e lcd_validate_vmode(char *mode)
{
	if (mode == NULL)
		return VMODE_MAX;

	if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
		return VMODE_LCD;

	return VMODE_MAX;
}

static struct vinfo_s *lcd_get_current_info(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	return lcd_drv->lcd_info;
}

static int lcd_set_current_vmode(enum vmode_e mode)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	mutex_lock(&lcd_vout_mutex);

	if (!(mode & VMODE_INIT_BIT_MASK)) {
		if (VMODE_LCD == (mode & VMODE_MODE_BIT_MASK)) {
			lcd_drv->driver_init_pre();
			ret = lcd_drv->driver_init();
		} else {
			ret = -EINVAL;
		}
	}

	lcd_vcbus_write(VPP_POSTBLEND_H_SIZE, lcd_drv->lcd_info->width);

	mutex_unlock(&lcd_vout_mutex);
	return ret;
}

static int lcd_vmode_is_supported(enum vmode_e mode)
{
	mode &= VMODE_MODE_BIT_MASK;
	if (lcd_debug_print_flag)
		LCDPR("%s vmode = %d\n", __func__, mode);

	if (mode == VMODE_LCD)
		return true;
	return false;
}

static int lcd_vout_disable(enum vmode_e cur_vmod)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_OFF, NULL);
	LCDPR("%s finished\n", __func__);
	return 0;
}

#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
struct lcd_vframe_match_s {
	int fps;
	int frame_rate; /* *100 */
	unsigned int duration_num;
	unsigned int duration_den;
};

static struct lcd_vframe_match_s lcd_vframe_match_table_1[] = {
	{5000, 5000, 50, 1},
	{2500, 5000, 50, 1},
	{6000, 6000, 60, 1},
	{3000, 6000, 60, 1},
	{2400, 6000, 60, 1},
	{2397, 5994, 5994, 100},
	{2997, 5994, 5994, 100},
	{5994, 5994, 5994, 100},
};

static struct lcd_vframe_match_s lcd_vframe_match_table_2[] = {
	{5000, 5000, 50, 1},
	{2500, 5000, 50, 1},
	{6000, 6000, 60, 1},
	{3000, 6000, 60, 1},
	{2400, 4800, 48, 1},
	{2397, 5994, 5994, 100},
	{2997, 5994, 5994, 100},
	{5994, 5994, 5994, 100},
};

static int lcd_framerate_automation_set_mode(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	/* update lcd config sync_duration, for calculate */
	lcd_drv->lcd_config->lcd_timing.sync_duration_num =
		lcd_drv->lcd_info->sync_duration_num;
	lcd_drv->lcd_config->lcd_timing.sync_duration_den =
		lcd_drv->lcd_info->sync_duration_den;

	/* update clk & timing config */
	lcd_tablet_config_update(lcd_drv->lcd_config);
	/* update interface timing if needed, current no need */
#ifdef CONFIG_AML_VPU
	request_vpu_clk_vmod(
		lcd_drv->lcd_config->lcd_timing.lcd_clk, VPU_VENCL);
#endif

	/* change clk parameter */
	switch (lcd_drv->lcd_config->lcd_timing.clk_change) {
	case LCD_CLK_PLL_CHANGE:
		lcd_clk_generate_parameter(lcd_drv->lcd_config);
		lcd_clk_set(lcd_drv->lcd_config);
		break;
	case LCD_CLK_FRAC_UPDATE:
		lcd_clk_update(lcd_drv->lcd_config);
		break;
	default:
		break;
	}
	lcd_venc_change(lcd_drv->lcd_config);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE,
		&lcd_drv->lcd_info->mode);

	return 0;
}
#endif

static int lcd_set_vframe_rate_hint(int duration)
{
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *info;
	int fr_policy;
	unsigned int frame_rate = 6000;
	unsigned int duration_num = 60, duration_den = 1;
	struct lcd_vframe_match_s *vtable = lcd_vframe_match_table_1;
	int fps, i, n;

	if (lcd_drv->lcd_status == 0) {
		LCDPR("%s: lcd is disabled, exit\n", __func__);
		return 0;
	}

	info = lcd_drv->lcd_info;

	fr_policy = lcd_drv->fr_auto_policy;
	switch (fr_policy) {
	case 1:
		vtable = lcd_vframe_match_table_1;
		n = ARRAY_SIZE(lcd_vframe_match_table_1);
		break;
	case 2:
		vtable = lcd_vframe_match_table_2;
		n = ARRAY_SIZE(lcd_vframe_match_table_2);
		break;
	default:
		LCDPR("%s: fr_auto_policy = %d, disabled\n",
			__func__, fr_policy);
		return 0;
		break;
	}
	fps = get_vsource_fps(duration);
	for (i = 0; i < n; i++) {
		if (fps == vtable[i].fps) {
			frame_rate = vtable[i].frame_rate;
			duration_num = vtable[i].duration_num;
			duration_den = vtable[i].duration_den;
		}
	}
	LCDPR("%s: policy = %d, duration = %d, fps = %d, frame_rate = %d\n",
		__func__, fr_policy, duration, fps, frame_rate);

	/* if the sync_duration is same as current */
	if ((duration_num == info->sync_duration_num) &&
		(duration_den == info->sync_duration_den)) {
		LCDPR("%s: sync_duration is the same, exit\n", __func__);
		return 0;
	}

	/* update vinfo */
	info->sync_duration_num = duration_num;
	info->sync_duration_den = duration_den;

	lcd_framerate_automation_set_mode();
#endif
	return 0;
}

static int lcd_set_vframe_rate_end_hint(void)
{
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *info;

	if (lcd_drv->lcd_status == 0) {
		LCDPR("%s: lcd is disabled, exit\n", __func__);
		return 0;
	}

	if (lcd_debug_print_flag)
		LCDPR("fr_auto_policy = %d\n", lcd_drv->fr_auto_policy);
	if (lcd_drv->fr_auto_policy) {
		info = lcd_drv->lcd_info;
		LCDPR("%s: return mode = %s, policy = %d\n", __func__,
			info->name, lcd_drv->fr_auto_policy);

		/* update vinfo */
		info->sync_duration_num = lcd_drv->std_duration.duration_num;
		info->sync_duration_den = lcd_drv->std_duration.duration_den;

		lcd_framerate_automation_set_mode();
	}
#endif
	return 0;
}

static int lcd_set_vframe_rate_policy(int policy)
{
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_drv->fr_auto_policy = policy;
	LCDPR("%s: %d\n", __func__, lcd_drv->fr_auto_policy);
#endif
	return 0;
}

static int lcd_get_vframe_rate_policy(void)
{
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	return lcd_drv->fr_auto_policy;
#else
	return 0;
#endif
}

#ifdef CONFIG_PM
static int lcd_suspend(void)
{
	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
	lcd_resume_flag = 0;
	LCDPR("%s finished\n", __func__);
	mutex_unlock(&lcd_power_mutex);
	return 0;
}
static int lcd_resume(void)
{
	mutex_lock(&lcd_power_mutex);
	if (lcd_resume_flag == 0) {
		lcd_resume_flag = 1;
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, NULL);
		LCDPR("%s finished\n", __func__);
	}
	mutex_unlock(&lcd_power_mutex);
	return 0;
}
#endif

static struct vout_server_s lcd_vout_server = {
	.name = "lcd_vout_server",
	.op = {
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported = lcd_vmode_is_supported,
		.disable = lcd_vout_disable,
		.set_vframe_rate_hint = lcd_set_vframe_rate_hint,
		.set_vframe_rate_end_hint = lcd_set_vframe_rate_end_hint,
		.set_vframe_rate_policy = lcd_set_vframe_rate_policy,
		.get_vframe_rate_policy = lcd_get_vframe_rate_policy,
#ifdef CONFIG_PM
		.vout_suspend = lcd_suspend,
		.vout_resume = lcd_resume,
#endif
	},
};

static void lcd_tablet_vinfo_update(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *vinfo;
	struct lcd_config_s *pconf;

	vinfo = lcd_drv->lcd_info;
	pconf = lcd_drv->lcd_config;
	if (vinfo) {
		vinfo->name = PANEL_NAME;
		vinfo->mode = VMODE_LCD;
		vinfo->width = pconf->lcd_basic.h_active;
		vinfo->height = pconf->lcd_basic.v_active;
		vinfo->field_height = pconf->lcd_basic.v_active;
		vinfo->aspect_ratio_num = pconf->lcd_basic.screen_width;
		vinfo->aspect_ratio_den = pconf->lcd_basic.screen_height;
		vinfo->screen_real_width = pconf->lcd_basic.screen_width;
		vinfo->screen_real_height = pconf->lcd_basic.screen_height;
		vinfo->sync_duration_num = pconf->lcd_timing.sync_duration_num;
		vinfo->sync_duration_den = pconf->lcd_timing.sync_duration_den;
		vinfo->video_clk = pconf->lcd_timing.lcd_clk;
		vinfo->htotal = pconf->lcd_basic.h_period;
		vinfo->vtotal = pconf->lcd_basic.v_period;
		vinfo->viu_color_fmt = TVIN_RGB444;

		lcd_hdr_vinfo_update();
	}
}

static void lcd_tablet_vinfo_update_default(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *vinfo;
	unsigned int h_active, v_active, h_total, v_total;

	if (lcd_drv->lcd_info == NULL) {
		LCDERR("no lcd_info exist\n");
		return;
	}

	h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END)
			- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN) + 1;
	v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE)
			- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE) + 1;
	h_total = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
	v_total = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

	vinfo = lcd_drv->lcd_info;
	if (vinfo) {
		vinfo->name = PANEL_NAME;
		vinfo->mode = VMODE_LCD;
		vinfo->width = h_active;
		vinfo->height = v_active;
		vinfo->field_height = v_active;
		vinfo->aspect_ratio_num = h_active;
		vinfo->aspect_ratio_den = v_active;
		vinfo->screen_real_width = h_active;
		vinfo->screen_real_height = v_active;
		vinfo->sync_duration_num = 60;
		vinfo->sync_duration_den = 1;
		vinfo->video_clk = 0;
		vinfo->htotal = h_total;
		vinfo->vtotal = v_total;
		vinfo->viu_color_fmt = TVIN_RGB444;
	}
}

void lcd_tablet_vout_server_init(void)
{
	lcd_tablet_vinfo_update_default();
	vout_register_server(&lcd_vout_server);
}

/* ************************************************** *
   lcd tablet config
 * ************************************************** */
static void lcd_config_print(struct lcd_config_s *pconf)
{
	LCDPR("%s, %s, %dbit, %dx%d\n",
		pconf->lcd_basic.model_name,
		lcd_type_type_to_str(pconf->lcd_basic.lcd_type),
		pconf->lcd_basic.lcd_bits,
		pconf->lcd_basic.h_active, pconf->lcd_basic.v_active);

	if (lcd_debug_print_flag == 0)
		return;

	LCDPR("h_period = %d\n", pconf->lcd_basic.h_period);
	LCDPR("v_period = %d\n", pconf->lcd_basic.v_period);
	LCDPR("screen_width = %d\n", pconf->lcd_basic.screen_width);
	LCDPR("screen_height = %d\n", pconf->lcd_basic.screen_height);

	LCDPR("h_period_min = %d\n", pconf->lcd_basic.h_period_min);
	LCDPR("h_period_max = %d\n", pconf->lcd_basic.h_period_max);
	LCDPR("v_period_min = %d\n", pconf->lcd_basic.v_period_min);
	LCDPR("v_period_max = %d\n", pconf->lcd_basic.v_period_max);
	LCDPR("pclk_min = %d\n", pconf->lcd_basic.lcd_clk_min);
	LCDPR("pclk_max = %d\n", pconf->lcd_basic.lcd_clk_max);

	LCDPR("hsync_width = %d\n", pconf->lcd_timing.hsync_width);
	LCDPR("hsync_bp = %d\n", pconf->lcd_timing.hsync_bp);
	LCDPR("hsync_pol = %d\n", pconf->lcd_timing.hsync_pol);
	LCDPR("vsync_width = %d\n", pconf->lcd_timing.vsync_width);
	LCDPR("vsync_bp = %d\n", pconf->lcd_timing.vsync_bp);
	LCDPR("vsync_pol = %d\n", pconf->lcd_timing.vsync_pol);

	LCDPR("fr_adjust_type = %d\n", pconf->lcd_timing.fr_adjust_type);
	LCDPR("ss_level = %d\n", pconf->lcd_timing.ss_level);
	LCDPR("clk_auto = %d\n", pconf->lcd_timing.clk_auto);

	if (pconf->lcd_basic.lcd_type == LCD_TTL) {
		LCDPR("clk_pol = %d\n",
			pconf->lcd_control.ttl_config->clk_pol);
		LCDPR("sync_valid = %d\n",
			pconf->lcd_control.ttl_config->sync_valid);
		LCDPR("swap_ctrl = %d\n",
			pconf->lcd_control.ttl_config->swap_ctrl);
	} else if (pconf->lcd_basic.lcd_type == LCD_LVDS) {
		LCDPR("lvds_repack = %d\n",
			pconf->lcd_control.lvds_config->lvds_repack);
		LCDPR("pn_swap = %d\n",
			pconf->lcd_control.lvds_config->pn_swap);
		LCDPR("dual_port = %d\n",
			pconf->lcd_control.lvds_config->dual_port);
		LCDPR("port_swap = %d\n",
			pconf->lcd_control.lvds_config->port_swap);
		LCDPR("lane_reverse = %d\n",
			pconf->lcd_control.lvds_config->lane_reverse);
	} else if (pconf->lcd_basic.lcd_type == LCD_VBYONE) {
		LCDPR("lane_count = %d\n",
			pconf->lcd_control.vbyone_config->lane_count);
		LCDPR("byte_mode = %d\n",
			pconf->lcd_control.vbyone_config->byte_mode);
		LCDPR("region_num = %d\n",
			pconf->lcd_control.vbyone_config->region_num);
		LCDPR("color_fmt = %d\n",
			pconf->lcd_control.vbyone_config->color_fmt);
	}
}

static int lcd_init_load_from_dts(struct lcd_config_s *pconf,
		struct device *dev)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		if (lcd_drv->lcd_status) /* lock pinmux if lcd in on */
			lcd_ttl_pinmux_set(1);
		else
			lcd_ttl_pinmux_set(0);
		break;
	case LCD_VBYONE:
		if (lcd_drv->lcd_status) /* lock pinmux if lcd in on */
			lcd_vbyone_pinmux_set(1);
		break;
	default:
		break;
	}

	pconf->rstc.encl = devm_reset_control_get(dev, "encl");
	if (IS_ERR(pconf->rstc.encl))
		LCDERR("get reset control encl error\n");
	pconf->rstc.vencl = devm_reset_control_get(dev, "vencl");
	if (IS_ERR(pconf->rstc.vencl))
		LCDERR("get reset control vencl error\n");

	return ret;
}

static int lcd_config_load_from_dts(struct lcd_config_s *pconf,
		struct device *dev)
{
	int ret = 0;
	const char *str;
	unsigned int para[10];
	struct device_node *child;
	struct lvds_config_s *lvdsconf;

	child = of_get_child_by_name(dev->of_node, pconf->lcd_propname);
	if (child == NULL) {
		LCDERR("failed to get %s\n", pconf->lcd_propname);
		return -1;
	}

	ret = of_property_read_string(child, "model_name", &str);
	if (ret) {
		LCDERR("failed to get model_name\n");
		strcpy(pconf->lcd_basic.model_name, pconf->lcd_propname);
	} else {
		strcpy(pconf->lcd_basic.model_name, str);
	}

	ret = of_property_read_string(child, "interface", &str);
	if (ret) {
		LCDERR("failed to get interface\n");
		str = "invalid";
	}
	pconf->lcd_basic.lcd_type = lcd_type_str_to_type(str);

	ret = of_property_read_u32_array(child, "basic_setting", &para[0], 7);
	if (ret) {
		LCDERR("failed to get basic_setting\n");
	} else {
		pconf->lcd_basic.h_active = para[0];
		pconf->lcd_basic.v_active = para[1];
		pconf->lcd_basic.h_period = para[2];
		pconf->lcd_basic.v_period = para[3];
		pconf->lcd_basic.lcd_bits = para[4];
		pconf->lcd_basic.screen_width = para[5];
		pconf->lcd_basic.screen_height = para[6];
	}
	ret = of_property_read_u32_array(child, "range_setting", &para[0], 6);
	if (ret) {
		LCDERR("failed to get range_setting\n");
		pconf->lcd_basic.h_period_min = pconf->lcd_basic.h_period;
		pconf->lcd_basic.h_period_max = pconf->lcd_basic.h_period;
		pconf->lcd_basic.v_period_min = pconf->lcd_basic.v_period;
		pconf->lcd_basic.v_period_max = pconf->lcd_basic.v_period;
		pconf->lcd_basic.lcd_clk_min = 0;
		pconf->lcd_basic.lcd_clk_max = 0;
	} else {
		pconf->lcd_basic.h_period_min = para[0];
		pconf->lcd_basic.h_period_max = para[1];
		pconf->lcd_basic.v_period_min = para[2];
		pconf->lcd_basic.v_period_max = para[3];
		pconf->lcd_basic.lcd_clk_min = para[4];
		pconf->lcd_basic.lcd_clk_max = para[5];
	}

	ret = of_property_read_u32_array(child, "lcd_timing", &para[0], 6);
	if (ret) {
		LCDERR("failed to get lcd_timing\n");
	} else {
		pconf->lcd_timing.hsync_width = (unsigned short)(para[0]);
		pconf->lcd_timing.hsync_bp = (unsigned short)(para[1]);
		pconf->lcd_timing.hsync_pol = (unsigned short)(para[2]);
		pconf->lcd_timing.vsync_width = (unsigned short)(para[3]);
		pconf->lcd_timing.vsync_bp = (unsigned short)(para[4]);
		pconf->lcd_timing.vsync_pol = (unsigned short)(para[5]);
	}

	ret = of_property_read_u32_array(child, "clk_attr", &para[0], 4);
	if (ret) {
		LCDERR("failed to get clk_attr\n");
		pconf->lcd_timing.fr_adjust_type = 0;
		pconf->lcd_timing.ss_level = 0;
		pconf->lcd_timing.clk_auto = 1;
		pconf->lcd_timing.lcd_clk = 60;
	} else {
		pconf->lcd_timing.fr_adjust_type = (unsigned char)(para[0]);
		pconf->lcd_timing.ss_level = (unsigned char)(para[1]);
		pconf->lcd_timing.clk_auto = (unsigned char)(para[2]);
		if (para[3] > 0) {
			pconf->lcd_timing.lcd_clk = para[3];
		} else { /* avoid 0 mistake */
			pconf->lcd_timing.lcd_clk = 60;
			LCDERR("lcd_clk is 0, default to 60Hz\n");
		}
	}
	if (pconf->lcd_timing.clk_auto == 0) {
		ret = of_property_read_u32_array(child, "clk_para",
			&para[0], 3);
		if (ret) {
			LCDERR("failed to get clk_para\n");
		} else {
			pconf->lcd_timing.pll_ctrl = para[0];
			pconf->lcd_timing.div_ctrl = para[1];
			pconf->lcd_timing.clk_ctrl = para[2];
		}
	}

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		ret = of_property_read_u32_array(child, "ttl_attr",
			&para[0], 5);
		if (ret) {
			LCDERR("failed to get ttl_attr\n");
		} else {
			pconf->lcd_control.ttl_config->clk_pol = para[0];
			pconf->lcd_control.ttl_config->sync_valid =
				((para[1] << 1) | (para[2] << 0));
			pconf->lcd_control.ttl_config->swap_ctrl =
				((para[3] << 1) | (para[4] << 0));
		}
		break;
	case LCD_LVDS:
		lvdsconf = pconf->lcd_control.lvds_config;
		ret = of_property_read_u32_array(child, "lvds_attr",
			&para[0], 5);
		if (ret) {
			if (lcd_debug_print_flag)
				LCDERR("load 4 parameters in lvds_attr\n");
			ret = of_property_read_u32_array(child, "lvds_attr",
				&para[0], 4);
			if (ret) {
				if (lcd_debug_print_flag)
					LCDPR("failed to get lvds_attr\n");
			} else {
				lvdsconf->lvds_repack = para[0];
				lvdsconf->dual_port = para[1];
				lvdsconf->pn_swap = para[2];
				lvdsconf->port_swap = para[3];
			}
		} else {
				lvdsconf->lvds_repack = para[0];
				lvdsconf->dual_port = para[1];
				lvdsconf->pn_swap = para[2];
				lvdsconf->port_swap = para[3];
				lvdsconf->lane_reverse = para[4];
		}
		ret = of_property_read_u32_array(child, "phy_attr",
			&para[0], 4);
		if (ret) {
			ret = of_property_read_u32_array(child, "phy_attr",
				&para[0], 2);
			if (ret) {
				if (lcd_debug_print_flag)
					LCDPR("failed to get phy_attr\n");
			} else {
				lvdsconf->phy_vswing = para[0];
				lvdsconf->phy_preem = para[1];
				lvdsconf->phy_clk_vswing = 0;
				lvdsconf->phy_clk_preem = 0;
				LCDPR("set phy vswing=0x%x, preemphasis=0x%x\n",
					lvdsconf->phy_vswing,
					lvdsconf->phy_preem);
			}
		} else {
			lvdsconf->phy_vswing = para[0];
			lvdsconf->phy_preem = para[1];
			lvdsconf->phy_clk_vswing = para[2];
			lvdsconf->phy_clk_preem = para[3];
			LCDPR("set phy vswing=0x%x, preemphasis=0x%x\n",
				lvdsconf->phy_vswing, lvdsconf->phy_preem);
			LCDPR("set phy_clk vswing=0x%x, preemphasis=0x%x\n",
				lvdsconf->phy_clk_vswing,
				lvdsconf->phy_clk_preem);
		}
		break;
	case LCD_VBYONE:
		ret = of_property_read_u32_array(child, "vbyone_attr",
			&para[0], 4);
		if (ret) {
			LCDERR("failed to get vbyone_attr\n");
		} else {
			pconf->lcd_control.vbyone_config->lane_count = para[0];
			pconf->lcd_control.vbyone_config->region_num = para[1];
			pconf->lcd_control.vbyone_config->byte_mode = para[2];
			pconf->lcd_control.vbyone_config->color_fmt = para[3];
		}
		ret = of_property_read_u32_array(child, "vbyone_intr_enable",
			&para[0], 2);
		if (ret) {
			LCDERR("failed to get vbyone_intr_enable\n");
		} else {
			pconf->lcd_control.vbyone_config->intr_en = para[0];
			pconf->lcd_control.vbyone_config->vsync_intr_en =
				para[1];
		}
		ret = of_property_read_u32_array(child, "phy_attr",
			&para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag)
				LCDPR("failed to get phy_attr\n");
		} else {
			pconf->lcd_control.vbyone_config->phy_vswing = para[0];
			pconf->lcd_control.vbyone_config->phy_preem = para[1];
			if (lcd_debug_print_flag) {
				LCDPR("phy vswing=%d, preemphasis=%d\n",
				pconf->lcd_control.vbyone_config->phy_vswing,
				pconf->lcd_control.vbyone_config->phy_preem);
			}
		}
		break;
	default:
		LCDERR("invalid lcd type\n");
		break;
	}

	ret = lcd_power_load_from_dts(pconf, child);

	return ret;
}

static int lcd_config_load_from_unifykey(struct lcd_config_s *pconf)
{
	unsigned char *para;
	int key_len, len;
	unsigned char *p;
	const char *str;
	struct aml_lcd_unifykey_header_s lcd_header;
	unsigned int temp;
	int ret;

	para = kmalloc((sizeof(unsigned char) * LCD_UKEY_LCD_SIZE), GFP_KERNEL);
	if (!para) {
		LCDERR("%s: Not enough memory\n", __func__);
		return -1;
	}
	key_len = LCD_UKEY_LCD_SIZE;
	memset(para, 0, (sizeof(unsigned char) * key_len));
	ret = lcd_unifykey_get("lcd", para, &key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	len = LCD_UKEY_HEAD_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("unifykey header length is incorrect\n");
		kfree(para);
		return -1;
	}

	lcd_unifykey_header_check(para, &lcd_header);
	len = 10 + 36 + 18 + 31 + 20;
	if (lcd_debug_print_flag) {
		LCDPR("unifykey header:\n");
		LCDPR("crc32             = 0x%08x\n", lcd_header.crc32);
		LCDPR("data_len          = %d\n", lcd_header.data_len);
		LCDPR("version           = 0x%04x\n", lcd_header.version);
		LCDPR("reserved          = 0x%04x\n", lcd_header.reserved);
	}

	/* step 2: check lcd parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("unifykey parameters length is incorrect\n");
		kfree(para);
		return -1;
	}

	/* basic: 36byte */
	p = para + LCD_UKEY_HEAD_SIZE;
	*(p + LCD_UKEY_MODEL_NAME - 1) = '\0'; /* ensure string ending */
	str = (const char *)p;
	strcpy(pconf->lcd_basic.model_name, str);
	p += LCD_UKEY_MODEL_NAME;
	pconf->lcd_basic.lcd_type = *p;
	p += LCD_UKEY_INTERFACE;
	pconf->lcd_basic.lcd_bits = *p;
	p += LCD_UKEY_LCD_BITS;
	pconf->lcd_basic.screen_width = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_SCREEN_WIDTH;
	pconf->lcd_basic.screen_height = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_SCREEN_HEIGHT;

	/* timing: 18byte */
	pconf->lcd_basic.h_active = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_H_ACTIVE;
	pconf->lcd_basic.v_active = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_V_ACTIVE;
	pconf->lcd_basic.h_period = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_H_PERIOD;
	pconf->lcd_basic.v_period = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_V_PERIOD;
	pconf->lcd_timing.hsync_width = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_HS_WIDTH;
	pconf->lcd_timing.hsync_bp = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_HS_BP;
	pconf->lcd_timing.hsync_pol = *p;
	p += LCD_UKEY_HS_POL;
	pconf->lcd_timing.vsync_width = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_VS_WIDTH;
	pconf->lcd_timing.vsync_bp = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_VS_BP;
	pconf->lcd_timing.vsync_pol = *p;
	p += LCD_UKEY_VS_POL;

	/* customer: 31byte */
	pconf->lcd_timing.fr_adjust_type = *p;
	p += LCD_UKEY_FR_ADJ_TYPE;
	pconf->lcd_timing.ss_level = *p;
	p += LCD_UKEY_SS_LEVEL;
	pconf->lcd_timing.clk_auto = *p;
	p += LCD_UKEY_CLK_AUTO_GEN;
	pconf->lcd_timing.lcd_clk = (*p | ((*(p + 1)) << 8) |
		((*(p + 2)) << 16) | ((*(p + 3)) << 24));
	p += LCD_UKEY_PCLK;
	pconf->lcd_basic.h_period_min = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_H_PERIOD_MIN;
	pconf->lcd_basic.h_period_max = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_H_PERIOD_MAX;
	pconf->lcd_basic.v_period_min = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_V_PERIOD_MIN;
	pconf->lcd_basic.v_period_max = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_V_PERIOD_MAX;
	pconf->lcd_basic.lcd_clk_min = (*p | ((*(p + 1)) << 8) |
		((*(p + 2)) << 16) | ((*(p + 3)) << 24));
	p += LCD_UKEY_PCLK_MIN;
	pconf->lcd_basic.lcd_clk_max = (*p | ((*(p + 1)) << 8) |
		((*(p + 2)) << 16) | ((*(p + 3)) << 24));
	p += LCD_UKEY_PCLK_MAX;
	/* dummy pointer */
	p += LCD_UKEY_CUST_VAL_8;
	p += LCD_UKEY_CUST_VAL_9;

	/* interface: 20byte */
	if (pconf->lcd_basic.lcd_type == LCD_LVDS) {
		if (lcd_header.version == 1) {
			pconf->lcd_control.lvds_config->lvds_repack =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_0;
			pconf->lcd_control.lvds_config->dual_port =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_1;
			pconf->lcd_control.lvds_config->pn_swap  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_2;
			pconf->lcd_control.lvds_config->port_swap  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_3;
			pconf->lcd_control.lvds_config->phy_vswing =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_4;
			pconf->lcd_control.lvds_config->phy_preem =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_5;
			pconf->lcd_control.lvds_config->phy_clk_vswing =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_6;
			pconf->lcd_control.lvds_config->phy_clk_preem  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_7;
			pconf->lcd_control.lvds_config->lane_reverse = 0;
			p += LCD_UKEY_IF_ATTR_8;

			/* dummy pointer */
			p += LCD_UKEY_IF_ATTR_9;
			}
		else if (lcd_header.version == 2) {
			pconf->lcd_control.lvds_config->lvds_repack =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_0;
			pconf->lcd_control.lvds_config->dual_port =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_1;
			pconf->lcd_control.lvds_config->pn_swap  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_2;
			pconf->lcd_control.lvds_config->port_swap  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_3;
			pconf->lcd_control.lvds_config->lane_reverse  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_4;
			pconf->lcd_control.lvds_config->phy_vswing =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_5;
			pconf->lcd_control.lvds_config->phy_preem =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_6;
			pconf->lcd_control.lvds_config->phy_clk_vswing =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_7;
			pconf->lcd_control.lvds_config->phy_clk_preem  =
					(*p | ((*(p + 1)) << 8)) & 0xff;
			p += LCD_UKEY_IF_ATTR_8;
			/* dummy pointer */
			p += LCD_UKEY_IF_ATTR_9;
			}
	} else if (pconf->lcd_basic.lcd_type == LCD_TTL) {
		pconf->lcd_control.ttl_config->clk_pol =
			(*p | ((*(p + 1)) << 8)) & 0x1;
		p += LCD_UKEY_IF_ATTR_0;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* de_valid */
		pconf->lcd_control.ttl_config->sync_valid = (temp << 1);
		p += LCD_UKEY_IF_ATTR_1;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* hvsync_valid */
		pconf->lcd_control.ttl_config->sync_valid |= (temp << 0);
		p += LCD_UKEY_IF_ATTR_2;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* rb_swap */
		pconf->lcd_control.ttl_config->swap_ctrl = (temp << 1);
		p += LCD_UKEY_IF_ATTR_3;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* bit_swap */
		pconf->lcd_control.ttl_config->swap_ctrl |= (temp << 0);
		p += LCD_UKEY_IF_ATTR_4;
		/* dummy pointer */
		p += LCD_UKEY_IF_ATTR_5;
		p += LCD_UKEY_IF_ATTR_6;
		p += LCD_UKEY_IF_ATTR_7;
		p += LCD_UKEY_IF_ATTR_8;
		p += LCD_UKEY_IF_ATTR_9;
	} else if (pconf->lcd_basic.lcd_type == LCD_VBYONE) {
		pconf->lcd_control.vbyone_config->lane_count =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_0;
		pconf->lcd_control.vbyone_config->region_num =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_1;
		pconf->lcd_control.vbyone_config->byte_mode  =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_2;
		pconf->lcd_control.vbyone_config->color_fmt  =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_3;
		pconf->lcd_control.vbyone_config->phy_vswing =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_4;
		pconf->lcd_control.vbyone_config->phy_preem =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_5;
		pconf->lcd_control.vbyone_config->intr_en =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_6;
		pconf->lcd_control.vbyone_config->vsync_intr_en =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_7;
		/* dummy pointer */
		p += LCD_UKEY_IF_ATTR_8;
		p += LCD_UKEY_IF_ATTR_9;
	} else {
		LCDERR("unsupport lcd_type: %d\n", pconf->lcd_basic.lcd_type);
		p += LCD_UKEY_IF_ATTR_0;
		p += LCD_UKEY_IF_ATTR_1;
		p += LCD_UKEY_IF_ATTR_2;
		p += LCD_UKEY_IF_ATTR_3;
		p += LCD_UKEY_IF_ATTR_4;
		p += LCD_UKEY_IF_ATTR_5;
		p += LCD_UKEY_IF_ATTR_6;
		p += LCD_UKEY_IF_ATTR_7;
		p += LCD_UKEY_IF_ATTR_8;
		p += LCD_UKEY_IF_ATTR_9;
	}

	/* step 3: check power sequence */
	ret = lcd_power_load_from_unifykey(pconf, para, key_len, len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	kfree(para);
	return 0;
}

static void lcd_config_init(struct lcd_config_s *pconf)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config();
	unsigned int ss_level;
	unsigned int clk;
	unsigned int sync_duration, h_period, v_period;

	clk = pconf->lcd_timing.lcd_clk;
	h_period = pconf->lcd_basic.h_period;
	v_period = pconf->lcd_basic.v_period;
	if (clk < 200) { /* regard as frame_rate */
		sync_duration = clk * 100;
		pconf->lcd_timing.lcd_clk = clk * h_period * v_period;
	} else { /* regard as pixel clock */
		sync_duration = ((clk / h_period) * 100) / v_period;
	}
	pconf->lcd_timing.sync_duration_num = sync_duration;
	pconf->lcd_timing.sync_duration_den = 100;
	pconf->lcd_timing.lcd_clk_dft = pconf->lcd_timing.lcd_clk;
	pconf->lcd_timing.h_period_dft = pconf->lcd_basic.h_period;
	pconf->lcd_timing.v_period_dft = pconf->lcd_basic.v_period;
	lcd_tcon_config(pconf);

	lcd_tablet_vinfo_update();

	lcd_tablet_config_update(pconf);
	lcd_clk_generate_parameter(pconf);
	ss_level = pconf->lcd_timing.ss_level;
	cconf->ss_level = (ss_level >= cconf->ss_level_max) ? 0 : ss_level;
}

static int lcd_get_config(struct lcd_config_s *pconf, struct device *dev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int load_id = 0;
	int ret;

	if (dev->of_node == NULL) {
		LCDERR("dev of_node is null\n");
		return -1;
	}
	if (lcd_drv->lcd_key_valid) {
		ret = lcd_unifykey_check("lcd");
		if (ret < 0)
			load_id = 0;
		else
			load_id = 1;
	}
	if (load_id) {
		LCDPR("%s from unifykey\n", __func__);
		lcd_drv->lcd_config_load = 1;
		lcd_config_load_from_unifykey(pconf);
	} else {
		LCDPR("%s from dts\n", __func__);
		lcd_drv->lcd_config_load = 0;
		lcd_config_load_from_dts(pconf, dev);
	}
	lcd_init_load_from_dts(pconf, dev);
	lcd_config_print(pconf);
	lcd_config_init(pconf);

	return 0;
}

/* ************************************************** *
   lcd notify
 * ************************************************** */
/* sync_duration is real_value * 100 */
static void lcd_set_vinfo(unsigned int sync_duration)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	LCDPR("%s: sync_duration=%d\n", __func__, sync_duration);

	/* update vinfo */
	lcd_drv->lcd_info->sync_duration_num = sync_duration;
	lcd_drv->lcd_info->sync_duration_den = 100;

	/* update interface timing */
	lcd_tablet_config_update(lcd_drv->lcd_config);
#ifdef CONFIG_AML_VPU
	request_vpu_clk_vmod(
		lcd_drv->lcd_config->lcd_timing.lcd_clk, VPU_VENCL);
#endif

	/* change clk parameter */
	switch (lcd_drv->lcd_config->lcd_timing.clk_change) {
	case LCD_CLK_PLL_CHANGE:
		lcd_clk_generate_parameter(lcd_drv->lcd_config);
		lcd_clk_set(lcd_drv->lcd_config);
		break;
	case LCD_CLK_FRAC_UPDATE:
		lcd_clk_update(lcd_drv->lcd_config);
		break;
	default:
		break;
	}
	lcd_venc_change(lcd_drv->lcd_config);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE,
		&lcd_drv->lcd_info->mode);
}

static int lcd_frame_rate_adjust_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	unsigned int *sync_duration;

	/* LCDPR("%s: 0x%lx\n", __func__, event); */
	if ((event & LCD_EVENT_FRAME_RATE_ADJUST) == 0)
		return NOTIFY_DONE;

	sync_duration = (unsigned int *)data;
	lcd_set_vinfo(*sync_duration);

	return NOTIFY_OK;
}

static struct notifier_block lcd_frame_rate_adjust_nb = {
	.notifier_call = lcd_frame_rate_adjust_notifier,
};

/* ************************************************** *
   lcd tablet
 * ************************************************** */
int lcd_tablet_probe(struct device *dev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int ret;

	lcd_drv->version = LCD_DRV_VERSION;
	lcd_drv->vout_server_init = lcd_tablet_vout_server_init;
	lcd_drv->driver_init_pre = lcd_tablet_driver_init_pre;
	lcd_drv->driver_init = lcd_tablet_driver_init;
	lcd_drv->driver_disable = lcd_tablet_driver_disable;
	lcd_drv->driver_tiny_enable = lcd_tablet_driver_tiny_enable;
	lcd_drv->driver_tiny_disable = lcd_tablet_driver_tiny_disable;

	lcd_get_config(lcd_drv->lcd_config, dev);

	ret = aml_lcd_notifier_register(&lcd_frame_rate_adjust_nb);
	if (ret)
		LCDERR("register lcd_frame_rate_adjust_nb failed\n");

	return 0;
}

int lcd_tablet_remove(struct device *dev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	aml_lcd_notifier_unregister(&lcd_frame_rate_adjust_nb);
	kfree(lcd_drv->lcd_info);
	lcd_drv->lcd_info = NULL;
	return 0;
}

