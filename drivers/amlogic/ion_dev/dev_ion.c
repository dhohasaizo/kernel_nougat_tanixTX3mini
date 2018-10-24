/*
 * drivers/amlogic/ion_dev/dev_ion.c
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


#include <linux/err.h>
#include <ion/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <ion/ion_priv.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/uaccess.h>
#include "meson_ion.h"

MODULE_DESCRIPTION("AMLOGIC ION driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic SH");

static unsigned debug = 1;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

#define dprintk(level, fmt, arg...)             \
	do {                                        \
		if (debug >= level)                     \
			pr_debug("ion-dev: " fmt, ## arg);  \
	} while (0)

/*
 * TODO instead with enum ion_heap_type from ion.h
 */
#define AML_ION_TYPE_SYSTEM	 0
#define AML_ION_TYPE_CARVEOUT   1
#define MAX_HEAP 5

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;
static struct ion_platform_heap my_ion_heap[MAX_HEAP];

struct ion_client *meson_ion_client_create(unsigned int heap_mask,
		const char *name) {

	/*
	 *	  * The assumption is that if there is a NULL device, the ion
	 *		   * driver has not yet probed.
	 *				*/
	if (idev == NULL) {
		dprintk(0, "create error");
		return ERR_PTR(-EPROBE_DEFER);
	}

	if (IS_ERR(idev)) {
		dprintk(0, "idev error");
		return (struct ion_client *)idev;
	}

	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(meson_ion_client_create);

int meson_ion_share_fd_to_phys(
	struct ion_client *client, int share_fd,
	ion_phys_addr_t *addr, size_t *len)
{
	struct ion_handle *handle;
	int ret;

	handle = ion_import_dma_buf(client, share_fd);
	if (IS_ERR_OR_NULL(handle)) {
		/* pr_err("%s,EINVAL, client=%p, share_fd=%d\n",
		 *	 __func__, client, share_fd);
		*/
		return PTR_ERR(handle);
	}

	ret = ion_phys(client, handle, addr, (size_t *)len);
	pr_debug("ion_phys ret=%d, phys=0x%lx\n", ret, *addr);
	if (ret < 0) {
		pr_err("ion_get_phys error, ret=%d\n", ret);
		return ret;
	}

	ion_free(client, handle);

	return 0;
}
EXPORT_SYMBOL(meson_ion_share_fd_to_phys);

static int meson_ion_get_phys(
	struct ion_client *client,
	unsigned long arg)
{
	struct meson_phys_data data;
	struct ion_handle *handle;
	size_t len;
	ion_phys_addr_t addr;
	int ret;

	if (copy_from_user(&data, (void __user *)arg,
		sizeof(struct meson_phys_data))) {
		return -EFAULT;
	}
	handle = ion_import_dma_buf(client, data.handle);
	if (IS_ERR_OR_NULL(handle)) {
		dprintk(0, "EINVAL, client=%p, share_fd=%d\n",
			client, data.handle);
		return PTR_ERR(handle);
	}

	ret = ion_phys(client, handle, &addr, (size_t *)&len);
	dprintk(1, "ret=%d, phys=0x%lX\n", ret, addr);
	if (ret < 0) {
		dprintk(0, "meson_ion_get_phys error, ret=%d\n", ret);
		return ret;
	}

	ion_free(client, handle);

	data.phys_addr = (unsigned int)addr;
	data.size = (unsigned int)len;
	if (copy_to_user((void __user *)arg, &data,
		sizeof(struct meson_phys_data))) {
		return -EFAULT;
	}
	return 0;
}

static long meson_custom_ioctl(
	struct ion_client *client,
	unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case ION_IOC_MESON_PHYS_ADDR:
		return meson_ion_get_phys(client, arg);
	default:
		return -ENOTTY;
	}
	return 0;
}

int dev_ion_probe(struct platform_device *pdev)
{
	int err = 0;
	int i;
	my_ion_heap[num_heaps].type = ION_HEAP_TYPE_SYSTEM;
	my_ion_heap[num_heaps].id = ION_HEAP_TYPE_SYSTEM;
	my_ion_heap[num_heaps].name = "vmalloc_ion";
	num_heaps++;

	my_ion_heap[num_heaps].type = ION_HEAP_TYPE_CUSTOM;
	my_ion_heap[num_heaps].id = ION_HEAP_TYPE_CUSTOM;
	my_ion_heap[num_heaps].name = "codec_mm_ion";
	my_ion_heap[num_heaps].base = (ion_phys_addr_t) NULL;
	my_ion_heap[num_heaps].size = 48 * 1024 * 1024;
	num_heaps++;

	/*add CMA ion heap*/
	my_ion_heap[num_heaps].type = ION_HEAP_TYPE_DMA;
	my_ion_heap[num_heaps].id = ION_HEAP_TYPE_DMA;
	my_ion_heap[num_heaps].name = "cma_ion";
	my_ion_heap[num_heaps].priv = &pdev->dev;
	num_heaps++;

	/* init reserved memory */
	err = of_reserved_mem_device_init(&pdev->dev);
	if (err != 0)
		dprintk(1, "failed get reserved memory\n");
	heaps = kzalloc(sizeof(struct ion_heap *) * num_heaps, GFP_KERNEL);
	if (!heaps)
		return -ENOMEM;
	/* idev = ion_device_create(NULL); */
	idev = ion_device_create(meson_custom_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
		panic(0);
		return PTR_ERR(idev);
	}

	platform_set_drvdata(pdev, idev);

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		heaps[i] = ion_heap_create(&my_ion_heap[i]);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto failed;
		}
		ion_device_add_heap(idev, heaps[i]);
		dprintk(2, "add heap type:%d id:%d\n",
			my_ion_heap[i].type, my_ion_heap[i].id);
	}

	dprintk(1, "%s, create %d heaps\n", __func__, num_heaps);

	return 0;

failed:
	dprintk(0, "ion heap create failed\n");
	kfree(heaps);
	heaps = NULL;
	panic(0);
	return err;
}

int dev_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);
	return 0;
}

static const struct of_device_id amlogic_ion_dev_dt_match[] = {
	{ .compatible = "amlogic, ion_dev", },
	{ },
};

static struct platform_driver ion_driver = {
	.probe = dev_ion_probe,
	.remove = dev_ion_remove,
	.driver = {
		.name = "ion_dev",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_ion_dev_dt_match
	}
};

/*
 * reserved memory initialize begin
 */
static int ion_carveout_init(struct reserved_mem *rmem, struct device *dev)
{
	return 0;

}

static const struct reserved_mem_ops sr_carveout_ops = {
	.device_init = ion_carveout_init,
};

static int __init ion_dev_mem_setup(struct reserved_mem *rmem)
{
	my_ion_heap[num_heaps].type = ION_HEAP_TYPE_CARVEOUT;
	my_ion_heap[num_heaps].id = ION_HEAP_TYPE_CARVEOUT;
	my_ion_heap[num_heaps].name = "carveout_ion";
	my_ion_heap[num_heaps].base = (ion_phys_addr_t) rmem->base;
	my_ion_heap[num_heaps].size = rmem->size;
	num_heaps++;

	rmem->ops = &sr_carveout_ops;

	return 0;
}

RESERVEDMEM_OF_DECLARE(ion_dev_mem, "amlogic, idev-mem", ion_dev_mem_setup);
/*
 * reserved memory initialize end
 */

static int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

static int ion_chunk_device_init(struct reserved_mem *rmem, struct device *dev)
{
	return 0;
}

static const struct reserved_mem_ops sr_rmem_ops = {
	.device_init = ion_chunk_device_init,
};

static int __init ion_chunk_setup(struct reserved_mem *rmem)
{
	phys_addr_t align = PAGE_SIZE;
	phys_addr_t mask = align - 1;
	if ((rmem->base & mask) || (rmem->size & mask)) {
		pr_err("Reserved memory: incorrect alignment of region\n");
		return -EINVAL;
	}
	/* add chunk ion heap */
	my_ion_heap[num_heaps].type = ION_HEAP_TYPE_CHUNK;
	my_ion_heap[num_heaps].id = ION_HEAP_TYPE_CHUNK;
	my_ion_heap[num_heaps].name = "chunck_mm_ion";
	my_ion_heap[num_heaps].base = (ion_phys_addr_t)rmem->base;
	my_ion_heap[num_heaps].size = rmem->size;
	num_heaps++;

	rmem->ops = &sr_rmem_ops;
	return 0;
}

RESERVEDMEM_OF_DECLARE(ion_chunk, "amlogic, chunk-reserve", ion_chunk_setup);

module_init(ion_init);
module_exit(ion_exit);
