/*
 * Intel Blind codec machine driver
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

// TODO: make the platform configurable. hard coded to HSW/BDW atm....

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include "../common/sst-dsp.h"
#include "../haswell/sst-haswell-ipc.h"

static int reef_blind_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
#if 0
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	snd_soc_dai_set_bclk_ratio(codec_dai, 32);

#endif
	return 0;
}


static int reef_blind_codec_fixup(struct snd_soc_pcm_runtime *rtd,
                           struct snd_pcm_hw_params *params)
{
       struct snd_interval *rate = hw_param_interval(params,
                       SNDRV_PCM_HW_PARAM_RATE);
       struct snd_interval *channels = hw_param_interval(params,
                                               SNDRV_PCM_HW_PARAM_CHANNELS);

       /* The DSP will covert the FE rate to 48k, stereo, 16bits */
       rate->min = rate->max = 48000;
       channels->min = channels->max = 2;

       /* set SSP2 to 16-bit */
       params_set_format(params, SNDRV_PCM_FORMAT_S16_LE);
       return 0;
}

static int reef_blind_init(struct snd_soc_pcm_runtime *runtime)
{
	snd_soc_set_dmi_name(runtime->card, "reef");
	return 0;
}

static struct snd_soc_ops reef_blind_ops = {
	.hw_params = reef_blind_hw_params,
};

static struct snd_soc_dai_link reef_blind_dais[] = {
	{
		.name = "PCM0",
		.stream_name = "System Playback/Capture",
		.cpu_dai_name = "PCM0 Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = reef_blind_init,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "PCM1",
		.stream_name = "PCM1 Playback",
		.cpu_dai_name = "PCM1 Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "Codec",
		.id = 0,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &reef_blind_ops,
		.dai_fmt = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.be_hw_params_fixup = reef_blind_codec_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};


static struct snd_soc_card reef_blind_card = {
	.name = "reef-blind",
	.dai_link = reef_blind_dais,
	.num_links = ARRAY_SIZE(reef_blind_dais),
	.fully_routed = true,
};

static int reef_blind_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &reef_blind_card;

	card->dev = &pdev->dev;
	return devm_snd_soc_register_card(&pdev->dev, card);
}

static struct platform_driver reef_blind_audio = {
	.probe = reef_blind_probe,
	.driver = {
		.name = "reef-blind",
		.pm = &snd_soc_pm_ops,
	},
};
module_platform_driver(reef_blind_audio)

MODULE_DESCRIPTION("ASoC Reef Blind");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:reef-blind");
