/*
 * drivers/amlogic/mmc/amlsd.h
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

#ifndef AMLSD_H
#define AMLSD_H


#define AML_MMC_MAJOR_VERSION   1
#define AML_MMC_MINOR_VERSION   07
#define AML_MMC_VERSION \
	((AML_MMC_MAJOR_VERSION << 8) | AML_MMC_MINOR_VERSION)
#define AML_MMC_VER_MESSAGE \
		"2017-4-26: Support new emmc host controller for version 3"
extern unsigned sdhc_debug;
extern unsigned sdio_debug;
extern unsigned sd_emmc_debug;
extern const u8 tuning_blk_pattern_4bit[64];
extern const u8 tuning_blk_pattern_8bit[128];
#define DEBUG_SD_OF		1
/* #define DEBUG_SD_OF			0 */

#define MODULE_NAME		"amlsd"

#define A0_GP_CFG0			(0xc8100240)
#define A0_GP_CFG2			(0xc8100248)
#define STORAGE_DEV_NOSET	(0)
#define STORAGE_DEV_EMMC	(1)
#define STORAGE_DEV_NAND	(2)
#define STORAGE_DEV_SPI		(3)
#define STORAGE_DEV_SDCARD	(4)
#define STORAGE_DEV_USB		(5)
#define LDO4DAC_REG_ADDR        0x4f
#define LDO4DAC_REG_1_8_V       0x24
#define LDO4DAC_REG_2_8_V       0x4c
#define LDO4DAC_REG_3_3_V       0x60

#define AMLSD_DBG_COMMON	(1<<0)
#define AMLSD_DBG_REQ		(1<<1)
#define AMLSD_DBG_RESP		(1<<2)
#define AMLSD_DBG_REG		(1<<3)
#define AMLSD_DBG_RD_TIME	(1<<4)
#define AMLSD_DBG_WR_TIME	(1<<5)
#define AMLSD_DBG_BUSY_TIME	(1<<6)
#define AMLSD_DBG_RD_DATA	(1<<7)
#define AMLSD_DBG_WR_DATA	(1<<8)
#define AMLSD_DBG_IOS		(1<<9)
#define AMLSD_DBG_IRQ		(1<<10)
#define AMLSD_DBG_CLKC		(1<<11)
#define AMLSD_DBG_TUNING	(1<<12)

#define     DETECT_CARD_IN          1
#define     DETECT_CARD_OUT         2
#define     DETECT_CARD_JTAG_IN     3
#define     DETECT_CARD_JTAG_OUT    4

#define EMMC_DAT3_PINMUX_CLR    0
#define EMMC_DAT3_PINMUX_SET    1

#define CHECK_RET(ret) { \
if (ret) \
	pr_info("[%s] gpio op failed(%d) at line %d\n",\
	__func__, ret, __LINE__); \
}

#define sdhc_dbg(dbg_level, fmt, args...) do {\
	if (dbg_level & sdhc_debug)	\
		pr_info("[%s]" fmt , __func__, ##args);	\
} while (0)

#define sdhc_err(fmt, args...) \
	pr_info("[%s] " fmt , __func__, ##args);


#define sdio_dbg(dbg_level, fmt, args...) do {\
	if (dbg_level & sdio_debug)	\
		pr_info("[%s]" fmt , __func__, ##args);	\
} while (0)

#define sdio_err(fmt, args...) \
	pr_info("[%s] " fmt , __func__, ##args);

#define sd_emmc_dbg(dbg_level, fmt, args...) do {\
	if (dbg_level & sd_emmc_debug)	\
		pr_info("[%s]" fmt , __func__, ##args);	\
} while (0)
#define sd_emmc_err(fmt, args...) \
	pr_warn("[%s] " fmt , __func__, ##args);

#define SD_PARSE_U32_PROP_HEX(node, prop_name, prop, value) do {	\
	if (!of_property_read_u32(node, prop_name, &prop)) {\
		value = prop;\
		prop = 0;\
		if (DEBUG_SD_OF) {	\
			pr_info("get property:%25s, value:0x%08x\n",	\
			prop_name, (unsigned int)value);	\
		} \
	} \
} while (0)

#define SD_PARSE_U32_PROP_DEC(node, prop_name, prop, value) do {	\
	if (!of_property_read_u32(node, prop_name, &prop)) {\
		value = prop;\
		prop = 0;\
		if (DEBUG_SD_OF) {	\
			pr_info("get property:%25s, value:%d\n",	\
			prop_name, (unsigned int)value);	\
		} \
	} \
} while (0)

#define SD_PARSE_GPIO_NUM_PROP(node, prop_name, str, gpio_pin) {\
	if (!of_property_read_string(node, prop_name, &str)) {\
		gpio_pin = \
		desc_to_gpio(of_get_named_gpiod_flags(node, \
						      prop_name, 0, NULL));\
		if (DEBUG_SD_OF) {	\
			pr_info("get property:%25s, str:%s\n",\
			prop_name, str);\
		} \
	} \
}

#define SD_PARSE_STRING_PROP(node, prop_name, str, prop) {\
	if (!of_property_read_string(node, prop_name, &str)) {\
		strcpy(prop, str);\
		if (DEBUG_SD_OF) {\
			pr_info("get property:%25s, str:%s\n",\
				prop_name, prop);	\
		} \
	} \
}

#define SD_CAPS(a, b) { .caps = a, .name = b }

struct sd_caps {
	unsigned caps;
	const char *name;
};

extern int storage_flag;

extern int sdio_reset_comm(struct mmc_card *card);
extern void aml_debug_print_buf(char *buf, int size);
extern int aml_buf_verify(int *buf, int blocks, int lba);
void aml_mmc_ver_msg_show(void);
extern void aml_sdhc_init_debugfs(struct mmc_host *mmc);
void aml_sdhc_print_reg_(u32 *buf);
extern void aml_sdhc_print_reg(struct amlsd_host *host);
extern void aml_sdio_init_debugfs(struct mmc_host *mmc);
extern void aml_sd_emmc_init_debugfs(struct mmc_host *mmc);
extern void aml_sdio_print_reg(struct amlsd_host *host);
extern void aml_sd_emmc_print_reg(struct amlsd_host *host);

extern int add_part_table(struct mtd_partition *part, unsigned int nr_part);
extern int add_emmc_partition(struct gendisk *disk);
extern size_t aml_sg_copy_buffer(struct scatterlist *sgl, unsigned int nents,
			     void *buf, size_t buflen, int to_buffer);

int amlsd_get_platform_data(struct platform_device *pdev,
		struct amlsd_platform *pdata,
		struct mmc_host *mmc, u32 index);

int amlsd_get_reg_base(struct platform_device *pdev,
				struct amlsd_host *host);

/* int of_amlsd_detect(struct amlsd_platform* pdata); */
void of_amlsd_irq_init(struct amlsd_platform *pdata);
void of_amlsd_pwr_prepare(struct amlsd_platform *pdata);
void of_amlsd_pwr_on(struct amlsd_platform *pdata);
void of_amlsd_pwr_off(struct amlsd_platform *pdata);
int of_amlsd_init(struct amlsd_platform *pdata);
void of_amlsd_xfer_pre(struct amlsd_platform *pdata);
void of_amlsd_xfer_post(struct amlsd_platform *pdata);
int of_amlsd_ro(struct amlsd_platform *pdata);

int aml_sd_uart_detect(struct amlsd_platform *pdata);
void aml_sd_uart_detect_clr(struct amlsd_platform *pdata);
irqreturn_t aml_sd_irq_cd(int irq, void *dev_id);
irqreturn_t aml_irq_cd_thread(int irq, void *data);
void aml_sduart_pre(struct amlsd_platform *pdata);
int aml_sd_voltage_switch(struct amlsd_platform *pdata, char signal_voltage);
int aml_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios);
int aml_check_unsupport_cmd(struct mmc_host *mmc, struct mmc_request *mrq);

 /* chip select high */
void aml_cs_high(struct amlsd_platform *pdata);

/* chip select don't care */
void aml_cs_dont_care(struct amlsd_platform *pdata);

 /* is eMMC/tSD exist */
bool is_emmc_exist(struct amlsd_host *host);
void aml_devm_pinctrl_put(struct amlsd_host *host);
/* void of_init_pins (struct amlsd_platform* pdata); */
extern void aml_emmc_hw_reset(struct mmc_host *mmc);

void aml_snprint (char **pp, int *left_size,  const char *fmt, ...);

void aml_dbg_print_pinmux(void);
#ifdef CONFIG_MMC_AML_DEBUG
void aml_dbg_verify_pull_up(struct amlsd_platform *pdata);
int aml_dbg_verify_pinmux(struct amlsd_platform *pdata);
#endif

#endif

