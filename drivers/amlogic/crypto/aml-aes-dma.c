/*
 * Cryptographic API.
 *
 * Support for Amlogic AES HW acceleration.
 *
 * Copyright (c) 2016 Amlogic Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Some ideas are borrowed from atmel-aes.c driver.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include "aml-crypto-dma.h"

/* AES flags */
#define AES_FLAGS_MODE_MASK	0x07
#define AES_FLAGS_ENCRYPT   BIT(0)
#define AES_FLAGS_CBC       BIT(1)
#define AES_FLAGS_CTR       BIT(2)

#define AES_FLAGS_INIT      BIT(8)
#define AES_FLAGS_DMA       BIT(9)
#define AES_FLAGS_FAST      BIT(10)
#define AES_FLAGS_BUSY      BIT(11)

#define AML_AES_QUEUE_LENGTH	50
#define AML_AES_DMA_THRESHOLD		16

#define DMA_THREAD_REG (DMA_T0 + AES_THREAD_INDEX)
#define DMA_STATUS_REG (DMA_STS0 + AES_THREAD_INDEX)

u8 map_in_aes_dma = 0;
struct aml_aes_dev;

struct aml_aes_ctx {
	struct aml_aes_dev *dd;

	int		keylen;
	u32		key[AES_KEYSIZE_256 / sizeof(u32)];

	u16		block_size;
};

struct aml_aes_reqctx {
	unsigned long mode;
};

struct aml_aes_dev {
	struct list_head	list;

	struct aml_aes_ctx	*ctx;
	struct device		*dev;
	int	irq;

	unsigned long		flags;
	int	err;

	spinlock_t		lock;
	struct crypto_queue	queue;

	struct tasklet_struct	done_task;
	struct tasklet_struct	queue_task;

	struct ablkcipher_request	*req;
	size_t	total;

	struct scatterlist	*in_sg;
	size_t			in_offset;
	struct scatterlist	*out_sg;
	size_t			out_offset;

	size_t	buflen;
	size_t	dma_size;

	void	*buf_in;
	dma_addr_t	dma_addr_in;

	void	*buf_out;
	dma_addr_t	dma_addr_out;

	void	*descriptor;
	dma_addr_t	dma_descript_tab;

	uint32_t fast_nents;
};

struct aml_aes_drv {
	struct list_head	dev_list;
	spinlock_t		lock;
};

static struct aml_aes_drv aml_aes = {
	.dev_list = LIST_HEAD_INIT(aml_aes.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_aes.lock),
};

static void set_aes_key_iv(struct aml_aes_dev *dd, u32 *key,
		uint32_t keylen, u32 *iv, uint8_t swap)
{
	struct dma_dsc *dsc = dd->descriptor;
	uint32_t key_iv[12];
	uint32_t *piv = key_iv + 8;
	uint32_t len = keylen;
	dma_addr_t dma_addr_key;

	memset(key_iv, 0, sizeof(key_iv));
	memcpy(key_iv, key, keylen);
	if (iv) {
		if (swap) {
			*(piv + 3) = swap_ulong32(*iv);
			*(piv + 2) = swap_ulong32(*(iv + 1));
			*(piv + 1) = swap_ulong32(*(iv + 2));
			*(piv + 0) = swap_ulong32(*(iv + 3));
		} else {
			memcpy(piv, iv, 16);
		}
		len = 48; /* full key storage */
	}

	dma_addr_key = dma_map_single(dd->dev, key_iv,
			sizeof(key_iv), DMA_TO_DEVICE);

	dsc->src_addr = (uint32_t)dma_addr_key;
	dsc->tgt_addr = 0;
	dsc->dsc_cfg.d32 = 0;
	dsc->dsc_cfg.b.length = len;
	dsc->dsc_cfg.b.mode = MODE_KEY;
	dsc->dsc_cfg.b.eoc = 1;
	dsc->dsc_cfg.b.owner = 1;

	dma_sync_single_for_device(dd->dev, dd->dma_descript_tab,
			PAGE_SIZE, DMA_TO_DEVICE);
	aml_write_crypto_reg(DMA_THREAD_REG,
			(uintptr_t) dd->dma_descript_tab | 2);
	aml_dma_debug(dsc, 1, __func__);
	while (aml_read_crypto_reg(DMA_STATUS_REG) == 0)
		;
	aml_write_crypto_reg(DMA_STATUS_REG, 0xf);
	dma_unmap_single(dd->dev, dma_addr_key,
			sizeof(key_iv), DMA_TO_DEVICE);
}

static size_t aml_aes_sg_copy(struct scatterlist **sg, size_t *offset,
		void *buf, size_t buflen, size_t total, int out)
{
	size_t count, off = 0;

	while (buflen && total) {
		count = min((*sg)->length - *offset, total);
		count = min(count, buflen);

		if (!count)
			return off;

		scatterwalk_map_and_copy(buf + off, *sg, *offset, count, out);

		off += count;
		buflen -= count;
		*offset += count;
		total -= count;

		if (*offset == (*sg)->length) {
			*sg = sg_next(*sg);
			if (*sg)
				*offset = 0;
			else
				total = 0;
		}
	}

	return off;
}

static size_t aml_aes_sg_dma(struct aml_aes_dev *dd, struct dma_dsc *dsc,
		uint32_t *nents, size_t total)
{
	size_t count = 0;
	uint32_t i = 0;
	int err = 0;
	struct scatterlist *in_sg = dd->in_sg;
	struct scatterlist *out_sg = dd->out_sg;
	dma_addr_t addr_in, addr_out;
	while (total && in_sg && out_sg && (in_sg->length == out_sg->length)
			&& *nents < MAX_NUM_TABLES) {
		count += min_t(unsigned int, total, in_sg->length);
		*nents += 1;
		total -= count;
		in_sg = sg_next(in_sg);
		out_sg = sg_next(out_sg);
	}
	err = dma_map_sg(dd->dev, dd->in_sg, *nents, DMA_TO_DEVICE);
	if (!err) {
		dev_err(dd->dev, "dma_map_sg() error\n");
		return 0;
	}

	err = dma_map_sg(dd->dev, dd->out_sg, *nents,
			DMA_FROM_DEVICE);
	if (!err) {
		dev_err(dd->dev, "dma_map_sg() error\n");
		dma_unmap_sg(dd->dev, dd->in_sg, *nents,
				DMA_TO_DEVICE);
		return 0;
	}

	in_sg = dd->in_sg;
	out_sg = dd->out_sg;
	for (i = 0; i < *nents; i++) {
		addr_in = sg_dma_address(in_sg);
		addr_out = sg_dma_address(out_sg);
		dsc[i].src_addr = (uintptr_t)addr_in;
		dsc[i].tgt_addr = (uintptr_t)addr_out;
		dsc[i].dsc_cfg.d32 = 0;
		dsc[i].dsc_cfg.b.length = sg_dma_len(in_sg);
		in_sg = sg_next(in_sg);
		out_sg = sg_next(out_sg);
	}
	return count;
}

static struct aml_aes_dev *aml_aes_find_dev(struct aml_aes_ctx *ctx)
{
	struct aml_aes_dev *aes_dd = NULL;
	struct aml_aes_dev *tmp;

	spin_lock_bh(&aml_aes.lock);
	if (!ctx->dd) {
		list_for_each_entry(tmp, &aml_aes.dev_list, list) {
			aes_dd = tmp;
			break;
		}
		ctx->dd = aes_dd;
	} else {
		aes_dd = ctx->dd;
	}

	spin_unlock_bh(&aml_aes.lock);

	return aes_dd;
}

static int aml_aes_hw_init(struct aml_aes_dev *dd)
{
	if (!(dd->flags & AES_FLAGS_INIT)) {
		dd->flags |= AES_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}

static void aml_aes_finish_req(struct aml_aes_dev *dd, int32_t err)
{
	struct ablkcipher_request *req = dd->req;
	dd->flags &= ~AES_FLAGS_BUSY;
	req->base.complete(&req->base, err);
}


static int aml_aes_crypt_dma(struct aml_aes_dev *dd, struct dma_dsc *dsc,
		uint32_t nents)
{
	uint32_t op_mode = OP_MODE_ECB;
	uint32_t i = 0;

	dd->flags |= AES_FLAGS_DMA;

	if (dd->flags & AES_FLAGS_CBC)
		op_mode = OP_MODE_CBC;
	else if (dd->flags & AES_FLAGS_CTR)
		op_mode = OP_MODE_CTR;

	for (i = 0; i < nents; i++) {
		dsc[i].dsc_cfg.b.enc_sha_only = dd->flags & AES_FLAGS_ENCRYPT;
		dsc[i].dsc_cfg.b.mode =
			((dd->ctx->keylen == AES_KEYSIZE_128) ? MODE_AES128 :
			 ((dd->ctx->keylen == AES_KEYSIZE_192) ?
			  MODE_AES192 : MODE_AES256));
		dsc[i].dsc_cfg.b.op_mode = op_mode;
		dsc[i].dsc_cfg.b.eoc = (i == (nents - 1));
		dsc[i].dsc_cfg.b.owner = 1;
	}

	dma_sync_single_for_device(dd->dev, dd->dma_descript_tab,
			PAGE_SIZE, DMA_TO_DEVICE);

	aml_dma_debug(dsc, nents, __func__);
	aml_write_crypto_reg(DMA_THREAD_REG, dd->dma_descript_tab | 2);
	return 0;
}

static int aml_aes_crypt_dma_start(struct aml_aes_dev *dd)
{
	int err = 0, fast = 0;
	int in, out;
	size_t count;
	dma_addr_t addr_in, addr_out;
	struct dma_dsc *dsc = dd->descriptor;
	uint32_t nents;

	/* fast dma */
	if ((!dd->in_offset) && (!dd->out_offset)) {
		/* check for alignment */
		in = IS_ALIGNED(dd->in_sg->length, dd->ctx->block_size);
		out = IS_ALIGNED(dd->out_sg->length, dd->ctx->block_size);
		fast = in && out;

		if (dd->in_sg->length != dd->out_sg->length
				|| dd->total < dd->ctx->block_size)
			fast = 0;
		dd->fast_nents = 0;
	}

	if (fast)  {
		count = aml_aes_sg_dma(dd, dsc, &dd->fast_nents, dd->total);
		dd->flags |= AES_FLAGS_FAST;
		nents = dd->fast_nents;
	} else {
		/* slow dma */
		/* use cache buffers */
		count = aml_aes_sg_copy(&dd->in_sg, &dd->in_offset,
				dd->buf_in, dd->buflen, dd->total, 0);
		addr_in = dd->dma_addr_in;
		addr_out = dd->dma_addr_out;
		dd->dma_size = count;
		dma_sync_single_for_device(dd->dev, addr_in, dd->dma_size,
				DMA_TO_DEVICE);
		dsc->src_addr = (uint32_t)addr_in;
		dsc->tgt_addr = (uint32_t)addr_out;
		dsc->dsc_cfg.d32 = 0;
		dsc->dsc_cfg.b.length = count;
		nents = 1;
		dd->flags &= ~AES_FLAGS_FAST;
	}
	dd->total -= count;

	err = aml_aes_crypt_dma(dd, dsc, nents);

	if (err && (dd->flags & AES_FLAGS_FAST)) {
		dma_unmap_sg(dd->dev, dd->in_sg,
				dd->fast_nents, DMA_TO_DEVICE);
		dma_unmap_sg(dd->dev, dd->out_sg,
				dd->fast_nents, DMA_TO_DEVICE);
	}

	return err;
}

static int aml_aes_write_ctrl(struct aml_aes_dev *dd)
{
	int err = 0;
	err = aml_aes_hw_init(dd);

	if (err)
		return err;

	if (dd->flags & AES_FLAGS_CBC)
		set_aes_key_iv(dd, dd->ctx->key, dd->ctx->keylen,
				dd->req->info, 0);
	else if  (dd->flags & AES_FLAGS_CTR)
		set_aes_key_iv(dd, dd->ctx->key, dd->ctx->keylen,
				dd->req->info, 1);
	else
		set_aes_key_iv(dd, dd->ctx->key, dd->ctx->keylen, NULL, 0);

	return err;
}

static int aml_aes_handle_queue(struct aml_aes_dev *dd,
		struct ablkcipher_request *req)
{
	struct crypto_async_request *async_req, *backlog;
	struct aml_aes_ctx *ctx;
	struct aml_aes_reqctx *rctx;
	unsigned long flags;
	int32_t err, ret = 0;

	spin_lock_irqsave(&dd->lock, flags);
	if (req)
		ret = ablkcipher_enqueue_request(&dd->queue, req);

	if (dd->flags & AES_FLAGS_BUSY) {
		spin_unlock_irqrestore(&dd->lock, flags);
		return ret;
	}
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (async_req)
		dd->flags |= AES_FLAGS_BUSY;
	spin_unlock_irqrestore(&dd->lock, flags);

	if (!async_req)
		return ret;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);

	/* assign new request to device */
	dd->req = req;
	dd->total = req->nbytes;
	dd->in_offset = 0;
	dd->in_sg = req->src;
	dd->out_offset = 0;
	dd->out_sg = req->dst;

	rctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	rctx->mode &= AES_FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~AES_FLAGS_MODE_MASK) | rctx->mode;
	dd->ctx = ctx;
	ctx->dd = dd;

	err = aml_aes_write_ctrl(dd);
	if (!err) {
		if (dd->total % AML_AES_DMA_THRESHOLD == 0)
			err = aml_aes_crypt_dma_start(dd);
		else {
			pr_err("size %zd is not multiple of %d",
					dd->total, AML_AES_DMA_THRESHOLD);
			err = -EINVAL;
		}
	}
	if (err) {
		/* aes_task will not finish it, so do it here */
		aml_aes_finish_req(dd, err);
		tasklet_schedule(&dd->queue_task);
	}

	return ret;
}

static int aml_aes_crypt_dma_stop(struct aml_aes_dev *dd)
{
	int err = -EINVAL;
	size_t count;

	if (dd->flags & AES_FLAGS_DMA) {
		err = 0;
		dma_sync_single_for_cpu(dd->dev, dd->dma_descript_tab,
				PAGE_SIZE, DMA_FROM_DEVICE);
		if  (dd->flags & AES_FLAGS_FAST) {
			dma_unmap_sg(dd->dev, dd->out_sg,
					dd->fast_nents, DMA_FROM_DEVICE);
			dma_unmap_sg(dd->dev, dd->in_sg,
					dd->fast_nents, DMA_TO_DEVICE);
		} else {
			dma_sync_single_for_cpu(dd->dev, dd->dma_addr_out,
					dd->dma_size, DMA_FROM_DEVICE);

			/* copy data */
			count = aml_aes_sg_copy(&dd->out_sg, &dd->out_offset,
					dd->buf_out, dd->buflen,
					dd->dma_size, 1);
			if (count != dd->dma_size) {
				err = -EINVAL;
				pr_err("not all data converted: %zu\n", count);
			}
		}
		dd->flags &= ~AES_FLAGS_DMA;
	}

	return err;
}


static int aml_aes_buff_init(struct aml_aes_dev *dd)
{
	int err = -ENOMEM;

	dd->buf_in = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buf_out = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->descriptor = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buflen = PAGE_SIZE;
	dd->buflen &= ~(AES_BLOCK_SIZE - 1);

	if (!dd->buf_in || !dd->buf_out || !dd->descriptor) {
		dev_err(dd->dev, "unable to alloc pages.\n");
		goto err_alloc;
	}

	/* MAP here */
	dd->dma_addr_in = dma_map_single(dd->dev, dd->buf_in,
			dd->buflen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_in)) {
		dev_err(dd->dev, "dma %zd bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_in;
	}

	dd->dma_addr_out = dma_map_single(dd->dev, dd->buf_out,
			dd->buflen, DMA_FROM_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_out)) {
		dev_err(dd->dev, "dma %zd bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_out;
	}

	dd->dma_descript_tab = dma_map_single(dd->dev, dd->descriptor,
			PAGE_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(dd->dev, dd->dma_descript_tab)) {
		dev_err(dd->dev, "dma descriptor error\n");
		err = -EINVAL;
		goto err_map_descriptor;
	}

	return 0;

err_map_descriptor:
	dma_unmap_single(dd->dev, dd->dma_descript_tab, PAGE_SIZE,
			DMA_TO_DEVICE);

err_map_out:
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
			DMA_TO_DEVICE);
err_map_in:
	free_page((uintptr_t)dd->buf_out);
	free_page((uintptr_t)dd->buf_in);
	free_page((uintptr_t)dd->descriptor);
err_alloc:
	if (err)
		pr_err("error: %d\n", err);
	return err;
}

static void aml_aes_buff_cleanup(struct aml_aes_dev *dd)
{
	dma_unmap_single(dd->dev, dd->dma_addr_out, dd->buflen,
			DMA_FROM_DEVICE);
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
			DMA_TO_DEVICE);
	dma_unmap_single(dd->dev, dd->dma_descript_tab, PAGE_SIZE,
			DMA_TO_DEVICE);
	free_page((uintptr_t)dd->buf_out);
	free_page((uintptr_t)dd->buf_in);
	free_page((uintptr_t)dd->descriptor);
}

static int aml_aes_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct aml_aes_ctx *ctx = crypto_ablkcipher_ctx(
			crypto_ablkcipher_reqtfm(req));
	struct aml_aes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct aml_aes_dev *dd;

	if (!IS_ALIGNED(req->nbytes, AES_BLOCK_SIZE)) {
		pr_err("request size is not exact amount of AES blocks\n");
		return -EINVAL;
	}
	ctx->block_size = AES_BLOCK_SIZE;

	dd = aml_aes_find_dev(ctx);
	if (!dd)
		return -ENODEV;

	rctx->mode = mode;

	return aml_aes_handle_queue(dd, req);
}

static int aml_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
		unsigned int keylen)
{
	struct aml_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
			keylen != AES_KEYSIZE_256) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int aml_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT);
}

static int aml_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			0);
}

static int aml_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT | AES_FLAGS_CBC);
}

static int aml_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_CBC);
}

static int aml_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT | AES_FLAGS_CTR);
}

static int aml_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	/* XXX: use encrypt to replace for decrypt */
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT | AES_FLAGS_CTR);
}

static int aml_aes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_aes_reqctx);

	return 0;
}

static void aml_aes_cra_exit(struct crypto_tfm *tfm)
{
}

static struct crypto_alg aes_algs[] = {
	{
		.cra_name         = "ecb(aes)",
		.cra_driver_name  = "ecb-aes-aml",
		.cra_priority   = 0,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_cra_init,
		.cra_exit       = aml_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_setkey,
			.encrypt	=    aml_aes_ecb_encrypt,
			.decrypt	=    aml_aes_ecb_decrypt,
		}
	},
	{
		.cra_name         = "cbc(aes)",
		.cra_driver_name  = "cbc-aes-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_cra_init,
		.cra_exit       = aml_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_setkey,
			.encrypt	=    aml_aes_cbc_encrypt,
			.decrypt	=    aml_aes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ctr(aes)",
		.cra_driver_name = "ctr-aes-aml",
		.cra_priority    = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_cra_init,
		.cra_exit       = aml_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_setkey,
			.encrypt	=    aml_aes_ctr_encrypt,
			.decrypt	=    aml_aes_ctr_decrypt,
		}
	}
};

static void aml_aes_queue_task(unsigned long data)
{
	struct aml_aes_dev *dd = (struct aml_aes_dev *)data;
	aml_aes_handle_queue(dd, NULL);
}

static void aml_aes_done_task(unsigned long data)
{
	struct aml_aes_dev *dd = (struct aml_aes_dev *) data;
	int err;

	err = aml_aes_crypt_dma_stop(dd);

	aml_dma_debug(dd->descriptor, dd->fast_nents ?
			dd->fast_nents : 1, __func__);

	err = dd->err ? : err;

	if (dd->total && !err) {
		if (dd->flags & AES_FLAGS_FAST) {
			uint32_t i = 0;
			for (i = 0; i < dd->fast_nents; i++) {
				dd->in_sg = sg_next(dd->in_sg);
				dd->out_sg = sg_next(dd->out_sg);
				if (!dd->in_sg || !dd->out_sg)
					err = -EINVAL;
			}
		}

		if (!err)
			err = aml_aes_crypt_dma_start(dd);
		if (!err)
			return; /* DMA started. Not fininishing. */
	}

	aml_aes_finish_req(dd, err);
	aml_aes_handle_queue(dd, NULL);
}

static irqreturn_t aml_aes_irq(int irq, void *dev_id)
{
	struct aml_aes_dev *aes_dd = dev_id;
	uint8_t status = aml_read_crypto_reg(DMA_STATUS_REG);

	if (status) {
		if (status == 0x1)
			pr_err("irq overwrite\n");
		if (AES_FLAGS_DMA & aes_dd->flags) {
			aml_write_crypto_reg(DMA_STATUS_REG, 0xf);
			tasklet_schedule(&aes_dd->done_task);
			return IRQ_HANDLED;
		} else {
			return IRQ_NONE;
		}
	}

	return IRQ_NONE;
}

static void aml_aes_unregister_algs(struct aml_aes_dev *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aes_algs); i++)
		crypto_unregister_alg(&aes_algs[i]);
}

static int aml_aes_register_algs(struct aml_aes_dev *dd)
{
	int err, i, j;

	for (i = 0; i < ARRAY_SIZE(aes_algs); i++) {
		err = crypto_register_alg(&aes_algs[i]);
		if (err)
			goto err_aes_algs;
	}

	return 0;

err_aes_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&aes_algs[j]);

	return err;
}

static int aml_aes_probe(struct platform_device *pdev)
{
	struct aml_aes_dev *aes_dd;
	struct device *dev = &pdev->dev;
	struct resource *res_irq = 0;
	struct resource *res_base = 0;
	int err = -EPERM;

	aes_dd = kzalloc(sizeof(struct aml_aes_dev), GFP_KERNEL);
	if (aes_dd == NULL) {
		dev_err(dev, "unable to alloc data struct.\n");
		err = -ENOMEM;
		goto aes_dd_err;
	}

	aes_dd->dev = dev;
	platform_set_drvdata(pdev, aes_dd);
	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, AES_THREAD_INDEX);
	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base) {
		dev_err(dev, "error to get normal IORESOURCE_MEM.\n");
		goto aes_dd_err;
	} else {
		if (!cryptoreg_offset) {
			cryptoreg_offset = ioremap(res_base->start,
					resource_size(res_base));
			map_in_aes_dma = 1;
		}
	}

	INIT_LIST_HEAD(&aes_dd->list);

	tasklet_init(&aes_dd->done_task, aml_aes_done_task,
			(unsigned long)aes_dd);
	tasklet_init(&aes_dd->queue_task, aml_aes_queue_task,
			(unsigned long)aes_dd);

	crypto_init_queue(&aes_dd->queue, AML_AES_QUEUE_LENGTH);

	aes_dd->irq = res_irq->start;

	err = request_irq(aes_dd->irq, aml_aes_irq, IRQF_SHARED, "aml-aes",
			aes_dd);
	if (err) {
		dev_err(dev, "unable to request aes irq.\n");
		goto aes_irq_err;
	}

	err = aml_aes_hw_init(aes_dd);
	if (err)
		goto err_aes_buff;

	err = aml_aes_buff_init(aes_dd);
	if (err)
		goto err_aes_buff;

	spin_lock(&aml_aes.lock);
	list_add_tail(&aes_dd->list, &aml_aes.dev_list);
	spin_unlock(&aml_aes.lock);

	err = aml_aes_register_algs(aes_dd);
	if (err)
		goto err_algs;

	dev_info(dev, "Aml AES_dma\n");

	return 0;

err_algs:
	spin_lock(&aml_aes.lock);
	list_del(&aes_dd->list);
	spin_unlock(&aml_aes.lock);
	aml_aes_buff_cleanup(aes_dd);
err_aes_buff:
	free_irq(aes_dd->irq, aes_dd);
aes_irq_err:

	if (map_in_aes_dma) {
		iounmap(cryptoreg_offset);
		map_in_aes_dma = 0;
	}

	tasklet_kill(&aes_dd->done_task);
	tasklet_kill(&aes_dd->queue_task);
	kfree(aes_dd);
	aes_dd = NULL;
aes_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_aes_remove(struct platform_device *pdev)
{
	static struct aml_aes_dev *aes_dd;

	aes_dd = platform_get_drvdata(pdev);
	if (!aes_dd)
		return -ENODEV;
	spin_lock(&aml_aes.lock);
	list_del(&aes_dd->list);
	spin_unlock(&aml_aes.lock);

	aml_aes_unregister_algs(aes_dd);

	tasklet_kill(&aes_dd->done_task);
	tasklet_kill(&aes_dd->queue_task);

	if (map_in_aes_dma) {
		iounmap(cryptoreg_offset);
		map_in_aes_dma = 0;
	}


	if (aes_dd->irq > 0)
		free_irq(aes_dd->irq, aes_dd);

	kfree(aes_dd);
	aes_dd = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_aes_dt_match[] = {
	{	.compatible = "amlogic,aes_dma",
	},
	{},
};
#else
#define aml_aes_dt_match NULL
#endif

static struct platform_driver aml_aes_driver = {
	.probe		= aml_aes_probe,
	.remove		= aml_aes_remove,
	.driver		= {
		.name	= "aml_aes_dma",
		.owner	= THIS_MODULE,
		.of_match_table = aml_aes_dt_match,
	},
};

module_platform_driver(aml_aes_driver);

MODULE_DESCRIPTION("Aml AES hw acceleration support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("matthew.shyu <matthew.shyu@amlogic.com>");
