/*
 * sound/soc/aml/m8/aml_i2s_dai.c
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
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/sound/aout_notify.h>

#include "aml_i2s_dai.h"
#include "aml_pcm.h"
#include "aml_i2s.h"
#include "aml_audio_hw.h"
#include "aml_spdif_dai.h"

struct aml_dai_info dai_info[3] = { {0} };

static int i2s_pos_sync;

/* extern int set_i2s_iec958_samesource(int enable); */

/*
the I2S hw  and IEC958 PCM output initation,958 initation here,
for the case that only use our ALSA driver for PCM s/pdif output.
*/
static void aml_hw_i2s_init(struct snd_pcm_runtime *runtime)
{
	unsigned i2s_mode = AIU_I2S_MODE_PCM16;
	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		i2s_mode = AIU_I2S_MODE_PCM32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		i2s_mode = AIU_I2S_MODE_PCM24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		i2s_mode = AIU_I2S_MODE_PCM16;
		break;
	}
#ifdef CONFIG_SND_AML_SPLIT_MODE
	audio_set_i2s_mode(i2s_mode, runtime->channels);
#else
	audio_set_i2s_mode(i2s_mode);
#endif
	audio_set_aiubuf(runtime->dma_addr, runtime->dma_bytes,
			 runtime->channels);
}

static int aml_dai_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd =
	    (struct aml_runtime_data *)runtime->private_data;
	struct audio_stream *s;

	if (prtd == NULL) {
		prtd =
		    (struct aml_runtime_data *)
		    kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			dev_err(substream->pcm->card->dev, "alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}
	s = &prtd->s;
	if (substream->stream
			== SNDRV_PCM_STREAM_PLAYBACK) {
		s->device_type = AML_AUDIO_I2SOUT;
	} else {
		s->device_type = AML_AUDIO_I2SIN;
	}
	return 0;
 out:
	return ret;
}

static void aml_dai_i2s_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	if (IEC958_mode_codec == 0)
		aml_spdif_play(0);
	return;
}

#define AOUT_EVENT_IEC_60958_PCM 0x1
static int aml_i2s_set_amclk(struct aml_i2s *i2s, unsigned long rate)
{
	int ret = 0;

	ret = clk_set_rate(i2s->clk_mpll, rate * 10);
	if (ret)
		return ret;

	ret = clk_set_parent(i2s->clk_mclk, i2s->clk_mpll);
	if (ret)
		return ret;

	ret = clk_set_rate(i2s->clk_mclk, rate);
	if (ret)
		return ret;

	audio_set_i2s_clk_div();
	set_hdmi_tx_clk_source(2);

	return 0;
}

static int aml_dai_i2s_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
	struct aml_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	audio_util_set_i2s_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		s->i2s_mode = dai_info[dai->id].i2s_mode;
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
			audio_in_i2s_set_buf(runtime->dma_addr,
					runtime->dma_bytes * 2,
					0, i2s_pos_sync, i2s->audin_fifo_src,
					runtime->channels);
			memset((void *)runtime->dma_area, 0,
					runtime->dma_bytes * 2);
		} else {
			audio_in_i2s_set_buf(runtime->dma_addr,
					runtime->dma_bytes,
					0, i2s_pos_sync, i2s->audin_fifo_src,
					runtime->channels);
			memset((void *)runtime->dma_area, 0,
					runtime->dma_bytes);
		}
		s->device_type = AML_AUDIO_I2SIN;
	} else {
		s->device_type = AML_AUDIO_I2SOUT;
		audio_out_i2s_enable(0);
		aml_hw_i2s_init(runtime);
		/* i2s/958 share the same audio hw buffer when PCM mode */
		if (IEC958_mode_codec == 0) {
			aml_hw_iec958_init(substream, 1);
			/* use the hw same sync for i2s/958 */
			dev_info(substream->pcm->card->dev, "i2s/958 same source\n");
		}
		if (runtime->channels == 8) {
			dev_info(substream->pcm->card->dev,
				"8ch PCM output->notify HDMI\n");
			aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM,
				substream);
		}
	}
	return 0;
}

static int aml_dai_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* TODO */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "I2S playback enable\n");
			audio_out_i2s_enable(1);
			if (IEC958_mode_codec == 0) {
				dev_info(substream->pcm->card->dev, "IEC958 playback enable\n");
				audio_hw_958_enable(1);
			}
		} else {
			audio_in_i2s_enable(1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "I2S playback disable\n");
			audio_out_i2s_enable(0);
			if (IEC958_mode_codec == 0) {
				dev_info(substream->pcm->card->dev, "IEC958 playback disable\n");
				audio_hw_958_enable(0);
			}
		} else {
			audio_in_i2s_enable(0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_dai_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct aml_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	int srate, mclk_rate;

	srate = params_rate(params);
	if (i2s->old_samplerate != srate) {
		i2s->old_samplerate = srate;
		mclk_rate = srate * DEFAULT_MCLK_RATIO_SR;
		aml_i2s_set_amclk(i2s, mclk_rate);
	}

	return 0;
}

static int aml_dai_set_i2s_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	if (fmt & SND_SOC_DAIFMT_CBS_CFS)	/* slave mode */
		dai_info[dai->id].i2s_mode = I2S_SLAVE_MODE;

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		i2s_pos_sync = 0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		i2s_pos_sync = 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int aml_dai_set_i2s_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_dai_i2s_suspend(struct snd_soc_dai *dai)
{
	struct aml_i2s *i2s = dev_get_drvdata(dai->dev);

	if (i2s && i2s->clk_mclk && !i2s->disable_clk_suspend)
		clk_disable_unprepare(i2s->clk_mclk);

	return 0;
}

static int aml_dai_i2s_resume(struct snd_soc_dai *dai)
{
	struct aml_i2s *i2s = dev_get_drvdata(dai->dev);

	if (i2s && i2s->clk_mclk && !i2s->disable_clk_suspend)
		clk_prepare_enable(i2s->clk_mclk);

	return 0;
}

#define AML_DAI_I2S_RATES		(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_I2S_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops aml_dai_i2s_ops = {
	.startup = aml_dai_i2s_startup,
	.shutdown = aml_dai_i2s_shutdown,
	.prepare = aml_dai_i2s_prepare,
	.trigger = aml_dai_i2s_trigger,
	.hw_params = aml_dai_i2s_hw_params,
	.set_fmt = aml_dai_set_i2s_fmt,
	.set_sysclk = aml_dai_set_i2s_sysclk,
};

struct snd_soc_dai_driver aml_i2s_dai[] = {
	{
	 .id = 0,
	 .suspend = aml_dai_i2s_suspend,
	 .resume = aml_dai_i2s_resume,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = AML_DAI_I2S_RATES,
		      .formats = AML_DAI_I2S_FORMATS,},
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = AML_DAI_I2S_RATES,
		     .formats = AML_DAI_I2S_FORMATS,},
	 .ops = &aml_dai_i2s_ops,
	 },
};
EXPORT_SYMBOL_GPL(aml_i2s_dai);

static const struct snd_soc_component_driver aml_component = {
	.name = "aml-i2s-dai",
};

static const char *const gate_names[] = {
	"top_glue", "aud_buf", "i2s_out", "amclk_measure",
	"aififo2", "aud_mixer", "mixer_reg", "adc",
	"top_level", "aoclk", "aud_in"
};

static int aml_i2s_dai_probe(struct platform_device *pdev)
{
	struct aml_i2s *i2s = NULL;
	struct reset_control *audio_reset;
	struct device_node *pnode = pdev->dev.of_node;
	int ret = 0, i;

	/* enable AIU module power gate first */
	for (i = 0; i < ARRAY_SIZE(gate_names); i++) {
		audio_reset = devm_reset_control_get(&pdev->dev, gate_names[i]);
		if (IS_ERR(audio_reset)) {
			dev_err(&pdev->dev, "Can't get aml audio gate:%s\n",
				gate_names[i]);

			if (1 == i && is_meson_txlx_cpu()) {
				pr_info("ignore aud_buf gate for txlx\n");
				continue;
			}

			return PTR_ERR(audio_reset);
		}
		reset_control_deassert(audio_reset);
	}

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct aml_i2s), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "Can't allocate aml_i2s\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, i2s);

	i2s->disable_clk_suspend =
		of_property_read_bool(pnode, "disable_clk_suspend");

	i2s->clk_mpll = devm_clk_get(&pdev->dev, "mpll2");
	if (IS_ERR(i2s->clk_mpll)) {
		dev_err(&pdev->dev, "Can't retrieve mpll2 clock\n");
		ret = PTR_ERR(i2s->clk_mpll);
		goto err;
	}

	i2s->clk_mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(i2s->clk_mclk)) {
		dev_err(&pdev->dev, "Can't retrieve clk_mclk clock\n");
		ret = PTR_ERR(i2s->clk_mclk);
		goto err;
	}

	/* now only 256fs is supported */
	ret = aml_i2s_set_amclk(i2s,
			DEFAULT_SAMPLERATE * DEFAULT_MCLK_RATIO_SR);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't set aml_i2s :%d\n", ret);
		goto err;
	}
	audio_util_set_i2s_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);

	ret = clk_prepare_enable(i2s->clk_mclk);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable I2S mclk clock: %d\n", ret);
		goto err;
	}

	if (of_property_read_bool(pdev->dev.of_node, "DMIC")) {
		i2s->audin_fifo_src = 3;
		dev_info(&pdev->dev, "DMIC is in platform!\n");
	} else {
		i2s->audin_fifo_src = 1;
		dev_info(&pdev->dev, "I2S Mic is in platform!\n");
	}

	ret = snd_soc_register_component(&pdev->dev, &aml_component,
					  aml_i2s_dai, ARRAY_SIZE(aml_i2s_dai));
	if (ret) {
		dev_err(&pdev->dev, "Can't register i2s dai: %d\n", ret);
		goto err_clk_dis;
	}
	return 0;

err_clk_dis:
	clk_disable_unprepare(i2s->clk_mclk);
err:
	return ret;
}

static int aml_i2s_dai_remove(struct platform_device *pdev)
{
	struct aml_i2s *i2s = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	clk_disable_unprepare(i2s->clk_mclk);

	return 0;
}

static void aml_i2s_dai_shutdown(struct platform_device *pdev)
{
	struct aml_i2s *i2s = dev_get_drvdata(&pdev->dev);

	if (i2s && i2s->clk_mclk)
		clk_disable_unprepare(i2s->clk_mclk);

	return;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_dai_dt_match[] = {
	{.compatible = "amlogic, aml-i2s-dai",},
	{},
};
#else
#define amlogic_dai_dt_match NULL
#endif

static struct platform_driver aml_i2s_dai_driver = {
	.driver = {
		   .name = "aml-i2s-dai",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_dai_dt_match,
		   },

	.probe = aml_i2s_dai_probe,
	.remove = aml_i2s_dai_remove,
	.shutdown = aml_i2s_dai_shutdown,
};

static int __init aml_i2s_dai_modinit(void)
{
	return platform_driver_register(&aml_i2s_dai_driver);
}

module_init(aml_i2s_dai_modinit);

static void __exit aml_i2s_dai_modexit(void)
{
	platform_driver_unregister(&aml_i2s_dai_driver);
}

module_exit(aml_i2s_dai_modexit);

/* Module information */
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML DAI driver for ALSA");
MODULE_LICENSE("GPL");
