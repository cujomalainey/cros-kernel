/*
 * Intel Baytrail SST RT5651 machine driver
 * Copyright (c) 2014, Intel Corporation.
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
#include "../../codecs/rt5651.h"

#include "../common/sst-dsp.h"
#include "../haswell/sst-haswell-ipc.h"

static const struct snd_soc_dapm_widget byt_rt5651_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

static const struct snd_soc_dapm_route byt_rt5651_audio_map[] = {
	{"micbias1", NULL, "Headset Mic"},
	{"IN3P", NULL, "micbias1"},
	{"Headphone", NULL, "HPOL"},
	{"Headphone", NULL, "HPOR"},
	{"Speaker", NULL, "LOUTL"},
	{"Speaker", NULL, "LOUTR"},

	/* CODEC BE connections */
	{"SSP CODEC IN", NULL, "AIF1 Capture"},
	{"AIF1 Playback", NULL, "SSP CODEC OUT"},
};

static const struct snd_soc_dapm_route byt_rt5651_intmic_dmic1_map[] = {
	{"DMIC1", NULL, "Internal Mic"},
};

static const struct snd_soc_dapm_route byt_rt5651_intmic_dmic2_map[] = {
	{"DMIC2", NULL, "Internal Mic"},
};

static const struct snd_soc_dapm_route byt_rt5651_intmic_in1_map[] = {
	{"Internal Mic", NULL, "micbias1"},
	{"IN1P", NULL, "Internal Mic"},
};

enum {
	BYT_RT5651_DMIC1_MAP,
	BYT_RT5651_DMIC2_MAP,
	BYT_RT5651_IN1_MAP,
};

#define BYT_RT5651_MAP(quirk)	((quirk) & 0xff)
#define BYT_RT5651_DMIC_EN	BIT(16)

static unsigned long byt_rt5651_quirk = BYT_RT5651_DMIC1_MAP |
					BYT_RT5651_DMIC_EN;

static const struct snd_kcontrol_new byt_rt5651_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
};

static int byt_rt5651_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	snd_soc_dai_set_bclk_ratio(codec_dai, 50);

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5651_SCLK_S_PLL1,
				     params_rate(params) * 256 * 2,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "can't set codec clock %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5651_PLL1_S_BCLK1,
				  params_rate(params) * 50,
				  params_rate(params) * 256 * 2);
	if (ret < 0) {
		dev_err(codec_dai->dev, "can't set codec pll: %d\n", ret);
		return ret;
	}

	return 0;
}

static int byt_rt5651_quirk_cb(const struct dmi_system_id *id)
{
	byt_rt5651_quirk = (unsigned long)id->driver_data;
	return 1;
}

static const struct dmi_system_id byt_rt5651_quirk_table[] = {
	{
		.callback = byt_rt5651_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "T100TA"),
		},
		.driver_data = (unsigned long *)BYT_RT5651_IN1_MAP,
	},
	{
		.callback = byt_rt5651_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "DellInc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Venue 8 Pro 5830"),
		},
		.driver_data = (unsigned long *)(BYT_RT5651_DMIC2_MAP |
						 BYT_RT5651_DMIC_EN),
	},
	{}
};

static int byt_codec_fixup(struct snd_soc_pcm_runtime *rtd,
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

static int byt_rt5651_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_card *card = runtime->card;
	const struct snd_soc_dapm_route *custom_map;
	int num_routes;
	struct sst_pdata *pdata = dev_get_platdata(runtime->platform->dev);
	struct sst_hsw *byt = pdata->dsp;

	snd_soc_set_dmi_name(card, NULL);

	card->dapm.idle_bias_off = true;

	ret = snd_soc_add_card_controls(card, byt_rt5651_controls,
					ARRAY_SIZE(byt_rt5651_controls));
	if (ret) {
		dev_err(card->dev, "unable to add card controls\n");
		return ret;
	}

#if 0
	dmi_check_system(byt_rt5651_quirk_table);
	switch (BYT_RT5651_MAP(byt_rt5651_quirk)) {
	case BYT_RT5651_IN1_MAP:
		custom_map = byt_rt5651_intmic_in1_map;
		num_routes = ARRAY_SIZE(byt_rt5651_intmic_in1_map);
		break;
	case BYT_RT5651_DMIC2_MAP:
		custom_map = byt_rt5651_intmic_dmic2_map;
		num_routes = ARRAY_SIZE(byt_rt5651_intmic_dmic2_map);
		break;
	default:
		custom_map = byt_rt5651_intmic_dmic1_map;
		num_routes = ARRAY_SIZE(byt_rt5651_intmic_dmic1_map);
	}

	ret = snd_soc_dapm_add_routes(&card->dapm, custom_map, num_routes);
	if (ret)
		return ret;

	//if (byt_rt5651_quirk & BYT_RT5651_DMIC_EN) {
	//	ret = rt5651_dmic_enable(codec, 0, 0);
	//	if (ret)
	//return ret;
	//}
#endif
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headphone");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Speaker");

#if 1
	/* Set ADSP SSP port settings */
	ret = sst_hsw_device_set_config(byt, SST_HSW_DEVICE_SSP_2,
		15360000,
		SST_HSW_DEVICE_CLOCK_MASTER, 9);
	if (ret < 0) {
		dev_err(runtime->dev, "error: failed to set device config\n");
		return ret;
	}
#endif
	return ret;
}

static struct snd_soc_ops byt_rt5651_ops = {
	.hw_params = byt_rt5651_hw_params,
};


#if 1
static struct snd_soc_dai_link byt_rt5651_dais[] = {
	{
		.name = "PCM0",
		.stream_name = "System Playback/Capture",
		.cpu_dai_name = "PCM0 Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = byt_rt5651_init,
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
		.codec_dai_name = "rt5651-aif1",
		.codec_name = "i2c-10EC5651:00",
		.ops = &byt_rt5651_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.be_hw_params_fixup = byt_codec_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

#else
static struct snd_soc_dai_link byt_rt5651_dais[] = {
	{
		.name = "System PCM",
		.stream_name = "System Playback/Capture",
		.cpu_dai_name = "System Pin",
		.platform_name = "haswell-pcm-audio",
//		.name = "Baytrail Audio",
//		.stream_name = "Audio",
//		.cpu_dai_name = "baytrail-pcm-audio",
		.codec_dai_name = "rt5651-aif1",
		.codec_name = "i2c-10EC5651:00",
//		.platform_name = "baytrail-pcm-audio",
		.dai_fmt = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.be_hw_params_fixup = byt_codec_fixup,
		.init = byt_rt5651_init,
		.ops = &byt_rt5651_ops,
	},
};
#endif

static struct snd_soc_card byt_rt5651_card = {
	.name = "byt-rt5651",
	.dai_link = byt_rt5651_dais,
	.num_links = ARRAY_SIZE(byt_rt5651_dais),
	.dapm_widgets = byt_rt5651_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_rt5651_widgets),
	.dapm_routes = byt_rt5651_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_rt5651_audio_map),
	.fully_routed = true,
};

static int byt_rt5651_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &byt_rt5651_card;

	card->dev = &pdev->dev;
	return devm_snd_soc_register_card(&pdev->dev, card);
}

static struct platform_driver byt_rt5651_audio = {
	.probe = byt_rt5651_probe,
	.driver = {
		.name = "byt-rt5651",
		.pm = &snd_soc_pm_ops,
	},
};
module_platform_driver(byt_rt5651_audio)

MODULE_DESCRIPTION("ASoC Intel(R) Baytrail Machine driver");
MODULE_AUTHOR("Omair Md Abdullah, Jarkko Nikula");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:byt-rt5651");
