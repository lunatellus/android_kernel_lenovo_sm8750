#include <linux/module.h>
#include <linux/platform_device.h>

#define NVT_SPI_NAME "NVT-ts"

extern int ts_touch_info_uevent(const struct device *dev,
			      struct kobj_uevent_env *env);

const struct device_type tp_dev_type = {
	.name = "novatek_ts",
	.uevent = ts_touch_info_uevent,
};

static int nvt_ts_platform_probe(struct platform_device *pdev)
{
	pr_err("[%s] %s %d: start\n", NVT_SPI_NAME, __func__, 37);
	pdev->dev.type = &tp_dev_type;
	pr_err("[%s] %s %d: end\n", NVT_SPI_NAME, __func__, 41);
	return 0;
}

static const struct platform_device_id nvt_ts_platform_id[] = {
	{ "novatek_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, nvt_ts_platform_id);

static struct platform_driver nvt_platform_driver = {
	.probe = nvt_ts_platform_probe,
	.driver = {
		.name = "novatek_ts",
		.owner = THIS_MODULE,
	},
	.id_table = nvt_ts_platform_id,
};

int32_t nvt_ts_platform_driver_init(void)
{
	pr_err("[%s] %s %d: enter\n", NVT_SPI_NAME, __func__, 64);
	return platform_driver_register(&nvt_platform_driver);
}
