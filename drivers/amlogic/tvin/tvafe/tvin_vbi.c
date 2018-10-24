/*
 * TVAFE char device driver.
 *
 * Copyright (c) 2010 Frank zhao <frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>

/* #include <linux/mutex.h> */
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/io.h> /* for virt_to_phys */


/* Local include */
#include "tvafe_regs.h"
#include "tvin_vbi.h"
#include "../tvin_global.h"

#define VBI_NAME               "vbi"
#define VBI_DRIVER_NAME        "vbi"
#define VBI_MODULE_NAME        "vbi"
#define VBI_DEVICE_NAME        "vbi"
#define VBI_CLASS_NAME         "vbi"

static dev_t vbi_id;
static struct class *vbi_clsp;

/******debug********/
static unsigned int vbi_dbg_en;
MODULE_PARM_DESC(vbi_dbg_en, "\n vbi_dbg_en\n");
module_param(vbi_dbg_en, uint, 0664);

static bool capture_print_en;
MODULE_PARM_DESC(capture_print_en, "\n capture_print_en\n");
module_param(capture_print_en, bool, 0664);

static bool data_print_en;
MODULE_PARM_DESC(data_print_en, "\n data_print_en\n");
module_param(data_print_en, bool, 0664);

static unsigned int vcnt = 1;

static void vbi_hw_reset(struct vbi_dev_s *devp)
{
	W_VBI_APB_REG(ACD_REG_22, 0x82080000);
	W_VBI_APB_REG(ACD_REG_22, 0x04080000);
	/* after hw reset,bellow paramters must be reset!!! */
	vcnt = 1;
	devp->pac_addr = devp->pac_addr_start;
	devp->current_pac_wptr = 0;
}

static void vbi_tt_start_code_set(struct vbi_dev_s *devp)
{
	unsigned int vbi_start_code = devp->vbi_start_code;
	/* start code */
	W_VBI_APB_REG(CVD2_VBI_TT_FRAME_CODE_CTL, vbi_start_code);
}

static void vbi_data_type_set(struct vbi_dev_s *devp)
{
	unsigned int vbi_data_type = devp->vbi_data_type;
	/* data type */
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE6,  vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE7 , vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE8 , vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE9 , vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE10, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE11, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE12, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE13, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE14, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE15, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE16, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE17, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE18, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE19, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE20, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE21, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE22, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE23, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE24, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE25, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE26, vbi_data_type);
}

static void vbi_dto_set(struct vbi_dev_s *devp)
{
	unsigned int dto_cc, dto_tt, dto_wss, dto_vps;

	dto_cc = devp->vbi_dto_cc;
	dto_tt = devp->vbi_dto_tt;
	dto_wss = devp->vbi_dto_wss;
	dto_vps = devp->vbi_dto_vps;
	W_VBI_APB_REG(CVD2_VBI_CC_DTO_MSB,	     ((dto_cc>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_CC_DTO_LSB,	     (dto_cc & 0xff));
	W_VBI_APB_REG(CVD2_VBI_TT_DTO_MSB,	     ((dto_tt>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_TT_DTO_LSB,	     (dto_tt & 0xff));
	W_VBI_APB_REG(CVD2_VBI_WSS_DTO_MSB,	     ((dto_wss>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_WSS_DTO_LSB,	     (dto_wss & 0xff));
	W_VBI_APB_REG(CVD2_VBI_VPS_DTO_MSB,	     ((dto_vps>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_VPS_DTO_LSB,	     (dto_vps & 0xff));

}
/* hw reset,config vbi line data type,start code,dto */
static void vbi_slicer_type_set(struct vbi_dev_s *devp)
{
	enum vbi_slicer_e slicer_type = devp->slicer->type;
	vbi_hw_reset(devp);
	switch (slicer_type) {
	case VBI_TYPE_USCC:
		devp->vbi_data_type = VBI_DATA_TYPE_USCC;
		devp->vbi_start_code = VBI_START_CODE_USCC;
		devp->vbi_dto_cc = VBI_DTO_USCC;
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE21,
			VBI_DATA_TYPE_USCC);
		break;
	case VBI_TYPE_EUROCC:
		devp->vbi_data_type = VBI_DATA_TYPE_EUROCC;
		devp->vbi_start_code = VBI_START_CODE_EUROCC;
		devp->vbi_dto_cc = VBI_DTO_EURCC;
		/*line18 for PAL M*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE18,
			VBI_DATA_TYPE_EUROCC);
		/*line22 for PAL B,D,G,H,I,N,CN*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE22,
			VBI_DATA_TYPE_EUROCC);
		break;
	case VBI_TYPE_VPS:
		devp->vbi_data_type = VBI_DATA_TYPE_VPS;
		devp->vbi_start_code = VBI_START_CODE_VPS;
		devp->vbi_dto_vps = VBI_DTO_VPS;
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE16, VBI_DATA_TYPE_VPS);
		break;
	case VBI_TYPE_TT_625A:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625A;
		devp->vbi_start_code = VBI_START_CODE_TT_625A_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625A;
		break;
	case VBI_TYPE_TT_625B:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625B;
		devp->vbi_start_code = VBI_START_CODE_TT_625B_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625B;
		break;
	case VBI_TYPE_TT_625C:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625B;
		devp->vbi_start_code = VBI_START_CODE_TT_625C_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625C;
		break;
	case VBI_TYPE_TT_625D:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625D;
		devp->vbi_start_code = VBI_START_CODE_TT_625D_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625D;
		break;
	case VBI_TYPE_TT_525B:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_525B;
		devp->vbi_start_code = VBI_START_CODE_TT_525B_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT525B;
		break;
	case VBI_TYPE_TT_525C:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_525C;
		devp->vbi_start_code = VBI_START_CODE_TT_525C_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT525C;
		break;
	case VBI_TYPE_TT_525D:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_525D;
		devp->vbi_start_code = VBI_START_CODE_TT_525D_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT525D;
		break;
	case VBI_TYPE_WSS625:
		devp->vbi_data_type = VBI_DATA_TYPE_WSS625;
		devp->vbi_start_code = VBI_START_CODE_WSS625;
		devp->vbi_dto_wss = VBI_DTO_WSS625;
		/*line17 for PAL M*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE17,
			VBI_DATA_TYPE_WSS625);
		/*line23 for PAL B,D,G,H,I,N,CN*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE23,
			VBI_DATA_TYPE_WSS625);
		break;
	case VBI_TYPE_WSSJ:
		devp->vbi_data_type = VBI_DATA_TYPE_WSSJ;
		devp->vbi_start_code = VBI_START_CODE_WSSJ;
		devp->vbi_dto_wss = VBI_DTO_WSSJ;
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE20,
			VBI_DATA_TYPE_WSSJ);
		break;
	case VBI_TYPE_NULL:
		devp->vbi_dto_cc = VBI_DTO_USCC;
		devp->vbi_dto_wss = VBI_DTO_WSS625;
		devp->vbi_dto_tt = VBI_DTO_TT625B;
		devp->vbi_dto_vps = VBI_DTO_VPS;
		devp->vbi_data_type = VBI_DATA_TYPE_NULL;
		vbi_data_type_set(devp);
		break;
	default:/*for tt625b+wss625+vps*/
		devp->vbi_dto_cc = VBI_DTO_USCC;
		devp->vbi_dto_wss = VBI_DTO_WSS625;
		devp->vbi_dto_tt = VBI_DTO_TT625B;
		devp->vbi_dto_vps = VBI_DTO_VPS;
		W_VBI_APB_REG(CVD2_VBI_TT_FRAME_CODE_CTL,
			VBI_START_CODE_TT_625B_REVERSE);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE16, VBI_DATA_TYPE_VPS);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE20,
			VBI_DATA_TYPE_TT_625B);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE21,
			VBI_DATA_TYPE_TT_625B);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE23,
			VBI_DATA_TYPE_WSS625);
		break;
	}
	if ((slicer_type >= VBI_TYPE_TT_625A) &&
		(slicer_type <= VBI_TYPE_TT_525D)) {
		vbi_data_type_set(devp);
		vbi_tt_start_code_set(devp);
	}
	vbi_dto_set(devp);
}

static void vbi_hw_init(struct vbi_dev_s *devp)
{
	/* vbi memory setting */
	memset(devp->pac_addr_start, 0, devp->mem_size);
	W_VBI_APB_REG(ACD_REG_2F, devp->mem_start >> 4);
	if (0) {/*(is_meson_txlx_cpu()) {*/
		W_VBI_APB_BIT(ACD_REG_42, ((devp->mem_size >> 4) - 1), 0, 24);
		W_VBI_APB_BIT(ACD_REG_42, 1, 31, 1);
	} else
		W_VBI_APB_BIT(ACD_REG_21, ((devp->mem_size >> 4) - 1), 16, 16);
	W_VBI_APB_BIT(ACD_REG_21, 0, AML_VBI_START_ADDR_BIT,
		AML_VBI_START_ADDR_WID);
	/*disable vbi*/
	W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL,   0x14);
	/* config vbi start line */
	W_VBI_APB_REG(CVD2_VBI_CC_START,	     VBI_START_CC);
	W_VBI_APB_REG(CVD2_VBI_WSS_START,	     VBI_START_WSS);
	W_VBI_APB_REG(CVD2_VBI_TT_START,	     VBI_START_TT);
	W_VBI_APB_REG(CVD2_VBI_VPS_START,	     VBI_START_VPS);
	W_VBI_APB_BIT(CVD2_VBI_CONTROL, 1, 0, 1);
	W_VBI_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_START, 0x00000000);
	W_VBI_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_END, 0x00000025);
	/* be care the polarity bellow!!! */
	W_VBI_APB_BIT(CVD2_VSYNC_TIME_CONSTANT, 0, 7, 1);
	/*enable vbi*/
	W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL,   0x15);
	pr_info("[vbi..] %s: vbi hw init done.\n", __func__);
}
#define vbi_get_byte(rdptr, total_buffer, retbyte) \
	do {\
		retbyte = *(rdptr); \
		rdptr += 1; \
	} while (0)
#define vbi_skip_bytes(rdptr, total_buffer, devp, nbytes) \
	do {\
		rdptr += nbytes;\
		if (rdptr > devp->pac_addr_end)\
			rdptr = devp->pac_addr_start;\
		total_buffer -= nbytes;\
	} while (0)

ssize_t vbi_ringbuffer_free(struct vbi_ringbuffer_s *rbuf)
{
	ssize_t free;

	free = rbuf->pread - rbuf->pwrite;
	if (free <= 0) {
		free += rbuf->size;
		if (capture_print_en)
			pr_info("[vbi..] %s: pread: %6d pwrite: %6d\n",
			__func__, rbuf->pread , rbuf->pwrite);
	}
	return free-1;
}


ssize_t vbi_ringbuffer_write(struct vbi_ringbuffer_s *rbuf,
		const struct vbi_data_s *buf, size_t len)
{
	size_t todo = len;
	size_t split;
	if (rbuf->data_wmode)
		len = rbuf->size;
	split =
	(rbuf->pwrite + len > rbuf->size) ? rbuf->size - rbuf->pwrite : 0;

	if (split > 0) {
		if (capture_print_en)
			pr_info("[vbi..] %s: pwrite: %6d\n",
			__func__, rbuf->pwrite);
		memcpy((char *)rbuf->data+rbuf->pwrite, (char *)buf, split);
		todo -= split;
		rbuf->pwrite = 0;
	}
	memcpy((char *)rbuf->data+rbuf->pwrite, (char *)buf+split, todo);
	rbuf->pwrite = (rbuf->pwrite + todo) % rbuf->size;

	return len;
}


static int vbi_buffer_write(struct vbi_ringbuffer_s *buf,
		   const struct vbi_data_s *src, size_t len)
{
	ssize_t free;

	if (!len) {
		if (capture_print_en)
			pr_info("[vbi..] %s: buffer len is zero\n", __func__);
		return 0;
	}
	if (!buf->data) {
		if (capture_print_en)
			pr_info("[vbi..] %s: buffer data pointer is zero\n",
			__func__);
		return 0;
	}

	free = vbi_ringbuffer_free(buf);
	if (len > free) {
		if (capture_print_en)
			pr_info("[vbi..] %s: buffer overflow ,len: %6Zd, free: %6Zd\n",
			__func__, len, free);
		return -EOVERFLOW;
	}

	return vbi_ringbuffer_write(buf, src, len);
}

static irqreturn_t vbi_isr(int irq, void *dev_id)
{
	ulong flags;
	struct vbi_dev_s *devp = (struct vbi_dev_s *)dev_id;
	spin_lock_irqsave(&devp->vbi_isr_lock, flags);
	if (devp->vs_delay > 0) {
		devp->vs_delay--;
		spin_unlock_irqrestore(&devp->vbi_isr_lock, flags);
		return IRQ_HANDLED;
	}
	if (devp->vbi_start == false) {
		spin_unlock_irqrestore(&devp->vbi_isr_lock, flags);
		return IRQ_HANDLED;
	}
	if (devp->tasklet_enable)
		tasklet_schedule(&devp->tsklt_slicer);
	spin_unlock_irqrestore(&devp->vbi_isr_lock, flags);
	return IRQ_HANDLED;
}
static unsigned int vbi_vcnt_search(struct vbi_dev_s *devp)
{
	unsigned char *rptr = devp->pac_addr;
	unsigned int size =  devp->mem_size;
	unsigned int i, j;
	unsigned char burst_data[VBI_WRITE_BURST_BYTE] = {0};
	unsigned char burst_data_vcnt[VBI_WRITE_BURST_BYTE] = {0};
	unsigned int match_cnt, ret;
	ret = 0;
	for (i = 0; i < VBI_WRITE_BURST_BYTE; i++) {
		burst_data_vcnt[i] = 0;
		burst_data[i] = 0;
	}
	burst_data_vcnt[0] = vcnt&0xff;
	burst_data_vcnt[1] = (vcnt >> 8)&0xff;
	for (i = 0; i < size; i += VBI_WRITE_BURST_BYTE) {
		if (rptr >= devp->pac_addr_end + 1)
			rptr = devp->pac_addr_start;
		match_cnt = 0;
		for (j = 0; j < VBI_WRITE_BURST_BYTE; j++) {
			burst_data[j] = *(rptr+i+j);
			if (burst_data[j] != burst_data_vcnt[j]) {
				match_cnt = 0;
				break;
			} else {
				match_cnt++;
			}
		}
		if (match_cnt == VBI_WRITE_BURST_BYTE)
			break;
	}
	if (vcnt == 0xffff)
		vcnt = 1;
	else
		vcnt++;
	if ((i < size) && i >= VBI_WRITE_BURST_BYTE &&
		match_cnt == VBI_WRITE_BURST_BYTE) {
		ret = i + VBI_WRITE_BURST_BYTE;
		return ret;
	} else {
		return ret;
	}
}
static void vbi_slicer_task(unsigned long arg)
{
	struct vbi_dev_s *devp = (struct vbi_dev_s *)arg;
	struct vbi_data_s vbi_data;
	unsigned char rbyte = 0;
	unsigned short pre_val = 0;
	int i = 0;
	unsigned char *local_rptr = NULL;
	unsigned int bytes_buffer, bytes_reserve, bytes_buffer_pre;
	unsigned char *current_pac_addr, *rptr;
	unsigned int sync_code = 0;
	unsigned int len;
	if (devp->vbi_start == false)
		return;
	rptr = devp->pac_addr;  /* backup package data pointer */
	/* get tatal bytes */
	bytes_buffer = vbi_vcnt_search(devp);
	if (bytes_buffer < VBI_WRITE_BURST_BYTE) {
		if (vbi_dbg_en)
			pr_info("[vbi..]: no enough data bytes_buffer:0x%x, rptr:0x%p ... ...\n",
			bytes_buffer, rptr);
		goto err_exit;
	}
	if (0)/*(is_meson_txlx_cpu())*/
		devp->current_pac_wptr = R_VBI_APB_REG(ACD_REG_43) << 4;
	else
		devp->current_pac_wptr = devp->mem_start + (devp->pac_addr -
		devp->pac_addr_start) + bytes_buffer;
	/*reg ACD_REG_0C is not used ?!! hope add in next ic!!*/
	/*devp->current_pac_wptr = R_VBI_APB_REG(ACD_REG_0C);*/
	if (devp->current_pac_wptr != 0 &&
		devp->current_pac_wptr > devp->mem_start)
		current_pac_addr = devp->pac_addr_start +
			(devp->current_pac_wptr - devp->mem_start);
	else
		current_pac_addr = devp->pac_addr_start;
	bytes_reserve = devp->pac_addr_end - devp->pac_addr + 1;

	if (bytes_buffer < bytes_reserve)
		memcpy(devp->pac_addr_end + 1, rptr, bytes_buffer);
	else {
		bytes_buffer_pre = bytes_buffer - bytes_reserve;
		memcpy(devp->pac_addr_end + 1, rptr, bytes_reserve);
		memcpy(devp->pac_addr_end + 1 + bytes_reserve,
			devp->pac_addr_start, bytes_buffer_pre);
	}
	local_rptr = devp->pac_addr_end + 1;
	if (vbi_dbg_en)
		pr_info("[vbi..]: Start parsing, bytes_buffer:%d, rptr:0x%p...\n",
		bytes_buffer, rptr);
	while (bytes_buffer) {
		vbi_get_byte(local_rptr, bytes_buffer, rbyte);
		vbi_skip_bytes(rptr, bytes_buffer, devp, 1);
		sync_code <<= 8;
		sync_code |= rbyte;
		if ((sync_code & 0xFFFFFF) != 0x00FFFF)
			continue;
		/* vbi_type & field_id */
		vbi_get_byte(local_rptr, bytes_buffer, rbyte);
		vbi_skip_bytes(rptr, bytes_buffer, devp, 1);
		vbi_data.vbi_type = (rbyte>>1) & 0x7;
		vbi_data.field_id = rbyte & 1;
	#if defined(VBI_TT_SUPPORT)
		vbi_data.tt_sys = rbyte >> 5;
	#endif
		if (vbi_data.vbi_type > MAX_PACKET_TYPE) {
			pr_info("[vbi..]: unsupport vbi_type_id:%d\n",
				vbi_data.vbi_type);
			continue;
		}
		/* byte counter */
		vbi_get_byte(local_rptr, bytes_buffer, rbyte);
		vbi_skip_bytes(rptr, bytes_buffer, devp, 1);
		vbi_data.nbytes = rbyte;
		/* line number */
		vbi_get_byte(local_rptr, bytes_buffer, rbyte);
		vbi_skip_bytes(rptr, bytes_buffer, devp, 1);
		pre_val = (u16)rbyte;
		vbi_get_byte(local_rptr, bytes_buffer, rbyte);
		vbi_skip_bytes(rptr, bytes_buffer, devp, 1);
		pre_val |= ((u16)rbyte & 0x3) << 8;
		vbi_data.line_num = pre_val;
		/* data */
		memcpy(&(vbi_data.b[0]), local_rptr, vbi_data.nbytes);
		local_rptr += vbi_data.nbytes;
		vbi_skip_bytes(rptr, bytes_buffer, devp, vbi_data.nbytes);
		/* capture data to vbi buffer */
		len = sizeof(struct vbi_data_s);
		vbi_buffer_write(&devp->slicer->buffer, &vbi_data, len);
		/* go to search next package */
		if (data_print_en) {
			pr_info("[vbi..]: field_id:%d, data_bytes:%4d, line_num:%4d;",
				vbi_data.field_id, bytes_buffer,
				vbi_data.line_num);
			for (i = 0; i < vbi_data.nbytes ; i++) {
				if (i%16 == 0)
					pr_info("\n");
				pr_info("%2x ", vbi_data.b[i]);
			}
			pr_info("\n");
		}
	}
	devp->pac_addr = rptr;
	if (devp->slicer->buffer.pread != devp->slicer->buffer.pwrite)
		wake_up(&devp->slicer->buffer.queue);
err_exit:
	return;
}
void vbi_ringbuffer_flush(struct vbi_ringbuffer_s *rbuf)
{
	rbuf->pread = rbuf->pwrite = 0;
	rbuf->error = 0;
}

static inline void vbi_slicer_state_set(struct vbi_dev_s *dev,
						int state)
{
	spin_lock_irq(&dev->lock);
	dev->slicer->state = state;
	spin_unlock_irq(&dev->lock);
}

static inline int vbi_slicer_reset(struct vbi_dev_s *dev)
{
	if (dev->slicer->state < VBI_STATE_SET)
		return 0;

	dev->slicer->type = VBI_TYPE_NULL;
	vbi_slicer_state_set(dev, VBI_STATE_ALLOCATED);
	return 0;
}

static int vbi_slicer_stop(struct vbi_slicer_s *vbi_slicer)
{
	if (vbi_slicer->state < VBI_STATE_GO)
		return 0;
	vbi_slicer->state = VBI_STATE_DONE;

	vbi_ringbuffer_flush(&vbi_slicer->buffer);

	return 0;
}

static int vbi_slicer_free(struct vbi_dev_s *vbi_dev,
		  struct vbi_slicer_s *vbi_slicer)
{
	mutex_lock(&vbi_dev->mutex);
	mutex_lock(&vbi_slicer->mutex);

	vbi_slicer_stop(vbi_slicer);
	vbi_slicer_reset(vbi_dev);

	if (vbi_slicer->buffer.data) {
		void *mem = vbi_slicer->buffer.data;

		spin_lock_irq(&vbi_dev->lock);
		vbi_slicer->buffer.data = NULL;
		spin_unlock_irq(&vbi_dev->lock);
		vfree(mem);
	}

	vbi_slicer_state_set(vbi_dev, VBI_STATE_FREE);
	wake_up(&vbi_slicer->buffer.queue);
	mutex_unlock(&vbi_slicer->mutex);
	mutex_unlock(&vbi_dev->mutex);

	return 0;
}

static void vbi_ringbuffer_init(struct vbi_ringbuffer_s *rbuf,
			void *data, size_t len)
{
	rbuf->pread = rbuf->pwrite = 0;
	if (data == NULL)
		rbuf->data = vmalloc(len*sizeof(struct vbi_data_s));
	else
		rbuf->data = data;
	rbuf->size = len*sizeof(struct vbi_data_s);
	rbuf->data_num = len;
	rbuf->error = 0;

	init_waitqueue_head(&rbuf->queue);

	spin_lock_init(&(rbuf->lock));
}

void vbi_ringbuffer_reset(struct vbi_ringbuffer_s *rbuf)
{
	rbuf->pread = rbuf->pwrite = 0;
	rbuf->error = 0;
}

static int vbi_set_buffer_size(struct vbi_dev_s *dev,
		      unsigned long size)
{
	struct vbi_slicer_s *vbi_slicer = dev->slicer;
	struct vbi_ringbuffer_s *buf = &vbi_slicer->buffer;
	void *newmem;
	void *oldmem;

	if (buf->size == size) {
		pr_info("[vbi..] %s: buf->size == size\n", __func__);
		return 0;
	}
	if (!size) {
		pr_info("[vbi..] %s: buffer size is 0!!!\n", __func__);
		return -EINVAL;
	}
	if (vbi_slicer->state >= VBI_STATE_GO) {
		pr_info("[vbi..] %s: vbi_slicer busy!!!\n", __func__);
		return -EBUSY;
	}

	newmem = vmalloc(size);
	if (!newmem) {
		pr_info("[vbi..] %s: get memory error!!!\n", __func__);
		return -ENOMEM;
	}

	oldmem = buf->data;

	spin_lock_irq(&dev->lock);
	buf->data = newmem;
	buf->size = size;

	/* reset and not flush in case the buffer shrinks */
	vbi_ringbuffer_reset(buf);
	spin_unlock_irq(&dev->lock);

	vfree(oldmem);

	return 0;
}

static int vbi_slicer_start(struct vbi_dev_s *dev)
{
	struct vbi_slicer_s *vbi_slicer = dev->slicer;
	void *mem;

	if (vbi_slicer->state < VBI_STATE_SET)
		return -EINVAL;

	if (vbi_slicer->state >= VBI_STATE_GO)
		vbi_slicer_stop(vbi_slicer);

	if (!vbi_slicer->buffer.data) {
		mem = vmalloc(vbi_slicer->buffer.size);
		if (!mem) {
			pr_info("[vbi..] %s: get memory error!!!\n", __func__);
			return -ENOMEM;
		}
		spin_lock_irq(&dev->lock);
		vbi_slicer->buffer.data = mem;
		spin_unlock_irq(&dev->lock);
	}

	vbi_ringbuffer_flush(&vbi_slicer->buffer);

	vbi_slicer_state_set(dev, VBI_STATE_GO);

	return 0;
}

static int vbi_slicer_set(struct vbi_dev_s *vbi_dev,
		 struct vbi_slicer_s *vbi_slicer)
{
	vbi_slicer_stop(vbi_slicer);

	vbi_slicer_state_set(vbi_dev, VBI_STATE_SET);

	vbi_slicer_type_set(vbi_dev);

	return 0;/* vbi_slicer_start(vbi_dev); */
}

ssize_t vbi_ringbuffer_avail(struct vbi_ringbuffer_s *rbuf)
{
	ssize_t avail;

	avail = rbuf->pwrite - rbuf->pread;
	if (avail < 0)
		avail += rbuf->size;
	return avail;
}

int vbi_ringbuffer_empty(struct vbi_ringbuffer_s *rbuf)
{
	return (int)(rbuf->pread == rbuf->pwrite);
}

ssize_t vbi_ringbuffer_read_user(struct vbi_ringbuffer_s *rbuf,
			u8 __user *buf, size_t len)
{
	size_t todo = len;
	size_t split;
	split = (rbuf->pread + len > rbuf->size) ? rbuf->size - rbuf->pread : 0;
	if (split > 0) {
		if (copy_to_user(buf, (char *)rbuf->data+rbuf->pread, split))
			return -EFAULT;
		buf += split;
		todo -= split;
		rbuf->pread = 0;
	}
	if (copy_to_user(buf, (char *)rbuf->data+rbuf->pread, todo))
		return -EFAULT;
	rbuf->pread = (rbuf->pread + todo) % rbuf->size;

	return len;
}

static ssize_t vbi_buffer_read(struct vbi_ringbuffer_s *src,
		      int non_blocking, char __user *buf,
		      size_t count, loff_t *ppos)
{
	size_t todo;
	ssize_t avail;
	ssize_t ret = 0;

	if (!src->data) {
		pr_info("[vbi..] %s: data null\n", __func__);
		return 0;
	}

	if (src->error) {
		ret = src->error;
		vbi_ringbuffer_flush(src);
		pr_info("[vbi..] %s: data error\n", __func__);
		return ret;
	}

	for (todo = count; todo > 0; todo -= ret) {
		if (non_blocking && vbi_ringbuffer_empty(src)) {
			ret = -EWOULDBLOCK;
			pr_info("[vbi..] %s: buffer empty, non_blocking: 0x%x, empty?: %d\n",
			__func__, non_blocking, vbi_ringbuffer_empty(src));
			break;
		}

		if (tvafe_clk_status == false) {
			ret = -EWOULDBLOCK;
			pr_info("[vbi..] %s: tvafe is closed.\n", __func__);
			break;
		}
		if (vbi_dbg_en)
			pr_info("[vbi...]%s:src->pread = %d;src->pwrite = %d\n",
			__func__,
					src->pread, src->pwrite);
		if (ret < 0) {
			pr_info("[vbi..] %s: read wait_event_interruptible error\n",
				__func__);
			break;
		}

		if (src->error) {
			ret = src->error;
			vbi_ringbuffer_flush(src);
			pr_info("[vbi..] %s: read buffer error\n", __func__);
			break;
		}

		avail = vbi_ringbuffer_avail(src);
		if (avail > todo)
			avail = todo;

		ret = vbi_ringbuffer_read_user(src, buf, avail);
		if (ret < 0) {
			pr_info("[vbi..] %s: vbi_ringbuffer_read_user error\n",
				__func__);
			break;
		}
		buf += ret;
	}

	if (tvafe_clk_status == false) {
		pr_info("[vbi..] %s: tvafe closed already.return 0\n",
			__func__);
		return 0;
	}

	if ((count - todo) <= 0)
		pr_info("[vbi..] %s: read error!! counter: %Zx or %Zx\n",
			__func__, (count - todo) , ret);

	return (count - todo) ? (count - todo) : ret;
}

static ssize_t vbi_read(struct file *file, char __user *buf, size_t count,
	loff_t *ppos)
{
	struct vbi_dev_s *vbi_dev = file->private_data;
	struct vbi_slicer_s *vbi_slicer = vbi_dev->slicer;
	int ret = 0;

	if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
		pr_info("[vbi..] %s: slicer mutex error\n", __func__);
		return -ERESTARTSYS;
	}

	ret = vbi_buffer_read(&vbi_slicer->buffer,
	file->f_flags & O_NONBLOCK,
	buf, count, ppos);

	mutex_unlock(&vbi_slicer->mutex);

	return ret;
}

static int vbi_open(struct inode *inode, struct file *file)
{
	struct vbi_dev_s *vbi_dev;
	int ret = 0;

	pr_info("[vbi..] %s: open start.\n", __func__);
	vbi_dev = container_of(inode->i_cdev, struct vbi_dev_s, cdev);
	file->private_data = vbi_dev;
	if (mutex_lock_interruptible(&vbi_dev->mutex)) {
		pr_info("[vbi..] %s: dev mutex_lock_interruptible error\n",
			__func__);
		return -ERESTARTSYS;
	}
	tasklet_enable(&vbi_dev->tsklt_slicer);
	mutex_init(&vbi_dev->slicer->mutex);
	vbi_ringbuffer_init(&vbi_dev->slicer->buffer, NULL,
		VBI_DEFAULT_BUFFER_PACKEGE_NUM);
	vbi_dev->slicer->type = VBI_TYPE_NULL;
	vbi_slicer_state_set(vbi_dev, VBI_STATE_ALLOCATED);
	spin_lock_init(&vbi_dev->vbi_isr_lock);
	/*disable data capture function*/
	vbi_dev->tasklet_enable = false;
	vbi_dev->vbi_start = false;
	/* request irq */
	ret = request_irq(vbi_dev->vs_irq, vbi_isr, IRQF_SHARED,
		vbi_dev->irq_name, (void *)vbi_dev);
	if (ret < 0)
		pr_err("[vbi..] %s: request_irq fail\n", __func__);

	mutex_unlock(&vbi_dev->mutex);
	pr_info("[vbi..]%s: open device ok.\n", __func__);
	return 0;
}


static int vbi_release(struct inode *inode, struct file *file)
{
	struct vbi_dev_s *vbi_dev = file->private_data;
	struct vbi_slicer_s *vbi_slicer = vbi_dev->slicer;
	int ret = 0;

	vbi_dev->tasklet_enable = false;
	vbi_dev->vbi_start = false;  /*disable data capture function*/
	tasklet_disable_nosync(&vbi_dev->tsklt_slicer);
	ret = vbi_slicer_free(vbi_dev, vbi_slicer);
	/* free irq */
	free_irq(vbi_dev->vs_irq, (void *)vbi_dev);
	/* vbi reset release, vbi agent enable */
	W_VBI_APB_REG(ACD_REG_22, 0x06080000);
	W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x00000014);
	pr_info("[vbi..]%s: device release OK.\n", __func__);
	return ret;
}


static long vbi_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	unsigned long buffer_size_t;

	struct vbi_dev_s *vbi_dev = file->private_data;
	struct vbi_slicer_s *vbi_slicer = vbi_dev->slicer;

	if (mutex_lock_interruptible(&vbi_dev->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case VBI_IOC_START:
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			pr_info("[vbi..] %s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		if (tvafe_clk_status)
			vbi_hw_init(vbi_dev);
		else {
			ret = -EINVAL;
			mutex_unlock(&vbi_slicer->mutex);
			break;
		}
		if (vbi_slicer->state < VBI_STATE_SET)
			ret = -EINVAL;
		else
			ret = vbi_slicer_start(vbi_dev);
		/* enable data capture function */
		vbi_dev->tasklet_enable = true;
		vbi_dev->vbi_start = true;
		vbi_dev->vs_delay = 40;

		mutex_unlock(&vbi_slicer->mutex);
		mdelay(1000);
		pr_info("[vbi..] %s: start slicer state:%d\n",
			__func__, vbi_slicer->state);
		break;

	case VBI_IOC_STOP:
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			pr_info("[vbi..] %s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		ret = vbi_slicer_stop(vbi_slicer);
		/* disable data capture function */
		vbi_dev->tasklet_enable = false;
		vbi_dev->vbi_start = false;
		/* manuel reset vbi */
		/*W_VBI_APB_REG(ACD_REG_22, 0x82080000);*/
		/* vbi reset release, vbi agent enable*/
		W_VBI_APB_REG(ACD_REG_22, 0x06080000);
		/*WAPB_REG(CVD2_VBI_CC_START, 0x00000054);*/
		W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x00000014);
		pr_info("[vbi..] %s: disable vbi function\n", __func__);
		mutex_unlock(&vbi_slicer->mutex);
		pr_info("[vbi..] %s: stop slicer state:%d\n",
			__func__, vbi_slicer->state);
		break;

	case VBI_IOC_SET_TYPE:
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			pr_info("[vbi..] %s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		if (copy_from_user(&vbi_slicer->type, argp, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		if (tvafe_clk_status)
			ret = vbi_slicer_set(vbi_dev, vbi_slicer);
		else
			ret = -EFAULT;
		mutex_unlock(&vbi_slicer->mutex);
		pr_info("[vbi..] %s: set slicer type to %d ,state:%d\n",
			__func__, vbi_slicer->type, vbi_slicer->state);
		break;

	case VBI_IOC_S_BUF_SIZE:
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			pr_info("[vbi..] %s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		if (copy_from_user(&buffer_size_t, argp, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		ret = vbi_set_buffer_size(vbi_dev, buffer_size_t);
		mutex_unlock(&vbi_slicer->mutex);
		pr_info("[vbi..] %s: set buf size to %d ,state:%d\n",
			__func__, vbi_slicer->buffer.size, vbi_slicer->state);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&vbi_dev->mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long vbi_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;
	arg = (unsigned long)compat_ptr(arg);
	ret = vbi_ioctl(file, cmd, arg);
	return ret;
}
#endif

/* vbi package data capture */
static int vbi_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long start, len, off;
	unsigned long pfn, size;
	struct vbi_dev_s *devp = file->private_data;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;


	/* capture the vbi data  */
	start = ((devp->mem_start)) & PAGE_MASK;
	pr_info("[vbi..]: cat start add:0x%x ,size:%x\n",
			((devp->mem_start)), (devp->mem_size));
	len = PAGE_ALIGN((start & ~PAGE_MASK) + (devp->mem_size));

	off = vma->vm_pgoff << PAGE_SHIFT;

	pr_info("[vbi..]: vend:%lx,vm_start:%lx, len:%lx\n",
			(vma->vm_end), vma->vm_start, len);
	/* if ((vma->vm_end - vma->vm_start + off) > len) { */
	/* pr_info("[vbi..]: error!! vend:%p,vm_start:%p, len:%x\n",
	(vma->vm_end), vma->vm_start, len); */
	/* return -EINVAL; */
	/* } */
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	size = vma->vm_end - vma->vm_start;
	pfn  = off >> PAGE_SHIFT;

	if (io_remap_pfn_range(vma, vma->vm_start, pfn, size,
		vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static unsigned int vbi_poll(struct file *file, poll_table *wait)
{
	struct vbi_dev_s *devp = file->private_data;
	struct vbi_slicer_s *vbi_slicer = devp->slicer;
	unsigned int mask = 0;

	if (!vbi_slicer) {
		pr_info("[vbi..] %s: slicer null\n", __func__);
		return -EINVAL;
	}
	poll_wait(file, &vbi_slicer->buffer.queue, wait);

	if (vbi_slicer->state != VBI_STATE_GO &&
		vbi_slicer->state != VBI_STATE_DONE &&
		vbi_slicer->state != VBI_STATE_TIMEDOUT)
		return 0;

	if (vbi_slicer->buffer.error)
		mask |= (POLLIN | POLLRDNORM | POLLPRI | POLLERR);

	if (!vbi_ringbuffer_empty(&vbi_slicer->buffer))
		mask |= (POLLIN | POLLRDNORM | POLLPRI);

	return mask;
}

/* File operations structure. Defined in linux/fs.h */
static const struct file_operations vbi_fops = {
	.owner   = THIS_MODULE,         /* Owner */
	.open    = vbi_open,            /* Open method */
	.release = vbi_release,         /* Release method */
	.unlocked_ioctl   = vbi_ioctl,           /* Ioctl method */
#ifdef CONFIG_COMPAT
	.compat_ioctl = vbi_compat_ioctl,
#endif
	.mmap    = vbi_mmap,
	.read    = vbi_read,
	.poll    = vbi_poll,
	/* ... */
};
static struct resource vbi_memobj;
static void vbi_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[2] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}
static void vbi_dump_mem(char *path, struct vbi_dev_s *devp)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	if (IS_ERR(filp)) {
		pr_info(KERN_ERR"create %s error.\n", path);
		return;
	}
	buf = phys_to_virt(devp->mem_start);
	if (buf == NULL) {
		pr_info(KERN_ERR"buf is null!!!.\n");
		return;
	}

	vfs_write(filp, buf, devp->mem_size, &pos);
	pr_info("write buffer addr:0x%p size: %2u  to %s.\n",
			buf, devp->mem_size, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}
static void vbi_dump_buf(char *path, struct vbi_dev_s *devp)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	if (IS_ERR(filp)) {
		pr_info(KERN_ERR"create %s error.\n", path);
		return;
	}
	buf = (void *)devp->slicer->buffer.data;
	if (buf == NULL) {
		pr_info(KERN_ERR"buf is null!!!.\n");
		return;
	}
	vfs_write(filp, buf, devp->slicer->buffer.size, &pos);
	pr_info("write buffer addr:0x%p size: %2u  to %s.\n",
			buf, devp->slicer->buffer.size, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static const char *vbi_debug_usage_str = {
	"Usage:\n"
	"echo dumpmem /udisk-path/***/xxx.txt > /sys/class/vbi/vbi/debug; dump vbi mem\n"
	"echo dumpbuf /udisk-path/***/xxx.txt > /sys/class/vbi/vbi/debug; dump vbi mem\n"
	"echo status > /sys/class/vbi/vbi/debug; dump vbi status\n"
	"echo enable_tasklet > /sys/class/vbi/vbi/debug; enable vbi tasklet\n"
	"echo data_wmode val(d) > /sys/class/vbi/vbi/debug; set vbi data write mode\n"
	"echo start > /sys/class/vbi/vbi/debug; start vbi\n"
	"echo stop > /sys/class/vbi/vbi/debug; stop vbi\n"
	"echo set_size val(d) > /sys/class/vbi/vbi/debug; set vbi buf size\n"
	"echo set_type val(x) > /sys/class/vbi/vbi/debug; set vbi type\n"
	"echo open > /sys/class/vbi/vbi/debug; open vbi device\n"
	"echo release > /sys/class/vbi/vbi/debug; release vbi device\n"
};

static ssize_t vbi_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	return sprintf(buff, "%s\n", vbi_debug_usage_str);
}

static ssize_t vbi_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t len)
{
	char *buf_orig;
	char *parm[6] = {NULL};
	struct vbi_dev_s *devp = dev_get_drvdata(dev);
	struct vbi_slicer_s *vbi_slicer = devp->slicer;
	struct vbi_ringbuffer_s *vbi_buffer = &(vbi_slicer->buffer);
	long val;
	int ret = 0;
	if (!buff || !devp)
		return len;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	vbi_parse_param(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "dumpmem", strlen("dumpmem"))) {
		vbi_dump_mem(parm[1], devp);
		pr_info("dump mem done!!\n");
	} else if (!strncmp(parm[0], "dumpbuf", strlen("dumpbuf"))) {
		vbi_dump_buf(parm[1], devp);
		pr_info("dump buf done!!\n");
	} else if (!strncmp(parm[0], "status", strlen("status"))) {
		pr_info("vcnt:0x%x\n", vcnt);
		pr_info("mem_start:0x%x,mem_size:0x%x\n",
			devp->mem_start, devp->mem_size);
		pr_info("vbi_start:%d,vbi_data_type:0x%x,vbi_start_code:0x%x,tasklet_enable:%d\n",
			devp->vbi_start, devp->vbi_data_type,
			devp->vbi_start_code, devp->tasklet_enable);
		pr_info("vbi_dto_cc:0x%x,vbi_dto_tt:0x%x\n",
			devp->vbi_dto_cc, devp->vbi_dto_tt);
		pr_info("vbi_dto_wss:0x%x,vbi_dto_vps:0x%x\n",
			devp->vbi_dto_wss, devp->vbi_dto_vps);
		pr_info("mem_start:0x%p,pac_addr_start:0x%p,pac_addr_end:0x%p\n",
			devp->pac_addr, devp->pac_addr_start,
			devp->pac_addr_end);
		if (vbi_slicer)
			pr_info("vbi_slicer:type:%d,state:%d\n",
				vbi_slicer->type, vbi_slicer->state);
		if (vbi_buffer)
			pr_info("vbi_buffer:size:%d,data_num:%d,pread:%d,pwrite:%d,data_wmode:%d\n",
				vbi_buffer->size, vbi_buffer->data_num,
				vbi_buffer->pread, vbi_buffer->pwrite,
				vbi_buffer->data_wmode);
		pr_info("dump satus done!!\n");
	} else if (!strncmp(parm[0], "enable_tasklet",
		strlen("enable_tasklet"))) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		devp->tasklet_enable = val;
		pr_info("tasklet_enable:%d\n", devp->tasklet_enable);
	} else if (!strncmp(parm[0], "data_wmode",
		strlen("data_wmode"))) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		vbi_buffer->data_wmode = val;
		pr_info("data_wmode:%d\n", vbi_buffer->data_wmode);
	} else if (!strncmp(parm[0], "start", strlen("start"))) {
		vbi_hw_init(devp);
		vbi_slicer_start(devp);
		/* enable data capture function */
		devp->tasklet_enable = true;
		devp->vbi_start = true;
		devp->vs_delay = 40;
		pr_info("start done!!!\n");
	} else if (!strncmp(parm[0], "stop", strlen("stop"))) {
		vbi_slicer_stop(vbi_slicer);
		/* disable data capture function */
		devp->tasklet_enable = false;
		devp->vbi_start = false;
		/* manuel reset vbi */
		/* vbi reset release, vbi agent enable*/
		W_VBI_APB_REG(ACD_REG_22, 0x06080000);
		W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x00000014);
		pr_info("[vbi..] disable vbi function\n");
		pr_info("stop done!!!\n");
	} else if (!strncmp(parm[0], "set_size", strlen("set_size"))) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		vbi_set_buffer_size(devp, val);
		pr_info("[vbi..] set buf size to %d\n",
			vbi_slicer->buffer.size);
	} else if (!strncmp(parm[0], "set_type", strlen("set_type"))) {
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		vbi_slicer->type = val;
		vbi_slicer_set(devp, vbi_slicer);
		pr_info("[vbi..] set slicer type to %d\n",
			vbi_slicer->type);
	} else if (!strncmp(parm[0], "open", strlen("open"))) {
		vbi_ringbuffer_init(vbi_buffer, NULL,
			VBI_DEFAULT_BUFFER_PACKEGE_NUM);
		devp->slicer->type = VBI_TYPE_NULL;
		vbi_slicer_state_set(devp, VBI_STATE_ALLOCATED);
		spin_lock_init(&devp->vbi_isr_lock);
		/*disable data capture function*/
		devp->tasklet_enable = false;
		devp->vbi_start = false;
		/* request irq */
		ret = request_irq(devp->vs_irq, vbi_isr, IRQF_SHARED,
			devp->irq_name, (void *)devp);
		if (ret < 0)
			pr_err("[vbi..] request_irq fail\n");
		pr_info("[vbi..] open ok.\n");
	} else if (!strncmp(parm[0], "release", strlen("release"))) {
		ret = vbi_slicer_free(devp, vbi_slicer);
		devp->tasklet_enable = false;
		devp->vbi_start = false;  /*disable data capture function*/
		/* free irq */
		free_irq(devp->vs_irq, (void *)devp);
		/* vbi reset release, vbi agent enable */
		W_VBI_APB_REG(ACD_REG_22, 0x06080000);
		W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x00000014);
		pr_info("[vbi..]device release OK.\n");
	} else {
		pr_info("[vbi..]unsupport cmd!!!\n");
	}
	return len;
}

static DEVICE_ATTR(debug, 0644, vbi_show, vbi_store);

static int vbi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct vbi_dev_s *vbi_dev;

	/* allocate memory for the per-device structure */
	vbi_dev = kmalloc(sizeof(struct vbi_dev_s), GFP_KERNEL);
	if (!vbi_dev) {
		pr_err("[vbi..]: failed to allocate memory for vbi device\n");
		return -ENOMEM;
	}
	memset(vbi_dev, 0, sizeof(struct vbi_dev_s));

	/* connect the file operations with cdev */
	cdev_init(&vbi_dev->cdev, &vbi_fops);
	vbi_dev->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&vbi_dev->cdev, vbi_id, 1);
	if (ret) {
		pr_err("[vbi..]: failed to add device\n");
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	vbi_dev->dev = device_create(vbi_clsp, &pdev->dev,
		MKDEV(MAJOR(vbi_id), 0), NULL, "%s", VBI_NAME);
	if (IS_ERR(vbi_dev->dev)) {
		pr_err("[vbi..]: failed to create device node\n");
		ret = PTR_ERR(vbi_dev->dev);
		goto fail_create_device;
	}
	/*create sysfs attribute files*/
	ret = device_create_file(vbi_dev->dev, &dev_attr_debug);
	if (ret < 0) {
		pr_err("tvafe: fail to create dbg attribute file\n");
		goto fail_create_dbg_file;
	}

	/* get device memory */
	res = &vbi_memobj;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		pr_info("\n vbi memory resource done.\n");
	else
		pr_info("vbi: can't get memory resource\n");
	vbi_dev->mem_start = res->start;
	vbi_dev->mem_size = res->end - res->start + 1;
	if (vbi_dev->mem_size > VBI_MEM_SIZE)
		vbi_dev->mem_size = VBI_MEM_SIZE;
	pr_info("[vbi..]: start_addr is:0x%x, size is:0x%x\n",
			vbi_dev->mem_start, vbi_dev->mem_size);

	/* remap the package vbi hardware address for our conversion */
	vbi_dev->pac_addr_start = phys_to_virt(vbi_dev->mem_start);
	/*ioremap_nocache(vbi_dev->mem_start, vbi_dev->mem_size);*/
	memset(vbi_dev->pac_addr_start, 0, vbi_dev->mem_size);
	vbi_dev->mem_size = vbi_dev->mem_size/2;
	vbi_dev->mem_size >>= 4;
	vbi_dev->mem_size <<= 4;
	vbi_dev->pac_addr_end = vbi_dev->pac_addr_start + vbi_dev->mem_size - 1;
	if (vbi_dev->pac_addr_start == NULL)
		pr_err("[vbi..]: ioremap error!!!\n");
	else
		pr_info("[vbi..]: vbi_dev->pac_addr_start=0x%p, end:0x%p, size:0x%x .......\n",
	vbi_dev->pac_addr_start, vbi_dev->pac_addr_end, vbi_dev->mem_size);
	vbi_dev->pac_addr = vbi_dev->pac_addr_start;

	mutex_init(&vbi_dev->mutex);
	spin_lock_init(&vbi_dev->lock);

	/* init drv data */
	dev_set_drvdata(vbi_dev->dev, vbi_dev);
	platform_set_drvdata(pdev, vbi_dev);

	/* Initialize tasklet */
	tasklet_init(&vbi_dev->tsklt_slicer, vbi_slicer_task,
				(unsigned long)vbi_dev);

	tasklet_disable_nosync(&vbi_dev->tsklt_slicer);
	vbi_dev->tasklet_enable = false;
	vbi_dev->vbi_start = false;
	vbi_dev->vs_delay = 40;

	vbi_dev->slicer = vmalloc(sizeof(struct vbi_slicer_s));
	if (!vbi_dev->slicer) {
		pr_err("[vbi..]: vmalloc error!!!\n");
		ret = -ENOMEM;
		goto fail_alloc_mem;
	}
	vbi_dev->slicer->buffer.data = NULL;
	vbi_dev->slicer->state = VBI_STATE_FREE;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("%s: can't get irq resource\n", __func__);
		ret = -ENXIO;
		goto fail_get_resource_irq;
	}
	vbi_dev->vs_irq = res->start;
	snprintf(vbi_dev->irq_name, sizeof(vbi_dev->irq_name),
			"vbi-irq");
	pr_info("vbi irq: %d\n", vbi_dev->vs_irq);

	pr_info("[vbi..]: driver probe ok\n");
	return 0;

fail_get_resource_irq:
fail_alloc_mem:
fail_create_dbg_file:
	device_destroy(vbi_clsp, MKDEV(MAJOR(vbi_id), 0));
fail_create_device:
	cdev_del(&vbi_dev->cdev);
fail_add_cdev:
	kfree(vbi_dev);
	return ret;
}

static int vbi_remove(struct platform_device *pdev)
{
	struct vbi_dev_s *vbi_dev;
	vbi_dev = platform_get_drvdata(pdev);

	tasklet_kill(&vbi_dev->tsklt_slicer);
	if (vbi_dev->pac_addr_start)
		iounmap(vbi_dev->pac_addr_start);
	vfree(vbi_dev->slicer);
	device_destroy(vbi_clsp, MKDEV(MAJOR(vbi_id), 0));
	cdev_del(&vbi_dev->cdev);
	kfree(vbi_dev);
	class_destroy(vbi_clsp);
	unregister_chrdev_region(vbi_id, 0);

	pr_info("[vbi..] : driver removed ok.\n");

	return 0;
}
static const struct of_device_id vbi_dt_match[] = {
	{
	.compatible     = "amlogic, vbi",
	},
	{},
};

static struct platform_driver vbi_driver = {
	.probe      = vbi_probe,
	.remove     = vbi_remove,
	.driver     = {
		.name   = VBI_DRIVER_NAME,
		.of_match_table = vbi_dt_match,
	}
};

static int __init vbi_init(void)
{
	int ret = 0;
	ret = alloc_chrdev_region(&vbi_id, 0, 1, VBI_NAME);
	if (ret < 0) {
		pr_err("[vbi..]: failed to allocate major number\n");
		goto fail_alloc_cdev_region;
	}

	vbi_clsp = class_create(THIS_MODULE, VBI_NAME);
	if (IS_ERR(vbi_clsp)) {
		pr_err("[vbi..]: can't get vbi_clsp\n");
		goto fail_class_create;
	}

	ret = platform_driver_register(&vbi_driver);
	if (ret != 0) {
		pr_err("[vbi..] failed to register vbi module, error %d\n",
			ret);
		goto fail_pdrv_register;
		return -ENODEV;
	}
	pr_info("vbi: vbi_init.\n");
	return 0;

fail_pdrv_register:
	class_destroy(vbi_clsp);
fail_class_create:
	unregister_chrdev_region(vbi_id, 1);
fail_alloc_cdev_region:
	return ret;
}

static void __exit vbi_exit(void)
{
	platform_driver_unregister(&vbi_driver);
}
static int vbi_mem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	s32 ret = 0;
	struct resource *res = NULL;
	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	res = &vbi_memobj;
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;

	pr_info("init vbi memsource 0x%lx->0x%lx\n",
		(unsigned long)res->start, (unsigned long)res->end);

	return 0;
}

static const struct reserved_mem_ops rmem_vbi_ops = {
	.device_init = vbi_mem_device_init,
};

static int __init vbi_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vbi_ops;
	pr_info("vbi share mem setup\n");
	return 0;
}

module_init(vbi_init);
module_exit(vbi_exit);
RESERVEDMEM_OF_DECLARE(vbi, "amlogic, vbi-mem",
	vbi_mem_setup);

MODULE_DESCRIPTION("AMLOGIC vbi driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("frank  <frank.zhao@amlogic.com>");

