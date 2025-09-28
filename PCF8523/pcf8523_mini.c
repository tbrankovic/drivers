#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/string.h>

#define PCF8523_ADDR	0x68

#define REG_SECONDS 0x03
#define REG_MINUTES 0x04
#define REG_HOURS   0x05
#define REG_DAYS    0x06
#define REG_WEEKDAY 0x07
#define REG_MONTHS  0x08
#define REG_YEARS   0x09


struct pcf8523_mini {
	struct i2c_client *client;
};

// helper functions
static inline u8 dec2bcd(int d) { return (u8)(((d / 10) << 4) | (d % 10)); }
static inline int bcd2dec(u8 b)  { return ((b >> 4) * 10) + (b & 0x0F); }

static ssize_t mini_time_show(struct device *dev, struct device_attribute *attr, char *buf) {

	struct i2c_client *client = to_i2c_client(dev);
	u8 start = REG_SECONDS;
	u8 data[7];
	int ret;

	ret = i2c_smbus_write_byte_data(client, start, 0);
	if (ret < 0)
		return ret; // i2c read error

	// read the 7 bytes of data starting at 0x03
	ret = i2c_smbus_read_i2c_block_data(client, REG_SECONDS, sizeof(data), data);
	if (ret < 0)
		return ret; // i2c read error

	int sec  = bcd2dec(data[0] & 0x7F);
	int min  = bcd2dec(data[1] & 0x7F);
	int hour = bcd2dec(data[2] & 0x3F);
	int day  = bcd2dec(data[3] & 0x3F);
	int wk   = data[4] & 0x07;             // not used in print, but read
	int mon  = bcd2dec(data[5] & 0x1F);
	int yr   = bcd2dec(data[6]);

	return scnprintf(buf, PAGE_SIZE, "20%02d-%02d-%02d %02d:%02d:%02d (wk=%d)\n",
			 yr, mon, day, hour, min, sec, wk);
}

// accepts "YYYY-MM-DD hh:mm:ss
static ssize_t mini_time_store(	struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int Y, M, D, h, m, s;
	u8 out[7];
	int ret;

	if (sscanf(buf, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6)
		return -EINVAL;

	if (Y < 2000 || Y > 2099)
		return -EINVAL;

	// convert deimal values to BCD
	out[0] = dec2bcd(s) & 0x7F;
	out[1] = dec2bcd(m) & 0x7F;
	out[2] = dec2bcd(h) & 0x3F;
	out[3] = dec2bcd(D) & 0x3F;
	out[4] = 0; // weekday is undefined in this case
	out[5] = dec2bcd(M & 0x1F);
	out[6] = dec2bcd(Y % 100); // since we are only considering 2000-2099 right now

	ret = i2c_smbus_write_i2c_block_data(client, REG_SECONDS, sizeof(out), out);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(mini_time);

static struct attribute *pcf8523_attrs[] = {
	&dev_attr_mini_time.attr,
	NULL,
};
ATTRIBUTE_GROUPS(pcf8523);

// called when the driver successfully binds to the device
static int pcf8523_mini_probe(struct i2c_client *client) {

	int ret;

	// warn if accidentally bound to wrong address
	if (client->addr != PCF8523_ADDR)
		dev_warn(&client->dev, "unexpected I2C address 0x%02x\n", client->addr);

	ret = sysfs_create_groups(&client->dev.kobj, pcf8523_groups);
	if (ret)
		return ret;

	dev_info(&client->dev, "pcf8523_mini bound at 0x%02x\n", client->addr);
	return 0;
}

// called when the driver is unloaded or the device is removed
static void pcf8523_mini_remove(struct i2c_client *client) {

	sysfs_remove_groups(&client->dev.kobj, pcf8523_groups);
	dev_info(&client->dev, "pcf8523_mini removed\n");
}

// match tables so the kernel knows when to load the driver
static const struct i2c_device_id pcf8523_mini_id[] = {
	{ "pcf8523-mini", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf8523_mini_id);

static const struct of_device_id pcf8523_mini_of_match[] = {
	{ .compatible = "my,pcf8523-mini" },
	{ }
};
MODULE_DEVICE_TABLE(of, pcf8523_mini_of_match);

// i2 driver object
static struct i2c_driver pcf8523_mini_driver = {
	.driver = {
		.name = "pcf8523_mini",
		.of_match_table = pcf8523_mini_of_match,
	},
	.probe	  = pcf8523_mini_probe,
	.remove	  = pcf8523_mini_remove,
	.id_table = pcf8523_mini_id,
};

module_i2c_driver(pcf8523_mini_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tristan");
MODULE_DESCRIPTION("Tiny educational PCF8523 I2C driver exposing sysfs time");

