// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2016-2019 Intel Corporation. All rights reserved. */
#include <linux/memremap.h>
#include <linux/pagemap.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pfn_t.h>
#include <linux/slab.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/nvpc.h>
#include "dax-private.h"
#include "bus.h"

/* Memory resource name used for add_memory_driver_managed(). */
static const char *nvpc_name;
/* Set if any memory will remain added when the driver will be unloaded. */
static bool any_hotremove_failed;

static int dax_nvpc_range(struct dev_dax *dev_dax, int i, struct range *r)
{
	struct dev_dax_range *dax_range = &dev_dax->ranges[i];
	struct range *range = &dax_range->range;

	/* memory-block align the hotplug range */
	r->start = ALIGN(range->start, memory_block_size_bytes());
	r->end = ALIGN_DOWN(range->end + 1, memory_block_size_bytes()) - 1;
	if (r->start >= r->end) {
		r->start = range->start;
		r->end = range->end;
		return -ENOSPC;
	}
	return 0;
}

struct dax_nvpc_data {
	const char *res_name;
	int mgid;
	struct resource *res[];
};

struct dev_dax_nvpc
{
	struct dev_dax *dev_dax;
	phys_addr_t phys_start;
	size_t size;
};

static int nvpc_zero_page_range(struct dax_device *dax_dev, pgoff_t pgoff, size_t nr_pages)
{
	struct dev_dax_nvpc *dax_nvpc = (struct dev_dax_nvpc *)dax_get_private(dax_dev);

	memset(__va(dax_nvpc->phys_start)+PFN_PHYS(pgoff), 
			0, nr_pages << PAGE_SHIFT);
	return 0;
}

static long nvpc_map_whole_dev(struct dax_device *dax_dev, void **kaddr, pfn_t *pfn)
{
	struct dev_dax_nvpc *dax_nvpc = (struct dev_dax_nvpc *)dax_get_private(dax_dev);
	pr_info("[KMEM TEST]: nvpc_map_whole_dev\n");
	
	*kaddr = __va(dax_nvpc->phys_start);
	*pfn = phys_to_pfn_t(dax_nvpc->phys_start, PFN_MAP);
	
	pr_info("[KMEM TEST]: nvpc_map_whole_dev kaddr: %p, pfn: %llx\n", *kaddr, pfn->val);
	return PHYS_PFN(dax_nvpc->size);
}

static const struct dax_operations nvpc_dax_ops = {
	.zero_page_range = nvpc_zero_page_range,
	.map_whole_dev = nvpc_map_whole_dev,
};


static int dev_dax_nvpc_probe(struct dev_dax *dev_dax)
{
	struct device *dev = &dev_dax->dev;
	unsigned long total_len = 0;
	struct dax_nvpc_data *data;
	int rc, mapped = 0;
	int numa_node;
	struct range range;
	struct resource *res;
	struct dev_dax_nvpc *dax_nvpc;

	void* kaddr;
	pfn_t pfn;
	long len;
	struct nvpc_opts init_opts;

	pr_info("[KMEM TEST]: dev_dax_nvpc_probe\n");

	/*
	 * Ensure good NUMA information for the persistent memory.
	 * Without this check, there is a risk that slow memory
	 * could be mixed in a node with faster memory, causing
	 * unavoidable performance issues.
	 */
	numa_node = dev_dax->target_node;
	if (numa_node < 0) {
		dev_warn(dev, "rejecting DAX region with invalid node: %d\n",
				numa_node);
		return -EINVAL;
	}

	// for (i = 0; i < dev_dax->nr_range; i++) {
	// struct range range;
	
	// NVXXX: only support the first range now
	rc = dax_nvpc_range(dev_dax, 0, &range);
	if (rc) {
		dev_info(dev, "mapping%d: %#llx-%#llx too small after alignment\n",
				0, range.start, range.end);
		// continue;
		return -EINVAL;
	}
	total_len += range_len(&range);
	// }

	if (!total_len) {
		dev_warn(dev, "rejecting DAX region without any memory after alignment\n");
		return -EINVAL;
	}

	data = kzalloc(struct_size(data, res, dev_dax->nr_range), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	rc = -ENOMEM;
	dax_nvpc = kzalloc(sizeof(struct dev_dax_nvpc), GFP_KERNEL);
	if (!dax_nvpc)
		goto err_dax_nvpc;

	data->res_name = kstrdup(dev_name(dev), GFP_KERNEL);
	if (!data->res_name)
		goto err_res_name;

	rc = memory_group_register_static(numa_node, PFN_UP(total_len));
	if (rc < 0)
		goto err_reg_mgid;
	data->mgid = rc;

	// for (i = 0; i < dev_dax->nr_range; i++) {
	// NVXXX: only support the first range now

	// struct resource *res;
	// struct range range;

	// rc = dax_nvpc_range(dev_dax, i, &range);
	// if (rc)
	// 	continue;

	/* Region is permanently reserved if hotremove fails. */
	res = request_mem_region(range.start, range_len(&range), data->res_name);
	if (!res) {
		dev_warn(dev, "mapping%d: %#llx-%#llx could not reserve region\n",
				0, range.start, range.end);
		/*
			* Once some memory has been onlined we can't
			* assume that it can be un-onlined safely.
			*/
		// if (mapped)
		// 	continue;
		rc = -EBUSY;
		goto err_request_mem;
	}
	data->res[0] = res;

	/*
		* Set flags appropriate for System RAM.  Leave ..._BUSY clear
		* so that add_memory() can add a child resource.  Do not
		* inherit flags from the parent since it may set new flags
		* unknown to us that will break add_memory() below.
		*/
	res->flags = IORESOURCE_SYSTEM_RAM;

	/*
		* Ensure that future kexec'd kernels will not treat
		* this as RAM automatically.
		* The memory is not onlined in any cases here.
		*/
	rc = add_memory_driver_managed(data->mgid, range.start,
			range_len(&range), nvpc_name, MHP_NID_IS_MGID|MHP_NO_ONLINE);

	if (rc) {
		dev_warn(dev, "mapping%d: %#llx-%#llx memory add failed\n",
				0, range.start, range.end);
		remove_resource(res);
		kfree(res);
		data->res[0] = NULL;
		// if (mapped)
		// 	continue;
		goto err_request_mem;
	}
	mapped++;
	// }

	dev_set_drvdata(dev, data);

	dax_nvpc->dev_dax = dev_dax;
	dax_nvpc->phys_start = range.start;
	dax_nvpc->size = total_len;

	pr_info("[KMEM TEST]: numa_node %d\n", numa_node);
	pr_info("[KMEM TEST]: nvpc_dax_ops %p\n", &nvpc_dax_ops);

	dev_dax->dax_dev = alloc_dax(dax_nvpc, "nvpc", &nvpc_dax_ops, 0);

	len = dax_map_whole_dev(dev_dax->dax_dev, &kaddr, &pfn);
	
	pr_info("[KMEM TEST]: kaddr %p\n", kaddr);
	pr_info("[KMEM TEST]: pfn %lx\n", pfn_t_to_pfn(pfn));
	pr_info("[KMEM TEST]: len %ld\n", len);

	init_opts.dev = dev_dax->dax_dev;
	init_opts.nid = numa_node;
	init_opts.lru = true;
	init_opts.lru_sz = 0x80000;
	init_opts.syn = true;
	init_opts.syn_sz = 0;
	init_opts.promote_level = 1;
	rc = init_nvpc(&init_opts);

	pr_info("[KMEM TEST]: init_nvpc return: %d\n", rc);

	return 0;

err_request_mem:
	memory_group_unregister(data->mgid);
err_reg_mgid:
	kfree(data->res_name);
err_res_name:
	kfree(dax_nvpc);
err_dax_nvpc:
	kfree(data);
	return rc;
}

#ifdef CONFIG_MEMORY_HOTREMOVE
static void dev_dax_nvpc_remove(struct dev_dax *dev_dax)
{
	int i, success = 0;
	struct device *dev = &dev_dax->dev;
	struct dax_nvpc_data *data = dev_get_drvdata(dev);
	struct dev_dax_nvpc *dax_nvpc = (struct dev_dax_nvpc *)dax_get_private(dev_dax->dax_dev);

	/*
	 * We have one shot for removing memory, if some memory blocks were not
	 * offline prior to calling this function remove_memory() will fail, and
	 * there is no way to hotremove this memory until reboot because device
	 * unbind will succeed even if we return failure.
	 */
	for (i = 0; i < dev_dax->nr_range; i++) {
		struct range range;
		int rc;

		rc = dax_nvpc_range(dev_dax, i, &range);
		if (rc)
			continue;

		rc = remove_memory(range.start, range_len(&range));
		if (rc == 0) {
			remove_resource(data->res[i]);
			kfree(data->res[i]);
			data->res[i] = NULL;
			success++;
			continue;
		}
		any_hotremove_failed = true;
		dev_err(dev,
			"mapping%d: %#llx-%#llx cannot be hotremoved until the next reboot\n",
				i, range.start, range.end);
	}

	if (success >= dev_dax->nr_range) {
		memory_group_unregister(data->mgid);
		kfree(data->res_name);
		kfree(dax_nvpc);
		kfree(data);
		dev_set_drvdata(dev, NULL);
	}
}
#else
static void dev_dax_nvpc_remove(struct dev_dax *dev_dax)
{
	/*
	 * Without hotremove purposely leak the request_mem_region() for the
	 * device-dax range and return '0' to ->remove() attempts. The removal
	 * of the device from the driver always succeeds, but the region is
	 * permanently pinned as reserved by the unreleased
	 * request_mem_region().
	 */
	any_hotremove_failed = true;
}
#endif /* CONFIG_MEMORY_HOTREMOVE */

static struct dax_device_driver device_dax_nvpc_driver = {
	.probe = dev_dax_nvpc_probe,
	.remove = dev_dax_nvpc_remove,
};

static int __init dax_nvpc_init(void)
{
	int rc;

	/* Resource name is permanently allocated if any hotremove fails. */
	nvpc_name = kstrdup_const("System RAM (nvpc)", GFP_KERNEL);
	if (!nvpc_name)
		return -ENOMEM;

	rc = dax_driver_register(&device_dax_nvpc_driver);
	if (rc)
		kfree_const(nvpc_name);
	return rc;
}

static void __exit dax_nvpc_exit(void)
{
	dax_driver_unregister(&device_dax_nvpc_driver);
	if (!any_hotremove_failed)
		kfree_const(nvpc_name);
}

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL v2");
module_init(dax_nvpc_init);
module_exit(dax_nvpc_exit);
MODULE_ALIAS_DAX_DEVICE(0);
