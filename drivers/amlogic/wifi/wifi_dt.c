/*
 * drivers/amlogic/wifi/wifi_dt.c
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

#include <linux/amlogic/wifi_dt.h>
#ifdef CONFIG_BCMDHD_USE_STATIC_BUF
#include <linux/amlogic/dhd_buf.h>
#endif

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm_meson.h>

#define OWNER_NAME "sdio_wifi"

int wifi_power_gpio = 0;
int wifi_power_gpio2 = 0;


/*
*there are two pwm channel outputs using one gpio
*for gxtvbb and the follows soc
*/
struct pwm_config_gxtvbb {
	unsigned int pwm_channel1;
	unsigned int pwm_channel2;
	unsigned int pwm_config1[3];
	unsigned int pwm_config2[3];
	/*pwm_config is used to storage peroid , duty and times*/
};

struct pwm_config_gxbb {
	unsigned int pwm_channel;
	unsigned int pwm_config[2];
	/*pwm_config is used to storage peroid and duty*/
};

struct wifi_plat_info {
	int interrupt_pin;
	int irq_num;
	int irq_trigger_type;

	int power_on_pin;
	int power_on_pin_level;
	int power_on_pin_OD;
	int power_on_pin2;

	int clock_32k_pin;
	struct gpio_desc *interrupt_desc;
	struct gpio_desc *powe_desc;

	int plat_info_valid;
	struct pinctrl *p;
	struct device		*dev;
	struct pwm_config_gxtvbb gxtv_conf;
	struct pwm_config_gxbb gxb_conf;
};

#define WIFI_POWER_MODULE_NAME	"wifi_power"
#define WIFI_POWER_DRIVER_NAME	"wifi_power"
#define WIFI_POWER_DEVICE_NAME	"wifi_power"
#define WIFI_POWER_CLASS_NAME		"wifi_power"

#define USB_POWER_UP    _IO('m', 1)
#define USB_POWER_DOWN  _IO('m', 2)
#define SDIO_POWER_UP    _IO('m', 3)
#define SDIO_POWER_DOWN  _IO('m', 4)
#define SDIO_GET_DEV_TYPE  _IO('m', 5)
static struct wifi_plat_info wifi_info;
static dev_t wifi_power_devno;
static struct cdev *wifi_power_cdev;
static struct device *devp;
struct wifi_power_platform_data *pdata;

static int usb_power;
#define BT_BIT	0
#define WIFI_BIT	1
static DEFINE_MUTEX(wifi_bt_mutex);

#define WIFI_INFO(fmt, args...) \
		dev_info(wifi_info.dev, "[%s] " fmt, __func__, ##args);

#ifdef CONFIG_OF
static const struct of_device_id wifi_match[] = {
	{	.compatible = "amlogic, aml_wifi",
		.data		= (void *)&wifi_info
	},
	{},
};

static struct wifi_plat_info *wifi_get_driver_data
	(struct platform_device *pdev)
{
	const struct of_device_id *match;
	match = of_match_node(wifi_match, pdev->dev.of_node);
	return (struct wifi_plat_info *)match->data;
}
#else
#define wifi_match NULL
#endif

#define CHECK_PROP(ret, msg, value)	\
{	\
	if (ret) { \
		WIFI_INFO("wifi_dt : no prop for %s\n", msg);	\
		return -1;	\
	} \
}	\

/*
#define CHECK_RET(ret) \
	if (ret) \
		WIFI_INFO("wifi_dt : gpio op failed(%d)
			at line %d\n", ret, __LINE__)
*/

/* extern const char *amlogic_cat_gpio_owner(unsigned int pin); */

#define SHOW_PIN_OWN(pin_str, pin_num)	\
	WIFI_INFO("%s(%d)\n", pin_str, pin_num)

static int set_power(int value)
{
	if (!wifi_info.power_on_pin_OD) {
		if (wifi_info.power_on_pin_level)
			return gpio_direction_output(wifi_info.power_on_pin,
					!value);
		else
			return gpio_direction_output(wifi_info.power_on_pin,
					value);
	} else {
		if (wifi_info.power_on_pin_level) {
			if (value)
				gpio_direction_input(wifi_info.power_on_pin);
			else
				gpio_direction_output(wifi_info.power_on_pin,
					0);
		} else {
			if (value)
				gpio_direction_output(wifi_info.power_on_pin,
					0);
			else
				gpio_direction_input(wifi_info.power_on_pin);
		}
	}
	return 0;
}

static int set_power2(int value)
{
	if (wifi_info.power_on_pin_level)
		return gpio_direction_output(wifi_info.power_on_pin2,
				!value);
	else
		return gpio_direction_output(wifi_info.power_on_pin2,
				value);
}

static int set_wifi_power(int is_power)
{
	int ret = 0;

	if (is_power) {
		if (wifi_info.power_on_pin) {
			ret = set_power(1);
			if (ret)
				WIFI_INFO("power up failed(%d)\n", ret);
		}
		if (wifi_info.power_on_pin2) {
			ret = set_power2(1);
			if (ret)
				WIFI_INFO("power2 up failed(%d)\n", ret);
		}
	} else {
		if (wifi_info.power_on_pin) {
			ret = set_power(0);
			if (ret)
				WIFI_INFO("power down failed(%d)\n", ret);
		}
		if (wifi_info.power_on_pin2) {
			ret = set_power2(0);
			if (ret)
				WIFI_INFO("power2 down failed(%d)\n", ret);
		}
	}
	return ret;
}

static void usb_power_control(int is_power, int shift)
{
	mutex_lock(&wifi_bt_mutex);
	if (is_power) {
		if (!usb_power) {
			set_wifi_power(is_power);
			WIFI_INFO("Set %s power on !\n", (shift ? "WiFi":"BT"));
			sdio_reinit();
		}
		usb_power |= (1 << shift);
	} else {
		usb_power &= ~(1 << shift);
		if (!usb_power) {
			set_wifi_power(is_power);
			WIFI_INFO("Set %s power down\n", (shift ? "WiFi":"BT"));
		}
	}
	mutex_unlock(&wifi_bt_mutex);
}

void set_usb_bt_power(int is_power)
{
	usb_power_control(is_power, BT_BIT);
}
EXPORT_SYMBOL(set_usb_bt_power);

void set_usb_wifi_power(int is_power)
{
	usb_power_control(is_power, WIFI_BIT);
}

static int  wifi_power_open(struct inode *inode, struct file *file)
{
	struct cdev *cdevp = inode->i_cdev;
	file->private_data = cdevp;
	return 0;
}

static int  wifi_power_release(struct inode *inode, struct file *file)
{
	return 0;
}
static long wifi_power_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	char dev_type[10] = {'\0'};

	switch (cmd) {
	case USB_POWER_UP:
		set_usb_wifi_power(0);
		mdelay(200);
		set_usb_wifi_power(1);
		WIFI_INFO(KERN_INFO "ioctl Set usb_sdio wifi power up!\n");
		break;
	case USB_POWER_DOWN:
		set_usb_wifi_power(0);
		WIFI_INFO(KERN_INFO "ioctl Set usb_sdio wifi power down!\n");
		break;
	case SDIO_POWER_UP:
		set_usb_wifi_power(0);
		mdelay(200);
		set_usb_wifi_power(1);
		mdelay(200);
		WIFI_INFO("ioctl Set sdio wifi power up!\n");
		break;
	case SDIO_POWER_DOWN:
		set_usb_wifi_power(0);
		WIFI_INFO("ioctl Set sdio wifi power down!\n");
		break;
	case SDIO_GET_DEV_TYPE:
		memcpy(dev_type, get_wifi_inf(), strlen(get_wifi_inf()));
		WIFI_INFO("wifi interface dev type: %s, length = %d\n",
				dev_type, (int)strlen(dev_type));
		if (copy_to_user((char __user *)arg,
				dev_type, strlen(dev_type)))
			return -ENOTTY;
		break;
	default:
		WIFI_INFO("usb wifi_power_ioctl: default !!!\n");
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations wifi_power_fops = {
	.unlocked_ioctl = wifi_power_ioctl,
	.compat_ioctl = wifi_power_ioctl,
	.open	= wifi_power_open,
	.release	= wifi_power_release,
};

static struct class wifi_power_class = {
	.name = WIFI_POWER_CLASS_NAME,
	.owner = THIS_MODULE,
};

static int wifi_setup_dt(void)
{
	int ret;
	uint flag;

	WIFI_INFO("wifi_setup_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_setup_dt : invalid device tree setting\n");
		return -1;
	}

/*
#if ((!(defined CONFIG_ARCH_MESON8))
	&& (!(defined CONFIG_ARCH_MESON8B)))
	//setup sdio pullup
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG4,
		0xf|1<<8|1<<9|1<<11|1<<12);
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG2,1<<7|1<<8|1<<9);
#endif
*/

	/* setup irq */
	if (wifi_info.interrupt_pin) {
		ret = gpio_request(wifi_info.interrupt_pin,
			OWNER_NAME);
		if (ret)
			WIFI_INFO("interrupt_pin request failed(%d)\n", ret);
		ret = gpio_set_pullup(wifi_info.interrupt_pin, 1);
		if (ret)
			WIFI_INFO("interrupt_pin disable pullup failed(%d)\n",
				ret)
		ret = gpio_direction_input(wifi_info.interrupt_pin);
		if (ret)
			WIFI_INFO("set interrupt_pin input failed(%d)\n", ret);
		if (wifi_info.irq_num) {
			flag = AML_GPIO_IRQ(wifi_info.irq_num,
				FILTER_NUM4, wifi_info.irq_trigger_type);
		} else {
			WIFI_INFO("wifi_dt : unsupported irq number - %d\n",
				wifi_info.irq_num);
			return -1;
		}
		ret = gpio_for_irq(wifi_info.interrupt_pin, flag);
		if (ret)
			WIFI_INFO("gpio to irq failed(%d)\n", ret)
		SHOW_PIN_OWN("interrupt_pin", wifi_info.interrupt_pin);
	}

	/* setup power */
	if (wifi_info.power_on_pin) {
		ret = gpio_request(wifi_info.power_on_pin, OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin request failed(%d)\n", ret);
		if (wifi_info.power_on_pin_level)
			ret = set_power(1);
		else
			ret = set_power(0);
		if (ret)
			WIFI_INFO("power_on_pin output failed(%d)\n", ret);
		SHOW_PIN_OWN("power_on_pin", wifi_info.power_on_pin);
	}

	if (wifi_info.power_on_pin2) {
		ret = gpio_request(wifi_info.power_on_pin2,
			OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin2 request failed(%d)\n", ret);
		if (wifi_info.power_on_pin_level)
			ret = set_power2(1);
		else
			ret = set_power2(0);
		if (ret)
			WIFI_INFO("power_on_pin2 output failed(%d)\n", ret);
		SHOW_PIN_OWN("power_on_pin2", wifi_info.power_on_pin2);
	}

	return 0;
}

static void wifi_teardown_dt(void)
{

	WIFI_INFO("wifi_teardown_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_teardown_dt : invalid device tree setting\n");
		return;
	}

	if (wifi_info.power_on_pin)
		gpio_free(wifi_info.power_on_pin);

	if (wifi_info.power_on_pin2)
		gpio_free(wifi_info.power_on_pin2);

	if (wifi_info.interrupt_pin)
		gpio_free(wifi_info.interrupt_pin);

}


/*
* fot gxb soc
*/
int pwm_single_channel_conf_dt(struct wifi_plat_info *plat)
{
	phandle pwm_phandle;
	int val;
	int ret;
	int count = 2;
	struct device_node *np_wifi_pwm_conf = plat->dev->of_node;

	ret = of_property_read_u32(np_wifi_pwm_conf, "pwm_config", &val);
	if (ret) {
		pr_err("not match wifi_pwm_config node\n");
		return -1;
	} else {
		pwm_phandle = val;
		np_wifi_pwm_conf = of_find_node_by_phandle(pwm_phandle);
		if (!np_wifi_pwm_conf) {
			pr_err("can't find wifi_pwm_config node\n");
			return -1;
		}
	}

	ret = of_property_read_u32(np_wifi_pwm_conf, "pwm_channel",
		  &(plat->gxb_conf.pwm_channel));
	if (ret) {
		pr_err("not config pwm channel num\n");
		return -1;
	}

	ret = of_property_read_u32_array(np_wifi_pwm_conf, "pwm_channel_conf",
		(plat->gxb_conf.pwm_config), count);
	if (ret) {
		pr_err("not config pwm channel parameters\n");
		return -1;
	}

	WIFI_INFO("pwm phandle val=%x,pwm-channel=%d\n",
	val, plat->gxb_conf.pwm_channel);
	WIFI_INFO("pwm_config[0] = %d,pwm_config[1] = %d\n",
	plat->gxb_conf.pwm_config[0], plat->gxb_conf.pwm_config[1]);
	WIFI_INFO("wifi pwm dt ok\n");

	return 0;
}

/*
*configuration for single pwm
*/
int pwm_single_channel_conf(struct wifi_plat_info *plat)
{
	struct pwm_device *pwm_ch = NULL;
	struct aml_pwm_chip *aml_chip = NULL;
	struct pwm_config_gxbb pg = plat->gxb_conf;
	unsigned int pwm_num = pg.pwm_channel;
	unsigned int pwm_period = pg.pwm_config[0];
	unsigned int pwm_duty = pg.pwm_config[1];

	pwm_ch = pwm_request(pwm_num, NULL);
	if (IS_ERR(pwm_ch)) {
		WIFI_INFO("request pwm %d failed\n",
		plat->gxb_conf.pwm_channel);
	}
	aml_chip = to_aml_pwm_chip(pwm_ch->chip);
	pwm_set_period(pwm_ch, pwm_period);
	pwm_config(pwm_ch, pwm_duty, pwm_period);
	pwm_enable(pwm_ch);
	WIFI_INFO("wifi pwm conf ok\n");

	return 0;
}

/*
* for gxtvbb,gxl,gxm,txl soc
*/
int pwm_double_channel_conf_dt(struct wifi_plat_info *plat)
{
	phandle pwm_phandle;
	int val;
	int ret;
	int count = 3;
	int i;
	struct device_node *np_wifi_pwm_conf = plat->dev->of_node;

	ret = of_property_read_u32(np_wifi_pwm_conf, "pwm_config", &val);
	if (ret) {
		pr_err("not match wifi_pwm_config node\n");
		return -1;
	} else {
		pwm_phandle = val;
		np_wifi_pwm_conf = of_find_node_by_phandle(pwm_phandle);
		if (!np_wifi_pwm_conf) {
			pr_err("can't find wifi_pwm_config node\n");
			return -1;
		}
	}

	ret = of_property_read_u32(np_wifi_pwm_conf, "pwm_channel1",
		  &(plat->gxtv_conf.pwm_channel1));
	if (ret) {
		pr_err("not config pwm channel 1 num\n");
		return -1;
	}
	ret = of_property_read_u32(np_wifi_pwm_conf, "pwm_channel2",
		&(plat->gxtv_conf.pwm_channel2));
	if (ret) {
		pr_err("not config pwm channel 2 num\n");
		return -1;
	}
	ret = of_property_read_u32_array(np_wifi_pwm_conf, "pwm_channel1_conf",
		(plat->gxtv_conf.pwm_config1), count);
	if (ret) {
		pr_err("not config pwm channel 1 parameters\n");
		return -1;
	}
	ret = of_property_read_u32_array(np_wifi_pwm_conf, "pwm_channel2_conf",
		(plat->gxtv_conf.pwm_config2), count);
	if (ret) {
		pr_err("not config pwm channel 2 parameters\n");
		return -1;
	}

	WIFI_INFO("pwm phandle val=%x;pwm-channel1=%d;pwm-channel2=%d\n",
			val, plat->gxtv_conf.pwm_channel1,
			plat->gxtv_conf.pwm_channel2);
	for (i = 0; i < count; i++) {
		WIFI_INFO("pwm_config1[%d] = %d\n",
		i, plat->gxtv_conf.pwm_config1[i]);
		WIFI_INFO("pwm_config2[%d] = %d\n",
		i, plat->gxtv_conf.pwm_config2[i]);
	}
	WIFI_INFO("wifi pwm dt ok\n");

	return 0;
}

/*
*configuration for double pwm
*/
int pwm_double_channel_conf(struct wifi_plat_info *plat)
{
	struct pwm_device *pwm_ch1 = NULL;
	struct pwm_device *pwm_ch2 = NULL;
	struct aml_pwm_chip *aml_chip1 = NULL;
	struct aml_pwm_chip *aml_chip2 = NULL;
	struct pwm_config_gxtvbb pg = plat->gxtv_conf;
	unsigned int pwm_ch1_num = pg.pwm_channel1;
	unsigned int pwm_ch2_num = pg.pwm_channel2;
	unsigned int pwm_ch1_period = pg.pwm_config1[0];
	unsigned int pwm_ch1_duty = pg.pwm_config1[1];
	unsigned int pwm_ch1_times = pg.pwm_config1[2];
	unsigned int pwm_ch2_period = pg.pwm_config2[0];
	unsigned int pwm_ch2_duty = pg.pwm_config2[1];
	unsigned int pwm_ch2_times = pg.pwm_config2[2];


	pwm_ch1 = pwm_request(pwm_ch1_num, NULL);
	if (IS_ERR(pwm_ch1)) {
		WIFI_INFO("request pwm %d failed\n",
		plat->gxtv_conf.pwm_channel1);
	}
	pwm_ch2 = pwm_request(pwm_ch2_num, NULL);
	if (IS_ERR(pwm_ch2)) {
		WIFI_INFO("request pwm %d failed\n",
		plat->gxtv_conf.pwm_channel2);
	}

	aml_chip1 = to_aml_pwm_chip(pwm_ch1->chip);
	aml_chip2 = to_aml_pwm_chip(pwm_ch2->chip);

	pwm_set_period(pwm_ch1, pwm_ch1_period);
	pwm_set_period(pwm_ch2, pwm_ch2_period);

	pwm_config(pwm_ch1, pwm_ch1_duty, pwm_ch1_period);
	pwm_config(pwm_ch2, pwm_ch2_duty, pwm_ch2_period);

	pwm_set_times(aml_chip1, pwm_ch1_num, pwm_ch1_times);
	pwm_set_times(aml_chip2, pwm_ch2_num, pwm_ch2_times);

	pwm_enable(pwm_ch1);
	pwm_enable(pwm_ch2);
	WIFI_INFO("wifi pwm conf ok\n");

	return 0;
}

static int wifi_dev_probe(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_OF
	struct wifi_plat_info *plat;
	const char *value;
	struct gpio_desc *desc;
#else
	struct wifi_plat_info *plat =
	 (struct wifi_plat_info *)(pdev->dev.platform_data);
#endif

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		plat = wifi_get_driver_data(pdev);
		plat->plat_info_valid = 0;
		plat->dev = &pdev->dev;

		ret = of_property_read_string(pdev->dev.of_node,
			"interrupt_pin", &value);
		if (ret) {
			WIFI_INFO("no interrupt pin");
			plat->interrupt_pin = 0;
		} else {
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"interrupt_pin", 0, NULL);
			plat->interrupt_desc = desc;
			plat->interrupt_pin = desc_to_gpio(desc);

			/* amlogic_gpio_name_map_num(value); */

			plat->irq_num =
				irq_of_parse_and_map(pdev->dev.of_node, 0);
			/*
			ret = of_property_read_u32(pdev->dev.of_node,
				"irq_num", &plat->irq_num);
			*/
			CHECK_PROP(ret, "irq_num", "null");


			ret = of_property_read_string(pdev->dev.of_node,
			"irq_trigger_type", &value);
			CHECK_PROP(ret, "irq_trigger_type", value);
			if (strcmp(value, "GPIO_IRQ_HIGH") == 0)
				plat->irq_trigger_type = GPIO_IRQ_HIGH;
			else if (strcmp(value, "GPIO_IRQ_LOW") == 0)
				plat->irq_trigger_type = GPIO_IRQ_LOW;
			else if (strcmp(value, "GPIO_IRQ_RISING") == 0)
				plat->irq_trigger_type = GPIO_IRQ_RISING;
			else if (strcmp(value, "GPIO_IRQ_FALLING") == 0)
				plat->irq_trigger_type = GPIO_IRQ_FALLING;
			else {
				WIFI_INFO("unknown irq trigger type-%s\n",
				 value);
				return -1;
			}
		}

		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin", &value);
		if (ret) {
			WIFI_INFO("no power_on_pin");
			plat->power_on_pin = 0;
			plat->power_on_pin_OD = 0;
		} else {
			wifi_power_gpio = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
			"power_on_pin", 0, NULL);
			plat->powe_desc = desc;
			plat->power_on_pin = desc_to_gpio(desc);
		}

		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_level", &plat->power_on_pin_level);

		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_OD", &plat->power_on_pin_OD);
		if (ret)
			plat->power_on_pin_OD = 0;
		pr_info("wifi: power_on_pin_OD = %d;\n", plat->power_on_pin_OD);
		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin2", &value);
		if (ret) {
			WIFI_INFO("no power_on_pin2");
			plat->power_on_pin2 = 0;
		} else {
			wifi_power_gpio2 = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"power_on_pin2", 0, NULL);
			plat->power_on_pin2 = desc_to_gpio(desc);
		}
#if 0
		ret = of_property_read_string(pdev->dev.of_node,
			"clock_32k_pin", &value);
		/* CHECK_PROP(ret, "clock_32k_pin", value); */
		if (ret)
			plat->clock_32k_pin = 0;
		else
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"clock_32k_pin", 0, NULL);
			plat->clock_32k_pin = desc_to_gpio(desc);
#endif
			/* amlogic_gpio_name_map_num(value); */
		if (of_get_property(pdev->dev.of_node,
			"pinctrl-names", NULL)) {
			unsigned int pwm_misc;
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
				pwm_double_channel_conf_dt(plat);
				pwm_double_channel_conf(plat);
			} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
				pwm_single_channel_conf_dt(plat);
				pwm_single_channel_conf(plat);
			} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8B) {
				/* pwm_e */
				WIFI_INFO("set pwm as 32k output");
				aml_write_cbus(0x21b0, 0x7980799);
				pwm_misc = aml_read_cbus(0x21b2);
				pwm_misc &= ~((0x7f << 8) | (3 << 4) |
					(1 << 2) | (1 << 0));
				pwm_misc |= ((1 << 15) | (4 << 8) | (2 << 4));
				aml_write_cbus(0x21b2, pwm_misc);
				aml_write_cbus(0x21b2, (pwm_misc | (1 << 0)));
			}

			plat->p = devm_pinctrl_get_select(&pdev->dev,
				"wifi_32k_pins");
		}
#ifdef CONFIG_BCMDHD_USE_STATIC_BUF
		if (of_get_property(pdev->dev.of_node,
			"dhd_static_buf", NULL)) {
			WIFI_INFO("dhd_static_buf setup\n");
			bcmdhd_init_wlan_mem();
		}
#endif

		plat->plat_info_valid = 1;

		WIFI_INFO("interrupt_pin=%d\n", plat->interrupt_pin);
		WIFI_INFO("irq_num=%d, irq_trigger_type=%d\n",
			plat->irq_num, plat->irq_trigger_type);
		WIFI_INFO("power_on_pin=%d\n", plat->power_on_pin);
		WIFI_INFO("clock_32k_pin=%d\n", plat->clock_32k_pin);
	}
#endif
	ret = alloc_chrdev_region(&wifi_power_devno,
			0, 1, WIFI_POWER_DRIVER_NAME);
	if (ret < 0) {
		ret = -ENODEV;
		goto out;
	}
	ret = class_register(&wifi_power_class);
	if (ret < 0)
		goto error1;
	wifi_power_cdev = cdev_alloc();
	if (!wifi_power_cdev)
		goto error2;
	cdev_init(wifi_power_cdev, &wifi_power_fops);
	wifi_power_cdev->owner = THIS_MODULE;
	ret = cdev_add(wifi_power_cdev, wifi_power_devno, 1);
	if (ret)
		goto error3;
	devp = device_create(&wifi_power_class, NULL,
			wifi_power_devno, NULL, WIFI_POWER_DEVICE_NAME);
	if (IS_ERR(devp)) {
		ret = PTR_ERR(devp);
		goto error3;
	}
	devp->platform_data = pdata;

	wifi_setup_dt();

	return 0;
error3:
	cdev_del(wifi_power_cdev);
error2:
	class_unregister(&wifi_power_class);
error1:
	unregister_chrdev_region(wifi_power_devno, 1);
out:
	return ret;
}

static int wifi_dev_remove(struct platform_device *pdev)
{
	WIFI_INFO("wifi_dev_remove\n");
	wifi_teardown_dt();
	return 0;
}

static struct platform_driver wifi_plat_driver = {
	.probe = wifi_dev_probe,
	.remove = wifi_dev_remove,
	.driver = {
	.name = "aml_wifi",
	.owner = THIS_MODULE,
	.of_match_table = wifi_match
	},
};

static int __init wifi_dt_init(void)
{
	int ret;
	ret = platform_driver_register(&wifi_plat_driver);
	return ret;
}
/* module_init(wifi_dt_init); */
fs_initcall_sync(wifi_dt_init);

static void __exit wifi_dt_exit(void)
{
	platform_driver_unregister(&wifi_plat_driver);
}
module_exit(wifi_dt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");

/**************** wifi mac *****************/
u8 WIFI_MAC[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static unsigned char chartonum(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;
	return 0;
}

static int __init mac_addr_set(char *line)
{
	unsigned char mac[6];
	int i = 0;
	WIFI_INFO("try to wifi mac from emmc key!\n");
	for (i = 0; i < 6 && line[0] != '\0' && line[1] != '\0'; i++) {
		mac[i] = chartonum(line[0]) << 4 | chartonum(line[1]);
		line += 3;
	}
	memcpy(WIFI_MAC, mac, 6);
	WIFI_INFO("uboot setup mac-addr: %x:%x:%x:%x:%x:%x\n",
	WIFI_MAC[0], WIFI_MAC[1], WIFI_MAC[2], WIFI_MAC[3], WIFI_MAC[4],
	WIFI_MAC[5]);

	return 1;
}

__setup("mac_wifi=", mac_addr_set);

u8 *wifi_get_mac(void)
{
	return WIFI_MAC;
}
EXPORT_SYMBOL(wifi_get_mac);

void extern_wifi_set_enable(int is_on)
{

	if (is_on) {
		set_wifi_power(1);
		WIFI_INFO("WIFI  Enable! %d\n", wifi_info.power_on_pin);
	} else {
		set_wifi_power(0);
		WIFI_INFO("WIFI  Disable! %d\n", wifi_info.power_on_pin);
	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

int wifi_irq_num(void)
{
	return wifi_info.irq_num;
}
EXPORT_SYMBOL(wifi_irq_num);

int wifi_irq_trigger_level(void)
{
	return wifi_info.irq_trigger_type;
}
EXPORT_SYMBOL(wifi_irq_trigger_level);
