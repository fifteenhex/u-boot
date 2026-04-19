// SPDX-License-Identifier: GPL-2.0+
/*
 * virtio GPU driver
 * 2025 Daniel Palmer <daniel@thingy.jp>
 */

#include <dm.h>
#include <malloc.h>
#include <video.h>
#include <virtio_types.h>
#include <virtio.h>
#include <virtio_ring.h>

#include "virtio_gpu.h"

#define VQ_CONTROL		0
#define VQ_CURSOR		1
#define NUM_VQS			2
#define FRAMEBUFFER_RESOURCE_ID	1

struct virtio_gpu_priv {
	struct virtqueue *vqs[NUM_VQS];
};

static int virtio_gpu_cmd(struct virtqueue *vq,
			  void *req, size_t req_size,
			  void *resp, size_t resp_size)
{
	struct virtio_sg resp_sg = { resp, resp_size };
	struct virtio_sg req_sg = { req, req_size };
	struct virtio_sg *sgs[] = { &req_sg, &resp_sg };
	struct virtio_gpu_ctrl_hdr *resp_hdr = resp;
	int ret;

	ret = virtqueue_add(vq, sgs, 1, 1);
	if (ret)
		return ret;

	virtqueue_kick(vq);

	while (!virtqueue_get_buf(vq, NULL))
		;

	/* Some basic error handling */
	switch (le32_to_cpu(resp_hdr->type)) {
	case VIRTIO_GPU_RESP_ERR_UNSPEC:
	case VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID:
	case VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID:
	case VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID:
	case VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER:
		return -EINVAL;
	case VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY:
		return -ENOMEM;
	}

	return 0;
}

static int virtio_gpu_get_display_info(struct virtqueue *vq,
				       struct virtio_gpu_resp_display_info *info)
{
	struct virtio_gpu_ctrl_hdr req = {
		.type = cpu_to_le32(VIRTIO_GPU_CMD_GET_DISPLAY_INFO),
	};

	return virtio_gpu_cmd(vq, &req, sizeof(req), info, sizeof(*info));
}

static int virtio_gpu_resource_create_2d(struct virtqueue *vq,
					 u32 resource_id,
					 u32 format,
					 u32 width,
					 u32 height)
{
	struct virtio_gpu_resource_create_2d req = {
		.hdr.type = cpu_to_le32(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D),
		.resource_id = cpu_to_le32(resource_id),
		.format	= cpu_to_le32(format),
		.width = cpu_to_le32(width),
		.height = cpu_to_le32(height),
	};
	struct virtio_gpu_ctrl_hdr resp;

	return virtio_gpu_cmd(vq, &req, sizeof(req), &resp, sizeof(resp));
}

struct virtio_gpu_attach_backing_hdr_ent {
	struct virtio_gpu_resource_attach_backing hdr;
	struct virtio_gpu_mem_entry entry;
};

static int virtio_gpu_resource_attach_backing(struct virtqueue *vq,
					      u32 resource_id,
					      u64 addr,
					      u32 length)
{
	struct virtio_gpu_attach_backing_hdr_ent req = {
		.hdr = {
			.hdr.type = cpu_to_le32(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING),
			.resource_id = cpu_to_le32(resource_id),
			.nr_entries = cpu_to_le32(1),
		},
		.entry = {
			.addr = cpu_to_le64(addr),
			.length = cpu_to_le32(length),
		},
	};
	struct virtio_gpu_ctrl_hdr resp;

	return virtio_gpu_cmd(vq, &req, sizeof(req), &resp, sizeof(resp));
}

static int virtio_gpu_set_scanout(struct virtqueue *vq,
				  u32 resource_id,
				  u32 width, u32 height)
{
	struct virtio_gpu_set_scanout req = {
		.hdr.type = cpu_to_le32(VIRTIO_GPU_CMD_SET_SCANOUT),
		.resource_id = cpu_to_le32(resource_id),
		.r = {
			.width = cpu_to_le32(width),
			.height = cpu_to_le32(height),
		},
	};
	struct virtio_gpu_ctrl_hdr resp;

	return virtio_gpu_cmd(vq, &req, sizeof(req), &resp, sizeof(resp));
}

static int virtio_gpu_set_transfer_to_host_2d(struct virtqueue *vq,
					      u32 resource_id,
					      u32 width,
					      u32 height)
{
	struct virtio_gpu_transfer_to_host_2d req = {
		.hdr.type = cpu_to_le32(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D),
		.resource_id = cpu_to_le32(resource_id),
		.r = {
			.width = cpu_to_le32(width),
			.height = cpu_to_le32(height),
		},
	};
	struct virtio_gpu_ctrl_hdr resp;

	return virtio_gpu_cmd(vq, &req, sizeof(req), &resp, sizeof(resp));
}

static int virtio_gpu_resource_flush(struct virtqueue *vq,
				     u32 resource_id,
				     u32 width,
				     u32 height)
{
	struct virtio_gpu_resource_flush req = {
		.hdr.type = cpu_to_le32(VIRTIO_GPU_CMD_RESOURCE_FLUSH),
		.resource_id = cpu_to_le32(resource_id),
		.r = {
			.width = cpu_to_le32(width),
			.height = cpu_to_le32(height),
		},
	};
	struct virtio_gpu_ctrl_hdr resp;

	return virtio_gpu_cmd(vq, &req, sizeof(req), &resp, sizeof(resp));
}

static int virtio_gpu_flush(struct udevice *dev)
{
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct virtqueue *vq = priv->vqs[VQ_CONTROL];
	int ret;

	ret = virtio_gpu_set_transfer_to_host_2d(vq,
						 FRAMEBUFFER_RESOURCE_ID,
						 uc_priv->xsize,
						 uc_priv->ysize);
	if (ret)
		return ret;

	ret = virtio_gpu_resource_flush(vq,
					FRAMEBUFFER_RESOURCE_ID,
					uc_priv->xsize,
					uc_priv->ysize);

	return ret;
}

static int virtio_gpu_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct virtio_gpu_resp_display_info display_info;
	struct virtqueue *vq;
	u32 width, height;
	int ret;
	int i;

	width = CONFIG_VIRTIO_DEFAULT_WIDTH;
	height = CONFIG_VIRTIO_DEFAULT_HEIGHT;

	ret = virtio_find_vqs(dev, NUM_VQS, priv->vqs);
	if (ret)
		return ret;

	vq = priv->vqs[VQ_CONTROL];

	ret = virtio_gpu_get_display_info(vq,
					  &display_info);
	if (ret)
		return ret;

	for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
		u32 h = le32_to_cpu(display_info.pmodes[i].r.height);
		u32 w = le32_to_cpu(display_info.pmodes[i].r.width);
		u32 e = le32_to_cpu(display_info.pmodes[i].enabled);

		if (!e)
			continue;

		/* For now just use the first enabled one */
		width = w;
		height = h;
		break;
	}

	/* Just use 32 bpp rgba for now */
	uc_priv->xsize  = width;
	uc_priv->ysize  = height;
	uc_priv->bpix   = VIDEO_BPP32;
	uc_priv->format = VIDEO_RGBA8888;

	ret = virtio_gpu_resource_create_2d(vq,
					    FRAMEBUFFER_RESOURCE_ID,
					    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM,
					    uc_priv->xsize,
					    uc_priv->ysize);
	if (ret)
		return ret;

	/* virtio GPU apparently doesn't care about alignment */
	plat->size = (width * height) * 4;
	plat->base = (ulong)malloc(plat->size);
	if (!plat->base)
		return -ENOMEM;

	ret = virtio_gpu_resource_attach_backing(vq,
						 FRAMEBUFFER_RESOURCE_ID,
						 plat->base,
						 round_up(plat->size, 4096));
	if (ret)
		goto err;

	ret = virtio_gpu_set_scanout(vq,
				     FRAMEBUFFER_RESOURCE_ID,
				     uc_priv->xsize,
				     uc_priv->ysize);
	if (ret)
		goto err;

	/* Do an initial flush to activate the output */
	ret = virtio_gpu_flush(dev);
	if (ret)
		goto err;

	return 0;

err:
	free((void *)plat->base);
	return ret;
}

static const struct video_ops virtio_gpu_ops = {
	.video_sync = virtio_gpu_flush,
};

U_BOOT_DRIVER(virtio_gpu) = {
	.name	   = VIRTIO_GPU_DRV_NAME,
	.id        = UCLASS_VIDEO,
	.ops       = &virtio_gpu_ops,
	.probe     = virtio_gpu_probe,
	.flags     = DM_FLAG_ACTIVE_DMA,
	.priv_auto = sizeof(struct virtio_gpu_priv),
};
