/*
 * /include/linux/amlogic/amvecm/amvecm.h
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


#ifndef AMVECM_H
#define AMVECM_H

#include "linux/amlogic/amvecm/ve.h"
#include "linux/amlogic/amvecm/cm.h"
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/cpu_version.h>

/* struct ve_dnlp_s          video_ve_dnlp; */

#define FLAG_RSV31              (1 << 31)
#define FLAG_VADJ1_COLOR        (1 << 30)
#define FLAG_VE_DNLP            (1 << 29)
#define FLAG_VE_NEW_DNLP        (1 << 28)
#define FLAG_RSV27              (1 << 27)
#define FLAG_RSV26              (1 << 26)
#define FLAG_3D_BLACK_DIS       (1 << 25)
#define FLAG_3D_BLACK_EN        (1 << 24)
#define FLAG_3D_SYNC_DIS        (1 << 23)
#define FLAG_3D_SYNC_EN         (1 << 22)
#define FLAG_VLOCK_DIS          (1 << 21)
#define FLAG_VLOCK_EN          (1 << 20)
#define FLAG_VE_DNLP_EN         (1 << 19)
#define FLAG_VE_DNLP_DIS        (1 << 18)
#define FLAG_VADJ1_CON			(1 << 17)
#define FLAG_VADJ1_BRI			(1 << 16)
#define FLAG_GAMMA_TABLE_EN     (1 << 15)
#define FLAG_GAMMA_TABLE_DIS    (1 << 14)
#define FLAG_GAMMA_TABLE_R      (1 << 13)
#define FLAG_GAMMA_TABLE_G      (1 << 12)
#define FLAG_GAMMA_TABLE_B      (1 << 11)
#define FLAG_RGB_OGO            (1 << 10)
#define FLAG_RSV9               (1 <<  9)
#define FLAG_MATRIX_UPDATE      (1 <<  8)
#define FLAG_BRI_CON            (1 <<  7)
#define FLAG_LVDS_FREQ_SW       (1 <<  6)
#define FLAG_REG_MAP5           (1 <<  5)
#define FLAG_REG_MAP4           (1 <<  4)
#define FLAG_REG_MAP3           (1 <<  3)
#define FLAG_REG_MAP2           (1 <<  2)
#define FLAG_REG_MAP1           (1 <<  1)
#define FLAG_REG_MAP0           (1 <<  0)

#define VPP_VADJ2_BLMINUS_EN        (1 << 3)
#define VPP_VADJ2_EN                (1 << 2)
#define VPP_VADJ1_BLMINUS_EN        (1 << 1)
#define VPP_VADJ1_EN                (1 << 0)

#define VPP_DEMO_DNLP_DIS           (1 << 3)
#define VPP_DEMO_DNLP_EN            (1 << 2)
#define VPP_DEMO_CM_DIS             (1 << 1)
#define VPP_DEMO_CM_EN              (1 << 0)

/*PQ USER LATCH*/
#define PQ_USER_SR1_DERING_DIS		(1 << 11)
#define PQ_USER_SR1_DERING_EN		(1 << 10)
#define PQ_USER_SR0_DERING_DIS		(1 << 9)
#define PQ_USER_SR0_DERING_EN		(1 << 8)
#define PQ_USER_SR1_PK_DIS			(1 << 7)
#define PQ_USER_SR1_PK_EN			(1 << 6)
#define PQ_USER_SR0_PK_DIS			(1 << 5)
#define PQ_USER_SR0_PK_EN			(1 << 4)
#define PQ_USER_BLK_SLOPE			(1 << 3)
#define PQ_USER_BLK_START			(1 << 2)
#define PQ_USER_BLK_DIS			(1 << 1)
#define PQ_USER_BLK_EN				(1 << 0)

/*white balance latch*/
#define MTX_BYPASS_RGB_OGO			(1 << 0)
#define MTX_RGB2YUVL_RGB_OGO		(1 << 1)

enum pq_table_name_e {
	TABLE_NAME_SHARPNESS0 = 0x1,/*in vpp*/
	TABLE_NAME_SHARPNESS1 = 0x2,/*in vpp*/
	TABLE_NAME_DNLP = 0x4,		/*in vpp*/
	TABLE_NAME_CM = 0x8,		/*in vpp*/
	TABLE_NAME_BLK_BLUE_EXT = 0x10,/*in vpp*/
	TABLE_NAME_BRIGHTNESS = 0x20,/*in vpp*/
	TABLE_NAME_CONTRAST = 0x40,	/*in vpp*/
	TABLE_NAME_SATURATION_HUE = 0x80,/*in vpp*/
	TABLE_NAME_CVD2 = 0x100,		/*in tvafe*/
	TABLE_NAME_DI = 0x200,		/*in di*/
	TABLE_NAME_NR = 0x400,		/*in di*/
	TABLE_NAME_MCDI = 0x800,	/*in di*/
	TABLE_NAME_DEBLOCK = 0x1000,	/*in di*/
	TABLE_NAME_DEMOSQUITO = 0x2000,/*in di*/
	TABLE_NAME_WB = 0X4000,		/*in vpp*/
	TABLE_NAME_GAMMA = 0X8000,	/*in vpp*/
	TABLE_NAME_XVYCC = 0x10000,	/*in vpp*/
	TABLE_NAME_HDR = 0x20000,	/*in vpp*/
	TABLE_NAME_DOLBY_VISION = 0x40000,/*in vpp*/
	TABLE_NAME_RESERVED1 = 0x80000,
	TABLE_NAME_RESERVED2 = 0x100000,
	TABLE_NAME_RESERVED3 = 0x200000,
	TABLE_NAME_RESERVED4 = 0x400000,
	TABLE_NAME_RESERVED5 = 0x800000,
	TABLE_NAME_MAX,
};

#define _VE_CM  'C'

#define AMVECM_IOC_VE_DNLP      _IOW(_VE_CM, 0x21, struct ve_dnlp_s)
#define AMVECM_IOC_G_HIST_AVG   _IOW(_VE_CM, 0x22, struct ve_hist_s)
#define AMVECM_IOC_VE_DNLP_EN   _IO(_VE_CM, 0x23)
#define AMVECM_IOC_VE_DNLP_DIS  _IO(_VE_CM, 0x24)
#define AMVECM_IOC_VE_NEW_DNLP  _IOW(_VE_CM, 0x25, struct ve_dnlp_table_s)
#define AMVECM_IOC_G_HIST_BIN   _IOW(_VE_CM, 0x26, struct vpp_hist_param_s)
#define AMVECM_IOC_G_HDR_METADATA _IOW(_VE_CM, 0x27, struct hdr_metadata_info_s)


/* VPP.CM IOCTL command list */
#define AMVECM_IOC_LOAD_REG  _IOW(_VE_CM, 0x30, struct am_regs_s)


/* VPP.GAMMA IOCTL command list */
#define AMVECM_IOC_GAMMA_TABLE_EN  _IO(_VE_CM, 0x40)
#define AMVECM_IOC_GAMMA_TABLE_DIS _IO(_VE_CM, 0x41)
#define AMVECM_IOC_GAMMA_TABLE_R _IOW(_VE_CM, 0x42, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_G _IOW(_VE_CM, 0x43, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_B _IOW(_VE_CM, 0x44, struct tcon_gamma_table_s)
#define AMVECM_IOC_S_RGB_OGO   _IOW(_VE_CM, 0x45, struct tcon_rgb_ogo_s)
#define AMVECM_IOC_G_RGB_OGO  _IOR(_VE_CM, 0x46, struct tcon_rgb_ogo_s)

/*VPP.VLOCK IOCTL command list*/
#define AMVECM_IOC_VLOCK_EN  _IO(_VE_CM, 0x47)
#define AMVECM_IOC_VLOCK_DIS _IO(_VE_CM, 0x48)

/*VPP.3D-SYNC IOCTL command list*/
#define AMVECM_IOC_3D_SYNC_EN  _IO(_VE_CM, 0x49)
#define AMVECM_IOC_3D_SYNC_DIS _IO(_VE_CM, 0x50)

#define _DI_	'D'

struct am_pq_parm_s {
	unsigned int table_name;
	unsigned int table_len;
	void *table_ptr;
	void *reserved;
};

#define AMDI_IOC_SET_PQ_PARM  _IOW(_DI_, 0x51, struct am_pq_parm_s)

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8) */
/* #define WRITE_VPP_REG(x,val)
 * WRITE_VCBUS_REG(x,val) */
/* #define WRITE_VPP_REG_BITS(x,val,start,length)
 * WRITE_VCBUS_REG_BITS(x,val,start,length) */
/* #define READ_VPP_REG(x)
 * READ_VCBUS_REG(x) */
/* #define READ_VPP_REG_BITS(x,start,length)
 * READ_VCBUS_REG_BITS(x,start,length) */
/* #else */
/* #define WRITE_VPP_REG(x,val)
 * WRITE_CBUS_REG(x,val) */
/* #define WRITE_VPP_REG_BITS(x,val,start,length)
 * WRITE_CBUS_REG_BITS(x,val,start,length) */
/* #define READ_VPP_REG(x)
 * READ_CBUS_REG(x) */
/* #define READ_VPP_REG_BITS(x,start,length)
 * READ_CBUS_REG_BITS(x,start,length) */
/* #endif */


static inline void WRITE_VPP_REG(uint32_t reg,
		const uint32_t value)
{
	aml_write_vcbus(reg, value);
}

static inline uint32_t READ_VPP_REG(uint32_t reg)
{
	return aml_read_vcbus(reg);
}

static inline void WRITE_VPP_REG_BITS(uint32_t reg,
		const uint32_t value,
		const uint32_t start,
		const uint32_t len)
{
	WRITE_VPP_REG(reg, ((READ_VPP_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t READ_VPP_REG_BITS(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((READ_VPP_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

extern signed int vd1_brightness, vd1_contrast;
extern bool gamma_en;

#define CSC_FLAG_TOGGLE_FRAME	1
#define CSC_FLAG_CHECK_OUTPUT	2

extern int amvecm_on_vs(
	struct vframe_s *display_vf,
	struct vframe_s *toggle_vf,
	int flags);
extern void refresh_on_vs(struct vframe_s *vf);
extern void pc_mode_process(void);
extern void pq_user_latch_process(void);

/* master_display_info for display device */
struct hdr_metadata_info_s {
	u32 primaries[3][2];		/* normalized 50000 in G,B,R order */
	u32 white_point[2];		/* normalized 50000 */
	u32 luminance[2];		/* max/min lumin, normalized 10000 */
};

extern void vpp_vd_adj1_saturation_hue(signed int sat_val,
	signed int hue_val, struct vframe_s *vf);

extern int metadata_read_u32(uint32_t *value);
extern int metadata_wait(struct vframe_s *vf);
extern int metadata_sync(uint32_t frame_id, uint64_t pts);

extern void enable_dolby_vision(int enable);
extern bool is_dolby_vision_enable(void);
extern bool is_dolby_vision_on(void);
extern bool for_dolby_vision_certification(void);
extern void set_dolby_vision_mode(int mode);
extern int get_dolby_vision_mode(void);
extern void dolby_vision_set_toggle_flag(int flag);
extern int dolby_vision_wait_metadata(struct vframe_s *vf);
extern int dolby_vision_pop_metadata(void);
extern int dolby_vision_update_metadata(struct vframe_s *vf);
extern int dolby_vision_process(struct vframe_s *vf, u32 display_size);
extern void dolby_vision_init_receiver(void *pdev);
extern void dolby_vision_vf_put(struct vframe_s *vf);
extern struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf);
extern void dolby_vision_dump_setting(int debug_flag);
extern void dolby_vision_dump_struct(void);
extern void enable_osd_path(int on, int shadow_mode);
extern void tv_dolby_vision_config(int);
extern void dolby_vision_update_pq_config(
	char *pq_config_buf);
extern int dolby_vision_update_setting(void);
extern bool is_dolby_vision_stb_mode(void);
extern void tv_dolby_vision_crc_clear(int flag);
extern char *tv_dolby_vision_get_crc(u32 *len);
extern void tv_dolby_vision_insert_crc(bool print);
extern int dolby_vision_check_hdr10(struct vframe_s *vf);
extern void tv_dolby_vision_dma_table_modify(
	u32 tbl_id, uint64_t value);
extern void tv_dolby_vision_efuse_info(void);
extern int dolby_vision_parse_metadata(
	struct vframe_s *vf, u8 toggle_mode, bool bypass_release);
extern void dolby_vision_update_vsvdb_config(
	char *vsvdb_buf, u32 tbl_size);
extern void tv_dolby_vision_el_info(void);

extern void amvecm_wakeup_queue(void);

extern int enable_rgb_to_yuv_matrix_for_dvll(
	int32_t on, uint32_t *coeff_orig, uint32_t bits);

#endif /* AMVECM_H */

