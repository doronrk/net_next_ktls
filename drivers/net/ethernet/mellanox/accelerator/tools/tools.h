/*
 * Copyright (c) 2015-2016 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __TOOLS_H__
#define __TOOLS_H__

#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/mlx5/accel_sdk.h>

#define MLX_ACCEL_TOOLS_DRIVER_NAME "mlx_accel_tools"

struct mlx_accel_tools_dev {
	/* Core device and connection to FPGA */
	struct mlx_accel_core_device *accel_device;

	/* Driver state per device */
	struct mutex mutex;

	/* Char device state */
	struct cdev cdev;
	dev_t dev;
	struct device *char_device;
	atomic_t open_count;
};

struct mlx_accel_tools_dev *
mlx_accel_tools_alloc(struct mlx_accel_core_device *device);
void mlx_accel_tools_free(struct mlx_accel_tools_dev *sb_dev);

int mlx_accel_tools_mem_write(struct mlx_accel_tools_dev *sb_dev,
			      void *buf, size_t count, u64 address,
			      enum mlx_accel_access_type access_type);
int mlx_accel_tools_mem_read(struct mlx_accel_tools_dev *sb_dev, void *buf,
			     size_t count, u64 address,
			     enum mlx_accel_access_type access_type);

#endif	/* __TOOLS_H__ */
