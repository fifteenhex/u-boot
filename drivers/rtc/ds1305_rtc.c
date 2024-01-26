// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 *
 */

#include <clk.h>
#include <dm.h>
#include <rtc.h>
#include <spi.h>
#include <bcd.h>

#define DS1305_SPI_MAX_CLOCK	2000000

struct ds1305_rtc_priv {
	struct spi_slave *spi;
	struct clk xtal;
};

#define OF_SEC 0x1
#define OF_MIN 0x2
#define OF_HOUR 0x3
#define OF_DAY	0x4
#define OF_DATE	0x5
#define OF_MONTH 0x6
#define OF_YEAR 0x7

static int ds1305_rtc_get(struct udevice *dev, struct rtc_time *tm)
{
	int ret;

	u8 buf[8] = { 0 };

	memset(tm, 0, sizeof(*tm));

	ret = dm_spi_claim_bus(dev);
	if (ret)
		return ret;

	ret = dm_spi_xfer(dev, sizeof(buf) * 8, buf, buf, SPI_XFER_BEGIN | SPI_XFER_END);
	dm_spi_release_bus(dev);
	if (ret)
		return ret;

	tm->tm_sec = bcd2bin(buf[OF_SEC] & 0x7f);
	tm->tm_min = bcd2bin(buf[OF_MIN] & 0x7f);
	tm->tm_hour = bcd2bin(buf[OF_HOUR] & 0x1f);
	tm->tm_wday = bcd2bin((buf[OF_DAY] & 0x07) - 1);
	tm->tm_mday = bcd2bin(buf[OF_DATE] & 0x3f);
	tm->tm_mon  = bcd2bin(buf[OF_MONTH] & 0x3f);
	tm->tm_year = bcd2bin(buf[OF_YEAR]);

	if (tm->tm_year < 70)
		tm->tm_year += 2000;
	else
		tm->tm_year += 1900;

	return 0;
}

static int ds1305_rtc_set(struct udevice *dev, const struct rtc_time *tm)
{
	u8 ctrl[2] = { 0 };
	u8 buf[8] = { 0 };
	int ret;

	ret = dm_spi_claim_bus(dev);
	if (ret)
		return ret;

	ctrl[0] = 0x8f;
	dm_spi_xfer(dev, sizeof(ctrl) * 8, ctrl, NULL, SPI_XFER_BEGIN | SPI_XFER_END);

	buf[0] = 0x80;

	buf[OF_SEC] = bin2bcd(tm->tm_sec) & 0x7f;
	buf[OF_MIN] = bin2bcd(tm->tm_min) & 0x7f;
	buf[OF_HOUR] = bin2bcd(tm->tm_hour) & 0x1f;
	buf[OF_DAY] = bin2bcd(tm->tm_wday + 1) & 0x07;
	buf[OF_DATE] = bin2bcd(tm->tm_mday) & 0x3f;
	buf[OF_MONTH] = bin2bcd(tm->tm_mon) & 0x3f;
	buf[OF_YEAR] = bin2bcd(tm->tm_year % 100);

	dm_spi_xfer(dev, sizeof(buf) * 8, buf, NULL, SPI_XFER_BEGIN | SPI_XFER_END);

	dm_spi_release_bus(dev);

	return ret;
}

static int ds1305_rtc_reset(struct udevice *dev)
{
	struct rtc_time tm = { 0 };

	return ds1305_rtc_set(dev, &tm);
}

static int ds1305_rtc_probe(struct udevice *dev)
{
	struct ds1305_rtc_priv *priv = dev_get_priv(dev);
	int ret;

	priv->spi = dev_get_parent_priv(dev);
	if (!priv->spi->max_hz)
		priv->spi->max_hz = DS1305_SPI_MAX_CLOCK;
	priv->spi->mode = SPI_MODE_1;
	priv->spi->wordlen = 8;

	ret = clk_get_by_name(dev, "xtal", &priv->xtal);
	if (ret)
		return ret;

	return 0;
}

static const struct rtc_ops ds1305_rtc_ops = {
	.get = ds1305_rtc_get,
	.set = ds1305_rtc_set,
	.reset = ds1305_rtc_reset,
};

static const struct udevice_id ds1305_rtc_ids[] = {
	{ .compatible = "dallas,ds1306" },
	{ .compatible = "dallas,ds1305" },
	{ }
};

U_BOOT_DRIVER(rtc_ds1305) = {
	.name	= "rtc-ds1305",
	.id	= UCLASS_RTC,
	.probe	= ds1305_rtc_probe,
	.of_match = ds1305_rtc_ids,
	.ops	= &ds1305_rtc_ops,
	.priv_auto	= sizeof(struct ds1305_rtc_priv),
};
