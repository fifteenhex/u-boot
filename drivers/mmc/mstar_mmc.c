// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 - Daniel Palmer
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <mmc.h>
#include <reset.h>
#include <asm/io.h>
#include <regmap.h>
#include <clk.h>
#include <cache.h>
#include <cpu_func.h>

#ifdef CONFIG_HEXDUMP
#include <hexdump.h>
#endif

#include "mstar-fcie.h"

struct mstar_mmc_platdata {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct mstar_mmc_priv {
	struct regmap	*regmap;
	struct clk	clk;

	// io control
	struct regmap_field *clk_en;
	struct regmap_field *bus_width;

	// transfer control
	struct regmap_field *blk_sz;
	struct regmap_field *blk_cnt;
	struct regmap_field *rspr2_en;
	struct regmap_field *rsp_en;
	struct regmap_field *cmd_en;
	struct regmap_field *dtrf_en;
	struct regmap_field *jobdir;
	struct regmap_field *adma_en;
	struct regmap_field *busydet_en;
	struct regmap_field *errdet_en;
	struct regmap_field *cmd_sz;
	struct regmap_field *rsp_sz;
	struct regmap_field *job_start;

	struct regmap_field *dmaaddr_l;
	struct regmap_field *dmaaddr_h;
	struct regmap_field *dmalen_l;
	struct regmap_field *dmalen_h;

	// status
	struct regmap_field *status;
	struct regmap_field *card_busy;

	// boot mode
	struct regmap_field *imisel;

	// reset
	struct regmap_field *nrst;
	struct regmap_field *rst_status;

	//
	bool cmd_done;
	bool data_done;
	bool busy_done;
	bool error;
};

static int mstar_mmc_set_ios(struct udevice *dev)
{
	struct mstar_mmc_priv *priv = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	int ret = 0;
	unsigned int bw;

	// setup the bus width
	switch(mmc->bus_width){
		case 0:
		case 1:
			bw = 0;
			break;
		case 4:
			bw = 1;
			break;
		case 8:
			bw = 2;
			break;
		default:
			dev_err(dev, "Can't set bus width %d\n", mmc->bus_width);
			goto clk;
	}

	regmap_field_write(priv->bus_width, bw);

clk:
	// setup the clock
	if(mmc->clk_disable)
		regmap_field_write(priv->clk_en, 0);
	else
		regmap_field_write(priv->clk_en, 1);

	return ret;
}

static void mstar_mmc_send_cmd_writecmd(struct mstar_mmc_priv* priv, u8 cmd, u32 arg){
	int i;
	u8 fifo[6] = {0};
	u8 *bytes = fifo;
	u16 *words = (u16*) fifo;

	*bytes++ = cmd | 0x40 ;

	arg = cpu_to_be32(arg);
	memcpy(bytes, &arg, sizeof(arg));

	for(i = 0; i < ARRAY_SIZE(fifo) / 2; i++){
		regmap_write(priv->regmap, REG_FIFO + (i * 4), words[i]);
	}
}

static int mstar_mmc_setupcmd(struct mstar_mmc_priv* priv, struct mmc_cmd *cmd){
	int rspsz = 0;

	// clear any existing flags
	regmap_write(priv->regmap, REG_SD_CTL, 0);

	// load the command into the fifo
	mstar_mmc_send_cmd_writecmd(priv, cmd->cmdidx, cmd->cmdarg);

	// configure the response length
	regmap_field_write(priv->rsp_en, 0);
	regmap_field_write(priv->rspr2_en, 0);
	if(cmd->resp_type & MMC_RSP_PRESENT){
		regmap_field_write(priv->rsp_en, 1);
		if(cmd->resp_type & MMC_RSP_136){
			regmap_field_write(priv->rspr2_en, 1);
			rspsz = 16;
		}
		else
			rspsz = 5;
	}

	regmap_field_write(priv->busydet_en, cmd->resp_type & MMC_RSP_BUSY ? 1 : 0);
	regmap_field_write(priv->errdet_en, cmd->resp_type & MMC_RSP_CRC ? 1 : 0);
	regmap_field_write(priv->cmd_en, 1);
	regmap_field_write(priv->cmd_sz, 0x5); // this is always the case right?
	regmap_field_write(priv->rsp_sz, rspsz);

	return rspsz;
}

static void msc313_fcie_parse_int_flags(struct mstar_mmc_priv *priv, unsigned int flags){
	//printf("int flags %x\n", flags);
	if(flags & INT_CMD_END){
		priv->cmd_done = true;
	}
	if(flags & INT_DATA_END){
		priv->data_done = true;
	}
	if(flags & INT_BUSY_END){
		priv->busy_done = true;
	}
	if(flags & INT_ERR){
		priv->error = true;
	}
}

static bool mstar_fcie_parse_and_check_flags(struct mstar_mmc_priv *priv, unsigned int flags,
						bool cmd, bool data, bool busy){
	bool ret = true;
	msc313_fcie_parse_int_flags(priv, flags);
	if(priv->error)
		goto out;

	if(cmd)
		ret &= priv->cmd_done;
	if(data)
		ret &= priv->data_done;
	if(busy)
		ret &= priv->busy_done;

out:
	return ret;
}

static int mstar_mmc_start_transfer_and_wait(struct mstar_mmc_priv *priv,
		bool cmd, bool data, bool busy, unsigned int* status)
{
	unsigned int intflags, poll_timeout;
	unsigned timeout = 100;
	unsigned tmp;
	int ret = 0;

	regmap_read(priv->regmap, REG_SD_CTL, &tmp);
	//printf("sdctl %04x\n", tmp);

	if(data)
		timeout += 1000;

	priv->cmd_done = false;
	priv->data_done = false;
	priv->busy_done = false;
	priv->error = false;

	// clear the flags and start the transfer
	regmap_field_write(priv->status, ~0);
	regmap_field_write(priv->job_start, 0);
	regmap_field_write(priv->job_start, 1);

	/*
	 * we have to wait some time before polling the flags otherwise
	 * the controller starts corrupting memory, probably because the
	 * flags we have are old
	 */
	mdelay(100);
	poll_timeout = regmap_read_poll_timeout(priv->regmap, REG_INT,
			intflags, mstar_fcie_parse_and_check_flags(priv,intflags,cmd,data,busy), 100, timeout);
	regmap_write(priv->regmap, REG_INT, ~0);

	if(poll_timeout){
		printf("timeout while polling\n");
		ret = 1;
		goto out;
	}

	if(priv->error){
		printf("error during transfer\n");
		ret = 1;
		goto out;
	}

	if(data){
		barrier();
		// surely this should be invalidate?
		flush_dcache_all();
	}

out:
	regmap_field_read(priv->status, status);
	return ret;
}

static int mstar_mmc_readrsp(struct mstar_mmc_priv *priv, u8 cmd, u32* rsp, int len, bool hasopcode){
	int i, j;
	unsigned int value;
	u8 *buf = (u8*) rsp;

	int words = (len % 2 ? len + 1 : len) / 2;

	for(i = 0; i < words; i++){
		regmap_read(priv->regmap, REG_FIFO + (i * 4), &value);
		for(j = 0; j < 2 && ((i * 2) + j) < len; j++){
			if(i == 0 && j == 0){
				// if the first byte is the opcode
				// check that it matches the expected opcode
				// as the fifo content could be stale.
				// This was added because sometimes the
				// error interrupt was not firing.
				if(hasopcode && (value & 0xff) != cmd)
					return EILSEQ;

				// always strip the first byte.
				continue;
			}
			*buf++ = (value >> (8 * j));
		}
	}

	rsp[0] = be32_to_cpu(rsp[0]);
	rsp[1] = be32_to_cpu(rsp[1]);
	rsp[2] = be32_to_cpu(rsp[2]);
	rsp[3] = be32_to_cpu(rsp[3]);
	return 0;
}

static int mstar_mmc_capturecmdresult(struct mstar_mmc_priv *priv,
		struct mmc_cmd *cmd, unsigned int status, int rspsz){
	int err;

	if(status & SD_STS_NORSP){
		printf("No response\n");
		err = ETIMEDOUT;
		return 1;
	}
	else if(status & SD_STS_CMDRSPCRCERR){
		// The vendor driver suggests that the CRC flag
		// is broken for R3 and R4 responses. But I think
		// this is for anything without a CRC.
		if(cmd->cmdarg & MMC_RSP_CRC){
			//cmd->error = EILSEQ;
			printf("CRC error\n");
			return 1;
		}
	}

	if(rspsz > 0){
		err = mstar_mmc_readrsp(priv, cmd->cmdidx,
				cmd->response, rspsz, cmd->cmdarg & MMC_RSP_OPCODE);
		if(err)
			return 1;
	}

	return 0;
}

static int mstar_mmc_send_cmd_prepcmd_and_tx(struct mstar_mmc_priv *priv, struct mmc_cmd *cmd){
	int rspsz = mstar_mmc_setupcmd(priv, cmd);
	unsigned int status;
	if(mstar_mmc_start_transfer_and_wait(priv, true, false, cmd->cmdarg & MMC_RSP_BUSY, &status)){
		//cmd->error = ETIMEDOUT;
	}

	if(mstar_mmc_capturecmdresult(priv, cmd, status, rspsz))
		return 1;

	return 0;
}

static int mstar_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	struct mstar_mmc_priv *priv = dev_get_priv(dev);
	int rspsz;
	bool dataread, chkcmddone;
	uint32_t dmaaddr, dmalen;
	int ret = 1;
	unsigned int status, cardbusy;

	if(data == NULL || (data->flags & MMC_DATA_WRITE)){
		ret = mstar_mmc_send_cmd_prepcmd_and_tx(priv, cmd);
	}
	else if(data){
		barrier();
		flush_dcache_all();

		//printf("data flags %x\n", data->flags);
		dataread = data->flags & MMC_DATA_READ;
		if(dataread){
			// we're doing a read so setup the command
			rspsz = mstar_mmc_setupcmd(priv, cmd);
			regmap_field_write(priv->jobdir, 0);
#ifdef CONFIG_MMC_TRACE
			dev_err("data <- %u x %u\n", data->blocksize, data->blocks);
			/* clear the destination, so you can see if the sd controller
			 * actually doing anything...
			 */
			dev_err(dev, "clearing destination memory\n");
			memset(data->dest, 0xff, data->blocksize * data->blocks);
#endif
		}
		else {
			regmap_write(priv->regmap, REG_SD_CTL, 0);
			// turning on errdet for a write stops the
			// trigger happening.
			regmap_field_write(priv->errdet_en, 0);
			regmap_field_write(priv->jobdir, 1);
#ifdef CONFIG_MMC_TRACE
			dev_info(dev,"data -> %u x %u\n", data->blocksize, data->blocks);
#endif
		}

		// setup for data tx/rx
		//printf("configuring data transfer, %d x %d blocks\n", data->blocksize, data->blocks);
		regmap_field_write(priv->dtrf_en, 1);
		regmap_field_write(priv->blk_sz, data->blocksize);
		regmap_field_write(priv->blk_cnt, data->blocks);

		chkcmddone = dataread;

		dmaaddr = ((uint32_t) data->dest);
		if(data->dest >= 0xa0000000){
#ifdef CONFIG_MMC_TRACE
			dev_info(dev, "using imi\n");
#endif
			//dmaaddr -= 0xa0000000;
			regmap_field_write(priv->imisel, 1);
		}
		else {
#ifdef CONFIG_MMC_TRACE
			dev_info(dev, "using miu\n");
#endif
			dmaaddr -= 0x20000000;
			regmap_field_write(priv->imisel, 0);
		}

		dmalen = data->blocks * data->blocksize;
		regmap_field_write(priv->dmaaddr_h, dmaaddr >> 16);
		regmap_field_write(priv->dmaaddr_l, dmaaddr);


		regmap_field_write(priv->dmalen_h, dmalen >> 16);
		regmap_field_write(priv->dmalen_l, dmalen);

		if(mstar_mmc_start_transfer_and_wait(priv, chkcmddone, true, false, &status)){
			//mrq->data->error = ETIMEDOUT;
			ret = 1;
			goto tfr_err;
		}

		// the first block will have also triggered sending the cmd
		// if this was a read so capture the rsp etc for that here
		// and clear the cmd flags for the next block
		if(chkcmddone){
			if(mstar_mmc_capturecmdresult(priv, cmd, status, rspsz))
				goto tfr_err;
			regmap_field_write(priv->cmd_en, 0);
			regmap_field_write(priv->rsp_en, 0);
			regmap_field_write(priv->rspr2_en, 0);
			regmap_field_write(priv->busydet_en, 0);
		}

		// check for errors
		if(status & SD_STS_DATRDCERR){
			dev_err(dev, "data read CRC error\n");
			//mrq->data->error = EILSEQ;
			ret = 1;
			goto tfr_err;
		}
		if(status & SD_STS_DATWRCERR){
			dev_err(dev, "data write CRC error\n");
			//mrq->data->error = EILSEQ;
			ret = 1;
			goto tfr_err;
		}

		// check if the card was still busy
		// and poll the busy bit until it's not
		// not totally sure if this is actually
		// correct.
		if(status & SD_STS_CARDBUSY){
			regmap_field_read_poll_timeout(priv->card_busy, cardbusy,
					!cardbusy, 0, 1000);
		}

#ifdef CONFIG_MMC_TRACE
#ifdef CONFIG_HEXDUMP
		print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, data->dest,
						     data->blocksize * data->blocks);
#endif
#endif
	}

	ret = 0;
	return ret;

tfr_err:
	dev_err(dev, "send command failed, status %04x\n", status);
	return ret;
}

static int mstar_mmc_getcd(struct udevice *dev)
{
	return 1;
}

static const struct dm_mmc_ops mstar_mmc_ops = {
	.send_cmd	= mstar_mmc_send_cmd,
	.set_ios	= mstar_mmc_set_ios,
	.get_cd		= mstar_mmc_getcd,
};

static int mstar_mmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct mstar_mmc_priv *priv = dev_get_priv(dev);
	struct mstar_mmc_platdata *plat = dev_get_platdata(dev);
	int ret;

	ret = regmap_init_mem(dev_ofnode(dev), &priv->regmap);
	if(ret){
		dev_err(dev, "failed to create reg map\n");
		goto out;
	}

	ret = clk_get_by_index(dev, 0, &priv->clk);
	//if(ret)
	//	goto out;
	ret = 0;

	plat->mmc.priv = priv;
	upriv->mmc = &plat->mmc;

	priv->clk_en = regmap_field_alloc(priv->regmap, sd_mode_clken_field);
	priv->bus_width = regmap_field_alloc(priv->regmap, sd_mode_buswidth_field);

	priv->blk_cnt = regmap_field_alloc(priv->regmap, blockcount_field);
	priv->blk_sz = regmap_field_alloc(priv->regmap, blocksize_field);

	priv->rspr2_en = regmap_field_alloc(priv->regmap, st_ctl_rspr2en_field);
	priv->rsp_en = regmap_field_alloc(priv->regmap, st_ctl_rspen_field);
	priv->adma_en = regmap_field_alloc(priv->regmap, st_ctl_admaen_field);
	priv->dtrf_en = regmap_field_alloc(priv->regmap, st_ctl_dtrfen_field);
	priv->jobdir = regmap_field_alloc(priv->regmap, st_ctl_jobdir_field);
	priv->cmd_en = regmap_field_alloc(priv->regmap, st_ctl_cmden_field);
	priv->busydet_en = regmap_field_alloc(priv->regmap, st_ctl_busydeten_field);
	priv->errdet_en = regmap_field_alloc(priv->regmap, st_ctl_errdeten_field);

	priv->cmd_sz = regmap_field_alloc(priv->regmap, cmd_rsp_size_cmdsz_field);
	priv->rsp_sz = regmap_field_alloc(priv->regmap, cmd_rsp_size_rspsz_field);
	priv->job_start = regmap_field_alloc(priv->regmap, st_ctl_jobstart_field);

	priv->status = regmap_field_alloc(priv->regmap, sd_sts_status_field);
	priv->card_busy = regmap_field_alloc(priv->regmap, sd_sts_cardbusy_field);

	priv->imisel = regmap_field_alloc(priv->regmap, bootmode_imisel_field);

	priv->nrst = regmap_field_alloc(priv->regmap, rst_nrst_field);
	priv->status = regmap_field_alloc(priv->regmap, rst_status_field);

	priv->dmaaddr_l = regmap_field_alloc(priv->regmap, dma_addr_l_field);
	priv->dmaaddr_h = regmap_field_alloc(priv->regmap, dma_addr_h_field);
	priv->dmalen_l = regmap_field_alloc(priv->regmap, dma_len_l_field);
	priv->dmalen_h = regmap_field_alloc(priv->regmap, dma_len_h_field);

out:
	return ret;
}

static int mstar_mmc_ofdata_to_platdata(struct udevice *dev)
{
	struct mstar_mmc_priv *priv = dev_get_priv(dev);
	struct mstar_mmc_platdata *plat = dev_get_platdata(dev);
	struct mmc_config *cfg;
	int ret;

	cfg = &plat->cfg;
	cfg->name = "MSC";
	cfg->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS;

	ret = mmc_of_parse(dev, cfg);
	if (ret < 0) {
		dev_err(dev, "failed to parse host caps\n");
		return ret;
	}

	cfg->f_min = 400000;
	cfg->f_max = 52000000;

	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	cfg->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	return 0;
}

static int mstar_mmc_bind(struct udevice *dev)
{
	struct mstar_mmc_platdata *plat = dev_get_platdata(dev);
	return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id mstar_mmc_ids[] = {
	{
	  .compatible = "mstar,msc313-sdio",
	},
	{ /* sentinel */ }
};

U_BOOT_DRIVER(mstar_mmc_drv) = {
	.id		= UCLASS_MMC,
	.name		= "mstar_mmc",
	.of_match	= mstar_mmc_ids,
	.ofdata_to_platdata = mstar_mmc_ofdata_to_platdata,
	.bind		= mstar_mmc_bind,
	.probe		= mstar_mmc_probe,
	.ops		= &mstar_mmc_ops,
	.platdata_auto_alloc_size = sizeof(struct mstar_mmc_platdata),
	.priv_auto_alloc_size = sizeof(struct mstar_mmc_priv),
};
