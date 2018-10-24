#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/video.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/amports/amstream.h>
#include "arch/vpp_regs.h"
#include "arch/vpp_hdr_regs.h"
#include "arch/vpp_dolbyvision_regs.h"
#include "dolby_vision/dolby_vision.h"

#include <linux/cma.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>

DEFINE_SPINLOCK(dovi_lock);

static const struct dolby_vision_func_s *p_funcs;

#define DOLBY_VISION_OUTPUT_MODE_IPT			0
#define DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL		1
#define DOLBY_VISION_OUTPUT_MODE_HDR10			2
#define DOLBY_VISION_OUTPUT_MODE_SDR10			3
#define DOLBY_VISION_OUTPUT_MODE_SDR8			4
#define DOLBY_VISION_OUTPUT_MODE_BYPASS			5
static unsigned int dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mode, "\n dolby_vision_mode\n");

/* STB: if sink support DV, always output DV
		else always output SDR/HDR */
/* TV:  when source is DV, convert to SDR */
#define DOLBY_VISION_FOLLOW_SINK		0

/* STB: output DV only if source is DV
		and sink support DV
		else always output SDR/HDR */
/* TV:  when source is DV or HDR, convert to SDR */
#define DOLBY_VISION_FOLLOW_SOURCE		1

/* STB: always follow dolby_vision_mode */
/* TV:  if set dolby_vision_mode to SDR8,
		convert all format to SDR by TV core,
		else bypass Dolby Vision */
#define DOLBY_VISION_FORCE_OUTPUT_MODE	2

static unsigned int dolby_vision_policy;
module_param(dolby_vision_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_policy, "\n dolby_vision_policy\n");

/* bit0: 0: bypass hdr to vpp, 1: process hdr by dolby core */
/* bit1: 0: output to sdr, 1: output to hdr if sink support hdr not dovi */
static unsigned int dolby_vision_hdr10_policy;
module_param(dolby_vision_hdr10_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_hdr10_policy, "\n dolby_vision_hdr10_policy\n");

static bool dolby_vision_enable;
module_param(dolby_vision_enable, bool, 0664);
MODULE_PARM_DESC(dolby_vision_enable, "\n dolby_vision_enable\n");

static bool force_stb_mode;

static bool dolby_vision_efuse_bypass;
module_param(dolby_vision_efuse_bypass, bool, 0664);
MODULE_PARM_DESC(dolby_vision_efuse_bypass, "\n dolby_vision_efuse_bypass\n");
static bool efuse_mode;

static bool el_mode;
module_param(force_stb_mode, bool, 0664);
MODULE_PARM_DESC(force_stb_mode, "\n force_stb_mode\n");

static uint dolby_vision_mask = 7;
module_param(dolby_vision_mask, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mask, "\n dolby_vision_mask\n");

#define BYPASS_PROCESS 0
#define SDR_PROCESS 1
#define HDR_PROCESS 2
#define DV_PROCESS 3
static uint dolby_vision_status;
module_param(dolby_vision_status, uint, 0664);
MODULE_PARM_DESC(dolby_vision_status, "\n dolby_vision_status\n");

/* delay before first frame toggle when core off->on */
static uint dolby_vision_wait_delay;
module_param(dolby_vision_wait_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_wait_delay, "\n dolby_vision_wait_delay\n");
static int dolby_vision_wait_count;

/* reset 1st fake frame (bit 0)
   and other fake frames (bit 1)
   and other toggle frames (bit 2) */
static uint dolby_vision_reset = (1 << 1) | (1 << 0);
module_param(dolby_vision_reset, uint, 0664);
MODULE_PARM_DESC(dolby_vision_reset, "\n dolby_vision_reset\n");

/* force run mode */
static uint dolby_vision_run_mode = 0xff; /* not force */
module_param(dolby_vision_run_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_run_mode, "\n dolby_vision_run_mode\n");

/* number of fake frame (run mode = 1) */
#define RUN_MODE_DELAY 2
static uint dolby_vision_run_mode_delay = RUN_MODE_DELAY;
module_param(dolby_vision_run_mode_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_run_mode_delay, "\n dolby_vision_run_mode_delay\n");

/* reset control -- end << 8 | start */
static uint dolby_vision_reset_delay =
	(RUN_MODE_DELAY << 8) | RUN_MODE_DELAY;
module_param(dolby_vision_reset_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_reset_delay, "\n dolby_vision_reset_delay\n");

static unsigned int dolby_vision_tunning_mode;
module_param(dolby_vision_tunning_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_tunning_mode, "\n dolby_vision_tunning_mode\n");

#ifdef V2_4
static unsigned int dv_ll_output_mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
module_param(dv_ll_output_mode, uint, 0664);
MODULE_PARM_DESC(dv_ll_output_mode, "\n dv_ll_output_mode\n");

#define DOLBY_VISION_LL_DISABLE		0
#define DOLBY_VISION_LL_YUV422				1
#define DOLBY_VISION_LL_RGB444				2
static u32 dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;
module_param(dolby_vision_ll_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_ll_policy, "\n dolby_vision_ll_policy\n");
static u32 last_dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;
#endif

static uint dolby_vision_on_count;

#define FLAG_BYPASS_CVM				0x02
#define FLAG_BYPASS_VPP				0x04
#define FLAG_USE_SINK_MIN_MAX		0x08
#define FLAG_CLKGATE_WHEN_LOAD_LUT	0x10
#define FLAG_SINGLE_STEP			0x20
#define FLAG_CERTIFICAION			0x40
#define FLAG_CHANGE_SEQ_HEAD		0x80
#define FLAG_DISABLE_COMPOSER		0x100
#define FLAG_BYPASS_CSC				0x200
#define FLAG_CHECK_ES_PTS			0x400
#define FLAG_DISABE_CORE_SETTING	0x800
#define FLAG_DISABLE_DMA_UPDATE		0x1000
#define FLAG_DISABLE_DOVI_OUT		0x2000
#define FLAG_FORCE_DOVI_LL			0x4000
#define FLAG_FORCE_RGB_OUTPUT		0x8000
/* #define FLAG_DOVI_LL_RGB_DESIRED	0x8000 */
#define FLAG_DOVI2HDR10_NOMAPPING 0x100000
#define FLAG_PRIORITY_GRAPHIC	 0x200000
#define FLAG_DISABLE_LOAD_VSVDB	 0x400000
#define FLAG_DISABLE_CRC	 0x800000
#define FLAG_TOGGLE_FRAME			0x80000000

#define FLAG_FRAME_DELAY_MASK	0xf
#define FLAG_FRAME_DELAY_SHIFT	16

static unsigned int dolby_vision_flags = FLAG_BYPASS_VPP;
module_param(dolby_vision_flags, uint, 0664);
MODULE_PARM_DESC(dolby_vision_flags, "\n dolby_vision_flags\n");

static unsigned int htotal_add = 0x140;
static unsigned int vtotal_add = 0x40;
static unsigned int vsize_add;
static unsigned int vwidth = 0x8;
static unsigned int hwidth = 0x8;
static unsigned int vpotch = 0x10;
static unsigned int hpotch = 0x8;
static unsigned int g_htotal_add = 0x40;
static unsigned int g_vtotal_add = 0x80;
static unsigned int g_vsize_add;
static unsigned int g_vwidth = 0x18;
static unsigned int g_hwidth = 0x10;
static unsigned int g_vpotch = 0x8;
static unsigned int g_hpotch = 0x10;
/*dma size:1877x2x64 bit = 30032 byte*/
#define TV_DMA_TBL_SIZE 3754
static unsigned int dma_size = 30032;
static dma_addr_t dma_paddr;
static void *dma_vaddr;
static unsigned int dma_start_line = 0x400;

#define CRC_BUFF_SIZE (256 * 1024)
static char *crc_output_buf;
static u32 crc_outpuf_buff_off;
static u32 crc_count;
static u32 crc_bypass_count;
static u32 setting_update_count;
static s32 crc_read_delay;
static u32 core1_disp_hsize;
static u32 core1_disp_vsize;
static u32 vsync_count;
#define FLAG_VSYNC_CNT 10

static bool is_osd_off;
static bool force_reset_core2;

module_param(vtotal_add, uint, 0664);
MODULE_PARM_DESC(vtotal_add, "\n vtotal_add\n");
module_param(vpotch, uint, 0664);
MODULE_PARM_DESC(vpotch, "\n vpotch\n");

static unsigned int dolby_vision_target_min = 50; /* 0.0001 */
#ifdef V2_4
static unsigned int dolby_vision_target_max[3][3] = {
	{ 4000, 1000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};
#else
static unsigned int dolby_vision_target_max[3][3] = {
	{ 4000, 4000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};
#endif

static unsigned int dolby_vision_default_max[3][3] = {
	{ 4000, 4000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_graphic_min = 50; /* 0.0001 */
static unsigned int dolby_vision_graphic_max = 100; /* 1 */
module_param(dolby_vision_graphic_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_min, "\n dolby_vision_graphic_min\n");
module_param(dolby_vision_graphic_max, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_max, "\n dolby_vision_graphic_max\n");

static unsigned int dv_cert_graphic_width = 1920;
static unsigned int dv_cert_graphic_height = 1080;
module_param(dv_cert_graphic_width, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_width, "\n dv_cert_graphic_width\n");
module_param(dv_cert_graphic_height, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_height, "\n dv_cert_graphic_height\n");

/* 0: video priority 1: graphic priority */
static unsigned int dolby_vision_graphics_priority;
module_param(dolby_vision_graphics_priority, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphics_priority, "\n dolby_vision_graphics_priority\n");

uint16_t L2PQ_100_500[] = {
	2081, /* 100 */
	2157, /* 120 */
	2221, /* 140 */
	2277, /* 160 */
	2327, /* 180 */
	2372, /* 200 */
	2413, /* 220 */
	2450, /* 240 */
	2485, /* 260 */
	2517, /* 280 */
	2547, /* 300 */
	2575, /* 320 */
	2602, /* 340 */
	2627, /* 360 */
	2651, /* 380 */
	2673, /* 400 */
	2694, /* 420 */
	2715, /* 440 */
	2734, /* 460 */
	2753, /* 480 */
	2771, /* 500 */
};

uint16_t L2PQ_500_4000[] = {
	2771, /* 500 */
	2852, /* 600 */
	2920, /* 700 */
	2980, /* 800 */
	3032, /* 900 */
	3079, /* 1000 */
	3122, /* 1100 */
	3161, /* 1200 */
	3197, /* 1300 */
	3230, /* 1400 */
	3261, /* 1500 */
	3289, /* 1600 */
	3317, /* 1700 */
	3342, /* 1800 */
	3366, /* 1900 */
	3389, /* 2000 */
	3411, /* 2100 */
	3432, /* 2200 */
	3451, /* 2300 */
	3470, /* 2400 */
	3489, /* 2500 */
	3506, /* 2600 */
	3523, /* 2700 */
	3539, /* 2800 */
	3554, /* 2900 */
	3570, /* 3000 */
	3584, /* 3100 */
	3598, /* 3200 */
	3612, /* 3300 */
	3625, /* 3400 */
	3638, /* 3500 */
	3651, /* 3600 */
	3662, /* 3700 */
	3674, /* 3800 */
	3686, /* 3900 */
	3697, /* 4000 */
};

static uint32_t tv_max_lin = 200;
static uint16_t tv_max_pq = 2372;

static unsigned int panel_max_lumin;
module_param(panel_max_lumin, uint, 0664);
MODULE_PARM_DESC(panel_max_lumin, "\n panel_max_lumin\n");

#ifdef V1_5
struct TargetDisplayConfig def_tgt_display_cfg = {
	4095, /* gain */
	0, /* offset */
	39322, /* gamma */
	0, /* eotf */
	12, /* bitDepth */
	0, /* rangeSpec */
	42, /* diagSize */
	2771, /* maxPq */
	62, /* minPq */
	2048, /* mSWeight */
	16380, /* mSEdgeWeight */
	0, /* minPQBias */
	0, /* midPQBias */
	0, /* maxPQBias */
	0, /* trimSlopeBias */
	0, /* trimOffsetBias */
	0, /* trimPowerBias */
	0, /* msWeightBias */
	0, /* brightness */
	0, /* contrast */
	0, /* chromaWeightBias */
	0, /* saturationGainBias */
	2048, /* chromaWeight */
	2048, /* saturationGain */
	655, /* crossTalk */
	0, /* tuningMode */
	0, /* reserved0 */
	0, /* dbgExecParamsPrintPeriod */
	0, /* dbgDmMdPrintPeriod */
	0, /* dbgDmCfgPrintPeriod */
	2771, /* maxPq_dupli */
	62, /* minPq_dupli */
	12288, /* keyWeight */
	24576, /* intensityVectorWeight */
	24576, /* chromaVectorWeight */
	0, /* chip_fpga_lowcomplex */
	{
		-245,
		-208,
		-171,
		-130,
		-93,
		-56,
		-19,
		20,
		57,
		94,
		131,
		172,
		209,
		246,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		-3685,
		-3070,
		-2456,
		-1842,
		-1228,
		-613,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		1, /* gdEnable */
		6553, /* gdWMin */
		131072000, /* gdWMax */
		26214400, /* gdWMm */
		16579442, /* gdWDynRngSqrt */
		4096, /* gdWeightMean */
		4096, /* gdWeightStd */
		0, /* gdDelayMilliSec_hdmi */
		1, /* gdRgb2YuvExt */
		{
			{5960, 20047, 2023},
			{-3286, -11052, 14336},
			{14336, -13022, -1316}
		}, /* gdM33Rgb2Yuv[3][3] */
		15, /* gdM33Rgb2YuvScale2P */
		1, /* gdRgb2YuvOffExt */
		{2048, 16384, 16384},/* gdV3Rgb2YuvOff[3] */
		2072430, /* gdUpBound */
		414486, /* gdLowBound */
		0, /* lastMaxPq */
		137, /*gdWMinPq */
		2771, /*gdWMaxPq */
		2081, /*gdWMmPq */
		0, /*gdTriggerPeriod */
		0, /* gdTriggerLinThresh */
		0, /* gdDelayMilliSec_ott */
#ifdef V1_5
		{0, 0, 0, 0, 0, 0},
#else
		{0, 0, 0, 0, 0, 0, 0, 0, 0}
#endif
	},
#ifdef V1_5
	{0, 0, 0, 68, 124, 49, 230},
	{0, 0, 0, 0, 0},
#endif
	1311, /* min_lin */
	131072000, /* max_lin */
	0, /* backlight_scaler */
	1311, /* min_lin_dupli */
	131072000, /* max_lin_dupli */
	{
		/* lms2RgbMat[3][3] */
		{
			{22416, -19015, 695},
			{-4609, 9392, -688},
			{122, -791, 4765}
		},
		12, /* lms2RgbMatScale */
		{128, 128, 128}, /* whitePoint */
		7, /* whitePointScale */
		{0, 0, 0} /* reserved[3] */
	},
	0, /* reserved00 */
	0, /* brightnessPreservation */
	81920, /* iintensityVectorWeight */
	24576, /* ichromaVectorWeight */
	0, /* isaturationGainBias */
	0, /* chip_12b_ocsc */
	0, /* chip_512_tonecurve */
	0, /* chip_nolvl5 */
	{0, 0, 0, 0, 0, 0, 0, 0} /* padding[8] */
};
#else
struct TargetDisplayConfig def_tgt_display_cfg = {
	2048, /* gain */
	4095, /* offset */
	39322, /* gamma */
	0, /* eotf */
	12, /* bitDepth */
	0, /* rangeSpec */
	42, /* diagSize */
	2372, /* maxPq */
	62, /* minPq */
	2048, /* mSWeight */
	16380, /* mSEdgeWeight */
	0, /* minPQBias */
	0, /* midPQBias */
	0, /* maxPQBias */
	0, /* trimSlopeBias */
	0, /* trimOffsetBias */
	0, /* trimPowerBias */
	0, /* msWeightBias */
	0, /* brightness */
	0, /* contrast */
	0, /* chromaWeightBias */
	0, /* saturationGainBias */
	2048, /* chromaWeight */
	2048, /* saturationGain */
	655, /* crossTalk */
	0, /* tuningMode */
	0, /* reserved0 */
	0, /* dbgExecParamsPrintPeriod */
	0, /* dbgDmMdPrintPeriod */
	0, /* dbgDmCfgPrintPeriod */
	2372, /* maxPq_dupli */
	62, /* minPq_dupli */
	12288, /* keyWeight */
	24576, /* intensityVectorWeight */
	24576, /* chromaVectorWeight */
	0, /* chip_fpga_lowcomplex */
	{
		-245,
		-207,
		-169,
		-131,
		-94,
		-56,
		-18,
		19,
		57,
		95,
		132,
		170,
		208,
		246,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	{
		0, /* gdEnable */
		262, /* gdWMin */
		131072000, /* gdWMax */
		26214400, /* gdWMm */
		82897211, /* gdWDynRngSqrt */
		4096, /* gdWeightMean */
		4096, /* gdWeightStd */
		0, /* gdDelayMilliSec_hdmi */
		1, /* gdRgb2YuvExt */
		{
			{5960, 20047, 2023},
			{-3286, -11052, 14336},
			{14336, -13022, -1316}
		}, /* gdM33Rgb2Yuv[3][3] */
		15, /* gdM33Rgb2YuvScale2P */
		1, /* gdRgb2YuvOffExt */
		{2048, 16384, 16384},/* gdV3Rgb2YuvOff[3] */
		414486, /* gdUpBound */
		82897, /* gdLowBound */
		0, /* lastMaxPq */
		26, /*gdWMinPq */
		2771, /*gdWMaxPq */
		2081, /*gdWMmPq */
		0, /*gdTriggerPeriod */
		0, /* gdTriggerLinThresh */
		0, /* gdDelayMilliSec_ott */
#ifdef V1_5
		{0, 0, 0, 0, 0, 0},
#else
		{0, 0, 0, 0, 0, 0, 0, 0, 0}
#endif
	},
#ifdef V1_5
	{0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
#endif
	1311, /* min_lin */
	200 << 18, /* max_lin 131072000 */
	4096, /* backlight_scaler */
	1311, /* min_lin_dupli */
	200 << 18, /* max_lin_dupli 131072000 */
	{
		/* lms2RgbMat[3][3] */
		{
			{17507, -14019, 608},
			{-3765, 8450, -589},
			{285, -640, 4451}
		},
		12, /* lms2RgbMatScale */
		{128, 128, 128}, /* whitePoint */
		7, /* whitePointScale */
		{0, 0, 0} /* reserved[3] */
	},
	0, /* reserved00 */
	0, /* brightnessPreservation */
	81920, /* iintensityVectorWeight */
	24576, /* ichromaVectorWeight */
	0, /* isaturationGainBias */
	0, /* chip_12b_ocsc */
	0, /* chip_512_tonecurve */
	0, /* chip_nolvl5 */
	{0, 0, 0, 0, 0, 0, 0, 0} /* padding[8] */
};
#endif

static unsigned int debug_dolby;
module_param(debug_dolby, uint, 0664);
MODULE_PARM_DESC(debug_dolby, "\n debug_dolby\n");

static unsigned int debug_dolby_frame = 0xffff;
module_param(debug_dolby_frame, uint, 0664);
MODULE_PARM_DESC(debug_dolby_frame, "\n debug_dolby_frame\n");

#define pr_dolby_dbg(fmt, args...)\
	do {\
		if (debug_dolby)\
			pr_info("DOLBY: " fmt, ## args);\
	} while (0)
#define pr_dolby_error(fmt, args...)\
	pr_info("DOLBY ERROR: " fmt, ## args)
#define dump_enable \
	((debug_dolby_frame >= 0xffff) || (debug_dolby_frame == frame_count))
#define is_graphics_output_off() \
	(!(READ_VPP_REG(VPP_MISC) & (1<<12)))
#define single_step_enable \
	(((debug_dolby_frame >= 0xffff) || \
	((debug_dolby_frame + 1) == frame_count)) && \
	(debug_dolby & 0x80))

static bool dolby_vision_on;
static bool dolby_vision_core1_on;
static bool dolby_vision_wait_on;
static bool dolby_vision_wait_init;
static unsigned int frame_count;
static struct hdr10_param_s hdr10_param;
static struct master_display_info_s hdr10_data;

static char md_buf[1024];
static char comp_buf[8196];

static struct dovi_setting_s dovi_setting;
static struct dovi_setting_s new_dovi_setting;

static struct pq_config_s pq_config;
static bool pq_config_set_flag;
struct ui_menu_params_s menu_param = {50, 50, 50};
static struct tv_dovi_setting_s tv_dovi_setting;
static bool tv_dovi_setting_change_flag;
static bool tv_dovi_setting_update_flag;
static bool dovi_setting_video_flag;
static struct platform_device *dovi_pdev;
static bool vsvdb_config_set_flag;

/* general control config change */
#define FLAG_CHANGE_CFG		0x000001
/* general metadata change */
#define FLAG_CHANGE_MDS		0x000002
/* metadata config (below level 2) change */
#define FLAG_CHANGE_MDS_CFG	0x000004
/* global dimming change */
#define FLAG_CHANGE_GD			0x000008
/* tone curve lut change */
#define FLAG_CHANGE_TC			0x000010
/* 2nd tone curve (graphics) lut change */
#define FLAG_CHANGE_TC2		0x000020
/* L2NL lut change (for DM3.1) */
#define FLAG_CHANGE_L2NL		0x000040
/* 3D lut change (for DM2.12) */
#define FLAG_CHANGE_3DLUT		0x000080
/* 0xX00000 for other status */
/* (video) tone mapping to a constant */
#define FLAG_CONST_TC			0x100000
/* 2nd tone mapping to a constant */
#define FLAG_CONST_TC2			0x200000
#define FLAG_CHANGE_ALL		0xffffffff
/* update all core */
static u32 stb_core_setting_update_flag = FLAG_CHANGE_ALL;

/* 256+(256*4+256*2)*4/8 64bit */
#define STB_DMA_TBL_SIZE (256+(256*4+256*2)*4/8)
static uint64_t stb_core1_lut[STB_DMA_TBL_SIZE];

static void dump_tv_setting(
	struct tv_dovi_setting_s *setting,
	int frame_cnt, int debug_flag)
{
	int i;
	uint64_t *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("\nreg\n");
		p = (uint64_t *)&setting->core1_reg_lut[0];
		for (i = 0; i < 222; i += 2)
			pr_info("%016llx, %016llx,\n", p[i], p[i+1]);
	}
	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ng2l_lut\n");
		p = (uint64_t *)&setting->core1_reg_lut[0];
		for (i = 222; i < 222 + 256; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i+1]);
	}
	if ((debug_flag & 0x40) && dump_enable) {
		pr_info("\n3d_lut\n");
		p = (uint64_t *)&setting->core1_reg_lut[0];
		for (i = 222 + 256; i < 222 + 256 + 3276; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i+1]);
		pr_info("\n");
	}
}

void dolby_vision_update_pq_config(char *pq_config_buf)
{
	memcpy(&pq_config, pq_config_buf, sizeof(struct pq_config_s));
	pr_info("update_pq_config[%ld] %x %x %x %x\n",
		sizeof(struct pq_config_s),
		pq_config_buf[1],
		pq_config_buf[2],
		pq_config_buf[3],
		pq_config_buf[4]);
	pq_config_set_flag = true;
}
EXPORT_SYMBOL(dolby_vision_update_pq_config);

void dolby_vision_update_vsvdb_config(char *vsvdb_buf, u32 tbl_size)
{
#ifdef V2_4
	if (tbl_size > sizeof(new_dovi_setting.vsvdb_tbl)) {
		pr_info(
			"update_vsvdb_config tbl size overflow %d\n", tbl_size);
		return;
	}
	memset(&new_dovi_setting.vsvdb_tbl[0],
		0, sizeof(new_dovi_setting.vsvdb_tbl));
	memcpy(&new_dovi_setting.vsvdb_tbl[0],
		vsvdb_buf, tbl_size);
	new_dovi_setting.vsvdb_len = tbl_size;
	new_dovi_setting.vsvdb_changed = 1;
	dolby_vision_set_toggle_flag(1);
	if (tbl_size >= 8)
		pr_info(
		"update_vsvdb_config[%d] %x %x %x %x %x %x %x %x\n",
		tbl_size,
		vsvdb_buf[0],
		vsvdb_buf[1],
		vsvdb_buf[2],
		vsvdb_buf[3],
		vsvdb_buf[4],
		vsvdb_buf[5],
		vsvdb_buf[6],
		vsvdb_buf[7]);
#endif
	vsvdb_config_set_flag = true;
}
EXPORT_SYMBOL(dolby_vision_update_vsvdb_config);

static int prepare_stb_dolby_core1_reg(
	uint32_t run_mode,
	uint32_t *p_core1_dm_regs,
	uint32_t *p_core1_comp_regs)
{
	int index = 0;
	int i;

	/* 4 */
	stb_core1_lut[index++] = ((uint64_t)4 << 32) | 4;
	stb_core1_lut[index++] = ((uint64_t)4 << 32) | 4;
	stb_core1_lut[index++] = ((uint64_t)3 << 32) | 1;
	stb_core1_lut[index++] = ((uint64_t)2 << 32) | 1;

	/* 1 + 14 + 10 + 1 */
	stb_core1_lut[index++] = ((uint64_t)1 << 32) | run_mode;
	for (i = 0; i < 14; i++)
		stb_core1_lut[index++] =
			((uint64_t)(6 + i) << 32)
			| p_core1_dm_regs[i];
	for (i = 17; i < 27; i++)
		stb_core1_lut[index++] =
			((uint64_t)(6 + i) << 32)
			| p_core1_dm_regs[i-3];
	stb_core1_lut[index++] = ((uint64_t)(6 + 27) << 32) | 0;

	/* 173 + 1 */
	for (i = 0; i < 173; i++)
		stb_core1_lut[index++] =
			((uint64_t)(6 + 44 + i) << 32)
			| p_core1_comp_regs[i];
	stb_core1_lut[index++] = ((uint64_t)3 << 32) | 1;

	if (index & 1) {
		pr_dolby_error("stb core1 reg tbl odd size\n");
		stb_core1_lut[index++] = ((uint64_t)3 << 32) | 1;
	}
	return index;
}

static void prepare_stb_dolby_core1_lut(uint32_t base, uint32_t *p_core1_lut)
{
	uint32_t *p_lut;
	int i;

	p_lut = &p_core1_lut[256*4]; /* g2l */
	for (i = 0; i < 128; i++) {
		stb_core1_lut[base+i] =
		stb_core1_lut[base+i+128] =
			((uint64_t)p_lut[1] << 32) |
			((uint64_t)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
	p_lut = &p_core1_lut[0]; /* 4 lut */
	for (i = 256; i < 768; i++) {
		stb_core1_lut[base+i] =
			((uint64_t)p_lut[1] << 32) |
			((uint64_t)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
}

static int stb_dolby_core1_set(
	uint32_t dm_count,
	uint32_t comp_count,
	uint32_t lut_count,
	uint32_t *p_core1_dm_regs,
	uint32_t *p_core1_comp_regs,
	uint32_t *p_core1_lut,
	int hsize,
	int vsize,
	int bl_enable,
	int el_enable,
	int el_41_mode,
	int scramble_en,
	bool dovi_src,
	int lut_endian,
	bool reset)
{
	uint32_t bypass_flag = 0;
	int composer_enable = el_enable;
	uint32_t run_mode = 0;
	int reg_size = 0;
	bool bypass_core1 = (!hsize || !vsize
		|| !(dolby_vision_mask & 1));

	if (dolby_vision_on
		&& (dolby_vision_flags &
		FLAG_DISABE_CORE_SETTING))
		return 0;

	WRITE_VPP_REG(
		DOLBY_TV_CLKGATE_CTRL, 0x2800);
	if (reset) {
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 1 << 9);
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 0);
		VSYNC_WR_MPEG_REG(
			DOLBY_TV_CLKGATE_CTRL, 0x2800);
	}

	if (!bl_enable)
		VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL5, 0x446);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL0, 0);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL3,
		(hwidth << 16) | vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL4,
		(hpotch << 16) | vpotch);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL2,
		(hsize << 16) | vsize);
	if (dolby_vision_flags & FLAG_CERTIFICAION)
		VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL6, 0xba000000);
	else
		VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL6, 0xb8000000);
	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL0,
		/* el dly 3, bl dly 1 after de*/
		(el_41_mode ? (0x3 << 4) : (0x1 << 8)) |
		bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);

	if (el_enable && (dolby_vision_mask & 1))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* vd2 to core1 */
			0, 17, 1);
	else
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* vd2 to vpp */
			1, 17, 1);

	if (dolby_vision_core1_on
		&& !bypass_core1)
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* enable core 1 */
			0, 16, 1);
	else if (dolby_vision_core1_on
		&& bypass_core1)
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* bypass core 1 */
			1, 16, 1);

	/* run mode = bypass, when fake frame */
	if (!bl_enable)
		bypass_flag |= 1;
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 12; /* bypass CSC */
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 13; /* bypass CVM */
	/* bypass composer to get 12bit when SDR and HDR source */
#ifndef V2_4
	if (!dovi_src)
		bypass_flag |= 1 << 14; /* bypass composer */
#endif
	if ((scramble_en) && !(dolby_vision_flags & FLAG_CERTIFICAION))
		bypass_flag |= 1 << 13; /* bypass CVM when tunnel out */

	if (dolby_vision_run_mode != 0xff)
		run_mode = dolby_vision_run_mode;
	else {
		run_mode = (0x7 << 6) |
			((el_41_mode ? 3 : 1) << 2) |
			bypass_flag;
		if (dolby_vision_on_count < dolby_vision_run_mode_delay) {
			run_mode |= 1;
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
		} else if (dolby_vision_on_count ==
			dolby_vision_run_mode_delay) {
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
		} else {
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC0,
				(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC1,
				0);
		}
	}
	if (reset)
		VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 1, run_mode);

	/* 962e work around to fix the uv swap issue when bl:el = 1:1 */
	if (el_41_mode)
		VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL5, 0x6);
	else
		VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL5, 0xa);

	/* axi dma for reg table */
	reg_size = prepare_stb_dolby_core1_reg(
		run_mode, p_core1_dm_regs, p_core1_comp_regs);
	/* axi dma for lut table */
	prepare_stb_dolby_core1_lut(reg_size, p_core1_lut);

	if (!dolby_vision_on) {
		/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL1,
			0x00000080 | (reg_size << 23));
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		/* dma3:23-12 cvm size */
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL3,
			0x80100000 | dma_start_line);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000062);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}
	if (reset) {
		/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL1,
			0x00000080 | (reg_size << 23));
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		/* dma3:23-12 cvm size */
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL3,
			0x80100000 | dma_start_line);
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000062);
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}

	tv_dovi_setting_update_flag = true;
	return 0;
}

static uint32_t tv_run_mode(int vsize, bool hdmi, bool hdr10, int el_41_mode)
{
	uint32_t run_mode = 1;
	if (hdmi) {
		if (vsize > 1080)
			run_mode =
				0x00000043;
		else
			run_mode =
				0x00000042;
	} else {
		if (hdr10) {
			run_mode =
				0x0000004c;
#ifndef V1_5
			run_mode |= 1 << 14; /* bypass COMPOSER */
#endif
		} else {
			if (el_41_mode)
				run_mode =
					0x0000004c;
			else
				run_mode =
					0x00000044;
		}
	}
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		run_mode |= 1 << 12; /* bypass CSC */
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		run_mode |= 1 << 13; /* bypass CVM */
	return run_mode;
}

static int tv_dolby_core1_set(
	uint64_t *dma_data,
	int hsize,
	int vsize,
	int bl_enable,
	int el_enable,
	int el_41_mode,
	int src_chroma_format,
	bool hdmi,
	bool hdr10,
	bool reset)
{
	uint64_t run_mode;
	int composer_enable = el_enable;
	bool bypass_core1 = (!hsize || !vsize
		|| !(dolby_vision_mask & 1));
	if (dolby_vision_on
		&& (dolby_vision_flags &
		FLAG_DISABE_CORE_SETTING))
		return 0;

	WRITE_VPP_REG(
		DOLBY_TV_CLKGATE_CTRL, 0x2800);

	if (reset) {
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 1 << 9);
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 0);
		VSYNC_WR_MPEG_REG(
			DOLBY_TV_CLKGATE_CTRL, 0x2800);
	}

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL0,
		/* el dly 3, bl dly 1 after de*/
		(el_41_mode ? (0x3 << 4) : (0x1 << 8)) |
		bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL1,
		((hsize + 0x80) << 16 | (vsize + 0x40)));
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL2, (hsize << 16) | vsize);
	/*0x2c2d0:5-4-1-3-2-0*/
	VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0x2c2d0, 14, 18);
	VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0xa, 0, 4);

	if ((hdmi) && (!hdr10))
		VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL5, 1, 4, 1);
	else
		VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0, 4, 1);

	VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 20, 1);
	/* bypass dither */
	if (dolby_vision_flags & FLAG_CERTIFICAION)
		VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 25, 1);
	else
		VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL6, 0, 25, 1);
	if (src_chroma_format == 2)
		VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 29, 1);
	else if (src_chroma_format == 1)
		VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL6, 0, 29, 1);
	/* input 12 or 10 bit */
	VSYNC_WR_MPEG_REG_BITS(DOLBY_TV_SWAP_CTRL7, 12, 0, 8);

	if (el_enable && (dolby_vision_mask & 1))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* vd2 to core1 */
			0, 17, 1);
	else
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* vd2 to vpp */
			1, 17, 1);

	if (dolby_vision_core1_on
		&& !bypass_core1)
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* enable core 1 */
			0, 16, 1);
	else if (dolby_vision_core1_on
		&& bypass_core1)
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* bypass core 1 */
			1, 16, 1);

	if (dolby_vision_run_mode != 0xff)
		run_mode = dolby_vision_run_mode;
	else {
		run_mode = tv_run_mode(vsize, hdmi, hdr10, el_41_mode);
		if (dolby_vision_on_count < dolby_vision_run_mode_delay) {
			run_mode = (run_mode & 0xfffffffc) | 1;
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
		} else if (dolby_vision_on_count ==
			dolby_vision_run_mode_delay) {
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
		} else {
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC0,
				(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_MPEG_REG(VPP_VD1_CLIP_MISC1,
				0);
		}
	}
	tv_dovi_setting.core1_reg_lut[1] =
		0x0000000100000000 | run_mode;
	if (reset)
		VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 1, run_mode);

	if (!dolby_vision_on) {
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL3,
			0x80000000 | dma_start_line);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000042);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}
	if (reset) {
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL3,
			0x80000000 | dma_start_line);
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000042);
		VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}

	tv_dovi_setting_update_flag = true;
	return 0;
}

int dolby_vision_update_setting(void)
{
	uint64_t *dma_data;
	uint32_t size = 0;
	int i;
	uint64_t *p;

	if (!p_funcs)
		return -1;
	if (!tv_dovi_setting_update_flag)
		return 0;
	if (dolby_vision_flags &
		FLAG_DISABLE_DMA_UPDATE) {
		tv_dovi_setting_update_flag = false;
		setting_update_count++;
		return -1;
	}
	if (efuse_mode == 1) {
		tv_dovi_setting_update_flag = false;
		setting_update_count++;
		return -1;
	}
	if (is_meson_txlx_package_962X() && !force_stb_mode) {
		dma_data = tv_dovi_setting.core1_reg_lut;
		size = 8 * TV_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	} else if (is_meson_txlx_package_962E() || force_stb_mode) {
		dma_data = stb_core1_lut;
		size = 8 * STB_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	}
	if (size && (debug_dolby & 0x8)) {
		p = (uint64_t *)dma_vaddr;
		pr_info("dma size = %d\n", STB_DMA_TBL_SIZE);
		for (i = 0; i < size / 8; i += 2)
			pr_info("%016llx, %016llx\n", p[i], p[i+1]);
	}
	tv_dovi_setting_update_flag = false;
	setting_update_count = frame_count;
	return -1;
}
EXPORT_SYMBOL(dolby_vision_update_setting);

static int dolby_core1_set(
	uint32_t dm_count,
	uint32_t comp_count,
	uint32_t lut_count,
	uint32_t *p_core1_dm_regs,
	uint32_t *p_core1_comp_regs,
	uint32_t *p_core1_lut,
	int hsize,
	int vsize,
	int bl_enable,
	int el_enable,
	int el_41_mode,
	int scramble_en,
	bool dovi_src,
	int lut_endian,
	bool reset)
{
	uint32_t count;
	uint32_t bypass_flag = 0;
	int composer_enable =
		bl_enable && el_enable && (dolby_vision_mask & 1);
	int i;
	bool set_lut = false;
	uint32_t *last_dm = (uint32_t *)&dovi_setting.dm_reg1;
	uint32_t *last_comp = (uint32_t *)&dovi_setting.comp_reg;
	bool bypass_core1 = (!hsize || !vsize
		|| !(dolby_vision_mask & 1));
	if (dolby_vision_on
		&& (dolby_vision_flags &
		FLAG_DISABE_CORE_SETTING))
		return 0;

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;

	if (dolby_vision_on_count
		== dolby_vision_run_mode_delay)
		reset = true;

	if ((!dolby_vision_on || reset) && bl_enable) {
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 1 << 9);
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 0);
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (stb_core_setting_update_flag & FLAG_CHANGE_TC)
		set_lut = true;

	if (bl_enable && el_enable && (dolby_vision_mask & 1))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* vd2 to core1 */
			0, 17, 1);
	else
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			/* vd2 to vpp */
			1, 17, 1);

	VSYNC_WR_MPEG_REG(DOLBY_CORE1_CLKGATE_CTRL, 0);
	/* VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL0, 0); */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL2, (hsize << 16) | vsize);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL5, 0xa);

	VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_CTRL, 0x0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 4, 4);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 2, 1);

	/* bypass composer to get 12bit when SDR and HDR source */
#ifndef V2_4
	if (!dovi_src)
		bypass_flag |= 1 << 0;
#endif
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (scramble_en) {
		/* bypass CVM when dolby output */
		if (!(dolby_vision_flags & FLAG_CERTIFICAION))
			bypass_flag |= 1 << 2;
	}
	if (el_41_mode)
		bypass_flag |= 1 << 3;

	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 1,
		0x70 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 1,
		0x70 | bypass_flag); /* for delay */
	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset ||
			(p_core1_dm_regs[i] !=
			last_dm[i]))
			VSYNC_WR_MPEG_REG(
				DOLBY_CORE1_REG_START + 6 + i,
				p_core1_dm_regs[i]);

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		if (reset ||
			(p_core1_comp_regs[i] !=
			last_comp[i]))
			VSYNC_WR_MPEG_REG(
				DOLBY_CORE1_REG_START + 6 + 44 + i,
				p_core1_comp_regs[i]);

	/* metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(DOLBY_CORE1_CLKGATE_CTRL,
				2, 2, 2);
		VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_CTRL, 0x1401);
		if (lut_endian)
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i+3]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i+2]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i+1]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i]);
			}
		else
			for (i = 0; i < count; i++)
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i]);
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(DOLBY_CORE1_CLKGATE_CTRL,
				0, 2, 2);
	}

	if (dolby_vision_on_count
		< dolby_vision_run_mode_delay) {
		VSYNC_WR_MPEG_REG(
			VPP_VD1_CLIP_MISC0,
			(0x200 << 10) | 0x200);
		VSYNC_WR_MPEG_REG(
			VPP_VD1_CLIP_MISC1,
			(0x200 << 10) | 0x200);
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1,
			1, 16, 1);
	} else {
		if (dolby_vision_on_count >
			dolby_vision_run_mode_delay) {
			VSYNC_WR_MPEG_REG(
				VPP_VD1_CLIP_MISC0,
				(0x3ff << 20) |
				(0x3ff << 10) |
				0x3ff);
			VSYNC_WR_MPEG_REG(
				VPP_VD1_CLIP_MISC1,
				0);
		}
		if (dolby_vision_core1_on
			&& !bypass_core1)
			VSYNC_WR_MPEG_REG_BITS(
				VIU_MISC_CTRL1,
				/* enable core 1 */
				0, 16, 1);
		else if (dolby_vision_core1_on
			&& bypass_core1)
			VSYNC_WR_MPEG_REG_BITS(
				VIU_MISC_CTRL1,
				/* bypass core 1 */
				1, 16, 1);
	}
	/* enable core1 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL0,
		bl_enable << 0 |
		composer_enable << 1 |
		el_41_mode << 2);
	tv_dovi_setting_update_flag = true;
	return 0;
}

static int dolby_core2_set(
	uint32_t dm_count,
	uint32_t lut_count,
	uint32_t *p_core2_dm_regs,
	uint32_t *p_core2_lut,
	int hsize,
	int vsize,
	int dolby_enable,
	int lut_endian)
{
	uint32_t count;
	int i;
	bool set_lut = false;
	bool reset = false;
	uint32_t *last_dm = (uint32_t *)&dovi_setting.dm_reg2;

	if (dolby_vision_on
		&& (dolby_vision_flags &
		FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on || force_reset_core2) {
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, (1 << 10));
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 0);
		force_reset_core2 = false;
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (stb_core_setting_update_flag & FLAG_CHANGE_TC2)
		set_lut = true;

	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_CLKGATE_CTRL, 0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL0, 0);

	if (is_meson_gxm_cpu() || reset) {
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL1,
			((hsize + g_htotal_add) << 16)
			| (vsize + g_vtotal_add + g_vsize_add));
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL2,
			(hsize << 16) | (vsize + g_vsize_add));
	}
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL3,
		(g_hwidth << 16) | g_vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL4,
		(g_hpotch << 16) | g_vpotch);
	if (is_meson_txlx_package_962E() || force_stb_mode)
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL5, 0xf8000000);
	else
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL5, 0x0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_CTRL, 0x0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 2, 1);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 1, 2);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 1, 2);

	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_CTRL, 0);

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset ||
			(p_core2_dm_regs[i] !=
			last_dm[i])) {
			VSYNC_WR_MPEG_REG(
				DOLBY_CORE2A_REG_START + 6 + i,
				p_core2_dm_regs[i]);
			set_lut = true;
		}
	/* core2 metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(DOLBY_CORE2A_CLKGATE_CTRL,
				2, 2, 2);
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_CTRL, 0x1401);
		if (lut_endian)
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i+3]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i+2]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i+1]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i]);
			}
		else
			for (i = 0; i < count; i++)
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i]);
		/* core2 lookup table program done */
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(
				DOLBY_CORE2A_CLKGATE_CTRL, 0, 2, 2);
	}

	/* enable core2 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL0, dolby_enable << 0);
	return 0;
}

static int dolby_core3_set(
	uint32_t dm_count,
	uint32_t md_count,
	uint32_t *p_core3_dm_regs,
	uint32_t *p_core3_md_regs,
	int hsize,
	int vsize,
	int dolby_enable,
	int scramble_en)
{
	uint32_t count;
	int i;
	int vsize_hold = 0x10;
	uint32_t diag_mode = 0;
	uint32_t cur_dv_mode = dolby_vision_mode;
	uint32_t *last_dm = (uint32_t *)&dovi_setting.dm_reg3;
	bool reset = false;
#ifdef V2_4
	u32 diag_enable = 0;
	bool reset_post_table = false;
	if (new_dovi_setting.diagnostic_enable
		|| new_dovi_setting.dovi_ll_enable)
		diag_enable = 1;
#endif
	if (dolby_vision_on
		&& (dolby_vision_flags &
		FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on ||
		(dolby_vision_flags & FLAG_CERTIFICAION))
		reset = true;

#ifdef V2_4
	if (((cur_dv_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
		|| (cur_dv_mode == DOLBY_VISION_OUTPUT_MODE_IPT))
		&& diag_enable) {
		cur_dv_mode = dv_ll_output_mode;
		diag_mode = 3;
	}
	if (dolby_vision_on &&
		((last_dolby_vision_ll_policy !=
		dolby_vision_ll_policy) ||
		new_dovi_setting.vsvdb_changed)) {
		last_dolby_vision_ll_policy =
			dolby_vision_ll_policy;
		new_dovi_setting.vsvdb_changed = 0;
		/* TODO: verify 962e case */
		if (is_meson_gxm_cpu()) {
			if (new_dovi_setting.dovi_ll_enable &&
				new_dovi_setting.diagnostic_enable == 0) {
				VSYNC_WR_MPEG_REG_BITS(
					VPP_DOLBY_CTRL,
					3, 6, 2); /* post matrix */
				VSYNC_WR_MPEG_REG_BITS(
					VPP_MATRIX_CTRL,
					1, 0, 1); /* post matrix */
			} else {
				VSYNC_WR_MPEG_REG_BITS(
					VPP_DOLBY_CTRL,
					0, 6, 2); /* post matrix */
				VSYNC_WR_MPEG_REG_BITS(
					VPP_MATRIX_CTRL,
					0, 0, 1); /* post matrix */
			}
		} else if (is_meson_txlx_package_962E()
			|| force_stb_mode) {
			if (new_dovi_setting.dovi_ll_enable &&
				new_dovi_setting.diagnostic_enable == 0) {
				VSYNC_WR_MPEG_REG(
					VPP_DAT_CONV_PARA1,
					0x8000800);
				/* enable wm tp vks
				   bypass gainoff to vks */
				VSYNC_WR_MPEG_REG_BITS(
					VPP_DOLBY_CTRL, 1, 1, 2);
				VSYNC_WR_MPEG_REG_BITS(
					VPP_MATRIX_CTRL,
					1, 0, 1); /* post matrix */
			} else {
				/* bypass wm tp vks
				   bypass gainoff to vks */
				VSYNC_WR_MPEG_REG_BITS(
					VPP_DOLBY_CTRL, 3, 1, 2);
				VSYNC_WR_MPEG_REG(
					VPP_DAT_CONV_PARA1,
					0x20002000);
				if (is_meson_txlx_package_962X())
					enable_rgb_to_yuv_matrix_for_dvll(
						0, NULL, 12);
				else
					VSYNC_WR_MPEG_REG_BITS(
						VPP_MATRIX_CTRL,
						0, 0, 1);
			}
		}
		reset_post_table = true;
	}
	/* flush post matrix table when ll mode on and setting changed */
	if (new_dovi_setting.dovi_ll_enable &&
		(new_dovi_setting.diagnostic_enable == 0) &&
		dolby_vision_on &&
		(reset_post_table || reset ||
		memcmp(&p_core3_dm_regs[18],
		&last_dm[18], 32)))
		enable_rgb_to_yuv_matrix_for_dvll(
			1, &p_core3_dm_regs[18], 12);
#endif

	VSYNC_WR_MPEG_REG(DOLBY_CORE3_CLKGATE_CTRL, 0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL1,
		((hsize + htotal_add) << 16)
		| (vsize + vtotal_add + vsize_add + vsize_hold * 2));
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL2,
		(hsize << 16) | (vsize + vsize_add));
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL3,
		(0x80 << 16) | vsize_hold);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL4,
		(0x04 << 16) | vsize_hold);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL5, 0x0000);
	if (cur_dv_mode !=
		DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL6, 0);
	else
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL6,
			0x10000000);  /* swap UV */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 5, 7);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 4, 4);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 4, 2);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 2, 1);
	/* Control Register, address 0x04 2:0 RW */
	/* Output_operating mode
	   00- IPT 12 bit 444 bypass Dolby Vision output
	   01- IPT 12 bit tunnelled over RGB 8 bit 444, dolby vision output
	   02- HDR10 output, RGB 10 bit 444 PQ
	   03- Deep color SDR, RGB 10 bit 444 Gamma
	   04- SDR, RGB 8 bit 444 Gamma
	*/
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 1, cur_dv_mode);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 1, cur_dv_mode);
	/* for delay */

	if (dm_count == 0)
		count = 26;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset || (p_core3_dm_regs[i] !=
			last_dm[i]))
			VSYNC_WR_MPEG_REG(
				DOLBY_CORE3_REG_START + 0x6 + i,
				p_core3_dm_regs[i]);
	/* from addr 0x18 */

	count = md_count;
	for (i = 0; i < count; i++) {
#ifdef FORCE_HDMI_META
		if ((i == 20) && (p_core3_md_regs[i] == 0x5140a3e))
			VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i,
				(p_core3_md_regs[i] & 0xffffff00) | 0x80);
		else
#endif
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i,
			p_core3_md_regs[i]);
	}
	for (; i < 30; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i, 0);

	/* from addr 0x90 */
	/* core3 metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 3, 1);

	VSYNC_WR_MPEG_REG(
		DOLBY_CORE3_DIAG_CTRL,
		diag_mode);

	if ((dolby_vision_flags & FLAG_CERTIFICAION)
		&& !(dolby_vision_flags & FLAG_DISABLE_CRC))
		VSYNC_WR_MPEG_REG(0x36fb, 1);
	/* enable core3 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL0, (dolby_enable << 0));
	return 0;
}

static void apply_stb_core_settings(
	int enable, unsigned int mask,
	bool reset, u32 frame_size)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	u32 h_size = (frame_size >> 16) & 0xffff;
	u32 v_size = frame_size & 0xffff;
#ifdef V2_4
	u32 core1_dm_count = 27;
#else
	u32 core1_dm_count = 24;
#endif
	u32 graphics_w = 1920;
	u32 graphics_h = 1080;
	if (is_dolby_vision_stb_mode()
		&& (dolby_vision_flags & FLAG_CERTIFICAION)) {
		graphics_w = dv_cert_graphic_width;
		graphics_h = dv_cert_graphic_height;
	}
	if (is_meson_txlx_package_962E()
		|| force_stb_mode) {
		if (vinfo->mode >= VMODE_1080P)
			dma_start_line = 0x400;
		else
			dma_start_line = 0x180;
		/* adjust core2 setting to work around
			fixing with 1080p24hz and 480p60hz */
		if (vinfo->mode < VMODE_720P)
			g_vpotch = 0x60;
		else
			g_vpotch = 0x20;
	}
	if (mask & 1) {
		if (is_meson_txlx_package_962E()
			|| force_stb_mode) {
			stb_dolby_core1_set(
				27, 173, 256 * 5,
				(uint32_t *)&new_dovi_setting.dm_reg1,
				(uint32_t *)&new_dovi_setting.comp_reg,
				(uint32_t *)&new_dovi_setting.dm_lut1,
				h_size,
				v_size,
				/* BL enable */
				enable,
				/* EL enable */
				enable && new_dovi_setting.el_flag,
				new_dovi_setting.el_halfsize_flag,
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
				new_dovi_setting.src_format == FORMAT_DOVI,
				1,
				reset);
		} else
			dolby_core1_set(
				core1_dm_count, 173, 256 * 5,
				(uint32_t *)&new_dovi_setting.dm_reg1,
				(uint32_t *)&new_dovi_setting.comp_reg,
				(uint32_t *)&new_dovi_setting.dm_lut1,
				h_size,
				v_size,
				/* BL enable */
				enable,
				/* EL enable */
				enable && new_dovi_setting.el_flag,
				new_dovi_setting.el_halfsize_flag,
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
				new_dovi_setting.src_format == FORMAT_DOVI,
				1,
				reset);
	}
	if (mask & 2)
		dolby_core2_set(
			24, 256 * 5,
			(uint32_t *)&new_dovi_setting.dm_reg2,
			(uint32_t *)&new_dovi_setting.dm_lut2,
			graphics_w, graphics_h, 1, 1);
	v_size = vinfo->height;
	if ((vinfo->mode == VMODE_480I) ||
		(vinfo->mode == VMODE_480I_RPT) ||
		(vinfo->mode == VMODE_576I) ||
		(vinfo->mode == VMODE_576I_RPT) ||
		(vinfo->mode == VMODE_1080I) ||
		(vinfo->mode == VMODE_1080I_50HZ))
		v_size = v_size/2;
	if (mask & 4)
		dolby_core3_set(
			26, new_dovi_setting.md_reg3.size,
			(uint32_t *)&new_dovi_setting.dm_reg3,
			new_dovi_setting.md_reg3.raw_metadata,
			vinfo->width, v_size,
			1,
			dolby_vision_mode ==
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL);
}

static void osd_bypass(int bypass)
{
	static uint32_t osd_backup_ctrl;
	static uint32_t osd_backup_eotf;
	static uint32_t osd_backup_mtx;
	if (bypass) {
		osd_backup_ctrl = VSYNC_RD_MPEG_REG(VIU_OSD1_CTRL_STAT);
		osd_backup_eotf = VSYNC_RD_MPEG_REG(VIU_OSD1_EOTF_CTL);
		osd_backup_mtx = VSYNC_RD_MPEG_REG(VPP_MATRIX_CTRL);
		VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_EOTF_CTL, 0, 31, 1);
		VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_CTRL_STAT, 0, 3, 1);
		VSYNC_WR_MPEG_REG_BITS(VPP_MATRIX_CTRL, 0, 7, 1);
	} else {
		VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, osd_backup_mtx);
		VSYNC_WR_MPEG_REG(VIU_OSD1_CTRL_STAT, osd_backup_ctrl);
		VSYNC_WR_MPEG_REG(VIU_OSD1_EOTF_CTL, osd_backup_eotf);
	}
}

static uint32_t viu_eotf_ctrl_backup;
static uint32_t xvycc_lut_ctrl_backup;
static uint32_t inv_lut_ctrl_backup;
static uint32_t vpp_vadj_backup;
static uint32_t vpp_gainoff_backup;
static uint32_t vpp_ve_enable_ctrl_backup;
static uint32_t xvycc_vd1_rgb_ctrst_backup;
static bool is_video_effect_bypass;
static void video_effect_bypass(int bypass)
{
	if (bypass) {
		if (!is_video_effect_bypass) {
			viu_eotf_ctrl_backup =
				VSYNC_RD_MPEG_REG(VIU_EOTF_CTL);
			xvycc_lut_ctrl_backup =
				VSYNC_RD_MPEG_REG(XVYCC_LUT_CTL);
			inv_lut_ctrl_backup =
				VSYNC_RD_MPEG_REG(XVYCC_INV_LUT_CTL);
			vpp_vadj_backup =
				VSYNC_RD_MPEG_REG(VPP_VADJ_CTRL);
			vpp_gainoff_backup =
				VSYNC_RD_MPEG_REG(VPP_GAINOFF_CTRL0);
			vpp_ve_enable_ctrl_backup =
				VSYNC_RD_MPEG_REG(VPP_VE_ENABLE_CTRL);
			xvycc_vd1_rgb_ctrst_backup =
				VSYNC_RD_MPEG_REG(XVYCC_VD1_RGB_CTRST);
			is_video_effect_bypass = true;
		}
		VSYNC_WR_MPEG_REG(VIU_EOTF_CTL, 0);
		VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL, 0);
		VSYNC_WR_MPEG_REG(XVYCC_INV_LUT_CTL, 0);
		VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, 0);
		VSYNC_WR_MPEG_REG(VPP_GAINOFF_CTRL0, 0);
		VSYNC_WR_MPEG_REG(VPP_VE_ENABLE_CTRL, 0);
		VSYNC_WR_MPEG_REG(XVYCC_VD1_RGB_CTRST, 0);
	} else if (is_video_effect_bypass) {
		VSYNC_WR_MPEG_REG(VIU_EOTF_CTL,
			viu_eotf_ctrl_backup);
		VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL,
			xvycc_lut_ctrl_backup);
		VSYNC_WR_MPEG_REG(XVYCC_INV_LUT_CTL,
			inv_lut_ctrl_backup);
		VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL,
			vpp_vadj_backup);
		VSYNC_WR_MPEG_REG(VPP_GAINOFF_CTRL0,
			vpp_gainoff_backup);
		VSYNC_WR_MPEG_REG(VPP_VE_ENABLE_CTRL,
			vpp_ve_enable_ctrl_backup);
		VSYNC_WR_MPEG_REG(XVYCC_VD1_RGB_CTRST,
			xvycc_vd1_rgb_ctrst_backup);
	}
}

static void osd_path_enable(int on)
{
	u32 i = 0;
	u32 addr_port;
	u32 data_port;
	struct hdr_osd_lut_s *lut = &hdr_osd_reg.lut_val;
	if (!on) {
		enable_osd_path(0, 0);
		VSYNC_WR_MPEG_REG(VIU_OSD1_EOTF_CTL, 0);
		VSYNC_WR_MPEG_REG(VIU_OSD1_OETF_CTL, 0);
	} else {
		enable_osd_path(1, -1);
		if ((hdr_osd_reg.viu_osd1_eotf_ctl & 0x80000000) != 0) {
			addr_port = VIU_OSD1_EOTF_LUT_ADDR_PORT;
			data_port = VIU_OSD1_EOTF_LUT_DATA_PORT;
			VSYNC_WR_MPEG_REG(
				addr_port, 0);
			for (i = 0; i < 16; i++)
				VSYNC_WR_MPEG_REG(
					data_port,
					lut->r_map[i * 2]
					| (lut->r_map[i * 2 + 1] << 16));
			VSYNC_WR_MPEG_REG(
				data_port,
				lut->r_map[EOTF_LUT_SIZE - 1]
				| (lut->g_map[0] << 16));
			for (i = 0; i < 16; i++)
				VSYNC_WR_MPEG_REG(
					data_port,
					lut->g_map[i * 2 + 1]
					| (lut->b_map[i * 2 + 2] << 16));
			for (i = 0; i < 16; i++)
				VSYNC_WR_MPEG_REG(
					data_port,
					lut->b_map[i * 2]
					| (lut->b_map[i * 2 + 1] << 16));
			VSYNC_WR_MPEG_REG(
				data_port, lut->b_map[EOTF_LUT_SIZE - 1]);

			/* load eotf matrix */
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_EOTF_COEF00_01,
				hdr_osd_reg.viu_osd1_eotf_coef00_01);
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_EOTF_COEF02_10,
				hdr_osd_reg.viu_osd1_eotf_coef02_10);
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_EOTF_COEF11_12,
				hdr_osd_reg.viu_osd1_eotf_coef11_12);
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_EOTF_COEF20_21,
				hdr_osd_reg.viu_osd1_eotf_coef20_21);
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_EOTF_COEF22_RS,
				hdr_osd_reg.viu_osd1_eotf_coef22_rs);
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_EOTF_CTL,
				hdr_osd_reg.viu_osd1_eotf_ctl);
		}
		/* restore oetf lut */
		if ((hdr_osd_reg.viu_osd1_oetf_ctl & 0xe0000000) != 0) {
			addr_port = VIU_OSD1_OETF_LUT_ADDR_PORT;
			data_port = VIU_OSD1_OETF_LUT_DATA_PORT;
			for (i = 0; i < 20; i++) {
				VSYNC_WR_MPEG_REG(
					addr_port, i);
				VSYNC_WR_MPEG_REG(
					data_port,
					lut->or_map[i * 2]
					| (lut->or_map[i * 2 + 1] << 16));
			}
			VSYNC_WR_MPEG_REG(
				addr_port, 20);
			VSYNC_WR_MPEG_REG(
				data_port,
				lut->or_map[41 - 1]
				| (lut->og_map[0] << 16));
			for (i = 0; i < 20; i++) {
				VSYNC_WR_MPEG_REG(
					addr_port, 21 + i);
				VSYNC_WR_MPEG_REG(
					data_port,
					lut->og_map[i * 2 + 1]
					| (lut->og_map[i * 2 + 2] << 16));
			}
			for (i = 0; i < 20; i++) {
				VSYNC_WR_MPEG_REG(
					addr_port, 41 + i);
				VSYNC_WR_MPEG_REG(
					data_port,
					lut->ob_map[i * 2]
					| (lut->ob_map[i * 2 + 1] << 16));
			}
			VSYNC_WR_MPEG_REG(
				addr_port, 61);
			VSYNC_WR_MPEG_REG(
				data_port,
				lut->ob_map[41 - 1]);
			VSYNC_WR_MPEG_REG(
				VIU_OSD1_OETF_CTL,
				hdr_osd_reg.viu_osd1_oetf_ctl);
		}
	}
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_PRE_OFFSET0_1,
		hdr_osd_reg.viu_osd1_matrix_pre_offset0_1);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_PRE_OFFSET2,
		hdr_osd_reg.viu_osd1_matrix_pre_offset2);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF00_01,
		hdr_osd_reg.viu_osd1_matrix_coef00_01);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF02_10,
		hdr_osd_reg.viu_osd1_matrix_coef02_10);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF11_12,
		hdr_osd_reg.viu_osd1_matrix_coef11_12);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF20_21,
		hdr_osd_reg.viu_osd1_matrix_coef20_21);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF22_30,
		hdr_osd_reg.viu_osd1_matrix_coef22_30);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF31_32,
		hdr_osd_reg.viu_osd1_matrix_coef31_32);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COEF40_41,
		hdr_osd_reg.viu_osd1_matrix_coef40_41);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_COLMOD_COEF42,
		hdr_osd_reg.viu_osd1_matrix_colmod_coef42);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_OFFSET0_1,
		hdr_osd_reg.viu_osd1_matrix_offset0_1);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_OFFSET2,
		hdr_osd_reg.viu_osd1_matrix_offset2);
	VSYNC_WR_MPEG_REG(
		VIU_OSD1_MATRIX_CTRL,
		hdr_osd_reg.viu_osd1_matrix_ctrl);
}

static uint32_t dolby_ctrl_backup;
static uint32_t viu_misc_ctrl_backup;
static uint32_t vpp_matrix_backup;
static uint32_t vpp_dummy1_backup;
static uint32_t vpp_data_conv_para0_backup;
static uint32_t vpp_data_conv_para1_backup;
void enable_dolby_vision(int enable)
{
	uint32_t size = 0;
	uint64_t *dma_data = tv_dovi_setting.core1_reg_lut;
	if (enable) {
		if (!dolby_vision_on) {
			dolby_ctrl_backup =
				VSYNC_RD_MPEG_REG(VPP_DOLBY_CTRL);
			viu_misc_ctrl_backup =
				VSYNC_RD_MPEG_REG(VIU_MISC_CTRL1);
			vpp_matrix_backup =
				VSYNC_RD_MPEG_REG(VPP_MATRIX_CTRL);
			vpp_dummy1_backup =
				VSYNC_RD_MPEG_REG(VPP_DUMMY_DATA1);
			if (is_meson_txlx_cpu()) {
				vpp_data_conv_para0_backup =
					VSYNC_RD_MPEG_REG(VPP_DAT_CONV_PARA0);
				vpp_data_conv_para1_backup =
					VSYNC_RD_MPEG_REG(VPP_DAT_CONV_PARA1);
				setting_update_count = 0;
			}
			if (is_meson_txlx_package_962X() && !force_stb_mode) {
				if (efuse_mode == 1) {
					size = 8 * TV_DMA_TBL_SIZE;
					memset(dma_vaddr, 0x0, size);
					memcpy((uint64_t *)dma_vaddr + 1,
						dma_data + 1,
						8);
				}
				if ((dolby_vision_mask & 1)
					&& dovi_setting_video_flag) {
					VSYNC_WR_MPEG_REG_BITS(
						VIU_MISC_CTRL1,
						0,
						16, 1); /* core1 */
					dolby_vision_core1_on = true;
				} else {
					VSYNC_WR_MPEG_REG_BITS(
						VIU_MISC_CTRL1,
						1,
						16, 1); /* core1 */
					dolby_vision_core1_on = false;
				}
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM, EO/OE */
					VSYNC_WR_MPEG_REG_BITS(
						VPP_DOLBY_CTRL, 3, 0, 2);
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12 bit unsigned to sign
					   before vadj1 */
					/* 12 bit sign to unsign
					   before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x08000800);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
					WRITE_VPP_REG(0x33e7, 0xb);
				} else {
					/* bypass all video effect */
					if (dolby_vision_flags
					& FLAG_BYPASS_VPP)
						video_effect_bypass(1);
					/* 12->10 before vadj1
					   10->12 before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x20002000);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
				}
				VSYNC_WR_MPEG_REG(
					VPP_DUMMY_DATA1,
					0x80200);
				/* osd rgb to yuv, vpp out yuv to rgb */
				VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0x81);
				pr_dolby_dbg("Dolby Vision TV core turn on\n");
			} else if (is_meson_txlx_package_962E()
				|| force_stb_mode) {
				size = 8 * STB_DMA_TBL_SIZE;
				if (efuse_mode == 1)
					memset(dma_vaddr, 0x0, size);
				osd_bypass(1);
				if (dolby_vision_mask & 4)
					VSYNC_WR_MPEG_REG_BITS(VPP_DOLBY_CTRL,
						1, 3, 1);   /* core3 enable */
				if ((dolby_vision_mask & 1)
					&& dovi_setting_video_flag) {
					VSYNC_WR_MPEG_REG_BITS(
						VIU_MISC_CTRL1,
						0,
						16, 1); /* core1 */
					dolby_vision_core1_on = true;
				} else {
					VSYNC_WR_MPEG_REG_BITS(
						VIU_MISC_CTRL1,
						1,
						16, 1); /* core1 */
					dolby_vision_core1_on = false;
				}
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					(((dolby_vision_mask & 1)
					&& dovi_setting_video_flag)
					? 0 : 1),
					16, 1); /* core1 */
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					((dolby_vision_mask & 2) ? 0 : 1),
					18, 1); /* core2 */
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM
					   bypass EO/OE
					   bypass vadj2/mtx/gainoff */
					VSYNC_WR_MPEG_REG_BITS(
						VPP_DOLBY_CTRL, 7, 0, 3);
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12 bit unsigned to sign
					   before vadj1 */
					/* 12 bit sign to unsign
					   before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x08000800);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
				} else {
					/* bypass all video effect */
					if (dolby_vision_flags
					& FLAG_BYPASS_VPP)
						video_effect_bypass(1);
					/* 12->10 before vadj1
					   10->12 before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x20002000);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
				}
				VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1,
					0x80200);
				if (is_meson_txlx_package_962X())
					VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 1);
				else
					VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0);
#ifdef V2_4
				if (((dolby_vision_mode ==
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
					|| (dolby_vision_mode ==
					DOLBY_VISION_OUTPUT_MODE_IPT)) &&
					(dovi_setting.diagnostic_enable == 0) &&
					dovi_setting.dovi_ll_enable) {
					u32 *reg =
						(u32 *)&dovi_setting.dm_reg3;
					/* input u12 -0x800 to s12 */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x8000800);
					/* bypass vadj */
					VSYNC_WR_MPEG_REG(
						VPP_VADJ_CTRL, 0);
					/* bypass gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_GAINOFF_CTRL0, 0);
					/* enable wm tp vks
					   bypass gainoff to vks */
					VSYNC_WR_MPEG_REG_BITS(
						VPP_DOLBY_CTRL, 1, 1, 2);
					enable_rgb_to_yuv_matrix_for_dvll(
						1, &reg[18], 12);
				} else
					enable_rgb_to_yuv_matrix_for_dvll(
						0, NULL, 12);
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
#endif
				stb_core_setting_update_flag = FLAG_CHANGE_ALL;
				pr_dolby_dbg("Dolby Vision STB cores turn on\n");
			} else {
				VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,
					/* cm_datx4_mode */
					(0x0<<21) |
					/* reg_front_cti_bit_mode */
					(0x0<<20) |
					/* vpp_clip_ext_mode 19:17 */
					(0x0<<17) |
					/* vpp_dolby2_en core3 */
					(((dolby_vision_mask & 4) ?
					(1 << 0) : (0 << 0)) << 16) |
					/* mat_xvy_dat_mode */
					(0x0<<15) |
					/* vpp_ve_din_mode */
					(0x1<<14) |
					/* mat_vd2_dat_mode 13:12 */
					(0x1<<12) |
					/* vpp_dpath_sel 10:8 */
					(0x3<<8) |
					/* vpp_uns2s_mode 7:0 */
					0x1f);
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					/* 23-20 ext mode */
					(0 << 2) |
					/* 19 osd2 enable */
					((dolby_vision_flags
					& FLAG_CERTIFICAION)
					? (0 << 1) : (1 << 1)) |
					/* 18 core2 bypass */
					((dolby_vision_mask & 2) ?
					0 : 1),
					18, 6);
				if ((dolby_vision_mask & 1)
					&& dovi_setting_video_flag) {
					VSYNC_WR_MPEG_REG_BITS(
						VIU_MISC_CTRL1,
						0,
						16, 1); /* core1 */
					dolby_vision_core1_on = true;
				} else {
					VSYNC_WR_MPEG_REG_BITS(
						VIU_MISC_CTRL1,
						1,
						16, 1); /* core1 */
					dolby_vision_core1_on = false;
				}
				/* bypass all video effect */
				if ((dolby_vision_flags & FLAG_BYPASS_VPP)
				|| (dolby_vision_flags & FLAG_CERTIFICAION))
					video_effect_bypass(1);
				VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1, 0x20000000);
				/* disable osd effect and shadow mode */
				osd_path_enable(0);
#ifdef V2_4
				if (((dolby_vision_mode ==
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
					|| (dolby_vision_mode ==
					DOLBY_VISION_OUTPUT_MODE_IPT)) &&
					(dovi_setting.diagnostic_enable == 0) &&
					dovi_setting.dovi_ll_enable) {
					u32 *reg =
						(u32 *)&dovi_setting.dm_reg3;
					VSYNC_WR_MPEG_REG_BITS(
						VPP_DOLBY_CTRL,
						3, 6, 2); /* post matrix */
					enable_rgb_to_yuv_matrix_for_dvll(
						1, &reg[18], 12);
				} else
					enable_rgb_to_yuv_matrix_for_dvll(
						0, NULL, 12);
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
#endif
				stb_core_setting_update_flag = FLAG_CHANGE_ALL;
				pr_dolby_dbg("Dolby Vision turn on\n");
			}
		} else {
			if (!dolby_vision_core1_on
				&& (dolby_vision_mask & 1)
				&& dovi_setting_video_flag) {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					0,
					16, 1); /* core1 */
				dolby_vision_core1_on = true;
			} else if (dolby_vision_core1_on
				&& (!(dolby_vision_mask & 1)
				|| !dovi_setting_video_flag)){
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					1,
					16, 1); /* core1 */
				dolby_vision_core1_on = false;
			}
		}
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_wait_count = 0;
		vsync_count = 0;
	} else {
		if (dolby_vision_on) {
			if (is_meson_txlx_package_962X() && !force_stb_mode) {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					/* vd2 connect to vpp */
					(1 << 1) |
					/* 16 core1 bl bypass */
					(1 << 0),
					16, 2);
#ifdef V1_5
				if (p_funcs) /* destroy ctx */
					p_funcs->tv_control_path(
						FORMAT_INVALID, 0,
						NULL, 0,
						NULL, 0,
						0,	0,
						SIG_RANGE_SMPTE,
						NULL, NULL,
						0,
						NULL,
						NULL);
#endif
				pr_dolby_dbg("Dolby Vision TV core turn off\n");
			} else if (is_meson_txlx_package_962E()
				|| force_stb_mode) {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_MPEG_REG_BITS(VPP_DOLBY_CTRL,
					0, 3, 1);   /* core3 disable */
				osd_bypass(0);
#ifdef V2_4
				if (p_funcs) /* destroy ctx */
					p_funcs->control_path(
						FORMAT_INVALID, 0,
						comp_buf, 0,
						md_buf, 0,
						0, 0, 0, SIG_RANGE_SMPTE,
						0, 0, 0, 0,
						0,
						&hdr10_param,
						&new_dovi_setting);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
#endif
				stb_core_setting_update_flag = FLAG_CHANGE_ALL;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				pr_dolby_dbg("Dolby Vision STB cores turn off\n");
			} else {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_MPEG_REG_BITS(VPP_DOLBY_CTRL,
					0, 16, 1);   /* core3 disable */
				/* enable osd effect and
					use default shadow mode */
				osd_path_enable(1);
#ifdef V2_4
				if (p_funcs) /* destroy ctx */
					p_funcs->control_path(
						FORMAT_INVALID, 0,
						comp_buf, 0,
						md_buf, 0,
						0, 0, 0, SIG_RANGE_SMPTE,
						0, 0, 0, 0,
						0,
						&hdr10_param,
						&new_dovi_setting);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
#endif
				stb_core_setting_update_flag = FLAG_CHANGE_ALL;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				pr_dolby_dbg("Dolby Vision turn off\n");
			}
			VSYNC_WR_MPEG_REG(VIU_SW_RESET, 3 << 9);
			VSYNC_WR_MPEG_REG(VIU_SW_RESET, 0);
			if (is_meson_txlx_cpu()) {
				VSYNC_WR_MPEG_REG(VPP_DAT_CONV_PARA0,
					vpp_data_conv_para0_backup);
				VSYNC_WR_MPEG_REG(VPP_DAT_CONV_PARA1,
					vpp_data_conv_para1_backup);
				VSYNC_WR_MPEG_REG(
					DOLBY_TV_CLKGATE_CTRL,
					0x2414);
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE2A_CLKGATE_CTRL,
					0x4);
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE3_CLKGATE_CTRL,
					0x414);
				VSYNC_WR_MPEG_REG(DOLBY_TV_AXI2DMA_CTRL0,
					0x01000042);
			}
			if (is_meson_gxm_cpu()) {
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE1_CLKGATE_CTRL,
					0x55555555);
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE2A_CLKGATE_CTRL,
					0x55555555);
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE3_CLKGATE_CTRL,
					0x55555555);
			}
			VSYNC_WR_MPEG_REG(
				VPP_VD1_CLIP_MISC0,
				(0x3ff << 20)
				| (0x3ff << 10) | 0x3ff);
			VSYNC_WR_MPEG_REG(
				VPP_VD1_CLIP_MISC1,
				0);
			video_effect_bypass(0);
			VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,
				dolby_ctrl_backup);
			/* always vd2 to vpp and bypass core 1 */
			viu_misc_ctrl_backup |=
				(VSYNC_RD_MPEG_REG(VIU_MISC_CTRL1) & 2);
			VSYNC_WR_MPEG_REG(VIU_MISC_CTRL1,
				viu_misc_ctrl_backup
				| (3 << 16));
			VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL,
				vpp_matrix_backup);
			VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1,
				vpp_dummy1_backup);
		}
		frame_count = 0;
		core1_disp_hsize = 0;
		core1_disp_vsize = 0;
		dolby_vision_on = false;
		dolby_vision_core1_on = false;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_wait_count = 0;
		dolby_vision_status = BYPASS_PROCESS;
		dolby_vision_on_count = 0;
	}
}
EXPORT_SYMBOL(enable_dolby_vision);

/*
    dolby vision enhanced layer receiver
*/
#define DVEL_RECV_NAME "dvel"

static inline void dvel_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(DVEL_RECV_NAME);
	int event = 0;
	if (vfp) {
		vf_put(vf, DVEL_RECV_NAME);
		event |= VFRAME_EVENT_RECEIVER_PUT;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
}

static inline struct vframe_s *dvel_vf_peek(void)
{
	return vf_peek(DVEL_RECV_NAME);
}

static inline struct vframe_s *dvel_vf_get(void)
{
	int event = 0;
	struct vframe_s *vf = vf_get(DVEL_RECV_NAME);
	if (vf) {
		event |= VFRAME_EVENT_RECEIVER_GET;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
	return vf;
}

static struct vframe_s *dv_vf[16][2];
static void *metadata_parser;
static bool metadata_parser_reset_flag;
static char meta_buf[1024];
static int dvel_receiver_event_fun(int type, void *data, void *arg)
{
	char *provider_name = (char *)data;
	int i;
	unsigned long flags;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("%s, provider %s unregistered\n",
			__func__, provider_name);
		spin_lock_irqsave(&dovi_lock, flags);
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0]) {
				if (dv_vf[i][1])
					dvel_vf_put(dv_vf[i][1]);
				dv_vf[i][1] = NULL;
			}
			dv_vf[i][0] = NULL;
		}
		/* if (metadata_parser && p_funcs) {
			p_funcs->metadata_parser_release();
			metadata_parser = NULL;
		} */
		new_dovi_setting.video_width =
		new_dovi_setting.video_height = 0;
		spin_unlock_irqrestore(&dovi_lock, flags);
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		return -1;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		pr_info("%s, provider %s registered\n",
			__func__, provider_name);
		spin_lock_irqsave(&dovi_lock, flags);
		for (i = 0; i < 16; i++)
			dv_vf[i][0] = dv_vf[i][1] = NULL;
		new_dovi_setting.video_width =
		new_dovi_setting.video_height = 0;
		spin_unlock_irqrestore(&dovi_lock, flags);
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
	}
	return 0;
}

static const struct vframe_receiver_op_s dvel_vf_receiver = {
	.event_cb = dvel_receiver_event_fun
};

static struct vframe_receiver_s dvel_vf_recv;

void dolby_vision_init_receiver(void *pdev)
{
	ulong alloc_size;

	pr_info("%s(%s)\n", __func__, DVEL_RECV_NAME);
	vf_receiver_init(&dvel_vf_recv, DVEL_RECV_NAME,
			&dvel_vf_receiver, &dvel_vf_recv);
	vf_reg_receiver(&dvel_vf_recv);
	pr_info("%s: %s\n", __func__, dvel_vf_recv.name);
	dovi_pdev = (struct platform_device *)pdev;
	alloc_size = dma_size;
	alloc_size = (alloc_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dma_vaddr = dma_alloc_coherent(&dovi_pdev->dev,
		alloc_size, &dma_paddr, GFP_KERNEL);
}
EXPORT_SYMBOL(dolby_vision_init_receiver);

#define MAX_FILENAME_LENGTH 64
static const char comp_file[] = "%s_comp.%04d.reg";
static const char dm_reg_core1_file[] = "%s_dm_core1.%04d.reg";
static const char dm_reg_core2_file[] = "%s_dm_core2.%04d.reg";
static const char dm_reg_core3_file[] = "%s_dm_core3.%04d.reg";
static const char dm_lut_core1_file[] = "%s_dm_core1.%04d.lut";
static const char dm_lut_core2_file[] = "%s_dm_core2.%04d.lut";

static void dump_struct(void *structure, int struct_length,
	const char file_string[], int frame_nr)
{
	char fn[MAX_FILENAME_LENGTH];
	struct file *fp;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	if (frame_nr == 0)
		return;
	set_fs(KERNEL_DS);
	snprintf(fn, MAX_FILENAME_LENGTH, file_string,
		"/data/tmp/tmp", frame_nr-1);
	fp = filp_open(fn, O_RDWR|O_CREAT, 0666);
	if (fp == NULL)
		pr_info("Error open file for writing %s\n", fn);
	vfs_write(fp, structure, struct_length, &pos);
	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(old_fs);
}

void dolby_vision_dump_struct(void)
{
	dump_struct(&dovi_setting.dm_reg1,
		sizeof(dovi_setting.dm_reg1),
		dm_reg_core1_file, frame_count);
	if (dovi_setting.el_flag)
		dump_struct(&dovi_setting.comp_reg,
			sizeof(dovi_setting.comp_reg),
		comp_file, frame_count);

	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_reg2,
			sizeof(dovi_setting.dm_reg2),
		dm_reg_core2_file, frame_count);

	dump_struct(&dovi_setting.dm_reg3,
		sizeof(dovi_setting.dm_reg3),
		dm_reg_core3_file, frame_count);

	dump_struct(&dovi_setting.dm_lut1,
		sizeof(dovi_setting.dm_lut1),
		dm_lut_core1_file, frame_count);
	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_lut2,
			sizeof(dovi_setting.dm_lut2),
			dm_lut_core2_file, frame_count);
	pr_dolby_dbg("setting for frame %d dumped\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_struct);

static void dump_setting(
	struct dovi_setting_s *setting,
	int frame_cnt, int debug_flag)
{
	int i;
	uint32_t *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core1\n");
		p = (uint32_t *)&setting->dm_reg1;
		for (i = 0; i < 27; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\ncomposer\n");
		p = (uint32_t *)&setting->comp_reg;
		for (i = 0; i < 173; i++)
			pr_info("%08x\n", p[i]);
		if (is_meson_gxm_cpu()) {
			pr_info("core1 swap\n");
			for (i = DOLBY_CORE1_CLKGATE_CTRL;
				i <= DOLBY_CORE1_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_REG(i));
			pr_info("core1 real reg\n");
			for (i = DOLBY_CORE1_REG_START;
				i <= DOLBY_CORE1_REG_START + 5;
				i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_REG(i));
			pr_info("core1 composer real reg\n");
			for (i = 0; i < 173 ; i++)
				pr_info("%08x\n",
					READ_VPP_REG(
					DOLBY_CORE1_REG_START
					+ 50 + i));
		} else if (is_meson_txlx_cpu()) {
			pr_info("core1 swap\n");
			for (i = DOLBY_TV_SWAP_CTRL0;
				i <= DOLBY_TV_STATUS1; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_REG(i));
			pr_info("core1 real reg\n");
			for (i = DOLBY_TV_REG_START;
				i <= DOLBY_CORE1_REG_START + 5;
				i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_REG(i));
		}
	}

	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ncore1lut\n");
		p = (uint32_t *)&setting->dm_lut1.TmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.TmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.SmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.SmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.G2L;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable && !is_graphics_output_off()) {
		pr_info("core2\n");
		p = (uint32_t *)&setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core2 swap\n");
		for (i = DOLBY_CORE2A_CLKGATE_CTRL;
			i <= DOLBY_CORE2A_DMA_PORT; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_REG(i));
		pr_info("core2 real reg\n");
		for (i = DOLBY_CORE2A_REG_START;
			i <= DOLBY_CORE2A_REG_START + 5; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_REG(i));
	}

	if ((debug_flag & 0x20) && dump_enable && !is_graphics_output_off()) {
		pr_info("\ncore2lut\n");
		p = (uint32_t *)&setting->dm_lut2.TmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.TmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.SmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.SmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.G2L;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core3\n");
		p = (uint32_t *)&setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core3 swap\n");
		for (i = DOLBY_CORE3_CLKGATE_CTRL;
			i <= DOLBY_CORE3_OUTPUT_CSC_CRC; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_REG(i));
		pr_info("core3 real reg\n");
		for (i = DOLBY_CORE3_REG_START;
			i <= DOLBY_CORE3_REG_START + 67; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_REG(i));
	}

	if ((debug_flag & 0x40) && dump_enable
	&& (dolby_vision_mode <= DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)) {
		pr_info("\ncore3_meta %d\n", setting->md_reg3.size);
		p = setting->md_reg3.raw_metadata;
		for (i = 0; i < setting->md_reg3.size; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\n");
	}
}

void dolby_vision_dump_setting(int debug_flag)
{
	pr_dolby_dbg("\n====== setting for frame %d ======\n", frame_count);
	if (is_meson_txlx_package_962X() && !force_stb_mode)
		dump_tv_setting(&tv_dovi_setting, frame_count, debug_flag);
	else
		dump_setting(&new_dovi_setting, frame_count, debug_flag);
	pr_dolby_dbg("=== setting for frame %d dumped ===\n\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_setting);

static int sink_support_dolby_vision(const struct vinfo_s *vinfo)
{
	if (!vinfo || !vinfo->dv_info)
		return 0;
	if (vinfo->dv_info->ieeeoui != 0x00d046)
		return 0;
	if (vinfo->dv_info->block_flag != CORRECT)
		return 0;
	if (dolby_vision_flags & FLAG_DISABLE_DOVI_OUT)
		return 0;
	/* 2160p60 if TV support */
	if ((vinfo->mode >= VMODE_4K2K_SMPTE_50HZ)
	&& (vinfo->mode >= VMODE_4K2K_SMPTE_50HZ)
	&& (vinfo->dv_info->sup_2160p60hz == 1))
		return 1;
	/* 1080p~2160p30 if TV support */
	else if ((vinfo->mode >= VMODE_1080P)
	&& (vinfo->mode <= VMODE_4K2K_SMPTE_30HZ))
		return 1;
	return 0;
}

static int sink_support_hdr(const struct vinfo_s *vinfo)
{
#define HDR_SUPPORT (1 << 2)
	if (!vinfo)
		return 0;
	if (vinfo->hdr_info.hdr_support & HDR_SUPPORT)
		return 1;
	return 0;
}

static int dolby_vision_policy_process(
	int *mode, enum signal_format_e src_format)
{
	const struct vinfo_s *vinfo;
	int mode_change = 0;

	if ((!dolby_vision_enable) || (!p_funcs))
		return mode_change;

	if (is_meson_txlx_package_962X() && !force_stb_mode) {
		if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
			if (*mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output mode change %d -> %d\n",
						dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else if (*mode == DOLBY_VISION_OUTPUT_MODE_SDR8) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi tv output mode change %d -> %d\n",
						dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else {
				pr_dolby_error(
					"not support dovi output mode %d\n",
					*mode);
				return mode_change;
			}
		} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
			/* bypass dv_mode with efuse */
			if ((efuse_mode == 1) && !dolby_vision_efuse_bypass) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else
					mode_change = 0;
				return mode_change;
			}
			if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL)) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
			/* bypass dv_mode with efuse */
			if ((efuse_mode == 1) && !dolby_vision_efuse_bypass) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else
					mode_change = 0;
				return mode_change;
			}
			if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL) ||
				(src_format == FORMAT_HDR10)) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		}
		return mode_change;
	}

	vinfo = get_current_vinfo();
	if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
		/* bypass dv_mode with efuse */
		if ((efuse_mode == 1) && !dolby_vision_efuse_bypass)  {
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else
				mode_change = 0;
			return mode_change;
		}
		if (vinfo && sink_support_dolby_vision(vinfo)) {
			/* TV support DOVI, All -> DOVI */
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_hdr(vinfo)
			&& (dolby_vision_hdr10_policy & 2)) {
			/* TV support HDR, All -> HDR */
			if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_HDR10) {
				pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
				mode_change = 1;
			}
		} else {
			/* TV not support DOVI */
			if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL)) {
				/* DOVI to SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (src_format == FORMAT_HDR10
				&& (dolby_vision_hdr10_policy & 1)) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					/* HDR10 to SDR */
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				/* HDR/SDR bypass */
				pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
		/* bypass dv_mode with efuse */
		if ((efuse_mode == 1) && !dolby_vision_efuse_bypass) {
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else
				mode_change = 0;
			return mode_change;
		}
		if ((src_format == FORMAT_DOVI) ||
			(src_format == FORMAT_DOVI_LL)) {
			/* DOVI source */
			if (vinfo && sink_support_dolby_vision(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI, DOVI -> SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (src_format == FORMAT_HDR10
			&& (dolby_vision_hdr10_policy & 1)) {
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_SDR8) {
				/* HDR10 to SDR */
				pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
				mode_change = 1;
			}
		} else if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			/* HDR/SDR bypass */
			pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
		if (dolby_vision_mode != *mode) {
			pr_dolby_dbg("dovi output mode change %d -> %d\n",
				dolby_vision_mode, *mode);
			mode_change = 1;
		}
	}
	return mode_change;
}

static bool is_dovi_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	char *p;
	unsigned size = 0;
	unsigned type = 0;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;

	if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
	&& is_meson_txlx_package_962X() && !force_stb_mode) {
		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if ((req.aux_buf && req.aux_size) ||
			(dolby_vision_flags & FLAG_FORCE_DOVI_LL))
			return 1;
		else
			return 0;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		vf_notify_provider_by_name("dvbldec",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (req.dv_enhance_exist)
			return true;
		if (!req.aux_buf || !req.aux_size)
			return 0;
		p = req.aux_buf;
		while (p < req.aux_buf + req.aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x01000000)
				return true;
			p += size;
		}
	}
	return false;
}

#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)
static bool is_hdr10_frame(struct vframe_s *vf)
{
	if ((signal_transfer_characteristic == 16)
		&& ((signal_color_primaries == 9)
		|| (signal_color_primaries == 2)))
		return true;
	return false;
}

int dolby_vision_check_hdr10(struct vframe_s *vf)
{
	int mode;
	if (is_hdr10_frame(vf) && !dolby_vision_on) {
		/* dovi source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(
			&mode, FORMAT_HDR10)) {
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
			&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS))
				dolby_vision_wait_on = true;
				return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10);

void dolby_vision_vf_put(struct vframe_s *vf)
{
	int i;
	if (vf)
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]) {
					if (debug_dolby & 2)
						pr_dolby_dbg("--- put bl(%p-%lld) with el(%p-%lld) ---\n",
							vf, vf->pts_us64,
							dv_vf[i][1],
							dv_vf[i][1]->pts_us64);
					dvel_vf_put(dv_vf[i][1]);
				} else if (debug_dolby & 2) {
					pr_dolby_dbg("--- put bl(%p-%lld) ---\n",
						vf, vf->pts_us64);
				}
				dv_vf[i][0] = NULL;
				dv_vf[i][1] = NULL;
			}
		}
}
EXPORT_SYMBOL(dolby_vision_vf_put);

struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf)
{
	int i;

	if (dolby_vision_flags && p_funcs) {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]
				&& (dolby_vision_status == BYPASS_PROCESS)
				&& !is_dolby_vision_on())
					dv_vf[i][1]->type |= VIDTYPE_VD2;
				return dv_vf[i][1];
			}
		}
	}
	return NULL;
}
EXPORT_SYMBOL(dolby_vision_vf_peek_el);

static void dolby_vision_vf_add(struct vframe_s *vf, struct vframe_s *el_vf)
{
	int i;
	for (i = 0; i < 16; i++) {
		if (dv_vf[i][0] == NULL) {
			dv_vf[i][0] = vf;
			dv_vf[i][1] = el_vf;
			break;
		}
	}
}

static int dolby_vision_vf_check(struct vframe_s *vf)
{
	int i;
	for (i = 0; i < 16; i++) {
		if (dv_vf[i][0] == vf) {
			if (debug_dolby & 2) {
				if (dv_vf[i][1])
					pr_dolby_dbg("=== bl(%p-%lld) with el(%p-%lld) toggled ===\n",
						vf,
						vf->pts_us64,
						dv_vf[i][1],
						dv_vf[i][1]->pts_us64);
				else
					pr_dolby_dbg("=== bl(%p-%lld) toggled ===\n",
						vf,
						vf->pts_us64);
			}
			return 0;
		}
	}
	return 1;
}

static int parse_sei_and_meta(
	struct vframe_s *vf,
	struct provider_aux_req_s *req,
	int *total_comp_size,
	int *total_md_size,
	enum signal_format_e *src_format)
{
	int i;
	char *p;
	unsigned size = 0;
	unsigned type = 0;
	int md_size = 0;
	int comp_size = 0;
	int parser_ready = 0;
	int ret = 2;
	unsigned long flags;
	bool parser_overflow = false;

	if ((req->aux_buf == NULL)
	|| (req->aux_size == 0))
		return 1;

	p = req->aux_buf;
	while (p < req->aux_buf + req->aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;

		if (type == 0x01000000) {
			/* source is VS10 */
			*total_comp_size = 0;
			*total_md_size = 0;
			*src_format = FORMAT_DOVI;
			if (size > (sizeof(meta_buf) - 3))
				size = (sizeof(meta_buf) - 3);
			meta_buf[0] = meta_buf[1] = meta_buf[2] = 0;
			memcpy(&meta_buf[3], p+1, size-1);
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", size);
				for (i = 0; i < size+2; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i+1],
						meta_buf[i+2],
						meta_buf[i+3],
						meta_buf[i+4],
						meta_buf[i+5],
						meta_buf[i+6],
						meta_buf[i+7]);
			}

			if (!p_funcs)
				return -1;
			/* prepare metadata parser */
			spin_lock_irqsave(&dovi_lock, flags);
			parser_ready = 0;
			if (metadata_parser == NULL) {
				metadata_parser =
					p_funcs->metadata_parser_init(
						dolby_vision_flags
						& FLAG_CHANGE_SEQ_HEAD
						? 1 : 0);
				p_funcs->metadata_parser_reset(1);
				if (metadata_parser != NULL) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dolby_dbg("metadata parser init OK\n");
				}
			} else {
				if (p_funcs->metadata_parser_reset(
					metadata_parser_reset_flag) == 0)
					metadata_parser_reset_flag = 0;
					parser_ready = 1;
			}
			if (!parser_ready) {
				spin_unlock_irqrestore(&dovi_lock, flags);
				pr_dolby_error(
					"meta(%d), pts(%lld) -> metadata parser init fail\n",
					size, vf->pts_us64);
				return 2;
			}

			md_size = comp_size = 0;
			if (p_funcs->metadata_parser_process(
				meta_buf, size + 2,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true)) {
				pr_dolby_error(
					"meta(%d), pts(%lld) -> metadata parser process fail\n",
					size, vf->pts_us64);
				ret = 2;
			} else {
				if (*total_comp_size + comp_size
					< sizeof(comp_buf))
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< sizeof(md_buf))
					*total_md_size += md_size;
				else
					parser_overflow = true;
				ret = 0;
			}
			spin_unlock_irqrestore(&dovi_lock, flags);
			if (parser_overflow) {
				ret = 2;
				break;
			}
		}
		p += size;
	}

	if (*total_md_size) {
		if (debug_dolby & 1)
			pr_dolby_dbg(
			"meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			size, vf->pts_us64,
			*total_md_size, *total_comp_size);
		if ((debug_dolby & 4) && dump_enable)  {
			pr_dolby_dbg("parsed md(%d):\n", *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i+1],
					md_buf[i+2],
					md_buf[i+3],
					md_buf[i+4],
					md_buf[i+5],
					md_buf[i+6],
					md_buf[i+7]);
			}
		}
	}
	return ret;
}

#define INORM	50000
static u32 bt2020_primaries[3][2] = {
	{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},	/* G */
	{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},	/* B */
	{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static int check_primaries(struct vframe_master_display_colour_s *p_mdc)
{
	if (!p_mdc->present_flag)
		return 0;
	if ((p_mdc->primaries[0][1] > p_mdc->primaries[1][1])
	&& (p_mdc->primaries[0][1] > p_mdc->primaries[2][1])
	&& (p_mdc->primaries[2][0] > p_mdc->primaries[0][0])
	&& (p_mdc->primaries[2][0] > p_mdc->primaries[1][0])) {
		/* reasonable g,b,r */
		return 2;
	} else if ((p_mdc->primaries[0][0] > p_mdc->primaries[1][0])
	&& (p_mdc->primaries[0][0] > p_mdc->primaries[2][0])
	&& (p_mdc->primaries[1][1] > p_mdc->primaries[0][1])
	&& (p_mdc->primaries[1][1] > p_mdc->primaries[2][1])) {
		/* reasonable r,g,b */
		return 1;
	}
	/* source not usable, use standard bt2020 */
	return 0;
}

void prepare_hdr10_param(
	struct vframe_master_display_colour_s *p_mdc,
	struct hdr10_param_s *p_hdr10_param)
{
	struct vframe_content_light_level_s *p_cll =
		&p_mdc->content_light_level;
	uint8_t flag = 0;
	uint32_t max_lum = 1000 * 10000;
	uint32_t min_lum = 50;
	int primaries_type = 0;

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		p_hdr10_param->
		min_display_mastering_luminance
			= min_lum;
		p_hdr10_param->
		max_display_mastering_luminance
			= max_lum;
		p_hdr10_param->Rx
			= bt2020_primaries[2][0];
		p_hdr10_param->Ry
			= bt2020_primaries[2][1];
		p_hdr10_param->Gx
			= bt2020_primaries[0][0];
		p_hdr10_param->Gy
			= bt2020_primaries[0][1];
		p_hdr10_param->Bx
			= bt2020_primaries[1][0];
		p_hdr10_param->By
			= bt2020_primaries[1][1];
		p_hdr10_param->Wx
			= bt2020_white_point[0];
		p_hdr10_param->Wy
			= bt2020_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_pic_average_light_level = 0;
		return;
	}

	primaries_type = check_primaries(p_mdc);
	if (primaries_type == 2) {
		if ((p_hdr10_param->
		max_display_mastering_luminance
			!= p_mdc->luminance[0])
		|| (p_hdr10_param->
		min_display_mastering_luminance
			!= p_mdc->luminance[1])
		|| (p_hdr10_param->Rx
			!= p_mdc->primaries[2][0])
		|| (p_hdr10_param->Ry
			!= p_mdc->primaries[2][1])
		|| (p_hdr10_param->Gx
			!= p_mdc->primaries[0][0])
		|| (p_hdr10_param->Gy
			!= p_mdc->primaries[0][1])
		|| (p_hdr10_param->Bx
			!= p_mdc->primaries[1][0])
		|| (p_hdr10_param->By
			!= p_mdc->primaries[1][1])
		|| (p_hdr10_param->Wx
			!= p_mdc->white_point[0])
		|| (p_hdr10_param->Wy
			!= p_mdc->white_point[1])) {
			flag |= 1;
			p_hdr10_param->
			max_display_mastering_luminance
				= p_mdc->luminance[0];
			p_hdr10_param->
			min_display_mastering_luminance
				= p_mdc->luminance[1];
			p_hdr10_param->Rx
				= p_mdc->primaries[2][0];
			p_hdr10_param->Ry
				= p_mdc->primaries[2][1];
			p_hdr10_param->Gx
				= p_mdc->primaries[0][0];
			p_hdr10_param->Gy
				= p_mdc->primaries[0][1];
			p_hdr10_param->Bx
				= p_mdc->primaries[1][0];
			p_hdr10_param->By
				= p_mdc->primaries[1][1];
			p_hdr10_param->Wx
				= p_mdc->white_point[0];
			p_hdr10_param->Wy
				= p_mdc->white_point[1];
		}
	} else if (primaries_type == 1) {
		if ((p_hdr10_param->
		max_display_mastering_luminance
			!= p_mdc->luminance[0])
		|| (p_hdr10_param->
		min_display_mastering_luminance
			!= p_mdc->luminance[1])
		|| (p_hdr10_param->Rx
			!= p_mdc->primaries[0][0])
		|| (p_hdr10_param->Ry
			!= p_mdc->primaries[0][1])
		|| (p_hdr10_param->Gx
			!= p_mdc->primaries[1][0])
		|| (p_hdr10_param->Gy
			!= p_mdc->primaries[1][1])
		|| (p_hdr10_param->Bx
			!= p_mdc->primaries[2][0])
		|| (p_hdr10_param->By
			!= p_mdc->primaries[2][1])
		|| (p_hdr10_param->Wx
			!= p_mdc->white_point[0])
		|| (p_hdr10_param->Wy
			!= p_mdc->white_point[1])) {
			flag |= 1;
			p_hdr10_param->
			max_display_mastering_luminance
				= p_mdc->luminance[0];
			p_hdr10_param->
			min_display_mastering_luminance
				= p_mdc->luminance[1];
			p_hdr10_param->Rx
				= p_mdc->primaries[0][0];
			p_hdr10_param->Ry
				= p_mdc->primaries[0][1];
			p_hdr10_param->Gx
				= p_mdc->primaries[1][0];
			p_hdr10_param->Gy
				= p_mdc->primaries[1][1];
			p_hdr10_param->Bx
				= p_mdc->primaries[2][0];
			p_hdr10_param->By
				= p_mdc->primaries[2][1];
			p_hdr10_param->Wx
				= p_mdc->white_point[0];
			p_hdr10_param->Wy
				= p_mdc->white_point[1];
		}
	} else {
		if ((p_hdr10_param->
		min_display_mastering_luminance
			!= min_lum)
		|| (p_hdr10_param->
		max_display_mastering_luminance
			!= max_lum)
		|| (p_hdr10_param->Rx
			!= bt2020_primaries[2][0])
		|| (p_hdr10_param->Ry
			!= bt2020_primaries[2][1])
		|| (p_hdr10_param->Gx
			!= bt2020_primaries[0][0])
		|| (p_hdr10_param->Gy
			!= bt2020_primaries[0][1])
		|| (p_hdr10_param->Bx
			!= bt2020_primaries[1][0])
		|| (p_hdr10_param->By
			!= bt2020_primaries[1][1])
		|| (p_hdr10_param->Wx
			!= bt2020_white_point[0])
		|| (p_hdr10_param->Wy
			!= bt2020_white_point[1])) {
			flag |= 2;
			p_hdr10_param->
			min_display_mastering_luminance
				= min_lum;
			p_hdr10_param->
			max_display_mastering_luminance
				= max_lum;
			p_hdr10_param->Rx
				= bt2020_primaries[2][0];
			p_hdr10_param->Ry
				= bt2020_primaries[2][1];
			p_hdr10_param->Gx
				= bt2020_primaries[0][0];
			p_hdr10_param->Gy
				= bt2020_primaries[0][1];
			p_hdr10_param->Bx
				= bt2020_primaries[1][0];
			p_hdr10_param->By
				= bt2020_primaries[1][1];
			p_hdr10_param->Wx
				= bt2020_white_point[0];
			p_hdr10_param->Wy
				= bt2020_white_point[1];
		}
	}

	if (p_cll->present_flag) {
		if ((p_hdr10_param->max_content_light_level
			!= p_cll->max_content)
		|| (p_hdr10_param->max_pic_average_light_level
			!= p_cll->max_pic_average))
			flag |= 4;
		if (flag & 4) {
			p_hdr10_param->max_content_light_level
				= p_cll->max_content;
			p_hdr10_param->max_pic_average_light_level
				= p_cll->max_pic_average;
		}
	} else {
		if ((p_hdr10_param->max_content_light_level != 0)
		|| (p_hdr10_param->max_pic_average_light_level != 0)) {
			p_hdr10_param->max_content_light_level = 0;
			p_hdr10_param->max_pic_average_light_level = 0;
			flag |= 8;
		}
	}

	if (flag) {
		pr_dolby_dbg("HDR10: primaries %d, maxcontent %d, flag %d\n",
			p_mdc->present_flag,
			p_cll->present_flag,
			flag);
		pr_dolby_dbg("\tR = %04x, %04x\n",
			p_hdr10_param->Rx,
			p_hdr10_param->Ry);
		pr_dolby_dbg("\tG = %04x, %04x\n",
			p_hdr10_param->Gx,
			p_hdr10_param->Gy);
		pr_dolby_dbg("\tB = %04x, %04x\n",
			p_hdr10_param->Bx,
			p_hdr10_param->By);
		pr_dolby_dbg("\tW = %04x, %04x\n",
			p_hdr10_param->Wx,
			p_hdr10_param->Wy);
		pr_dolby_dbg("\tMax = %d\n",
			p_hdr10_param->
			max_display_mastering_luminance);
		pr_dolby_dbg("\tMin = %d\n",
			p_hdr10_param->
			min_display_mastering_luminance);
		pr_dolby_dbg("\tMCLL = %d\n",
			p_hdr10_param->
			max_content_light_level);
		pr_dolby_dbg("\tMPALL = %d\n\n",
			p_hdr10_param->
			max_pic_average_light_level);
	}
}

#ifdef V2_4
static int prepare_vsif_pkt(
	struct dv_vsif_para *vsif,
	struct dovi_setting_s *setting,
	const struct vinfo_s *vinfo)
{
	if (!vsif || !vinfo || !setting || !vinfo->dv_info)
		return -1;
	vsif->vers.ver2.low_latency =
		setting->dovi_ll_enable;
	vsif->vers.ver2.dobly_vision_signal = 1;
	if (vinfo->dv_info &&
		vinfo->dv_info->sup_backlight_control
		&& (setting->ext_md.available_level_mask
		& EXT_MD_AVAIL_LEVEL_2)) {
		vsif->vers.ver2.backlt_ctrl_MD_present = 1;
		vsif->vers.ver2.eff_tmax_PQ_hi =
			setting->ext_md.level_2.target_max_PQ_hi & 0xf;
		vsif->vers.ver2.eff_tmax_PQ_low =
			setting->ext_md.level_2.target_max_PQ_lo;
	} else {
		vsif->vers.ver2.backlt_ctrl_MD_present = 0;
		vsif->vers.ver2.eff_tmax_PQ_hi = 0;
		vsif->vers.ver2.eff_tmax_PQ_low = 0;
	}

	if (setting->dovi_ll_enable
		&& (setting->ext_md.available_level_mask
		& EXT_MD_AVAIL_LEVEL_255)) {
		vsif->vers.ver2.auxiliary_MD_present = 1;
		vsif->vers.ver2.auxiliary_runmode =
			setting->ext_md.level_255.dm_run_mode;
		vsif->vers.ver2.auxiliary_runversion =
			setting->ext_md.level_255.dm_run_version;
		vsif->vers.ver2.auxiliary_debug0 =
			setting->ext_md.level_255.dm_debug0;
	} else {
		vsif->vers.ver2.auxiliary_MD_present = 0;
		vsif->vers.ver2.auxiliary_runmode = 0;
		vsif->vers.ver2.auxiliary_runversion = 0;
		vsif->vers.ver2.auxiliary_debug0 = 0;
	}
	return 0;
}
#endif

static bool send_hdmi_pkt(
	enum signal_format_e dst_format,
	const struct vinfo_s *vinfo)
{
	struct hdr_10_infoframe_s *p_hdr;
	int i;
	bool flag = false;

	if (dst_format == FORMAT_HDR10) {
		p_hdr = &dovi_setting.hdr_info;
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (9 << 16)	/* bt2020 */
			| (0x10 << 8)	/* bt2020-10 */
			| (10 << 0);/* bt2020c */
		if (hdr10_data.primaries[0][0] !=
			((p_hdr->display_primaries_x_0_MSB << 8)
			| p_hdr->display_primaries_x_0_LSB))
			flag = true;
		hdr10_data.primaries[0][0] =
			(p_hdr->display_primaries_x_0_MSB << 8)
			| p_hdr->display_primaries_x_0_LSB;

		if (hdr10_data.primaries[0][1] !=
			((p_hdr->display_primaries_y_0_MSB << 8)
			| p_hdr->display_primaries_y_0_LSB))
			flag = true;
		hdr10_data.primaries[0][1] =
			(p_hdr->display_primaries_y_0_MSB << 8)
			| p_hdr->display_primaries_y_0_LSB;

		if (hdr10_data.primaries[1][0] !=
			((p_hdr->display_primaries_x_1_MSB << 8)
			| p_hdr->display_primaries_x_1_LSB))
			flag = true;
		hdr10_data.primaries[1][0] =
			(p_hdr->display_primaries_x_1_MSB << 8)
			| p_hdr->display_primaries_x_1_LSB;

		if (hdr10_data.primaries[1][1] !=
			((p_hdr->display_primaries_y_1_MSB << 8)
			| p_hdr->display_primaries_y_1_LSB))
			flag = true;
		hdr10_data.primaries[1][1] =
			(p_hdr->display_primaries_y_1_MSB << 8)
			| p_hdr->display_primaries_y_1_LSB;

		if (hdr10_data.primaries[2][0] !=
			((p_hdr->display_primaries_x_2_MSB << 8)
			| p_hdr->display_primaries_x_2_LSB))
			flag = true;
		hdr10_data.primaries[2][0] =
			(p_hdr->display_primaries_x_2_MSB << 8)
			| p_hdr->display_primaries_x_2_LSB;

		if (hdr10_data.primaries[2][1] !=
			((p_hdr->display_primaries_y_2_MSB << 8)
			| p_hdr->display_primaries_y_2_LSB))
			flag = true;
		hdr10_data.primaries[2][1] =
			(p_hdr->display_primaries_y_2_MSB << 8)
			| p_hdr->display_primaries_y_2_LSB;

		if (hdr10_data.white_point[0] !=
			((p_hdr->white_point_x_MSB << 8)
			| p_hdr->white_point_x_LSB))
			flag = true;
		hdr10_data.white_point[0] =
			(p_hdr->white_point_x_MSB << 8)
			| p_hdr->white_point_x_LSB;

		if (hdr10_data.white_point[1] !=
			((p_hdr->white_point_y_MSB << 8)
			| p_hdr->white_point_y_LSB))
			flag = true;
		hdr10_data.white_point[1] =
			(p_hdr->white_point_y_MSB << 8)
			| p_hdr->white_point_y_LSB;

		if (hdr10_data.luminance[0] !=
			((p_hdr->max_display_mastering_luminance_MSB << 8)
			| p_hdr->max_display_mastering_luminance_LSB))
			flag = true;
		hdr10_data.luminance[0] =
			(p_hdr->max_display_mastering_luminance_MSB << 8)
			| p_hdr->max_display_mastering_luminance_LSB;

		if (hdr10_data.luminance[1] !=
			((p_hdr->min_display_mastering_luminance_MSB << 8)
			| p_hdr->min_display_mastering_luminance_LSB))
			flag = true;
		hdr10_data.luminance[1] =
			(p_hdr->min_display_mastering_luminance_MSB << 8)
			| p_hdr->min_display_mastering_luminance_LSB;

		if (hdr10_data.max_content !=
			((p_hdr->max_content_light_level_MSB << 8)
			| p_hdr->max_content_light_level_LSB))
			flag = true;
		hdr10_data.max_content =
			(p_hdr->max_content_light_level_MSB << 8)
			| p_hdr->max_content_light_level_LSB;

		if (hdr10_data.max_frame_average !=
			((p_hdr->max_frame_average_light_level_MSB << 8)
			| p_hdr->max_frame_average_light_level_LSB))
			flag = true;
		hdr10_data.max_frame_average =
			(p_hdr->max_frame_average_light_level_MSB << 8)
			| p_hdr->max_frame_average_light_level_LSB;
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&hdr10_data);
		if (vinfo->fresh_tx_vsif_pkt)
			vinfo->fresh_tx_vsif_pkt(0, 0, NULL);

		if (flag) {
			pr_dolby_dbg("Info frame for hdr10 changed:\n");
			for (i = 0; i < 3; i++)
				pr_dolby_dbg(
						"\tprimaries[%1d] = %04x, %04x\n",
						i,
						hdr10_data.primaries[i][0],
						hdr10_data.primaries[i][1]);
			pr_dolby_dbg("\twhite_point = %04x, %04x\n",
				hdr10_data.white_point[0],
				hdr10_data.white_point[1]);
			pr_dolby_dbg("\tMax = %d\n",
				hdr10_data.luminance[0]);
			pr_dolby_dbg("\tMin = %d\n",
				hdr10_data.luminance[1]);
			pr_dolby_dbg("\tMCLL = %d\n",
				hdr10_data.max_content);
			pr_dolby_dbg("\tMPALL = %d\n\n",
				hdr10_data.max_frame_average);
		}
	} else if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;
		memset(&vsif, 0, sizeof(vsif));
#ifdef V2_4
		prepare_vsif_pkt(
			&vsif, &dovi_setting, vinfo);
#endif
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		for (i = 0; i < 3; i++) {
			hdr10_data.primaries[i][0] = 0;
			hdr10_data.primaries[i][1] = 0;
		}
		hdr10_data.white_point[0] = 0;
		hdr10_data.white_point[1] = 0;
		hdr10_data.luminance[0] = 0;
		hdr10_data.luminance[1] = 0;
		hdr10_data.max_content = 0;
		hdr10_data.max_frame_average = 0;
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&hdr10_data);
		if (vinfo->fresh_tx_vsif_pkt) {
#ifdef V2_4
			if (dovi_setting.dovi_ll_enable)
				vinfo->fresh_tx_vsif_pkt(
					EOTF_T_LL_MODE,
					dovi_setting.diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif);
			else
#endif
				vinfo->fresh_tx_vsif_pkt(
					EOTF_T_DOLBYVISION,
					dolby_vision_mode ==
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL
					? RGB_8BIT : YUV422_BIT12, &vsif);
		}
	} else {
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		for (i = 0; i < 3; i++) {
			hdr10_data.primaries[i][0] = 0;
			hdr10_data.primaries[i][1] = 0;
		}
		hdr10_data.white_point[0] = 0;
		hdr10_data.white_point[1] = 0;
		hdr10_data.luminance[0] = 0;
		hdr10_data.luminance[1] = 0;
		hdr10_data.max_content = 0;
		hdr10_data.max_frame_average = 0;
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&hdr10_data);
		if (vinfo->fresh_tx_vsif_pkt)
			vinfo->fresh_tx_vsif_pkt(0, 0, NULL);
	}
	return flag;
}

static uint32_t null_vf_cnt;
static bool video_off_handled;
static int is_video_output_off(struct vframe_s *vf)
{
	if ((READ_VPP_REG(VPP_MISC) & (1<<10)) == 0) {
		/*Not reset frame0/1 clipping
		when core off to avoid green garbage*/
		if (is_meson_txlx_package_962X() && (vf == NULL) &&
			(dolby_vision_on_count <= dolby_vision_run_mode_delay))
			return 0;
		if (vf == NULL)
			null_vf_cnt++;
		else
			null_vf_cnt = 0;
		if (null_vf_cnt > dolby_vision_wait_delay + 3) {
			null_vf_cnt = 0;
			return 1;
		}
	} else
		video_off_handled = 0;
	return 0;
}

static void calculate_panel_max_pq(
	const struct vinfo_s *vinfo,
	struct TargetDisplayConfig *config)
{
	uint32_t max_lin = tv_max_lin;
	uint16_t max_pq = tv_max_pq;

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		return;
	if (panel_max_lumin)
		max_lin = panel_max_lumin;
	else if (vinfo->hdr_info.sink_flag)
		max_lin = vinfo->hdr_info.lumi_max;
	if (max_lin < 100)
		max_lin = 100;
	else if (max_lin > 4000)
		max_lin = 4000;
	if (max_lin != tv_max_lin) {
		if (max_lin < 500) {
			max_lin = max_lin - 100 + 10;
			max_lin = (max_lin / 20) * 20 + 100;
			max_pq = L2PQ_100_500[(max_lin - 100) / 20];
		} else {
			max_lin = max_lin - 500 + 50;
			max_lin = (max_lin / 100) * 100 + 500;
			max_pq = L2PQ_100_500[(max_lin - 500) / 100];
		}
		pr_info("panel max lumin changed from %d(%d) to %d(%d)\n",
			tv_max_lin, tv_max_pq, max_lin, max_pq);
		tv_max_lin = max_lin;
		tv_max_pq = max_pq;
		config->max_lin =
		config->max_lin_dupli =
			tv_max_lin << 18;
		config->maxPq =
		config->maxPq_dupli =
			tv_max_pq;
	}
}

static u32 last_total_md_size;
static u32 last_total_comp_size;
/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
int dolby_vision_parse_metadata(
	struct vframe_s *vf, u8 toggle_mode, bool bypass_release)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct vframe_s *el_vf;
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	int flag;
	enum signal_format_e src_format = FORMAT_SDR;
	enum signal_format_e check_format;
	enum signal_format_e dst_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	bool el_flag = 0;
	bool el_halfsize_flag = 1;
	uint32_t w = 3840;
	uint32_t h = 2160;
	int meta_flag_bl = 1;
	int meta_flag_el = 1;
	int src_chroma_format = 0;
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	uint32_t target_lumin_max = 0;
	enum input_mode_e input_mode = INPUT_MODE_OTT;
	enum priority_mode_e pri_mode = VIDEO_PRIORITY;
	u32 graphic_min = 50; /* 0.0001 */
	u32 graphic_max = 100; /* 1 */
	if (!dolby_vision_enable)
		return -1;

	if (vf) {
		video_frame = true;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	if (is_meson_txlx_package_962X() && !force_stb_mode && vf
	&& (vf->source_type == VFRAME_SOURCE_TYPE_HDMI)) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;
		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		input_mode = INPUT_MODE_HDMI;
		if (debug_dolby & 1)
			pr_dolby_dbg("vdin0 get aux data %p %x, ll:%d\n",
				req.aux_buf, req.aux_size, req.low_latency);

		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL)
			|| (req.low_latency == 1)) {
			src_format = FORMAT_DOVI_LL;
			src_chroma_format = 0;
			memset(md_buf, 0, sizeof(md_buf));
			memset(comp_buf, 0, sizeof(comp_buf));
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_buf && req.aux_size) {
			memcpy(md_buf, req.aux_buf, req.aux_size);
			src_format = FORMAT_DOVI;
		} else {
			if (toggle_mode == 2)
				src_format = tv_dovi_setting.src_format;
			if (vf->type & VIDTYPE_VIU_422)
				src_chroma_format = 1;
			p_mdc =	&vf->prop.master_display_colour;
			if (is_hdr10_frame(vf)) {
				src_format = FORMAT_HDR10;
				/* prepare parameter from hdmi for hdr10 */
				p_mdc->luminance[0] *= 10000;
				prepare_hdr10_param(
					p_mdc, &hdr10_param);
			}
		}
		if (debug_dolby & 4) {
			pr_dolby_dbg("metadata(%d):\n", req.aux_size);
			for (i = 0; i < req.aux_size + 8; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i+1],
					md_buf[i+2],
					md_buf[i+3],
					md_buf[i+4],
					md_buf[i+5],
					md_buf[i+6],
					md_buf[i+7]);
		}

		total_md_size = req.aux_size;
		total_comp_size = 0;
		meta_flag_bl = 0;
		if (req.aux_buf && req.aux_size) {
			last_total_md_size = total_md_size;
			last_total_comp_size = total_comp_size;
		} else if (toggle_mode == 2) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;
		}
		if (debug_dolby & 1)
			pr_dolby_dbg(
			"frame %d pts %lld, format: %s\n",
			frame_count, vf->pts_us64,
			(src_format == FORMAT_HDR10) ? "HDR10" :
			((src_format == FORMAT_DOVI) ? "DOVI" :
			((src_format == FORMAT_DOVI_LL) ? "DOVI_LL" : "SDR")));

		if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dolby_dbg(
					"+++ get bl(%p-%lld) +++\n",
					vf, vf->pts_us64);
			dolby_vision_vf_add(vf, NULL);
		}
	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		/* check source format */
		input_mode = INPUT_MODE_OTT;
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		vf_notify_provider_by_name("dvbldec",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (debug_dolby & 1 && req.aux_buf && req.aux_size)
			pr_dolby_dbg("dvbldec get aux data %p %x\n",
				req.aux_buf, req.aux_size);
		/* parse meta in base layer */
		if (toggle_mode != 2)
			meta_flag_bl =
			parse_sei_and_meta(
				vf, &req,
				&total_comp_size,
				&total_md_size,
				&src_format);
		else if (is_dolby_vision_stb_mode())
			src_format = dovi_setting.src_format;
		else if (is_meson_txlx_package_962X())
			src_format = tv_dovi_setting.src_format;

		if ((src_format != FORMAT_DOVI)
			&& is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &hdr10_param);
			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			memset(&req, 0, sizeof(req));
		}
#ifdef V2_4
		/* TODO: need 962e ? */
		if ((src_format == FORMAT_SDR)
			&& is_dolby_vision_stb_mode()
			&& !req.dv_enhance_exist)
			src_bdp = 10;
#endif
		if (((debug_dolby & 1)
			|| (frame_count == 0))
			&& (toggle_mode == 1))
			pr_info(
			"DOLBY: frame %d pts %lld, src bdp: %d format: %s, aux_size:%d, enhance: %d\n",
			frame_count, vf->pts_us64, src_bdp,
			(src_format == FORMAT_HDR10) ? "HDR10" :
			(src_format == FORMAT_DOVI ? "DOVI" :
			(req.dv_enhance_exist ? "DOVI (el meta)" : "SDR")),
			req.aux_size, req.dv_enhance_exist);

		if (req.dv_enhance_exist &&
			(toggle_mode == 1)) {
			el_vf = dvel_vf_get();
			if (el_vf &&
			((el_vf->pts_us64 == vf->pts_us64)
			|| !(dolby_vision_flags & FLAG_CHECK_ES_PTS))) {
				if (debug_dolby & 2)
					pr_dolby_dbg("+++ get bl(%p-%lld) with el(%p-%lld) +++\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				if (meta_flag_bl) {
					int el_md_size = 0;
					int el_comp_size = 0;
					el_req.vf = el_vf;
					el_req.bot_flag = 0;
					el_req.aux_buf = NULL;
					el_req.aux_size = 0;
					vf_notify_provider_by_name("dveldec",
					VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					(void *)&el_req);
					if (el_req.aux_buf
						&& el_req.aux_size) {
						meta_flag_el =
							parse_sei_and_meta(
							el_vf, &el_req,
							&el_comp_size,
							&el_md_size,
							&src_format);
					}
					if (!meta_flag_el) {
						total_comp_size =
							el_comp_size;
						total_md_size =
							el_md_size;
						src_bdp = 12;
					}
					/* force set format as DOVI
						when meta data error */
					if (meta_flag_el
						&& el_req.aux_buf
						&& el_req.aux_size)
						src_format = FORMAT_DOVI;
					if (debug_dolby & 2)
						pr_dolby_dbg(
						"meta data el mode: el_src_format: %d, meta_flag_el: %d\n",
						src_format,
						meta_flag_el);
					if (meta_flag_el && frame_count == 0)
						pr_info(
							"DOVI el meta mode, but parser meta error, el vf %p, size:%d\n",
							el_req.aux_buf,
							el_req.aux_size);
				}
				dolby_vision_vf_add(vf, el_vf);
				el_flag = 1;
				if (vf->width == el_vf->width)
					el_halfsize_flag = 0;
			} else {
				if (!el_vf)
					pr_dolby_error(
						"bl(%p-%lld) not found el\n",
						vf, vf->pts_us64);
				else
					pr_dolby_error(
						"bl(%p-%lld) not found el(%p-%lld)\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
			}
		} else if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dolby_dbg(
					"+++ get bl(%p-%lld) +++\n",
					vf, vf->pts_us64);
			dolby_vision_vf_add(vf, NULL);
		}

		if ((toggle_mode == 0)
			&& req.dv_enhance_exist)
			el_flag = 1;

		if (toggle_mode != 2) {
			last_total_md_size = total_md_size;
			last_total_comp_size = total_comp_size;
		} else if (meta_flag_bl && meta_flag_el) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;
			if (is_dolby_vision_stb_mode())
				el_flag = dovi_setting.el_flag;
			else
				el_flag = tv_dovi_setting.el_flag;
			meta_flag_bl = 0;
		}
		if ((src_format == FORMAT_DOVI)
		&& meta_flag_bl && meta_flag_el) {
			/* dovi frame no meta or meta error */
			/* use old setting for this frame   */
			return -1;
		}
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	if ((src_format == FORMAT_DOVI)
	&& meta_flag_bl && meta_flag_el) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		return -1;
	}

	/* if not DOVI, release metadata_parser */
	if ((src_format != FORMAT_DOVI)
		&& metadata_parser
		&& !bypass_release) {
		if (p_funcs)
			p_funcs->metadata_parser_release();
		metadata_parser = NULL;
	}

	check_format = src_format;
	current_mode = dolby_vision_mode;
	if (dolby_vision_policy_process(
		&current_mode, check_format)) {
		if (!dolby_vision_wait_init)
			dolby_vision_set_toggle_flag(1);
		pr_dolby_dbg("[dolby_vision_parse_metadata] output change from %d to %d\n",
			dolby_vision_mode, current_mode);
		dolby_vision_mode = current_mode;
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		return -1;
	}

	if (!p_funcs)
		return -1;

	/* TV core */
	if (is_meson_txlx_package_962X() && !force_stb_mode) {
		if (!pq_config_set_flag) {
			memcpy(&pq_config.target_display_config,
				&def_tgt_display_cfg,
				sizeof(def_tgt_display_cfg));
			pq_config_set_flag = true;
		}
		calculate_panel_max_pq(
			vinfo,
			&pq_config.target_display_config);
		tv_dovi_setting.video_width = w << 16;
		tv_dovi_setting.video_height = h << 16;
		pq_config.target_display_config.tuningMode =
				dolby_vision_tunning_mode;
		if (dolby_vision_flags & FLAG_DISABLE_COMPOSER) {
			pq_config.target_display_config.tuningMode |=
				TUNINGMODE_EL_FORCEDDISABLE;
			el_halfsize_flag = 0;
		} else
			pq_config.target_display_config.tuningMode &=
				(~TUNINGMODE_EL_FORCEDDISABLE);
#ifdef V1_5
		/* disable global dimming */
		if (dolby_vision_flags & FLAG_CERTIFICAION)
			pq_config.target_display_config.tuningMode &=
				(~TUNINGMODE_EXTLEVEL4_DISABLE);
		else
			pq_config.target_display_config.tuningMode |=
				TUNINGMODE_EXTLEVEL4_DISABLE;

		if (src_format != tv_dovi_setting.src_format)
			p_funcs->tv_control_path(
				FORMAT_INVALID, 0,
				NULL, 0,
				NULL, 0,
				0, 0,
				SIG_RANGE_SMPTE,
				NULL, NULL,
				0,
				NULL,
				NULL);
#endif
		flag = p_funcs->tv_control_path(
			src_format, input_mode,
			comp_buf, total_comp_size,
			md_buf, total_md_size,
			src_bdp,
			src_chroma_format,
			SIG_RANGE_SMPTE, /* bit/chroma/range */
			&pq_config, &menu_param,
			(!el_flag) ||
			(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
			&hdr10_param,
			&tv_dovi_setting);
		if (flag >= 0) {
			if (input_mode == INPUT_MODE_HDMI) {
				if (h > 1080)
					tv_dovi_setting.core1_reg_lut[1] =
						0x0000000100000043;
				else
					tv_dovi_setting.core1_reg_lut[1] =
						0x0000000100000042;
			} else {
				if (src_format == FORMAT_HDR10)
					tv_dovi_setting.core1_reg_lut[1] =
						0x000000010000404c;
				else if (el_halfsize_flag)
					tv_dovi_setting.core1_reg_lut[1] =
						0x000000010000004c;
				else
					tv_dovi_setting.core1_reg_lut[1] =
						0x0000000100000044;
			}
			/* enable CRC */
			if (dolby_vision_flags & FLAG_CERTIFICAION)
				tv_dovi_setting.core1_reg_lut[3] =
					0x000000ea00000001;
			tv_dovi_setting.src_format = src_format;
			tv_dovi_setting.el_flag = el_flag;
			tv_dovi_setting.el_halfsize_flag = el_halfsize_flag;
			tv_dovi_setting.video_width = w;
			tv_dovi_setting.video_height = h;
			tv_dovi_setting.input_mode = input_mode;
			tv_dovi_setting_change_flag = true;
			dovi_setting_video_flag = video_frame;
			if (debug_dolby & 1) {
				if (el_flag)
					pr_dolby_dbg("tv setting %s-%d: flag=%02x,md=%d,comp=%d\n",
						input_mode == INPUT_MODE_HDMI ?
						"hdmi" : "ott",
						src_format,
						flag,
						total_md_size,
						total_comp_size);
				else
					pr_dolby_dbg("tv setting %s-%d: flag=%02x,md=%d\n",
						input_mode == INPUT_MODE_HDMI ?
						"hdmi" : "ott",
						src_format,
						flag,
						total_md_size);
			}
			dump_tv_setting(&tv_dovi_setting,
				frame_count, debug_dolby);
			el_mode = el_flag;
			return 0; /* setting updated */
		} else {
			tv_dovi_setting.video_width = 0;
			tv_dovi_setting.video_height = 0;
			pr_dolby_error("tv_control_path() failed\n");
		}
		return -1;
	}

	/* STB core */
	/* check target luminance */
	if (is_graphics_output_off()) {
		graphic_min = 0;
		graphic_max = 0;
		is_osd_off = true;
	} else {
		graphic_min = dolby_vision_graphic_min;
		graphic_max = dolby_vision_graphic_max;
		/* force reset core2 when osd off->on */
		/* TODO: 962e need ? */
		if (is_osd_off)
			force_reset_core2 = true;
		is_osd_off = false;
	}
	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->dv_info->ieeeoui == 0x00d046) {
			if (vinfo->dv_info->ver == 0) {
				/* need lookup PQ table ... */
				/*
				graphic_min =
				dolby_vision_target_min =
					vinfo->dv_info.ver0.target_min_pq;
				graphic_max =
				target_lumin_max =
					vinfo->dv_info.ver0.target_max_pq;
				*/
			} else if (vinfo->dv_info->ver == 1) {
				if (vinfo->dv_info->tmaxLUM) {
					/* Target max luminance = 100+50*CV */
					graphic_max =
					target_lumin_max =
						(vinfo->dv_info->tmaxLUM
						* 50 + 100);
					/* Target min luminance = (CV/127)^2 */
					graphic_min =
					dolby_vision_target_min =
						(vinfo->dv_info->tminLUM ^ 2)
						* 10000 / (127 * 127);
				}
			}
		} else if (vinfo->hdr_info.hdr_support & 4) {
			if (vinfo->hdr_info.lumi_max) {
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				graphic_max =
				target_lumin_max = 50 *
					(2 ^ (vinfo->hdr_info.lumi_max >> 5));
				/* Desired Content Min Luminance =
					Desired Content Max Luminance
					* (CV/255) * (CV/255) / 100	*/
				graphic_min =
				dolby_vision_target_min =
					target_lumin_max * 10000
					* vinfo->hdr_info.lumi_min
					* vinfo->hdr_info.lumi_min
					/ (255 * 255 * 100);
			}
		}
		if (target_lumin_max) {
			dolby_vision_target_max[0][0] =
			dolby_vision_target_max[0][1] =
			dolby_vision_target_max[1][0] =
			dolby_vision_target_max[1][1] =
			dolby_vision_target_max[2][0] =
			dolby_vision_target_max[2][1] =
				target_lumin_max;
		} else {
			memcpy(
				dolby_vision_target_max,
				dolby_vision_default_max,
				sizeof(dolby_vision_target_max));
		}
	}

	/* check dst format */
	if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
	|| (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT))
		dst_format = FORMAT_DOVI;
	else if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_HDR10)
		dst_format = FORMAT_HDR10;
	else
		dst_format = FORMAT_SDR;

#ifdef V2_4
	if ((src_format != dovi_setting.src_format)
		|| (dst_format != dovi_setting.dst_format))
		p_funcs->control_path(
			FORMAT_INVALID, 0,
			comp_buf, 0,
			md_buf, 0,
			0, 0, 0, SIG_RANGE_SMPTE,
			0, 0, 0, 0,
			0,
			&hdr10_param,
			&new_dovi_setting);
	if (!vsvdb_config_set_flag) {
		memset(&new_dovi_setting.vsvdb_tbl[0],
			0, sizeof(new_dovi_setting.vsvdb_tbl));
		new_dovi_setting.vsvdb_len = 0;
		new_dovi_setting.vsvdb_changed = 1;
		vsvdb_config_set_flag = true;
	}
	if ((dolby_vision_flags &
		FLAG_DISABLE_LOAD_VSVDB) == 0) {
		/* check if vsvdb is changed */
		if (vinfo && vinfo->dv_info &&
			(vinfo->dv_info->ieeeoui == 0x00d046)
			&& (vinfo->dv_info->block_flag == CORRECT)) {
			if (new_dovi_setting.vsvdb_len
				!= (vinfo->dv_info->length + 1))
				new_dovi_setting.vsvdb_changed = 1;
			else if (memcmp(&new_dovi_setting.vsvdb_tbl[0],
				&vinfo->dv_info->rawdata[0],
				vinfo->dv_info->length + 1))
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
				0, sizeof(new_dovi_setting.vsvdb_tbl));
			memcpy(&new_dovi_setting.vsvdb_tbl[0],
				&vinfo->dv_info->rawdata[0],
				vinfo->dv_info->length + 1);
			new_dovi_setting.vsvdb_len =
				vinfo->dv_info->length + 1;
			if (new_dovi_setting.vsvdb_changed
				&& new_dovi_setting.vsvdb_len) {
				int k = 0;
				pr_dolby_dbg(
					"new vsvdb[%d]:\n",
					new_dovi_setting.vsvdb_len);
				pr_dolby_dbg(
					"---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					new_dovi_setting.vsvdb_tbl[k + 0],
					new_dovi_setting.vsvdb_tbl[k + 1],
					new_dovi_setting.vsvdb_tbl[k + 2],
					new_dovi_setting.vsvdb_tbl[k + 3],
					new_dovi_setting.vsvdb_tbl[k + 4],
					new_dovi_setting.vsvdb_tbl[k + 5],
					new_dovi_setting.vsvdb_tbl[k + 6],
					new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg(
					"---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					new_dovi_setting.vsvdb_tbl[k + 0],
					new_dovi_setting.vsvdb_tbl[k + 1],
					new_dovi_setting.vsvdb_tbl[k + 2],
					new_dovi_setting.vsvdb_tbl[k + 3],
					new_dovi_setting.vsvdb_tbl[k + 4],
					new_dovi_setting.vsvdb_tbl[k + 5],
					new_dovi_setting.vsvdb_tbl[k + 6],
					new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg(
					"---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					new_dovi_setting.vsvdb_tbl[k + 0],
					new_dovi_setting.vsvdb_tbl[k + 1],
					new_dovi_setting.vsvdb_tbl[k + 2],
					new_dovi_setting.vsvdb_tbl[k + 3],
					new_dovi_setting.vsvdb_tbl[k + 4],
					new_dovi_setting.vsvdb_tbl[k + 5],
					new_dovi_setting.vsvdb_tbl[k + 6],
					new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg(
					"---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					new_dovi_setting.vsvdb_tbl[k + 0],
					new_dovi_setting.vsvdb_tbl[k + 1],
					new_dovi_setting.vsvdb_tbl[k + 2],
					new_dovi_setting.vsvdb_tbl[k + 3],
					new_dovi_setting.vsvdb_tbl[k + 4],
					new_dovi_setting.vsvdb_tbl[k + 5],
					new_dovi_setting.vsvdb_tbl[k + 6],
					new_dovi_setting.vsvdb_tbl[k + 7]);
			}
		} else {
			if (new_dovi_setting.vsvdb_len)
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
				0, sizeof(new_dovi_setting.vsvdb_tbl));
			new_dovi_setting.vsvdb_len = 0;
		}
	}
	if (dolby_vision_graphics_priority ||
		(dolby_vision_flags &
		FLAG_PRIORITY_GRAPHIC))
		pri_mode = GRAPHIC_PRIORITY;
	else
		pri_mode = VIDEO_PRIORITY;

	if (dst_format == FORMAT_DOVI) {
		if ((dolby_vision_flags
			& FLAG_FORCE_DOVI_LL) ||
			(dolby_vision_ll_policy
			>= DOLBY_VISION_LL_YUV422))
			new_dovi_setting.use_ll_flag = 1;
		else
			new_dovi_setting.use_ll_flag = 0;
		if ((dolby_vision_ll_policy ==
			DOLBY_VISION_LL_RGB444)
			|| (dolby_vision_flags
			& FLAG_FORCE_RGB_OUTPUT))
			new_dovi_setting.ll_rgb_desired = 1;
		else
			new_dovi_setting.ll_rgb_desired = 0;
	} else {
		new_dovi_setting.use_ll_flag = 0;
		new_dovi_setting.ll_rgb_desired = 0;
	}
	if ((dst_format == FORMAT_HDR10) &&
		(dolby_vision_flags & FLAG_DOVI2HDR10_NOMAPPING))
		new_dovi_setting.dovi2hdr10_nomapping = 1;
	else
		new_dovi_setting.dovi2hdr10_nomapping = 0;

	/* always use rgb setting */
#if 1
	new_dovi_setting.g_bitdepth = 8;
	new_dovi_setting.g_format = GF_SDR_RGB;
#else
	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		new_dovi_setting.g_bitdepth = 8;
		new_dovi_setting.g_format = GF_SDR_RGB;
	} else {
		new_dovi_setting.g_bitdepth = 10;
		new_dovi_setting.g_format = GF_SDR_YUV;
	}
#endif
	new_dovi_setting.diagnostic_enable = 0;
	new_dovi_setting.diagnostic_mux_select = 0;
	new_dovi_setting.dovi_ll_enable = 0;
	if (vinfo) {
		new_dovi_setting.vout_width = vinfo->width;
		new_dovi_setting.vout_height = vinfo->height;
	} else {
		new_dovi_setting.vout_width = 0;
		new_dovi_setting.vout_height = 0;
	}
	memset(&new_dovi_setting.ext_md, 0, sizeof(struct ext_md_s));
#endif
	new_dovi_setting.video_width = w << 16;
	new_dovi_setting.video_height = h << 16;
	flag = p_funcs->control_path(
		src_format, dst_format,
		comp_buf, total_comp_size,
		md_buf, total_md_size,
		pri_mode,
		src_bdp, 0, SIG_RANGE_SMPTE, /* bit/chroma/range */
		graphic_min,
		graphic_max * 10000,
		dolby_vision_target_min,
		dolby_vision_target_max[src_format][dst_format] * 10000,
		(!el_flag) ||
		(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
		&hdr10_param,
		&new_dovi_setting);
	if (flag >= 0) {
#ifdef V2_4
		stb_core_setting_update_flag = flag;
		if ((dolby_vision_flags
			& FLAG_FORCE_DOVI_LL)
			&& (dst_format == FORMAT_DOVI))
			new_dovi_setting.dovi_ll_enable = 1;
		if ((dolby_vision_flags
			& FLAG_FORCE_RGB_OUTPUT)
			&& (dst_format == FORMAT_DOVI)) {
			new_dovi_setting.dovi_ll_enable = 1;
			new_dovi_setting.diagnostic_enable = 1;
			new_dovi_setting.diagnostic_mux_select = 1;
		}
		if (debug_dolby & 2)
			pr_dolby_dbg(
				"ll_enable=%d,diagnostic=%d,ll_policy=%d\n",
				new_dovi_setting.dovi_ll_enable,
				new_dovi_setting.diagnostic_enable,
				dolby_vision_ll_policy);
#endif
		new_dovi_setting.src_format = src_format;
		new_dovi_setting.dst_format = dst_format;
		new_dovi_setting.el_flag = el_flag;
		new_dovi_setting.el_halfsize_flag = el_halfsize_flag;
		new_dovi_setting.video_width = w;
		new_dovi_setting.video_height = h;
		dovi_setting_video_flag = video_frame;
		if (debug_dolby & 1) {
			if (el_flag)
				pr_dolby_dbg("setting %d->%d(T:%d-%d): flag=%02x,md=%d,comp=%d, frame:%d\n",
				src_format, dst_format,
				dolby_vision_target_min,
				dolby_vision_target_max[src_format][dst_format],
				flag,
				total_md_size, total_comp_size,
				frame_count);
			else
				pr_dolby_dbg("setting %d->%d(T:%d-%d): flag=%02x,md=%d, frame:%d\n",
				src_format, dst_format,
				dolby_vision_target_min,
				dolby_vision_target_max[src_format][dst_format],
				flag,
				total_md_size, frame_count);
		}
		dump_setting(&new_dovi_setting, frame_count, debug_dolby);
		el_mode = el_flag;
		return 0; /* setting updated */
	} else {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		pr_dolby_error("control_path() failed\n");
	}
	return -1; /* do nothing for this frame */
}

/* 0: no el; >0: with el */
/* 1: need wait el vf    */
/* 2: no match el found  */
/* 3: found match el     */
int dolby_vision_wait_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct vframe_s *el_vf;
	int ret = 0;
	unsigned int mode;

	if (single_step_enable) {
		if (dolby_vision_flags & FLAG_SINGLE_STEP)
			/* wait fake el for "step" */
			return 1;
		else
			dolby_vision_flags |= FLAG_SINGLE_STEP;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		bool ott_mode = true;
		if (is_meson_txlx_package_962X()
			&& !force_stb_mode)
			ott_mode =
				(tv_dovi_setting.input_mode !=
				INPUT_MODE_HDMI);
		if ((setting_update_count > crc_count)
			&& (ott_mode == true))
			return 1;
	}

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)
		vf_notify_provider_by_name("dvbldec",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);

	if (req.dv_enhance_exist) {
		el_vf = dvel_vf_peek();
		while (el_vf) {
			if (debug_dolby & 2)
				pr_dolby_dbg("=== peek bl(%p-%lld) with el(%p-%lld) ===\n",
					vf, vf->pts_us64,
					el_vf, el_vf->pts_us64);
			if ((el_vf->pts_us64 == vf->pts_us64)
			|| !(dolby_vision_flags & FLAG_CHECK_ES_PTS)) {
				/* found el */
				ret = 3;
				break;
			} else if (el_vf->pts_us64 < vf->pts_us64) {
				if (debug_dolby & 2)
					pr_dolby_dbg("bl(%p-%lld) => skip el pts(%p-%lld)\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				el_vf = dvel_vf_get();
				dvel_vf_put(el_vf);
				vf_notify_provider(DVEL_RECV_NAME,
					VFRAME_EVENT_RECEIVER_PUT, NULL);
				if (debug_dolby & 2)
					pr_dolby_dbg("=== get & put el(%p-%lld) ===\n",
						el_vf, el_vf->pts_us64);

				/* skip old el and peek new */
				el_vf = dvel_vf_peek();
			} else {
				/* no el found */
				ret = 2;
				break;
			}
		}
		/* need wait el */
		if (el_vf == NULL) {
			if (debug_dolby & 2)
				pr_dolby_dbg(
					"=== bl wait el(%p-%lld) ===\n",
					vf, vf->pts_us64);
			ret = 1;
		}
	}
	if (ret == 1)
		return ret;

	if (!dolby_vision_wait_init
		&& !dolby_vision_core1_on) {
		if (is_dovi_frame(vf)) {
			if (dolby_vision_policy_process(
				&mode, FORMAT_DOVI)) {
				if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
				&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)) {
					dolby_vision_wait_init = true;
					dolby_vision_wait_count =
						dolby_vision_wait_delay;
					dolby_vision_wait_on = true;
				}
			}
		} else if (is_hdr10_frame(vf)) {
			if (dolby_vision_policy_process(
				&mode, FORMAT_HDR10)) {
				if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
				&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)) {
					dolby_vision_wait_init = true;
					dolby_vision_wait_count =
						dolby_vision_wait_delay;
					dolby_vision_wait_on = true;
				}
			}
		} else if (dolby_vision_policy_process(
			&mode, FORMAT_SDR)) {
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
				&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)) {
					dolby_vision_wait_init = true;
					dolby_vision_wait_count =
						dolby_vision_wait_delay;
					dolby_vision_wait_on = true;
				}
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (dolby_vision_wait_init &&
			(READ_VPP_REG(VPP_MISC) & (1<<10)))
			dolby_vision_on_count =
				dolby_vision_run_mode_delay + 1;
	}
	if (dolby_vision_wait_init
		&& dolby_vision_wait_count) {
		dolby_vision_wait_count--;
		pr_dolby_dbg("delay wait %d\n",
		dolby_vision_wait_count);
		ret = 1;
	} else if (dolby_vision_core1_on
		&& (dolby_vision_on_count <=
		dolby_vision_run_mode_delay))
		ret = 1;

	return ret;
}

int dolby_vision_update_metadata(struct vframe_s *vf)
{
	int ret = -1;

	if (!dolby_vision_enable)
		return -1;

	if (vf && dolby_vision_vf_check(vf)) {
		ret = dolby_vision_parse_metadata(
			vf, 1, false);
		frame_count++;
	}

	return ret;
}
EXPORT_SYMBOL(dolby_vision_update_metadata);

static void update_dolby_vision_status(enum signal_format_e src_format)
{
	if (((src_format == FORMAT_DOVI)
		|| (src_format == FORMAT_DOVI_LL))
		&& (dolby_vision_status != DV_PROCESS)) {
		pr_dolby_dbg(
			"Dolby Vision mode changed to DV_PROCESS %d\n",
			src_format);
		dolby_vision_status = DV_PROCESS;
	} else if ((src_format == FORMAT_HDR10)
	&& (dolby_vision_status != HDR_PROCESS)) {
		pr_dolby_dbg(
			"Dolby Vision mode changed to HDR_PROCESS %d\n",
			src_format);
		dolby_vision_status = HDR_PROCESS;
	} else if ((src_format == FORMAT_SDR)
	&& (dolby_vision_status != SDR_PROCESS)) {
		pr_dolby_dbg(
			"Dolby Vision mode changed to SDR_PROCESS %d\n",
			src_format);
		dolby_vision_status = SDR_PROCESS;
	}
}

static unsigned int last_dolby_vision_policy;
int dolby_vision_process(struct vframe_s *vf, u32 display_size)
{
	int src_chroma_format = 0;
	u32 h_size = (display_size >> 16) & 0xffff;
	u32 v_size = display_size & 0xffff;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool reset_flag = false;
	bool force_set = false;

	if (!is_meson_gxm_cpu() && !is_meson_txlx_cpu())
		return -1;

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		if (vf) {
			h_size = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compWidth : vf->width;
			v_size = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compHeight : vf->height;
		} else {
			h_size = 0;
			v_size = 0;
		}
		dolby_vision_on_count = 1 +
			dolby_vision_run_mode_delay;
	}

	if ((core1_disp_hsize != h_size)
		|| (core1_disp_vsize != v_size))
		force_set = true;

	if ((dolby_vision_flags & FLAG_CERTIFICAION)
		&& (setting_update_count > crc_count)
		&& is_dolby_vision_on()) {
		s32 delay_count =
			(dolby_vision_flags >>
			FLAG_FRAME_DELAY_SHIFT)
			& FLAG_FRAME_DELAY_MASK;
		bool ott_mode = true;
		if (is_meson_txlx_package_962X()
			&& !force_stb_mode)
			ott_mode =
				(tv_dovi_setting.input_mode !=
				INPUT_MODE_HDMI);
		if ((is_meson_txlx_package_962E()
			|| is_meson_gxm_cpu()
			|| force_stb_mode)
			&& (setting_update_count == 1)
			&& (crc_read_delay == 1)) {
			/* work around to enable crc for frame 0 */
			VSYNC_WR_MPEG_REG(0x36fb, 1);
			crc_read_delay++;
		} else {
			crc_read_delay++;
			if ((crc_read_delay > delay_count)
				&& (ott_mode == true)) {
				tv_dolby_vision_insert_crc(
					(crc_count == 0) ? true : false);
				crc_read_delay = 0;
			}
		}
	}

	if (dolby_vision_on
		&& is_video_output_off(vf)
		&& !video_off_handled) {
		dolby_vision_set_toggle_flag(1);
		frame_count = 0;
		if (debug_dolby & 2) {
			video_off_handled = 1;
			pr_dolby_dbg("video off\n");
		}
	}

	if (last_dolby_vision_policy != dolby_vision_policy) {
		/* handle policy change */
		dolby_vision_set_toggle_flag(1);
		last_dolby_vision_policy = dolby_vision_policy;
	}

#ifdef V2_4
	if (is_meson_txlx_package_962E()
		|| is_meson_gxm_cpu()
		|| force_stb_mode) {
		if (last_dolby_vision_ll_policy
			!= dolby_vision_ll_policy) {
			/* handle ll mode policy change */
			dolby_vision_set_toggle_flag(1);
		}
	}
#endif

	if (!vf) {
		if (dolby_vision_flags & FLAG_TOGGLE_FRAME)
			dolby_vision_parse_metadata(
				NULL, 1, false);
	}
	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		if (vinfo && sink_support_dolby_vision(vinfo))
			dolby_vision_set_toggle_flag(1);
		if (!is_meson_txlx_package_962X() || force_stb_mode) {
			if ((!vinfo->dv_info)
				&& (vsync_count < FLAG_VSYNC_CNT)) {
				vsync_count++;
				return 0;
			}
		}
		if (dolby_vision_status != BYPASS_PROCESS) {
			enable_dolby_vision(0);
			if (!is_meson_txlx_package_962X() && !force_stb_mode)
				send_hdmi_pkt(FORMAT_SDR, vinfo);
			if (dolby_vision_flags & FLAG_TOGGLE_FRAME)
				dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		}
		return 0;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		video_effect_bypass(1);

	if (!p_funcs) {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		tv_dovi_setting_change_flag = false;
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		return 0;
	}

	if ((debug_dolby & 2) && force_set
		&& !(dolby_vision_flags & FLAG_CERTIFICAION))
		pr_dolby_dbg(
			"core1 size changed--old: %d x %d, new: %d x %d\n",
			core1_disp_hsize, core1_disp_vsize,
			h_size, v_size);
	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (!(dolby_vision_flags & FLAG_CERTIFICAION))
			reset_flag =
				(dolby_vision_reset & 1)
				&& (!dolby_vision_core1_on)
				&& (dolby_vision_on_count == 0);
		if (is_meson_txlx_package_962X() && !force_stb_mode) {
			if (tv_dovi_setting_change_flag) {
				if (vf && (vf->type & VIDTYPE_VIU_422))
					src_chroma_format = 2;
				else if (vf)
					src_chroma_format = 1;
				if (force_set &&
					!(dolby_vision_flags
					& FLAG_CERTIFICAION))
					reset_flag = true;
				tv_dolby_core1_set(
					tv_dovi_setting.core1_reg_lut,
					h_size,
					v_size,
					dovi_setting_video_flag, /* BL enable */
					dovi_setting_video_flag
					&& tv_dovi_setting.el_flag, /* EL en */
					tv_dovi_setting.el_halfsize_flag,
					src_chroma_format,
					tv_dovi_setting.input_mode ==
					INPUT_MODE_HDMI,
					tv_dovi_setting.src_format ==
					FORMAT_HDR10,
					reset_flag
				);
				if (!h_size || !v_size)
					dovi_setting_video_flag = false;
				if (dovi_setting_video_flag
				&& (dolby_vision_on_count == 0))
					pr_dolby_dbg("first frame reset %d\n",
						reset_flag);
				enable_dolby_vision(1);
				tv_dovi_setting_change_flag = false;
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				update_dolby_vision_status(
					tv_dovi_setting.src_format);
			}
		} else {
			if ((new_dovi_setting.video_width & 0xffff)
			&& (new_dovi_setting.video_height & 0xffff)) {
				if (force_set &&
					!(dolby_vision_flags
					& FLAG_CERTIFICAION))
					reset_flag = true;
				apply_stb_core_settings(
					dovi_setting_video_flag,
					dolby_vision_mask & 0x7,
					reset_flag,
					(h_size << 16) | v_size);
				memcpy(&dovi_setting, &new_dovi_setting,
					sizeof(dovi_setting));
				new_dovi_setting.video_width =
				new_dovi_setting.video_height = 0;
				if (!h_size || !v_size)
					dovi_setting_video_flag = false;
				if (dovi_setting_video_flag
				&& (dolby_vision_on_count == 0))
					pr_dolby_dbg("first frame reset %d\n",
						reset_flag);
				enable_dolby_vision(1);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				/* send HDMI packet according to dst_format */
				if (!force_stb_mode)
					send_hdmi_pkt(
						dovi_setting.dst_format, vinfo);
				update_dolby_vision_status(
					dovi_setting.src_format);
			}
		}
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	} else if (dolby_vision_core1_on &&
		!(dolby_vision_flags & FLAG_CERTIFICAION)) {
		bool reset_flag =
			(dolby_vision_reset & 2)
			&& (dolby_vision_on_count
			<= (dolby_vision_reset_delay >> 8))
			&& (dolby_vision_on_count
			>= (dolby_vision_reset_delay & 0xff));
		if (is_meson_txlx_package_962E()
			|| force_stb_mode) {
			if ((dolby_vision_on_count <=
				dolby_vision_run_mode_delay)
				|| force_set) {
				if (force_set)
					reset_flag = true;
				apply_stb_core_settings(
					dovi_setting_video_flag,
					/* core 1 only */
					dolby_vision_mask & 0x1,
					reset_flag,
					(h_size << 16) | v_size);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (dolby_vision_on_count <=
					dolby_vision_run_mode_delay)
					pr_dolby_dbg("fake frame %d reset %d\n",
						dolby_vision_on_count,
						reset_flag);
			}
		} else if (is_meson_txlx_package_962X()) {
			if ((dolby_vision_on_count <=
				dolby_vision_run_mode_delay)
				|| force_set) {
				if (force_set)
					reset_flag = true;
				tv_dolby_core1_set(
					tv_dovi_setting.core1_reg_lut,
					h_size,
					v_size,
					dovi_setting_video_flag, /* BL enable */
					dovi_setting_video_flag &&
					tv_dovi_setting.el_flag, /* EL enable */
					tv_dovi_setting.el_halfsize_flag,
					src_chroma_format,
					tv_dovi_setting.input_mode ==
					INPUT_MODE_HDMI,
					tv_dovi_setting.src_format ==
					FORMAT_HDR10,
					reset_flag);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (dolby_vision_on_count <=
					dolby_vision_run_mode_delay)
					pr_dolby_dbg("fake frame %d reset %d\n",
						dolby_vision_on_count,
						reset_flag);
			}
		} else if (is_meson_gxm_cpu()) {
			if ((dolby_vision_on_count <=
				dolby_vision_run_mode_delay)
				|| force_set) {
				if (force_set)
					reset_flag = true;
				apply_stb_core_settings(
					true, /* always enable */
					/* core 1 only */
					dolby_vision_mask & 0x1,
					reset_flag,
					(h_size << 16) | v_size);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (dolby_vision_on_count <
					dolby_vision_run_mode_delay)
					pr_dolby_dbg("fake frame %d reset %d\n",
						dolby_vision_on_count,
						reset_flag);
			}
		}
	}
	if (dolby_vision_core1_on) {
		if (dolby_vision_on_count <=
			dolby_vision_run_mode_delay)
			dolby_vision_on_count++;
	} else
		dolby_vision_on_count = 0;
	return 0;
}
EXPORT_SYMBOL(dolby_vision_process);
EXPORT_SYMBOL(dolby_vision_parse_metadata);

bool is_dolby_vision_on(void)
{
	return dolby_vision_on
	|| dolby_vision_wait_on;
}
EXPORT_SYMBOL(is_dolby_vision_on);

bool for_dolby_vision_certification(void)
{
	return is_dolby_vision_on() &&
		dolby_vision_flags & FLAG_CERTIFICAION;
}
EXPORT_SYMBOL(for_dolby_vision_certification);

void dolby_vision_set_toggle_flag(int flag)
{
	if (flag)
		dolby_vision_flags |= FLAG_TOGGLE_FRAME;
	else
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
}
EXPORT_SYMBOL(dolby_vision_set_toggle_flag);

void set_dolby_vision_mode(int mode)
{
	if ((is_meson_gxm_cpu() || is_meson_txlx_cpu())
	&& dolby_vision_enable) {
		if (dolby_vision_policy_process(
			&mode, FORMAT_SDR)) {
			dolby_vision_set_toggle_flag(1);
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
			&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS))
				dolby_vision_wait_on = true;
			pr_info("DOVI output change from %d to %d\n",
				dolby_vision_mode, mode);
			dolby_vision_mode = mode;
		}
	}
}
EXPORT_SYMBOL(set_dolby_vision_mode);

int get_dolby_vision_mode(void)
{
	return dolby_vision_mode;
}
EXPORT_SYMBOL(get_dolby_vision_mode);

bool is_dolby_vision_enable(void)
{
	return dolby_vision_enable;
}
EXPORT_SYMBOL(is_dolby_vision_enable);

bool is_dolby_vision_stb_mode(void)
{
	return force_stb_mode ||
	is_meson_txlx_package_962E() ||
	is_meson_gxm_cpu();
}
EXPORT_SYMBOL(is_dolby_vision_stb_mode);

int register_dv_functions(const struct dolby_vision_func_s *func)
{
	int ret = -1;
	unsigned int reg_clk;
	unsigned int reg_value;
	if (!p_funcs && func) {
		pr_info("*** register_dv_functions. version %s ***\n",
			func->version_info);
		p_funcs = func;
		ret = 0;
		/* get efuse flag*/
		reg_clk = READ_VPP_REG(DOLBY_TV_CLKGATE_CTRL);
		WRITE_VPP_REG(DOLBY_TV_CLKGATE_CTRL, 0x2800);
		reg_value = READ_VPP_REG(DOLBY_TV_REG_START + 1);
		if (is_meson_txlx_package_962X()
		|| is_meson_txlx_package_962E()) {
			if ((reg_value & 0x400) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
		} else {
			if ((reg_value & 0x100) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
		}
		WRITE_VPP_REG(DOLBY_TV_CLKGATE_CTRL, reg_clk);
		pr_dolby_dbg
			("efuse_mode=%d reg_value = 0x%x\n",
			efuse_mode,
			reg_value);
		if (is_meson_gxm_cpu())
			dolby_vision_run_mode_delay = 3;
	}
	return ret;
}
EXPORT_SYMBOL(register_dv_functions);

int unregister_dv_functions(void)
{
	int ret = -1;
	if (p_funcs) {
		pr_info("*** unregister_dv_functions ***\n");
		p_funcs = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_dv_functions);

void tv_dolby_vision_crc_clear(int flag)
{
	crc_outpuf_buff_off = 0;
	crc_count = 0;
	crc_bypass_count = 0;
	setting_update_count = 0;
	if (!crc_output_buf)
		crc_output_buf = vmalloc(CRC_BUFF_SIZE);
	pr_info(
		"tv_dolby_vision_crc_clear, crc_output_buf %p\n",
		crc_output_buf);
	if (crc_output_buf)
		memset(crc_output_buf, 0, CRC_BUFF_SIZE);
	return;
}

char *tv_dolby_vision_get_crc(u32 *len)
{
	if ((!crc_output_buf) ||
		(!len) ||
		(crc_outpuf_buff_off == 0))
		return NULL;
	*len = crc_outpuf_buff_off;
	return crc_output_buf;
}

void tv_dolby_vision_insert_crc(bool print)
{
	char str[64];
	int len;
	bool crc_enable;
	u32 crc;

	if (dolby_vision_flags & FLAG_DISABLE_CRC) {
		crc_bypass_count++;
		crc_count++;
		return;
	}
	if (is_meson_txlx_package_962X()
		&& !force_stb_mode) {
		crc_enable = (READ_VPP_REG(0x33e7) == 0xb);
		crc = READ_VPP_REG(0x33ef);
	} else {
		crc_enable = true; /* (READ_VPP_REG(0x36fb) & 1); */
		crc = READ_VPP_REG(0x36fd);
	}
	if ((crc == 0) || (crc_enable == false) || (!crc_output_buf)) {
		crc_bypass_count++;
		crc_count++;
		return;
	}
	if (crc_count < crc_bypass_count)
		crc_bypass_count = crc_count;
	memset(str, 0, sizeof(str));
	snprintf(str, 64, "crc(%d) = 0x%08x",
		crc_count - crc_bypass_count, crc);
	len = strlen(str);
	str[len] = 0xa;
	len++;
	memcpy(
		&crc_output_buf[crc_outpuf_buff_off],
		&str[0], len);
	crc_outpuf_buff_off += len;
	if (print || (debug_dolby & 2))
		pr_info("%s\n", str);
	crc_count++;
	return;
}

void tv_dolby_vision_dma_table_modify(u32 tbl_id, uint64_t value)
{
	uint64_t *tbl = NULL;
	if (!dma_vaddr || (tbl_id >= 3754)) {
		pr_info("No dma table %p to write or id %d overflow\n",
			dma_vaddr, tbl_id);
		return;
	}
	tbl = (uint64_t *)dma_vaddr;
	pr_info("dma_vaddr:%p, modify table[%d]=0x%llx -> 0x%llx\n",
		dma_vaddr, tbl_id, tbl[tbl_id], value);
	tbl[tbl_id] = value;
	return;
}

void tv_dolby_vision_efuse_info(void)
{
	if (p_funcs != NULL) {
		pr_info("\n dv efuse info:\n");
		pr_info("efuse_mode:%d, version: %s\n",
			efuse_mode, p_funcs->version_info);
	} else {
		pr_info("\n p_funcs is NULL\n");
		pr_info("efuse_mode:%d\n", efuse_mode);
	}
	return;
}

void tv_dolby_vision_el_info(void)
{
	pr_info("el_mode:%d\n", el_mode);
	return;
}

