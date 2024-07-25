// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#define LOG_CATEGORY UCLASS_SCSI

#include <dm.h>
#include <os.h>
#include <malloc.h>
#include <scsi.h>
#include <asm/io.h>

/* Bits for MVME147 DMA */
#include <regmap.h>
#include <syscon.h>
#include <asm/mvme147/mvme147.h>

#define DEBUG 1
#define PIO


#ifndef PIO
static void mvme147_setup_dma(struct regmap *pcc8,
							  struct regmap *pcc32,
							  void *data,
							  size_t len,
							  bool to_scsi)
{
	uint ctrl = MVME147_PCC8_DMACTRLSTAT_EN;

	if (to_scsi)
		ctrl |= MVME147_PCC8_DMACTRLSTAT_MSSM;

	regmap_write(pcc8, MVME147_PCC8_DMACTRLSTAT, ctrl);
	regmap_write(pcc32, MVME147_PCC32_DATAADDR, (uint) data);
	regmap_write(pcc32, MVME147_PCC32_BYTECOUNT, (uint) len);
}
#endif

static void mvme147_scsi_reset(struct regmap *pcc8, bool assert)
{
	uint scsi_port_ctrl;

	regmap_read(pcc8, MVME147_PCC8_SCSI_PORT_CTRL, &scsi_port_ctrl);

	if (assert)
		scsi_port_ctrl |= MVME147_PCC8_SCSI_PORT_CTRL_RSTSCSI;
	else
		scsi_port_ctrl &= ~MVME147_PCC8_SCSI_PORT_CTRL_RSTSCSI;

	regmap_write(pcc8, MVME147_PCC8_SCSI_PORT_CTRL, scsi_port_ctrl);

	mdelay(10);
}

#define STATUS_DBR	BIT(0)
#define STATUS_PE	BIT(1)
#define STATUS_FFE	BIT(2)
#define STATUS_CIP	BIT(4)
#define STATUS_BUSY	BIT(5)
#define STATUS_INT	BIT(7)

#define REG_OWNID			0x00
#define REG_CONTROL			0x01
#define REG_TIMEOUTPERIOD	0x02
#define REG_COMMAND_PHASE	0x10
#define REG_TRANSFER_COUNT2	0x12
#define REG_TRANSFER_COUNT1	0x13
#define REG_TRANSFER_COUNT0	0x14
#define REG_DESTINATION_ID	0x15
#define REG_SOURCE_ID		0x16
#define REG_SCSI_STATUS		0x17
#define REG_COMMAND			0x18
#define REG_DATA			0x19

#define COMMAND_RESET				0x00
#define COMMAND_ABORT				0x01
#define COMMAND_ASSERT_ATN			0x02
#define COMMAND_NEGATE_ACK			0x03
#define COMMAND_DISCONNECT			0x04
#define COMMAND_RESELECT			0x05
#define COMMAND_SELECT_ATN			0x06
#define COMMAND_SELECT_WO_ATN		0x07
#define COMMAND_SELECT_ATN_TFR		0x08
#define COMMAND_SELECT_WO_ATN_TFR	0x09
#define COMMAND_SETIDI				0x0F
#define COMMAND_RECEIVE_COMMAND		0x10
#define COMMAND_RECEIVE_DATA		0x11
#define COMMAND_RECEIVE_MSG_OUT		0x12
#define COMMAND_RECEIVE_UNSPC_OUT	0x13
#define COMMAND_SEND_STATUS			0x14
#define COMMAND_SEND_DATA			0x15
#define COMMAND_SEND_MSGIN			0x16
#define COMMAND_SEND_UNSPC			0x17
#define COMMAND_TRANSLATE_ADDRESS	0x18
#define COMMAND_TRSF_INFO			0x20
#define COMMAND_SBT					BIT(7)

#define STATUS_MASK					(0xf << 4)
#define STATUS_SUCCESS				(0x1 << 4)
#define STATUS_PAUSED				(0x2 << 4)
#define STATUS_ERROR				(0x4 << 4)
#define STATUS_SERVICE_REQUIRED		(0x8 << 4)

#define STATUS_CODE_MASK			0xf
#define STATUS_CODE_DISCONNECTED	0x5

#define STATUS_HAVE_MCI				BIT(3)
#define STATUS_CODE_INITIATOR		BIT(0)


#define MCI_MASK			0x7
#define MCI_DATA_OUT		0x0
#define MCI_DATA_IN			0x1
#define MCI_COMMAND			0x2
#define MCI_STATUS			0x3
#define MCI_UNSPC_INFO_OUT	0x4
#define MCI_UNSPC_INFO_IN	0x5
#define MCI_MESSAGE_OUT		0x6
#define MCI_MESSAGE_IN		0x7

struct wd33c93_scsi_priv {
	void *base;
	struct regmap *pcc8;
	struct regmap *pcc32;
};

static inline u8 wd33c93_scsi_read_status(struct wd33c93_scsi_priv *priv)
{
	return readb(priv->base);
}

static inline u8 wd33c93_scsi_read_reg(struct wd33c93_scsi_priv *priv, unsigned int which)
{
	writeb(which, priv->base);
	return readb(priv->base + 1);
}

static inline void wd33c93_scsi_write_reg(struct wd33c93_scsi_priv *priv, unsigned int which, u8 val)
{
	writeb(which, priv->base);
	return writeb(val, priv->base + 1);
}

static void wd33c93_scsi_dump_regs(struct wd33c93_scsi_priv *priv)
{
#ifdef DEBUG
#ifndef PIO
	uint intctrl, ctrlstat, dmastat, dataaddr, bytecount;

	regmap_read(priv->pcc8, MVME147_PCC8_DMAINTCTRL, &intctrl);
	regmap_read(priv->pcc8, MVME147_PCC8_DMACTRLSTAT, &ctrlstat);
	regmap_read(priv->pcc8, MVME147_PCC8_DMASTATUS, &dmastat);
	regmap_read(priv->pcc32, MVME147_PCC32_DATAADDR, &dataaddr);
	regmap_read(priv->pcc32, MVME147_PCC32_BYTECOUNT, &bytecount);

	printf("pcc dma int control: 0x%02x\n", intctrl);
	printf("pcc dma control stat: 0x%02x\n", ctrlstat);
	printf("pcc dma stat: 0x%02x\n", dmastat);
	printf("pcc data addr: 0x%08x\n", dataaddr);
	printf("pcc byte counter: 0x%08x\n", bytecount);
#endif

	debug("status: 0x%02x\n", wd33c93_scsi_read_status(priv));
#if 0
	for (int i = 0; i < 0x1b; i++) {
		if (i == REG_DATA)
			continue;

		printf("reg 0x%02x: 0x%02x\n", i, wd33c93_scsi_read_reg(priv, i));
	}
#endif
#endif
}

static inline void wd33c93_scsi_do_cmd(struct wd33c93_scsi_priv *priv, u8 cmd)
{
	printf("%s:%d - cmd: 0x%02x\n", __func__, __LINE__, cmd);

	/* Clear any left over interrupt */
	wd33c93_scsi_read_reg(priv, REG_SCSI_STATUS);

	do {
		u8 status = wd33c93_scsi_read_status(priv);
		u8 phase = wd33c93_scsi_read_reg(priv, REG_COMMAND_PHASE);

		if (!(status & STATUS_CIP))
			break;

		debug("waiting for command to process: status: 0x%02x, phase: 0x%02x\n",
				(unsigned int) status, (unsigned int) phase);
	} while(true);

	wd33c93_scsi_write_reg(priv, REG_COMMAND, cmd);
}

static int wd33c93_scsi_fill_fifo(struct wd33c93_scsi_priv *priv, void *data, size_t len)
{
	u8* bytes = data;

	for (int i = 0; i < len; i++) {
		u8 status;

		/* Wait for DBR to be set */
		do {
			status = wd33c93_scsi_read_status(priv);
		} while(!(status & STATUS_DBR));

		/* Send it */
		wd33c93_scsi_write_reg(priv, REG_DATA, bytes[i]);
	}

	return 0;
}

static int wd33c93_scsi_read_fifo(struct wd33c93_scsi_priv *priv, void *data, size_t len)
{
	int i;
	u8 b;
	u8* bytes = data;

	for (i = 0; i < len; i++) {
		u8 status;

		/* Wait for DBR to be set */
		do {
			status = wd33c93_scsi_read_status(priv);

			/* check for interrupt, errors,... */
			if (status & STATUS_INT) {
				u8 scsi_stat = wd33c93_scsi_read_reg(priv, REG_SCSI_STATUS);
				printf("mmm int 0x%02x - 0x%02x\n", status, scsi_stat);

				//if (scsi_stat == (STATUS_SERVICE_REQUIRED | STATUS_CODE_DISCONNECTED))
				//	return -EIO;
				//else if(scsi_stat == 0x41)
				//	goto done;
				return -EIO;
			}

			if (status & STATUS_PE){
				printf("parity error at byte %d\n", i);
				return -EIO;
			}

			if (status & STATUS_DBR)
				break;

			debug("waiting for fifo 0x%02x for byte %d\n",
					status, i);
		} while(true);

		/* Read from fifo */
		b = wd33c93_scsi_read_reg(priv, REG_DATA);
		bytes[i] = b;
		 //printf("byte %d 0x%02x\n", i, (unsigned int) b);
	}

	return i;
}

static void wd33c93_scsi_wait_int(struct wd33c93_scsi_priv *priv, u8* scsi_status)
{
	u8 status;

	do {
		status = wd33c93_scsi_read_status(priv);

		/* Wait for command to complete */
		if (status & STATUS_CIP)
			continue;

		if (status & STATUS_INT)
			break;

		if (!(status & STATUS_BUSY)) {
			debug("Busy status cleared without interrupt\n");
			break;
		}

		debug("waiting for int: status: 0x%02x\n",
				(unsigned int) status);

		/* yield */
		mdelay(1);
	} while(true);

	if (scsi_status)
		*scsi_status = wd33c93_scsi_read_reg(priv, REG_SCSI_STATUS);
}

static int wd33c9c_scsi_exec_select(struct wd33c93_scsi_priv *priv,
									struct scsi_cmd *req)
{
	u8 scsi_status;
	unsigned int target = req->target;

	wd33c93_scsi_write_reg(priv, REG_DESTINATION_ID, target);
	wd33c93_scsi_do_cmd(priv, COMMAND_SELECT_WO_ATN);
	wd33c93_scsi_wait_int(priv, &scsi_status);

	if (scsi_status == (STATUS_SUCCESS | STATUS_CODE_INITIATOR))
		debug("Became initiator with target %d\n", target);
	else {
		debug("Failed to select target %d\n", target);
		return -EIO;
	}

	return 0;
}

static int wd33c93_scsi_do_trsf_info(struct wd33c93_scsi_priv *priv, size_t len)
{
	wd33c93_scsi_write_reg(priv, REG_TRANSFER_COUNT0, len & 0xff);
	wd33c93_scsi_write_reg(priv, REG_TRANSFER_COUNT1, (len >> 8) & 0xff);
	wd33c93_scsi_write_reg(priv, REG_TRANSFER_COUNT2, (len >> 16) & 0xff);
	wd33c93_scsi_do_cmd(priv, COMMAND_TRSF_INFO);

	return 0;
}

static int wd33c93_scsi_exec_sendcmd(struct wd33c93_scsi_priv *priv,
									 struct scsi_cmd *req)
{
	debug("%s:%d - 0x%02x\n",__func__, __LINE__, (unsigned int) req->cmd[0]);

	/* Start a transfer */
	wd33c93_scsi_do_trsf_info(priv, req->cmdlen);

	/* Put command into fifo */
	wd33c93_scsi_fill_fifo(priv, req->cmd, req->cmdlen);

	return 0;
}

static int wd33c93_scsi_exec_datain(struct wd33c93_scsi_priv *priv,
									 struct scsi_cmd *req)
{
	int ret;
	unsigned int len = req->datalen;

	// INQUIRY only sends 255 bytes
	if (req->cmd[0] == SCSI_INQUIRY)
		len = min(len, (unsigned int) 255);

	wd33c93_scsi_do_trsf_info(priv, len);

	ret = wd33c93_scsi_read_fifo(priv, req->pdata, len);

	return ret;
}

static int wd33c93_scsi_exec_getstatus(struct wd33c93_scsi_priv *priv,
										struct scsi_cmd *req,
										u8* status)
{
	debug("%s:%d\n",__func__, __LINE__);
	int ret;

	/* Start a transfer for a single byte*/
	wd33c93_scsi_do_cmd(priv, COMMAND_SBT | COMMAND_TRSF_INFO);

	/* Get status from fifo */
	return wd33c93_scsi_read_fifo(priv, status, sizeof(*status));
}

static int wd33c93_scsi_exec_msgin(struct wd33c93_scsi_priv *priv,
									 struct scsi_cmd *req)
{
	//wd33c93_scsi_do_trsf_info(priv, len);
	wd33c93_scsi_do_cmd(priv, COMMAND_SBT | COMMAND_TRSF_INFO);

	return wd33c93_scsi_read_fifo(priv, req->msgin, 1);
}

static int wd33c93_scsi_exec(struct udevice *dev, struct scsi_cmd *req)
{
	struct wd33c93_scsi_priv *priv = dev_get_priv(dev);
	int ret;
	u8 scsi_status;
	u8 cmd_status = S_GOOD;

	debug("%s:%d\n",__func__, __LINE__);

	//wd33c93_scsi_read_status(priv);

	//wd33c93_scsi_dump_regs(priv);

	ret = wd33c9c_scsi_exec_select(priv, req);
	if (ret)
		goto err_eio;

	wd33c93_scsi_exec_sendcmd(priv, req);

	do {
		/* Wait for command to finish */
		wd33c93_scsi_wait_int(priv, &scsi_status);

		/* Work out what to do next */
		debug("scsi state after transfer info 0x%02x\n", scsi_status);
		switch(scsi_status & STATUS_MASK) {
		case STATUS_SUCCESS:
			debug("success\n");
			if (scsi_status & STATUS_HAVE_MCI) {
				switch (scsi_status & MCI_MASK){
				case MCI_DATA_IN:
					debug("Target requests data in\n");
					ret = wd33c93_scsi_exec_datain(priv, req);
					if (ret < 0)
						goto err_eio;
					break;
				case MCI_STATUS:
					debug("Target requests status\n");
					ret = wd33c93_scsi_exec_getstatus(priv, req, &cmd_status);
					break;
				case MCI_MESSAGE_IN:
					debug("Target requests message in\n");
					wd33c93_scsi_exec_msgin(priv, req);
					break;
				}
			}
			break;
		case STATUS_PAUSED:
			debug("transfer info paused\n");
			wd33c93_scsi_do_cmd(priv, COMMAND_NEGATE_ACK);
			break;
		case STATUS_ERROR:
			printf("An error happened..\n");
			if (scsi_status & STATUS_HAVE_MCI) {
				printf("unexpected information phase\n");
				switch (scsi_status & MCI_MASK) {
				}
			}
			goto err_eio;
		case STATUS_SERVICE_REQUIRED:
			switch(scsi_status & STATUS_CODE_MASK) {
			case STATUS_CODE_DISCONNECTED:
				goto done;
			}
			break;
		}
	} while(true);

done:
	if (cmd_status != S_GOOD) {
		printf("bad command status, 0x%02x\n", (unsigned int) cmd_status);
		return -EIO;
	}

	debug("exec completed\n");

	return 0;

err_eio:
	wd33c93_scsi_do_cmd(priv, COMMAND_ABORT);
	wd33c93_scsi_wait_int(priv, NULL);

	return -EIO;
}

static int wd33c93_scsi_bus_reset(struct udevice *dev)
{
	struct wd33c93_scsi_priv *priv = dev_get_priv(dev);
	u8 ownid, control;

	ownid = wd33c93_scsi_read_reg(priv, REG_OWNID);
	control = wd33c93_scsi_read_reg(priv, REG_CONTROL);

	debug("%s:%d, ownid: 0x%02x, control: 0x%02x\n",
			__func__, __LINE__, ownid, control);

	mvme147_scsi_reset(priv->pcc8, true);
	mvme147_scsi_reset(priv->pcc8, false);

	ownid |= 0x7;
	wd33c93_scsi_write_reg(priv, REG_OWNID, ownid);


	wd33c93_scsi_do_cmd(priv, COMMAND_RESET);
	wd33c93_scsi_wait_int(priv, NULL);

	/* Make sure busy is deasserted when selected a non-existing target */
	wd33c93_scsi_write_reg(priv, REG_TIMEOUTPERIOD, 0x1f);

	return 0;
}

static int wd33c93_scsi_of_to_plat(struct udevice *dev)
{
	struct wd33c93_scsi_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);

	return 0;
}

static int wd33c93_scsi_probe(struct udevice *dev)
{
	struct scsi_plat *scsi_plat = dev_get_uclass_plat(dev);
	struct wd33c93_scsi_priv *priv = dev_get_priv(dev);

	debug("%s:%d\n", __func__, __LINE__);

	scsi_plat->max_id = 7;
	scsi_plat->max_lun = 1;

	priv->pcc8 = syscon_regmap_lookup_by_phandle(dev, "pcc8");
	if (!priv->pcc8)
		return -ENODEV;

	priv->pcc32 = syscon_regmap_lookup_by_phandle(dev, "pcc32");
	if (!priv->pcc32)
		return -ENODEV;

	wd33c93_scsi_bus_reset(dev);

	return 0;
}

static int wd33c93_scsi_remove(struct udevice *dev)
{
	struct wd33c93_scsi_priv *priv = dev_get_priv(dev);

	return 0;
}

struct scsi_ops wd33c93_scsi_ops = {
	.exec		= wd33c93_scsi_exec,
	.bus_reset	= wd33c93_scsi_bus_reset,
};

static const struct udevice_id sanbox_scsi_ids[] = {
	{ .compatible = "wdc,wd33c93b" },
	{ }
};

U_BOOT_DRIVER(wd33c93_scsi) = {
	.name		= "wd33c93_scsi",
	.id		= UCLASS_SCSI,
	.ops		= &wd33c93_scsi_ops,
	.of_match	= sanbox_scsi_ids,
	.of_to_plat	= wd33c93_scsi_of_to_plat,
	.probe		= wd33c93_scsi_probe,
	.remove		= wd33c93_scsi_remove,
	.priv_auto	= sizeof(struct wd33c93_scsi_priv),
};
