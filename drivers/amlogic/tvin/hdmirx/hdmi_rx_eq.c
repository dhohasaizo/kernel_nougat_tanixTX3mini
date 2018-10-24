/*
 * hdmi rx eq adjust module for g9tv
 *
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/spinlock_types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/amlogic/tvin/tvin.h>
/* Local include */
#include "hdmi_rx_eq.h"
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"

/*------------------------variable define------------------------------*/

int eq_setting_back = 0;
int fat_bit_status = 0;
static int min_max_diff = 4;
static int long_cable_best_setting = 6;
struct st_eq_data eq_ch0;
struct st_eq_data eq_ch1;
struct st_eq_data eq_ch2;
char pre_eq_freq = E_EQ_NONE;
unsigned char run_eq_flag = E_EQ_START;

static int tmds_valid_cnt_max = 2;
MODULE_PARM_DESC(tmds_valid_cnt_max, "\n tmds_valid_cnt_max\n");
module_param(tmds_valid_cnt_max, int, 0664);

static int force_clk_rate;
MODULE_PARM_DESC(force_clk_rate, "\n force_clk_rate\n");
module_param(force_clk_rate, int, 0664);

static int delay_ms_cnt = 10; /* 5 */
MODULE_PARM_DESC(delay_ms_cnt, "\n delay_ms_cnt\n");
module_param(delay_ms_cnt, int, 0664);

static int eq_sts_stable_max = 2;
MODULE_PARM_DESC(eq_sts_stable_max, "\n eq_sts_stable_max\n");
module_param(eq_sts_stable_max, int, 0664);

static int eq_max_setting = 7;
MODULE_PARM_DESC(eq_max_setting, "\n eq_max_setting\n");
module_param(eq_max_setting, int, 0664);
unsigned int last_clk_rate;

static int eq_cfg_hd = 4;
MODULE_PARM_DESC(eq_cfg_hd, "\n eq_cfg_hd\n");
module_param(eq_cfg_hd, int, 0664);

static int eq_cfg_3g = 5;
MODULE_PARM_DESC(eq_cfg_3g, "\n eq_cfg_3g\n");
module_param(eq_cfg_3g, int, 0664);

static int eq_cfg_6g = 6;
MODULE_PARM_DESC(eq_cfg_6g, "\n eq_cfg_6g\n");
module_param(eq_cfg_6g, int, 0664);

/*------------------------variable define end------------------------------*/

bool eq_maxvsmin(int ch0Setting, int ch1Setting, int ch2Setting)
{
	int min = ch0Setting;
	int max = ch0Setting;

	if (ch1Setting > max)
		max = ch1Setting;
	if (ch2Setting > max)
		max = ch2Setting;
	if (ch1Setting < min)
		min = ch1Setting;
	if (ch2Setting < min)
		min = ch2Setting;
	if ((max - min) > min_max_diff) {
		rx_pr("MINMAX ERROR\n");
		return 0;
	}
	return 1;
}

void initvars(struct st_eq_data *ch_data)
{
	/* Slope accumulator */
	ch_data->acc = 0;
	/* Early Counter dataAcquisition data */
	ch_data->acq = 0;
	ch_data->lastacq = 0;
	ch_data->validLongSetting = 0;
	ch_data->validShortSetting = 0;
	/* BEST Setting = short */
	ch_data->bestsetting = shortcableSetting;
	/* TMDS VALID not valid */
	ch_data->tmdsvalid = 0;
	memset(ch_data->acq_n, 0, sizeof(uint16_t)*15);
}


void hdmi_rx_phy_ConfEqualSingle(void)
{
	hdmirx_wr_phy(PHY_EQCTRL1_CH0, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL1_CH1, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL1_CH2, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0024 | (avgAcq << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0024 | (avgAcq << 11));
}

void hdmi_rx_phy_ConfEqualSetting(uint16_t lockVector)
{
	hdmirx_wr_phy(PHY_EQCTRL4_CH0, lockVector);
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4024 | (avgAcq << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4026 | (avgAcq << 11));
}

void hdmi_rx_phy_ConfEqualAutoCalib(void)
{
	hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
	hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1819);
	hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
}

uint16_t rx_phy_rd_earlycnt_ch0(void)
{

	return hdmirx_rd_phy(PHY_EQSTAT3_CH0);
}

uint16_t rx_phy_rd_earlycnt_ch1(void)
{

	return hdmirx_rd_phy(PHY_EQSTAT3_CH1);
}

uint16_t rx_phy_rd_earlycnt_ch2(void)
{

	return hdmirx_rd_phy(PHY_EQSTAT3_CH2);
}

uint16_t hdmi_rx_phy_CoreStatusCh0(void)
{

	return hdmirx_rd_phy(PHY_CORESTATUS_CH0);
}

uint16_t hdmi_rx_phy_CoreStatusCh1(void)
{

	return hdmirx_rd_phy(PHY_CORESTATUS_CH1);
}

uint16_t hdmi_rx_phy_CoreStatusCh2(void)
{

	return hdmirx_rd_phy(PHY_CORESTATUS_CH2);
}

void phy_conf_eq_setting(int ch0_lockVector,
				int ch1_lockVector, int ch2_lockVector)
{
	/* ConfEqualSetting */
	if (is_meson_gxtvbb_cpu()) {
		hdmirx_wr_phy(PHY_EQCTRL4_CH0, 1<<ch0_lockVector);
		hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x0024);
		hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x0026);

		hdmirx_wr_phy(PHY_EQCTRL4_CH1, 1<<ch1_lockVector);
		hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0024);
		hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0026);

		hdmirx_wr_phy(PHY_EQCTRL4_CH2, 1<<ch2_lockVector);
		hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0024);
		hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0026);
	} else {
		hdmirx_wr_phy(PHY_EQCTRL4_CH0, 1<<ch0_lockVector);
		hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4024 | (avgAcq << 11));
		hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4026 | (avgAcq << 11));

		hdmirx_wr_phy(PHY_EQCTRL4_CH1, 1<<ch1_lockVector);
		hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x4024 | (avgAcq << 11));
		hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x4026 | (avgAcq << 11));

		hdmirx_wr_phy(PHY_EQCTRL4_CH2, 1<<ch2_lockVector);
		hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x4024 | (avgAcq << 11));
		hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x4026 | (avgAcq << 11));
	}
	hdmirx_phy_pddq(1);
	hdmirx_phy_pddq(0);
}

uint8_t testType(uint16_t setting, struct st_eq_data *ch_data)
{
	uint16_t stepSlope = 0;
	/* LONG CABLE EQUALIZATION */
	if ((ch_data->acq < ch_data->lastacq) &&
		(ch_data->tmdsvalid == 1)) {
		ch_data->acc += (ch_data->lastacq-ch_data->acq);
		if (ch_data->validLongSetting == 0 &&
			ch_data->acq < equalizedCounterValue &&
			ch_data->acc > AccMinLimit) {
			ch_data->bestLongSetting = setting;
			ch_data->validLongSetting = 1;
		}
		stepSlope = ch_data->lastacq-ch_data->acq;
	}
	/* SHORT CABLE EQUALIZATION */
	if (ch_data->tmdsvalid == 1 &&
		ch_data->validShortSetting == 0) {
		/* Short setting better than default, system over-equalized */
		if (setting < shortcableSetting &&
			ch_data->acq < equalizedCounterValue)  {
			ch_data->validShortSetting = 1;
			ch_data->bestShortSetting = setting;
		}
		/* default Short setting is valid */
		if (setting == shortcableSetting) {
			ch_data->validShortSetting = 1;
			ch_data->bestShortSetting = shortcableSetting;
		}
	}
	/* Exit type Long cable
	(early-late count curve well behaved
	and 50% threshold achived) */
	if (ch_data->validLongSetting  == 1 &&
		ch_data->acc > AccLimit) {
		ch_data->bestsetting = ch_data->bestLongSetting;
		if (ch_data->bestsetting > long_cable_best_setting)
			ch_data->bestsetting = long_cable_best_setting;
		if (log_level & EQ_LOG)
			rx_pr("longcable1");
		return 1;
	}
	/* Exit type short cable
	(early-late count curve  behaved as a short cable) */
	if (setting == eq_max_setting &&
		ch_data->acc < AccLimit &&
		ch_data->validShortSetting == 1) {
		ch_data->bestsetting = ch_data->bestShortSetting;
		if (log_level & EQ_LOG)
			rx_pr("shortcable");
		return 2;
	}
	/* Exit type long cable
	(early-late count curve well behaved
	nevertheless 50% threshold not achieved) */
	if ((setting == eq_max_setting) &&
		(ch_data->tmdsvalid == 1) &&
		(ch_data->acc > AccLimit) &&
		(stepSlope > minSlope)) {
		ch_data->bestsetting = long_cable_best_setting;
		if (log_level & EQ_LOG)
			rx_pr("longcable2");
		return 3;
	}
	/* error cable */
	if (setting == eq_max_setting) {
		if (log_level & EQ_LOG)
			rx_pr("errcable");
		ch_data->bestsetting = ErrorcableSetting;
		return 255;
	}
	/* Cable not detected,
	continue to next setting */
	return 0;
}


uint8_t aquireEarlyCnt(uint16_t setting)
{
	uint16_t lockVector = 0x0001;
	int timeout_cnt = 20;
	lockVector = lockVector << setting;
	hdmi_rx_phy_ConfEqualSetting(lockVector);
	hdmi_rx_phy_ConfEqualAutoCalib();
	/* mdelay(delay_ms_cnt); */
	while (timeout_cnt--) {
		mdelay(delay_ms_cnt);
		eq_ch0.tmdsvalid =
			(hdmi_rx_phy_CoreStatusCh0() & 0x0080) > 0 ? 1 : 0;
		eq_ch1.tmdsvalid =
			(hdmi_rx_phy_CoreStatusCh1() & 0x0080) > 0 ? 1 : 0;
		eq_ch2.tmdsvalid =
			(hdmi_rx_phy_CoreStatusCh2() & 0x0080) > 0 ? 1 : 0;
		if ((eq_ch0.tmdsvalid |
			eq_ch1.tmdsvalid |
			eq_ch2.tmdsvalid) != 0)
			break;
	}
	if ((eq_ch0.tmdsvalid |
		eq_ch1.tmdsvalid |
		eq_ch2.tmdsvalid) == 0) {
		if (log_level & EQ_LOG)
			rx_pr("invalid-earlycnt\n");
		return 0;
	}

	if (!is_meson_gxtvbb_cpu()) {
		/* End the acquisitions if no TMDS valid */
		/* hdmi_rx_phy_ConfEqualSetting(lockVector); */
		/* phy_conf_eq_setting(setting, setting, setting); */
		/* sleep_time_CDR should be enough
			to have TMDS valid asserted. */
		/* TMDS VALID can be obtained either
			by per channel basis or global pin */
		/* TMDS VALID BY channel basis (Option #1) */
		/* Get early counters */
		eq_ch0.acq = rx_phy_rd_earlycnt_ch0() >> avgAcq;
		eq_ch0.acq_n[setting] = eq_ch0.acq;
		if (log_level & ERR_LOG)
			rx_pr("eq_ch0_acq #%d = %d\n", setting, eq_ch0.acq);
		eq_ch1.acq = rx_phy_rd_earlycnt_ch1() >> avgAcq;
		eq_ch1.acq_n[setting] = eq_ch1.acq;
		if (log_level & ERR_LOG)
			rx_pr("eq_ch1_acq #%d = %d\n", setting, eq_ch1.acq);
		eq_ch2.acq = rx_phy_rd_earlycnt_ch2() >> avgAcq;
		eq_ch2.acq_n[setting] = eq_ch2.acq;
		if (log_level & ERR_LOG)
			rx_pr("eq_ch2_acq #%d = %d\n", setting, eq_ch2.acq);
	} else {
		uint16_t cnt;
		uint16_t upperBound_acqCH0;
		uint16_t upperBound_acqCH1;
		uint16_t upperBound_acqCH2;
		uint16_t lowerBound_acqCH0;
		uint16_t lowerBound_acqCH1;
		uint16_t lowerBound_acqCH2;
		uint8_t outBound_acqCH0;
		uint8_t outBound_acqCH1;
		uint8_t outBound_acqCH2;
		/* Maximum allowable deviation to
			consider a acquisition stable = 20*2 */
		uint16_t boundspread = 20;
		/* Minimum number of acquisitions to evaluate the stability */
		uint8_t minACQtoStableDetection = 3;
		uint16_t acq_ch0 = 0;
		uint16_t acq_ch1 = 0;
		uint16_t acq_ch2 = 0;

		/* get TMDSVALID and early counters */
		eq_ch0.acq = 0;
		eq_ch1.acq = 0;
		eq_ch2.acq = 0;
		outBound_acqCH0 = 0;
		outBound_acqCH1 = 0;
		outBound_acqCH2 = 0;

		/* Get fisrt set of early counters */
		acq_ch0 = rx_phy_rd_earlycnt_ch0();
		acq_ch1 = rx_phy_rd_earlycnt_ch1();
		acq_ch2 = rx_phy_rd_earlycnt_ch2();

		eq_ch0.acq += acq_ch0;
		eq_ch1.acq += acq_ch1;
		eq_ch2.acq += acq_ch2;

		upperBound_acqCH0 = acq_ch0 + boundspread;
		lowerBound_acqCH0 = acq_ch0 - boundspread;
		upperBound_acqCH1 = acq_ch1 + boundspread;
		lowerBound_acqCH1 = acq_ch1 - boundspread;
		upperBound_acqCH2 = acq_ch2 + boundspread;
		lowerBound_acqCH2 = acq_ch2 - boundspread;

		for (cnt = 1; cnt < setting; cnt++) {
			hdmi_rx_phy_ConfEqualAutoCalib();
			mdelay(delay_ms_cnt);
			if (acq_ch0 > upperBound_acqCH0 ||
				acq_ch0 < lowerBound_acqCH0)
				outBound_acqCH0++;
			if (acq_ch1 > upperBound_acqCH1 ||
				acq_ch1 < lowerBound_acqCH1)
				outBound_acqCH1++;
			if (acq_ch2 > upperBound_acqCH2 ||
				acq_ch2 < lowerBound_acqCH2)
				outBound_acqCH2++;

			/* Stable detection, minimum 3 readouts */
			if (cnt == minACQtoStableDetection) {
				if (outBound_acqCH0 == 0 &&
					outBound_acqCH1 == 0 &&
					outBound_acqCH2 == 0) {
					rx_pr("STABLE ACQ\n");
					setting = 3;
					break;
				}

			}
			acq_ch0 = rx_phy_rd_earlycnt_ch0();
			acq_ch1 = rx_phy_rd_earlycnt_ch1();
			acq_ch2 = rx_phy_rd_earlycnt_ch2();
			eq_ch0.acq += acq_ch0;
			eq_ch1.acq += acq_ch1;
			eq_ch2.acq += acq_ch2;
		}
		eq_ch0.acq = eq_ch0.acq/setting;
		eq_ch1.acq = eq_ch1.acq/setting;
		eq_ch2.acq = eq_ch2.acq/setting;
	}
	return 1;
}

uint8_t SettingFinder(void)
{
	uint16_t actSetting = 0;
	uint16_t retcodeCH0 = 0;
	uint16_t retcodeCH1 = 0;
	uint16_t retcodeCH2 = 0;
	uint8_t tmds_valid = 0;

	initvars(&eq_ch0);
	initvars(&eq_ch1);
	initvars(&eq_ch2);

	/* Get statistics of early-late counters for setting 0 */
	tmds_valid = aquireEarlyCnt(actSetting);


	while (retcodeCH0 == 0 || retcodeCH1 == 0 || retcodeCH2 == 0) {
		actSetting++;
		/* Update last acquisition value,
		for threshold crossing detection */
		eq_ch0.lastacq = eq_ch0.acq;
		eq_ch1.lastacq = eq_ch1.acq;
		eq_ch2.lastacq = eq_ch2.acq;
		/* Get statistics of early-late
		counters for next setting */
		tmds_valid = aquireEarlyCnt(actSetting);
		/* check for cable type, stop after detection */
		if (retcodeCH0 == 0) {
			retcodeCH0 = testType(actSetting, &eq_ch0);
			if ((log_level & EQ_LOG) && retcodeCH0)
				rx_pr("-CH0\n");
		}
		if (retcodeCH1 == 0) {
			retcodeCH1 = testType(actSetting, &eq_ch1);
			if ((log_level & EQ_LOG) && retcodeCH1)
				rx_pr("-CH1\n");
		}
		if (retcodeCH2 == 0) {
			retcodeCH2 = testType(actSetting, &eq_ch2);
			if ((log_level & EQ_LOG) && retcodeCH2)
				rx_pr("-CH2\n");
		}

	}
	if (retcodeCH0 == 255 || retcodeCH1 == 255 || retcodeCH2 == 255)
		return 0;

	return 1;

}

void hdmirx_phy_conf_eq_setting(int rx_port_sel, int ch0Setting,
				int ch1Setting, int ch2Setting)
{
	unsigned int data32;
	if (log_level & VIDEO_LOG)
		rx_pr("hdmirx_phy_conf_eq_setting\n");
	/* PDDQ = 1'b1; PHY_RESET = 1'b0; */
	data32  = 0;
	data32 |= 1             << 6;   /* [6]      physvsretmodez */
	data32 |= 1             << 4;   /* [5:4]    cfgclkfreq */
	data32 |= rx_port_sel   << 2;   /* [3:2]    portselect */
	data32 |= 1             << 1;   /* [1]      phypddq */
	data32 |= 0             << 0;   /* [0]      phyreset */
	/*DEFAULT: {27'd0, 3'd0, 2'd1} */
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
	if ((ch0Setting == 0) &&
		(ch1Setting == 0) &&
		(ch2Setting == 0))
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);
	else {
		hdmirx_wr_phy(PHY_CH0_EQ_CTRL3, ch0Setting);
		hdmirx_wr_phy(PHY_CH1_EQ_CTRL3, ch1Setting);
		hdmirx_wr_phy(PHY_CH2_EQ_CTRL3, ch2Setting);
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x40);
	}
	/* PDDQ = 1'b0; PHY_RESET = 1'b0; */
	data32  = 0;
	data32 |= 1             << 6;   /* [6]      physvsretmodez */
	data32 |= 1             << 4;   /* [5:4]    cfgclkfreq */
	data32 |= rx_port_sel   << 2;   /* [3:2]    portselect */
	data32 |= 0             << 1;   /* [1]      phypddq */
	data32 |= 0             << 0;   /* [0]      phyreset */
	/* DEFAULT: {27'd0, 3'd0, 2'd1} */
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
}

bool hdmirx_phy_clk_rate_monitor(void)
{
	unsigned int clk_rate;
	bool changed = false;
	int i;
	int error = 0;

	if (force_clk_rate & 0x10)
		clk_rate = force_clk_rate & 1;
	else
		clk_rate = (hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1;

	if (clk_rate != last_clk_rate) {
		changed = true;
		for (i = 0; i < 3; i++) {
			if (1 == clk_rate) {
				error = hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
					hdmirx_rd_phy(PHY_CDR_CTRL_CNT)|(1<<8));
			} else {
				error = hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
					hdmirx_rd_phy(
						PHY_CDR_CTRL_CNT)&(~(1<<8)));
			}
			if (error == 0)
				break;
		}
		if (log_level & EQ_LOG)
			rx_pr("clk_rate:%d, last_clk_rate: %d\n",
			clk_rate, last_clk_rate);
		last_clk_rate = clk_rate;
	}
	return changed;
}

#if 0
bool hdmirx_phy_check_tmds_valid(void)
{
	if ((((hdmirx_rd_phy(0x30) & 0x80) == 0x80) | finish_flag[EQ_CH0]) &&
	(((hdmirx_rd_phy(0x50) & 0x80) == 0x80) | finish_flag[EQ_CH1]) &&
	(((hdmirx_rd_phy(0x70) & 0x80) == 0x80) | finish_flag[EQ_CH2]))
		return true;
	else
		return false;
}
#endif

bool rx_need_eq_algorithm(void)
{
	int mfsm_status = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);

	/* configure FATBITS PHY */
	if (hdmirx_tmds_6g()) {
		fat_bit_status = EQ_FATBIT_MASK_HDMI20;
		min_max_diff = MINMAX_maxDiff_HDMI20;
		if ((pre_eq_freq == E_EQ_6G) && (run_eq_flag == E_EQ_PASS)) {
			if (log_level & EQ_LOG)
				rx_pr("EQ_6G_same\n");
			return false;
		} else if (run_eq_flag == E_EQ_START) {
			if (log_level & EQ_LOG)
				rx_pr("EQ_6G_def\n");
			eq_ch0.bestsetting = eq_cfg_6g;
			eq_ch1.bestsetting = eq_cfg_6g;
			eq_ch2.bestsetting = eq_cfg_6g;
			pre_eq_freq = E_EQ_6G;
			return false;
		} else {
			pre_eq_freq = E_EQ_6G;
			if (log_level & EQ_LOG)
				rx_pr("EQ_6G\n");
		}
	} else if ((mfsm_status & 0x600) == 0x00) {
		fat_bit_status = EQ_FATBIT_MASK_4k;
		min_max_diff = MINMAX_maxDiff;
		if ((pre_eq_freq == E_EQ_3G) && (run_eq_flag == E_EQ_PASS)) {
			if (log_level & EQ_LOG)
				rx_pr("EQ_3G_same\n");
			return false;
		} else if (run_eq_flag == E_EQ_START) {
			if (log_level & EQ_LOG)
				rx_pr("EQ_3G_def\n");
			pre_eq_freq = E_EQ_3G;
			eq_ch0.bestsetting = eq_cfg_3g;
			eq_ch1.bestsetting = eq_cfg_3g;
			eq_ch2.bestsetting = eq_cfg_3g;
			return false;
		} else {
			pre_eq_freq = E_EQ_3G;
			if (log_level & EQ_LOG)
				rx_pr("EQ_3G\n");
		}
	} else if ((mfsm_status & 0x400) == 0x400) {
		fat_bit_status = EQ_FATBIT_MASK;
		min_max_diff = MINMAX_maxDiff;
		if (pre_eq_freq == E_EQ_SD)
			return false;
		else if (E_EQ_FAIL == run_eq_flag) {
			eq_ch0.bestsetting = eq_cfg_hd;
			eq_ch1.bestsetting = eq_cfg_hd;
			eq_ch2.bestsetting = eq_cfg_hd;
		} else {
			eq_ch0.bestsetting = 0;
			eq_ch1.bestsetting = 0;
			eq_ch2.bestsetting = 0;
		}
		pre_eq_freq = E_EQ_SD;
		return false;
	} else {
		/* 94.5 ~ 148.5 */
		fat_bit_status = EQ_FATBIT_MASK;
		min_max_diff = MINMAX_maxDiff;
		if ((pre_eq_freq == E_EQ_HD) && (run_eq_flag == E_EQ_PASS)) {
			if (log_level & EQ_LOG)
				rx_pr("EQ_HD_same\n");
			return false;
			}
		else if (run_eq_flag == E_EQ_START) {
			if (log_level & EQ_LOG)
				rx_pr("EQ_HD_def\n");
			eq_ch0.bestsetting = eq_cfg_hd;
			eq_ch1.bestsetting = eq_cfg_hd;
			eq_ch2.bestsetting = eq_cfg_hd;
			pre_eq_freq = E_EQ_HD;
			return false;
		} else {
			pre_eq_freq = E_EQ_HD;
			if (log_level & EQ_LOG)
				rx_pr("EQ_HD\n");
		}
	}
	hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);
	hdmi_rx_phy_ConfEqualSingle();
	hdmirx_wr_phy(PHY_EQCTRL6_CH0, fat_bit_status);
	hdmirx_wr_phy(PHY_EQCTRL6_CH1, fat_bit_status);
	hdmirx_wr_phy(PHY_EQCTRL6_CH2, fat_bit_status);

	return true;
}

void dump_eq_data(void)
{
	int i = 0;
	rx_pr("/n*****************\n");
	for (i = 0; i < 15; i++)
		rx_pr("CH0-acq #%d: %d\n", i, eq_ch0.acq_n[i]);
	for (i = 0; i < 15; i++)
		rx_pr("CH1-acq #%d: %d\n", i, eq_ch1.acq_n[i]);
	for (i = 0; i < 15; i++)
		rx_pr("CH2-acq #%d: %d\n", i, eq_ch2.acq_n[i]);
}

