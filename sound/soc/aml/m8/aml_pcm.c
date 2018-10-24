/*
 * sound/soc/aml/m8/aml_pcm.c
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
#define pr_fmt(fmt) "aml_pcm: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/soundcard.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <linux/major.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/amlogic/sound/audin_regs.h>
#include "aml_pcm.h"
#include "aml_audio_hw_pcm.h"

/*--------------------------------------------------------------------------*\
 * Hardware definition
\*--------------------------------------------------------------------------*/
/* TODO: These values were taken from the AML platform driver, check
 *	 them against real values for AML
 */
static const struct snd_pcm_hardware aml_pcm_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,
	.formats =
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min = 32,
	.period_bytes_max = 8 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 64 * 1024,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 8,
};

static const struct snd_pcm_hardware aml_pcm_capture = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,

	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min = 64,
	.period_bytes_max = 32 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 64 * 1024,

	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 8,
	.fifo_size = 0,
};


static unsigned int period_sizes[] = { 64, 128, 256, 512,
	1024, 2048, 4096, 8192
};

static struct snd_pcm_hw_constraint_list hw_constraints_period_sizes = {
	.count = ARRAY_SIZE(period_sizes),
	.list = period_sizes,
	.mask = 0
};

static unsigned int aml_pcm_offset_tx(struct aml_pcm_runtime_data *prtd)
{
	unsigned int value = 0;
	signed int diff = 0;

	value = pcm_out_rd_ptr();
	diff = value - prtd->buffer_start;
	if (diff < 0)
		diff = 0;
	else if (diff >= prtd->buffer_size)
		diff = prtd->buffer_size;

	pr_debug("%s value: 0x%08x offset: 0x%08x\n", __func__,
		  value, diff);
	return (unsigned int)diff;
}

static unsigned int aml_pcm_offset_rx(struct aml_pcm_runtime_data *prtd)
{
	unsigned int value = 0;
	signed int diff = 0;

	value = pcm_in_wr_ptr();
	diff = value - prtd->buffer_start;
	if (diff < 0)
		diff = 0;
	else if (diff >= prtd->buffer_size)
		diff = prtd->buffer_size;

	pr_debug(KERN_DEBUG "%s value: 0x%08x offset: 0x%08x\n", __func__,
		  value, diff);
	return (unsigned int)diff;
}

static void aml_pcm_timer_update(struct aml_pcm_runtime_data *prtd)
{
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int offset = 0;
	unsigned int size = 0;

	if (prtd->running && snd_pcm_running(substream)) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			offset = aml_pcm_offset_tx(prtd);
			if (offset < prtd->buffer_offset) {
				size =
				    prtd->buffer_size + offset -
				    prtd->buffer_offset;
			} else {
				size = offset - prtd->buffer_offset;
			}
		} else {
			int rx_overflow = 0;
			offset = aml_pcm_offset_rx(prtd);
			if (offset < prtd->buffer_offset) {
				size =
				    prtd->buffer_size + offset -
				    prtd->buffer_offset;
			} else
				size = offset - prtd->buffer_offset;

			rx_overflow = pcm_in_fifo_int() & (1 << 2);
			if (rx_overflow) {
				pr_info("%s AUDIN_FIFO overflow !!\n",
					__func__);
			}
		}
	}

	prtd->buffer_offset = offset;
	prtd->data_size += size;
	if (prtd->data_size >= frames_to_bytes(runtime, runtime->period_size))
		prtd->peroid_elapsed++;

}

static void aml_pcm_timer_rearm(struct aml_pcm_runtime_data *prtd)
{
	prtd->timer.expires = jiffies + prtd->timer_period;
	add_timer(&prtd->timer);
}

static int aml_pcm_timer_start(struct aml_pcm_runtime_data *prtd)
{
	pr_info("%s\n", __func__);
	spin_lock(&prtd->lock);
	aml_pcm_timer_rearm(prtd);
	prtd->running = 1;
	spin_unlock(&prtd->lock);
	return 0;
}

static int aml_pcm_timer_stop(struct aml_pcm_runtime_data *prtd)
{
	pr_info("%s\n", __func__);
	spin_lock(&prtd->lock);
	prtd->running = 0;
	del_timer(&prtd->timer);
	spin_unlock(&prtd->lock);
	return 0;
}

static void aml_pcm_timer_callback(unsigned long data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	unsigned long flags;
	unsigned int elapsed = 0;
	unsigned int datasize = 0;

	spin_lock_irqsave(&prtd->lock, flags);
	aml_pcm_timer_update(prtd);
	aml_pcm_timer_rearm(prtd);
	elapsed = prtd->peroid_elapsed;
	datasize = prtd->data_size;
	if (elapsed) {
		prtd->peroid_elapsed--;
		prtd->data_size -=
		    frames_to_bytes(runtime, runtime->period_size);
	}
	spin_unlock_irqrestore(&prtd->lock, flags);
	if (elapsed) {
		if (elapsed > 1)
			pr_info("PCM timer callback not fast enough!\n");

		snd_pcm_period_elapsed(prtd->substream);
	}
}

static int aml_pcm_timer_create(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;

	pr_info("%s\n", __func__);
	init_timer(&prtd->timer);
	prtd->timer_period = 1;
	prtd->timer.data = (unsigned long)substream;
	prtd->timer.function = aml_pcm_timer_callback;
	prtd->running = 0;
	return 0;
}

static int
aml_pcm_hw_params(struct snd_pcm_substream *substream,
		     struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	size_t size = params_buffer_bytes(params);
	int ret = 0;
	pr_info("enter %s\n", __func__);

	ret = snd_pcm_lib_malloc_pages(substream, size);
	if (ret < 0)
		pr_err("%s malloc_pages return: %d\n", __func__, ret);
	else {
		prtd->buffer_start = runtime->dma_addr;
		prtd->buffer_size = runtime->dma_bytes;

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		} else {
		}
	}

	return ret;
}

static int aml_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_info("enter %s\n", __func__);
	snd_pcm_lib_free_pages(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
	} else {
	}

	return 0;
}

static int aml_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_info("enter %s\n", __func__);

	return 0;
}

static int aml_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	int ret = 0;
	pr_info("enter %s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		aml_pcm_timer_start(prtd);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		aml_pcm_timer_stop(prtd);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t aml_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	snd_pcm_uframes_t frames;

	/* pr_info("enter %s\n", __func__); */
	frames = bytes_to_frames(runtime, (ssize_t) prtd->buffer_offset);

	return frames;
}

static int aml_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd;
	int ret;
	pr_info("enter %s, stream:%d\n", __func__, substream->stream);


	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_set_runtime_hwparams(substream, &aml_pcm_hardware);
	else
		snd_soc_set_runtime_hwparams(substream, &aml_pcm_capture);

	/* Ensure that peroid size is a multiple of 32bytes */
	ret =
	    snd_pcm_hw_constraint_list(runtime, 0,
				       SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				       &hw_constraints_period_sizes);
	if (ret < 0) {
		pr_err("set period bytes constraint error\n");
		goto out;
	}

	/* Ensure that buffer size is a multiple of period size */
	ret =
	    snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_err("set periods constraint error\n");
		goto out;
	}

	prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
	if (prtd == NULL) {
		pr_err("out of memory\n");
		ret = -ENOMEM;
		goto out;
	}

	runtime->private_data = prtd;
	aml_pcm_timer_create(substream);
	prtd->substream = substream;
	spin_lock_init(&prtd->lock);

	return 0;
 out:
	return ret;
}

static int aml_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;

	pr_info("enter %s type: %d\n", __func__, substream->stream);
	if (prtd)
		kfree(runtime->private_data);

	return 0;
}

static int
aml_pcm_copy_playback(struct snd_pcm_runtime *runtime, int channel,
			 snd_pcm_uframes_t pos,
			 void __user *buf, snd_pcm_uframes_t count)
{
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	unsigned char *hwbuf =
	    runtime->dma_area + frames_to_bytes(runtime, pos);
	unsigned int wrptr = 0;
	int ret = 0;

/*	pr_info("enter %s channel: %d pos: %ld count: %ld\n",
		  __func__, channel, pos, count);
*/
	if (copy_from_user(hwbuf, buf, frames_to_bytes(runtime, count))) {
		pr_err("%s copy from user failed!\n", __func__);
		return -EFAULT;
	} else {
		wrptr = prtd->buffer_start + frames_to_bytes(runtime, pos)
			+ frames_to_bytes(runtime, count);
		if (wrptr >= (prtd->buffer_start + prtd->buffer_size))
			wrptr = prtd->buffer_start + prtd->buffer_size;

		pcm_out_set_wr_ptr(wrptr);
	}
	return ret;
}

static int
aml_pcm_copy_capture(struct snd_pcm_runtime *runtime, int channel,
			snd_pcm_uframes_t pos,
			void __user *buf, snd_pcm_uframes_t count)
{
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	signed short *hwbuf =
	    (signed short *)(runtime->dma_area + frames_to_bytes(runtime, pos));
	unsigned int rdptr = 0;
	int ret = 0;

/*	pr_info("enter %s channel: %d pos: %ld count: %ld\n",
		  __func__, channel, pos, count);
*/
	if (copy_to_user(buf, hwbuf, frames_to_bytes(runtime, count))) {
		pr_err("%s copy to user failed!\n", __func__);
		return -EFAULT;
	} else {
		/* memset(hwbuf, 0xff, frames_to_bytes(runtime, count)); */
		rdptr =
		    prtd->buffer_start + frames_to_bytes(runtime,
							 pos) +
		    frames_to_bytes(runtime, count);
		if (rdptr >= (prtd->buffer_start + prtd->buffer_size))
			rdptr = prtd->buffer_start + prtd->buffer_size;

		pcm_in_set_rd_ptr(rdptr);
	}
	return ret;
}

static int aml_pcm_copy(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos,
	void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret =
		    aml_pcm_copy_playback(runtime, channel, pos, buf, count);
	} else {
		ret =
		    aml_pcm_copy_capture(runtime, channel, pos, buf, count);
	}

	return ret;
}

static int aml_pcm_silence(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned char *ppos = NULL;
	ssize_t n;

	pr_info("enter %s\n", __func__);
	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);

	return 0;
}

static struct snd_pcm_ops aml_pcm_ops = {
	.open = aml_pcm_open,
	.close = aml_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_pcm_hw_params,
	.hw_free = aml_pcm_hw_free,
	.prepare = aml_pcm_prepare,
	.trigger = aml_pcm_trigger,
	.pointer = aml_pcm_pointer,
	.copy = aml_pcm_copy,
	.silence = aml_pcm_silence,
};

/*--------------------------------------------------------------------------*\
 * ASoC platform driver
\*--------------------------------------------------------------------------*/

static u64 aml_pcm_dmamask = DMA_BIT_MASK(32);

static int aml_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = aml_pcm_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
				       &buf->addr, GFP_KERNEL);
	if (!buf->area) {
		dev_err(pcm->dev, "aml_pcm alloc failed!\n");
		return -ENOMEM;
	}

	buf->bytes = size;

	return 0;
}

static int aml_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_card *card = rtd->card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_soc_dai *dai;
	dai = rtd->cpu_dai;
	pr_info("enter %s dai->name: %s dai->id: %d\n", __func__,
		  dai->name, dai->id);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &aml_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = aml_pcm_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = aml_pcm_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

 out:
	return ret;
}

static void aml_pcm_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	pr_info("enter %s\n", __func__);
	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_coherent(pcm->card->dev, buf->bytes, buf->area,
				  buf->addr);
		buf->area = NULL;
	}
}

#ifdef CONFIG_PM
static int aml_pcm_suspend(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	if (!runtime)
		return 0;

	/* TODO */
	return 0;
}

static int aml_pcm_resume(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	if (!runtime)
		return 0;

	/* TODO */
	return 0;
}
#else
#define aml_pcm_suspend	NULL
#define aml_pcm_resume	NULL
#endif

struct snd_soc_platform_driver aml_soc_platform_pcm = {
	.ops = &aml_pcm_ops,
	.pcm_new = aml_pcm_new,
	.pcm_free = aml_pcm_free,
	.suspend  = aml_pcm_suspend,
	.resume   = aml_pcm_resume,
};
EXPORT_SYMBOL_GPL(aml_soc_platform_pcm);

static int aml_soc_platform_pcm_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &aml_soc_platform_pcm);
}

static int aml_soc_platform_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_audio_dt_match[] = {
	{
		.compatible = "amlogic, aml-pcm",
	},
	{},
};
#else
#define amlogic_audio_dt_match NULL
#endif

static struct platform_driver aml_platform_pcm_driver = {
	.driver = {
		   .name = "aml-pcm",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_audio_dt_match,
		   },

	.probe = aml_soc_platform_pcm_probe,
	.remove = aml_soc_platform_pcm_remove,
};

static int __init aml_alsa_pcm_init(void)
{
	return platform_driver_register(&aml_platform_pcm_driver);
}

static void __exit aml_alsa_pcm_exit(void)
{
	platform_driver_unregister(&aml_platform_pcm_driver);
}

module_init(aml_alsa_pcm_init);
module_exit(aml_alsa_pcm_exit);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AML audio driver for ALSA");
