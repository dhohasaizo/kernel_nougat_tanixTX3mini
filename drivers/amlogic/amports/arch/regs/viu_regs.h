/*
 * drivers/amlogic/amports/arch/regs/viu_regs.h
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

#ifndef VIU_REGS_HEADER_
#define VIU_REGS_HEADER_



#define VIU_ADDR_START 0x1a00
#define VIU_ADDR_END 0x1aff
#define VIU_SW_RESET 0x1a01
#define VIU_MISC_CTRL0 0x1a06
#define VIU_MISC_CTRL1 0x1a07
#define D2D3_INTF_LENGTH 0x1a08
#define D2D3_INTF_CTRL0 0x1a09
#define VIU_OSD1_CTRL_STAT 0x1a10
#define VIU_OSD1_CTRL_STAT2 0x1a2d
#define VIU_OSD1_COLOR_ADDR 0x1a11
#define VIU_OSD1_COLOR 0x1a12
#define VIU_OSD1_TCOLOR_AG0 0x1a17
#define VIU_OSD1_TCOLOR_AG1 0x1a18
#define VIU_OSD1_TCOLOR_AG2 0x1a19
#define VIU_OSD1_TCOLOR_AG3 0x1a1a
#define VIU_OSD1_BLK0_CFG_W0 0x1a1b
#define VIU_OSD1_BLK1_CFG_W0 0x1a1f
#define VIU_OSD1_BLK2_CFG_W0 0x1a23
#define VIU_OSD1_BLK3_CFG_W0 0x1a27
#define VIU_OSD1_BLK0_CFG_W1 0x1a1c
#define VIU_OSD1_BLK1_CFG_W1 0x1a20
#define VIU_OSD1_BLK2_CFG_W1 0x1a24
#define VIU_OSD1_BLK3_CFG_W1 0x1a28
#define VIU_OSD1_BLK0_CFG_W2 0x1a1d
#define VIU_OSD1_BLK1_CFG_W2 0x1a21
#define VIU_OSD1_BLK2_CFG_W2 0x1a25
#define VIU_OSD1_BLK3_CFG_W2 0x1a29
#define VIU_OSD1_BLK0_CFG_W3 0x1a1e
#define VIU_OSD1_BLK1_CFG_W3 0x1a22
#define VIU_OSD1_BLK2_CFG_W3 0x1a26
#define VIU_OSD1_BLK3_CFG_W3 0x1a2a
#define VIU_OSD1_BLK0_CFG_W4 0x1a13
#define VIU_OSD1_BLK1_CFG_W4 0x1a14
#define VIU_OSD1_BLK2_CFG_W4 0x1a15
#define VIU_OSD1_BLK3_CFG_W4 0x1a16
#define VIU_OSD1_FIFO_CTRL_STAT 0x1a2b
#define VIU_OSD1_TEST_RDDATA 0x1a2c
#define VIU_OSD1_PROT_CTRL 0x1a2e
#define VIU_OSD2_CTRL_STAT 0x1a30
#define VIU_OSD2_CTRL_STAT2 0x1a4d
#define VIU_OSD2_COLOR_ADDR 0x1a31
#define VIU_OSD2_COLOR 0x1a32
#define VIU_OSD2_HL1_H_START_END 0x1a33
#define VIU_OSD2_HL1_V_START_END 0x1a34
#define VIU_OSD2_HL2_H_START_END 0x1a35
#define VIU_OSD2_HL2_V_START_END 0x1a36
#define VIU_OSD2_TCOLOR_AG0 0x1a37
#define VIU_OSD2_TCOLOR_AG1 0x1a38
#define VIU_OSD2_TCOLOR_AG2 0x1a39
#define VIU_OSD2_TCOLOR_AG3 0x1a3a
#define VIU_OSD2_BLK0_CFG_W0 0x1a3b
#define VIU_OSD2_BLK1_CFG_W0 0x1a3f
#define VIU_OSD2_BLK2_CFG_W0 0x1a43
#define VIU_OSD2_BLK3_CFG_W0 0x1a47
#define VIU_OSD2_BLK0_CFG_W1 0x1a3c
#define VIU_OSD2_BLK1_CFG_W1 0x1a40
#define VIU_OSD2_BLK2_CFG_W1 0x1a44
#define VIU_OSD2_BLK3_CFG_W1 0x1a48
#define VIU_OSD2_BLK0_CFG_W2 0x1a3d
#define VIU_OSD2_BLK1_CFG_W2 0x1a41
#define VIU_OSD2_BLK2_CFG_W2 0x1a45
#define VIU_OSD2_BLK3_CFG_W2 0x1a49
#define VIU_OSD2_BLK0_CFG_W3 0x1a3e
#define VIU_OSD2_BLK1_CFG_W3 0x1a42
#define VIU_OSD2_BLK2_CFG_W3 0x1a46
#define VIU_OSD2_BLK3_CFG_W3 0x1a4a
#define VIU_OSD2_BLK0_CFG_W4 0x1a64
#define VIU_OSD2_BLK1_CFG_W4 0x1a65
#define VIU_OSD2_BLK2_CFG_W4 0x1a66
#define VIU_OSD2_BLK3_CFG_W4 0x1a67
#define VIU_OSD2_FIFO_CTRL_STAT 0x1a4b
#define VIU_OSD2_TEST_RDDATA 0x1a4c
#define VIU_OSD2_PROT_CTRL 0x1a4e
#define VD1_IF0_GEN_REG 0x1a50
#define VD1_IF0_CANVAS0 0x1a51
#define VD1_IF0_CANVAS1 0x1a52
#define VD1_IF0_LUMA_X0 0x1a53
#define VD1_IF0_LUMA_Y0 0x1a54
#define VD1_IF0_CHROMA_X0 0x1a55
#define VD1_IF0_CHROMA_Y0 0x1a56
#define VD1_IF0_LUMA_X1 0x1a57
#define VD1_IF0_LUMA_Y1 0x1a58
#define VD1_IF0_CHROMA_X1 0x1a59
#define VD1_IF0_CHROMA_Y1 0x1a5a
#define VD1_IF0_RPT_LOOP 0x1a5b
#define VD1_IF0_LUMA0_RPT_PAT 0x1a5c
#define VD1_IF0_CHROMA0_RPT_PAT 0x1a5d
#define VD1_IF0_LUMA1_RPT_PAT 0x1a5e
#define VD1_IF0_CHROMA1_RPT_PAT 0x1a5f
#define VD1_IF0_LUMA_PSEL 0x1a60
#define VD1_IF0_CHROMA_PSEL 0x1a61
#define VD1_IF0_DUMMY_PIXEL 0x1a62
#define VD1_IF0_LUMA_FIFO_SIZE 0x1a63
#define VD1_IF0_RANGE_MAP_Y 0x1a6a
#define VD1_IF0_RANGE_MAP_CB 0x1a6b
#define VD1_IF0_RANGE_MAP_CR 0x1a6c
#define VD1_IF0_GEN_REG2 0x1a6d
#define VD1_IF0_PROT_CNTL 0x1a6e
#define VIU_VD1_FMT_CTRL 0x1a68
#define VIU_VD1_FMT_W 0x1a69
#define VD2_IF0_GEN_REG 0x1a70
#define VD2_IF0_CANVAS0 0x1a71
#define VD2_IF0_CANVAS1 0x1a72
#define VD2_IF0_LUMA_X0 0x1a73
#define VD2_IF0_LUMA_Y0 0x1a74
#define VD2_IF0_CHROMA_X0 0x1a75
#define VD2_IF0_CHROMA_Y0 0x1a76
#define VD2_IF0_LUMA_X1 0x1a77
#define VD2_IF0_LUMA_Y1 0x1a78
#define VD2_IF0_CHROMA_X1 0x1a79
#define VD2_IF0_CHROMA_Y1 0x1a7a
#define VD2_IF0_RPT_LOOP 0x1a7b
#define VD2_IF0_LUMA0_RPT_PAT 0x1a7c
#define VD2_IF0_CHROMA0_RPT_PAT 0x1a7d
#define VD2_IF0_LUMA1_RPT_PAT 0x1a7e
#define VD2_IF0_CHROMA1_RPT_PAT 0x1a7f
#define VD2_IF0_LUMA_PSEL 0x1a80
#define VD2_IF0_CHROMA_PSEL 0x1a81
#define VD2_IF0_DUMMY_PIXEL 0x1a82
#define VD2_IF0_LUMA_FIFO_SIZE 0x1a83
#define VD2_IF0_RANGE_MAP_Y 0x1a8a
#define VD2_IF0_RANGE_MAP_CB 0x1a8b
#define VD2_IF0_RANGE_MAP_CR 0x1a8c
#define VD2_IF0_GEN_REG2 0x1a8d
#define VD2_IF0_PROT_CNTL 0x1a8e
#define VIU_VD2_FMT_CTRL 0x1a88
#define VIU_VD2_FMT_W 0x1a89
#if 0
#define LDIM_STTS_GCLK_CTRL0 0x1a90
#define LDIM_STTS_CTRL0 0x1a91
#define LDIM_STTS_WIDTHM1_HEIGHTM1 0x1a92
#define LDIM_STTS_MATRIX_COEF00_01 0x1a93
#define LDIM_STTS_MATRIX_COEF02_10 0x1a94
#define LDIM_STTS_MATRIX_COEF11_12 0x1a95
#define LDIM_STTS_MATRIX_COEF20_21 0x1a96
#define LDIM_STTS_MATRIX_COEF22 0x1a97
#define LDIM_STTS_MATRIX_OFFSET0_1 0x1a98
#define LDIM_STTS_MATRIX_OFFSET2 0x1a99
#define LDIM_STTS_MATRIX_PRE_OFFSET0_1 0x1a9a
#define LDIM_STTS_MATRIX_PRE_OFFSET2 0x1a9b
#define LDIM_STTS_MATRIX_HL_COLOR 0x1a9c
#define LDIM_STTS_MATRIX_PROBE_POS 0x1a9d
#define LDIM_STTS_MATRIX_PROBE_COLOR 0x1a9e
#define LDIM_STTS_HIST_REGION_IDX 0x1aa0
#define LDIM_STTS_HIST_SET_REGION 0x1aa1
#define LDIM_STTS_HIST_READ_REGION 0x1aa2
#endif
#define AFBC_ENABLE 0x1ae0
#define AFBC_MODE 0x1ae1
#define AFBC_SIZE_IN 0x1ae2
#define AFBC_DEC_DEF_COLOR 0x1ae3
#define AFBC_CONV_CTRL 0x1ae4
#define AFBC_LBUF_DEPTH 0x1ae5
#define AFBC_HEAD_BADDR 0x1ae6
#define AFBC_BODY_BADDR 0x1ae7
#define AFBC_SIZE_OUT     0x1ae8
#define AFBC_OUT_YSCOPE 0x1ae9
#define AFBC_STAT 0x1aea
#define AFBC_VD_CFMT_CTRL 0x1aeb
#define AFBC_VD_CFMT_W 0x1aec
#define AFBC_MIF_HOR_SCOPE 0x1aed
#define AFBC_MIF_VER_SCOPE 0x1aee
#define AFBC_PIXEL_HOR_SCOPE 0x1aef
#define AFBC_PIXEL_VER_SCOPE 0x1af0
#define AFBC_VD_CFMT_H 0x1af1
#define VD2_AFBC_ENABLE 0x3180
#define VD2_AFBC_MODE  0x3181
#define VD2_AFBC_SIZE_IN 0x3182
#define VD2_AFBC_DEC_DEF_COLOR 0x3183
#define VD2_AFBC_CONV_CTRL 0x3184
#define VD2_AFBC_LBUF_DEPTH 0x3185
#define VD2_AFBC_HEAD_BADDR 0x3186
#define VD2_AFBC_BODY_BADDR 0x3187
#define VD2_AFBC_SIZE_OUT   0x3188
#define VD2_AFBC_OUT_XSCOPE 0x3188
#define VD2_AFBC_OUT_YSCOPE 0x3189
#define VD2_AFBC_STAT 0x318a
#define VD2_AFBC_VD_CFMT_CTRL 0x318b
#define VD2_AFBC_VD_CFMT_W 0x318c
#define VD2_AFBC_MIF_HOR_SCOPE  0x318d
#define VD2_AFBC_MIF_VER_SCOPE 0x318e
#define VD2_AFBC_PIXEL_HOR_SCOPE 0x318f
#define VD2_AFBC_PIXEL_VER_SCOPE 0x3190
#define VD2_AFBC_VD_CFMT_H 0x3191

#define VD2_IF0_GEN_REG3 0x1aa8

#endif

