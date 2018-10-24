/*
 * gpio-fan.c - driver for fans controlled by GPIO.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/time.h>
#include <linux/workqueue.h>

#define KHADAS_FAN_TRIG_TEMP_LEVEL0		50	// 50 degree if not set
#define KHADAS_FAN_TRIG_TEMP_LEVEL1		60	// 60 degree if not set
#define KHADAS_FAN_TRIG_TEMP_LEVEL2		70	// 70 degree if not set
#define KHADAS_FAN_TRIG_MAXTEMP		80
#define KHADAS_FAN_LOOP_SECS 		30 * HZ	// 30 seconds
#define KHADAS_FAN_TEST_LOOP_SECS   5 * HZ  // 5 seconds
#define KHADAS_FAN_LOOP_NODELAY_SECS   	0
#define KHADAS_FAN_GPIO_OFF		0
#define KHADAS_FAN_GPIO_ON		1

enum khadas_fan_mode {
	KHADAS_FAN_STATE_MANUAL = 0,
	KHADAS_FAN_STATE_AUTO,
};

enum khadas_fan_level {
	KHADAS_FAN_LEVEL_0 = 0,
	KHADAS_FAN_LEVEL_1,
	KHADAS_FAN_LEVEL_2,
	KHADAS_FAN_LEVEL_3,
};

enum khadas_fan_enable {
	KHADAS_FAN_DISABLE = 0,
	KHADAS_FAN_ENABLE,
};

struct khadas_fan_data {
	struct platform_device *pdev;
	struct class *class;
	struct delayed_work work;
	struct delayed_work fan_test_work;
	enum    khadas_fan_enable enable;
	enum 	khadas_fan_mode mode;
	enum 	khadas_fan_level level;
	int	ctrl_gpio0;
	int	ctrl_gpio1;
	int	trig_temp_level0;
	int	trig_temp_level1;
	int	trig_temp_level2;
};

void khadas_fan_level_set(struct khadas_fan_data *fan_data, int level )
{
	if(0 == level){
		gpio_set_value(fan_data->ctrl_gpio0, KHADAS_FAN_GPIO_OFF);
		gpio_set_value(fan_data->ctrl_gpio1, KHADAS_FAN_GPIO_OFF);
	}else if(1 == level){
		gpio_set_value(fan_data->ctrl_gpio0, KHADAS_FAN_GPIO_ON);
		gpio_set_value(fan_data->ctrl_gpio1, KHADAS_FAN_GPIO_OFF);
	}else if(2 == level){
		gpio_set_value(fan_data->ctrl_gpio0, KHADAS_FAN_GPIO_OFF);
		gpio_set_value(fan_data->ctrl_gpio1, KHADAS_FAN_GPIO_ON);
	}else if(3 == level){
		gpio_set_value(fan_data->ctrl_gpio0, KHADAS_FAN_GPIO_ON);
		gpio_set_value(fan_data->ctrl_gpio1, KHADAS_FAN_GPIO_ON);
	}

}

extern int get_cpu_temp(void);
static void fan_work_func(struct work_struct *_work)
{
	int temp = -EINVAL;
	struct khadas_fan_data *fan_data = container_of(_work,
		   struct khadas_fan_data, work.work);

	temp = get_cpu_temp();

	if(temp != -EINVAL){
		if(temp < fan_data->trig_temp_level0 ){
			khadas_fan_level_set(fan_data,0);

		}else if(temp < fan_data->trig_temp_level1 ){
			khadas_fan_level_set(fan_data,1);

		}else if(temp < fan_data->trig_temp_level2 ){
			khadas_fan_level_set(fan_data,2);

		}else{
			khadas_fan_level_set(fan_data,3);
		}
	}

	schedule_delayed_work(&fan_data->work, KHADAS_FAN_LOOP_SECS);
}

static void fan_test_work_func(struct work_struct *_work)
{
	struct khadas_fan_data *fan_data = container_of(_work,
			struct khadas_fan_data, fan_test_work.work);


	khadas_fan_level_set(fan_data,0);

}


static void khadas_fan_set(struct khadas_fan_data  *fan_data)
{

	cancel_delayed_work(&fan_data->work);

	if (fan_data->enable == KHADAS_FAN_DISABLE) {
		khadas_fan_level_set(fan_data,0);
		return;
	}
	switch (fan_data->mode) {
	case KHADAS_FAN_STATE_MANUAL:
		switch(fan_data->level){
		case KHADAS_FAN_LEVEL_1:
			khadas_fan_level_set(fan_data,1);
			break;
		case KHADAS_FAN_LEVEL_2:
			khadas_fan_level_set(fan_data,2);
			break;
		case KHADAS_FAN_LEVEL_3:
			khadas_fan_level_set(fan_data,3);
			break;
		default:
			break;
		}
		break;

	case KHADAS_FAN_STATE_AUTO:
		// FIXME: achieve with a better way
		schedule_delayed_work(&fan_data->work, KHADAS_FAN_LOOP_NODELAY_SECS);
		break;

	default:
		break;
	}
}

static ssize_t fan_enable_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);

	return sprintf(buf, "Fan enable: %d\n", fan_data->enable);
}

static ssize_t fan_enable_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);
	int enable;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;

	// 0: manual, 1: auto
	if( enable >= 0 && enable < 2 ){
		fan_data->enable = enable;
		khadas_fan_set(fan_data);
	}

	return count;
}

static ssize_t fan_mode_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);

	return sprintf(buf, "Fan mode: %d\n", fan_data->mode);
}

static ssize_t fan_mode_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);
	int mode;

	if (kstrtoint(buf, 0, &mode))
		return -EINVAL;

	// 0: manual, 1: auto
	if( mode >= 0 && mode < 2 ){
		fan_data->mode = mode;
		khadas_fan_set(fan_data);
	}

	return count;
}

static ssize_t fan_level_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);

	return sprintf(buf, "Fan level: %d\n", fan_data->level);
}

static ssize_t fan_level_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);
	int level;

	if (kstrtoint(buf, 0, &level))
		return -EINVAL;

	if( level >= 0 && level < 4){
		fan_data->level = level;
		khadas_fan_set(fan_data);
	}

	return count;
}


static ssize_t fan_temp_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);
	int temp = -EINVAL;
    temp = get_cpu_temp();

	return sprintf(buf, "cpu_temp:%d\nFan trigger temperature: level0:%d level1:%d level2:%d\n", temp, fan_data->trig_temp_level0, fan_data->trig_temp_level1, fan_data->trig_temp_level2);
}

#if 0
static ssize_t fan_temp_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct khadas_fan_data *fan_data = dev_get_drvdata(dev);
	int temp;

	if (kstrtoint(buf, 0, &temp))
		return -EINVAL;

	if (temp > KHADAS_FAN_TRIG_MAXTEMP)
		temp = KHADAS_FAN_TRIG_MAXTEMP;
	fan_data->trig_temp_level0 = temp;

	return count;
}
#endif

static struct device_attribute fan_class_attrs[] = {
	__ATTR(enable, S_IRUGO | S_IWUGO, fan_enable_show, fan_enable_store),
	__ATTR(mode, S_IRUGO | S_IWUGO, fan_mode_show, fan_mode_store),
	__ATTR(level, S_IRUGO | S_IWUGO, fan_level_show, fan_level_store),
	__ATTR(temp, S_IRUGO, fan_temp_show, NULL),
};

static int khadas_fan_probe(struct platform_device *pdev)
{
	struct khadas_fan_data *fan_data = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct class *fclass;
	int ret;
	int i;

	printk("khadas_fan_probe\n");

	fan_data = devm_kzalloc(dev, sizeof(struct khadas_fan_data), GFP_KERNEL);
	if (!fan_data)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "trig_temp_level0", &fan_data->trig_temp_level0);
	if (ret < 0)
		fan_data->trig_temp_level0 = KHADAS_FAN_TRIG_TEMP_LEVEL0;
	ret = of_property_read_u32(dev->of_node, "trig_temp_level1", &fan_data->trig_temp_level1);
	if (ret < 0)
		fan_data->trig_temp_level1 = KHADAS_FAN_TRIG_TEMP_LEVEL1;
	ret = of_property_read_u32(dev->of_node, "trig_temp_level2", &fan_data->trig_temp_level2);
	if (ret < 0)
		fan_data->trig_temp_level2 = KHADAS_FAN_TRIG_TEMP_LEVEL2;

	fan_data->ctrl_gpio0 = of_get_named_gpio(dev->of_node, "fan_ctl0", 0);
	fan_data->ctrl_gpio1 = of_get_named_gpio(dev->of_node, "fan_ctl1", 0);
	if ((gpio_request(fan_data->ctrl_gpio0, "FAN") != 0)|| (gpio_request(fan_data->ctrl_gpio1, "FAN") != 0))
		return -EIO;

	gpio_direction_output(fan_data->ctrl_gpio0, KHADAS_FAN_GPIO_OFF);
	gpio_direction_output(fan_data->ctrl_gpio1, KHADAS_FAN_GPIO_OFF);
	fan_data->mode = KHADAS_FAN_STATE_AUTO;
	fan_data->level = KHADAS_FAN_LEVEL_0;
	fan_data->enable = KHADAS_FAN_DISABLE;

	INIT_DELAYED_WORK(&fan_data->work, fan_work_func);
	khadas_fan_level_set(fan_data,1);
	INIT_DELAYED_WORK(&fan_data->fan_test_work, fan_test_work_func);
	schedule_delayed_work(&fan_data->fan_test_work, KHADAS_FAN_TEST_LOOP_SECS);

	fan_data->pdev = pdev;
	platform_set_drvdata(pdev, fan_data);

	fclass = fan_data->class;
	fclass = class_create(THIS_MODULE, "fan");
	if (IS_ERR(fclass))
		return PTR_ERR(fclass);

	dev = device_create(fclass, dev->parent, 0, fan_data, "ctrl");
	for (i = 0; i < ARRAY_SIZE(fan_class_attrs); i++){
		ret = device_create_file(dev, &fan_class_attrs[i]);
		if(0!=ret){
			printk("khadas_fan_probe,device_create_file%d failed \n", i);
		}
	}
	dev_info(dev, "trigger temperature is level0:%d, level1:%d, level2:%d.\n", fan_data->trig_temp_level0, fan_data->trig_temp_level1, fan_data->trig_temp_level2);
	return 0;
}

static int khadas_fan_remove(struct platform_device *pdev)
{
	struct khadas_fan_data *fan_data = platform_get_drvdata(pdev);

	fan_data->enable = KHADAS_FAN_DISABLE;
	khadas_fan_set(fan_data);

	return 0;
}

static void khadas_fan_shutdown(struct platform_device *pdev)
{
	struct khadas_fan_data *fan_data = platform_get_drvdata(pdev);

	fan_data->enable = KHADAS_FAN_DISABLE;
	khadas_fan_set(fan_data);
}

#ifdef CONFIG_PM
static int khadas_fan_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct khadas_fan_data *fan_data = platform_get_drvdata(pdev);
	cancel_delayed_work(&fan_data->work);
	khadas_fan_level_set(fan_data, 0);
	return 0;
}

static int khadas_fan_resume(struct platform_device *pdev)
{
	struct khadas_fan_data *fan_data = platform_get_drvdata(pdev);
	khadas_fan_set(fan_data);
	return 0;
}
#endif

static struct of_device_id of_khadas_fan_match[] = {
	{ .compatible = "fanctl", },
	{},
};

static struct platform_driver khadas_fan_driver = {
	.probe	= khadas_fan_probe,
#ifdef CONFIG_PM
	.suspend = khadas_fan_suspend,
	.resume = khadas_fan_resume,
#endif
	.remove	= khadas_fan_remove,
	.shutdown = khadas_fan_shutdown,
	.driver	= {
		.name	= "fanctl",
		.of_match_table = of_match_ptr(of_khadas_fan_match),
	},
};

module_platform_driver(khadas_fan_driver);

MODULE_AUTHOR("kenny <kenny@khadas.com>");
MODULE_DESCRIPTION("khadas GPIO Fan driver");
MODULE_LICENSE("GPL");
