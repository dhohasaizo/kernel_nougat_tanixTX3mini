#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/amlogic/efuse-amlogic.h>
#include <linux/of.h>
#include <linux/ctype.h>

#include "unifykey.h"
#include "amlkey_if.h"

#define UNIFYKEYS_MODULE_NAME    "aml_keys-t"
#define UNIFYKEYS_DRIVER_NAME	"aml_keys-t"
#define UNIFYKEYS_DEVICE_NAME    "unifykeys"
#define UNIFYKEYS_CLASS_NAME     "unifykeys"

#define KEY_NO_EXIST	0
#define KEY_BURNED		1

#define KEY_READ_PERMIT		10
#define KEY_READ_PROHIBIT	11

#define KEY_WRITE_MASK		(0x0f<<4)
#define KEY_WRITE_PERMIT	(10<<4)
#define KEY_WRITE_PROHIBIT	(11<<4)

#define SHA256_SUM_LEN	32
#define DBG_DUMP_DATA	(0)
static struct unifykey_dev_t *unifykey_devp;
static dev_t unifykey_devno;
static struct device *unifykey_device;

struct key_item_t *curkey = NULL;
typedef int (*key_unify_dev_init)(char *buf, unsigned int len);
typedef int (*key_unify_dev_uninit)(void);

static int init_flag;
static int module_init_flag;
static int lock_flag;

static int key_storage_init(char *buf, unsigned int len)
{
	/* fixme, todo. */
	return amlkey_init((uint8_t *)buf, len);

}

static int key_storage_size(char *keyname)
{
	return amlkey_size((uint8_t *)keyname);
}

static int key_storage_write(char *keyname, unsigned char *keydata,
		unsigned int datalen, int flag)
{
	int ret = 0;
	ssize_t writenLen = 0;
	/* fixme, todo write down this. */

	writenLen = amlkey_write((uint8_t *)keyname,
		(uint8_t *)keydata,
		datalen,
		flag);
	if (writenLen != datalen) {
		pr_err("Want to write %u bytes, but only %zd Bytes\n",
			datalen,
			writenLen);
		return -EINVAL;
	}

	return ret;
}

static int key_secure_read_hash(char *keyname, unsigned char *keydata,
		unsigned int datalen, unsigned int *reallen, int flag)
{
	int ret;

	ret = amlkey_hash_4_secure((uint8_t *)keyname, keydata);
	if (ret) {
		pr_err("Failed when gen hash for sec-key[%s],ret=%d\n",
				keyname,
				ret);
		return -EINVAL;
	}
	*reallen = SHA256_SUM_LEN;
	return ret;
}

static int key_storage_read(char *keyname, unsigned char *keydata,
		unsigned int datalen, unsigned int *reallen, int flag)
{
	int ret = 0;
	ssize_t readLen = 0;
	int encrypt, encrypt_dts;
	struct key_item_t *unifykey;
	/* fixme, todo */

	ret = amlkey_issecure((uint8_t *)keyname);
	if (ret) {
		pr_err("key[%s] can't read, is configured secured?\n", keyname);
		return -EINVAL;
	}

	/* make sure attr in storage & dts are the same! */
	encrypt = amlkey_isencrypt((uint8_t *)keyname);

	unifykey = unifykey_find_item_by_name(keyname);
	if (unifykey == NULL) {
		pr_err("%s:%d,%s key name is not exist\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	encrypt_dts = (unifykey->attr & KEY_UNIFY_ATTR_ENCRYPT_MASK) ?
		1:0;
	if (encrypt != encrypt_dts) {
		pr_err("key[%s] can't read, encrypt?\n", keyname);
		return -EINVAL;
	}

	readLen = amlkey_read((uint8_t *)keyname,
		(uint8_t *)keydata,
		datalen);
	if (readLen != datalen) {
		pr_err("key[%s], want read %u Bytes, but %zd bytes\n",
			keyname,
			datalen,
			readLen);
		return -EINVAL;
	}
	*reallen = readLen;
	return ret;
}

static int key_storage_query(char *keyname, unsigned int *keystate)
{
	int ret = 0;
	/*fixme, todo */
	ret = amlkey_isexsit((uint8_t *)keyname);
	if (ret)
		*keystate = 1;
	else
		*keystate = 0;

	return ret;
}

static int key_efuse_init(char *buf, unsigned int len)
{
	char ver;
	ver = unifykey_get_efuse_version();
	return 0;
}

static int key_efuse_write(char *keyname, unsigned char *keydata,
		unsigned int datalen)
{
#ifdef CONFIG_EFUSE
	char *title = keyname;
	struct efusekey_info info;

	if (efuse_getinfo(title, &info) < 0)
		return -EINVAL;

	if (efuse_user_attr_store(keyname,
		(const char *)keydata, (size_t)datalen) < 0) {
		pr_err("error: efuse write fail.\n");
		return -1;
	} else {
		pr_err("%s written done.\n", info.keyname);
	}
	return 0;
#else
	return -EINVAL;
#endif
}

static int key_efuse_read(char *keyname, unsigned char *keydata,
		unsigned int datalen, unsigned int *reallen)
{
#ifdef CONFIG_EFUSE
	char *title = keyname;
	struct efusekey_info info;
	int err = 0;
	char *buf;
	if (efuse_getinfo(title, &info) < 0)
		return -EINVAL;

	buf = kzalloc(info.size, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("%s:%d,kzalloc mem fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(buf, 0, info.size);

	err = efuse_user_attr_show(title, buf);
	if (err >= 0) {
		*reallen = info.size;
		if (datalen > info.size)
			datalen = info.size;
		memcpy(keydata, buf, datalen);
	}
	kfree(buf);
	return err;
#else
	return -EINVAL;
#endif
}

static int key_efuse_query(char *keyname, unsigned int *keystate)
{
	int err =  -EINVAL;
#ifdef CONFIG_EFUSE
	int i;
	char *title = keyname;
	struct efusekey_info info;
	char *buf;
	if (efuse_getinfo(title, &info) < 0)
		return -EINVAL;
	buf = kzalloc(info.size, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("%s:%d,kzalloc mem fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(buf, 0, info.size);
	err = efuse_user_attr_show(title, buf);
	*keystate = KEY_NO_EXIST;
	if (err > 0) {
		for (i = 0; i < info.size; i++) {
			if (buf[i] != 0) {
				*keystate = KEY_BURNED;
				break;
			}
		}
	}
	kfree(buf);
#endif
	return err;
}

int key_unify_init(char *buf, unsigned int len)
{
	int bakerr, err =  -EINVAL;

	char *dev_node[] = {"unkown", "efuse", "normal", "secure"};
	key_unify_dev_init dev_initfunc[] = {NULL, key_efuse_init,
		key_storage_init};
	int i, cnt;

	if (init_flag == 1) {
		pr_err(" %s() already inited!\n", __func__);
		return 0;
	}

	bakerr = 0;
	cnt = sizeof(dev_initfunc)/sizeof(dev_initfunc[0]);
	for (i = 0; i < cnt; i++) {
		if (dev_initfunc[i]) {
			err = dev_initfunc[i](buf, len);
			if (err < 0) {
				pr_err("%s:%d,%s device ini fail\n",
					__func__, __LINE__, dev_node[i]);
				bakerr = err;
			}
		}
	}
	/* fixme, */
	init_flag = 1;

	return bakerr;
}
EXPORT_SYMBOL(key_unify_init);

/* funtion name: key_unify_write
 * keyname : key name is ascii string
 * keydata : key data buf
 * datalen : key buf len
 * return  0: ok, -0x1fe: no space, other fail
 * */
int key_unify_write(char *keyname, unsigned char *keydata,
	unsigned int datalen)
{
	int err = 0;
	int attr;
	struct key_item_t *unifykey;

	unifykey = unifykey_find_item_by_name(keyname);
	if (unifykey == NULL) {
		pr_err("%s:%d,%s key name is not exist\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	if (unifykey_item_verify_check(unifykey)) {
		pr_err("%s:%d,%s key name is invalid\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	if (unifykey->permit & KEY_M_PERMIT_WRITE) {
		err = -EINVAL;

		switch (unifykey->dev) {
		case KEY_M_EFUSE:
			err = key_efuse_write(keyname, keydata, datalen);
			break;
		case KEY_M_SECURE:
		case KEY_M_NORMAL:
			attr = ((unifykey->dev == KEY_M_SECURE) ?
				KEY_UNIFY_ATTR_SECURE : 0);
			attr |= (unifykey->attr & KEY_UNIFY_ATTR_ENCRYPT_MASK) ?
				KEY_UNIFY_ATTR_ENCRYPT : 0;
			err = key_storage_write(keyname, keydata,
				datalen, attr);
			break;
		case KEY_M_UNKNOW_DEV:
		default:
			pr_err("%s:%d,%s key not know device\n",
				__func__, __LINE__, keyname);
			break;
		}
	}
	return err;
}
EXPORT_SYMBOL(key_unify_write);

/*
 *function name: key_unify_read
 * keyname : key name is ascii string
 * keydata : key data buf
 * datalen : key buf len
 * reallen : key real len
 * return : <0 fail, >=0 ok
 * */
int key_unify_read(char *keyname, unsigned char *keydata,
	unsigned int datalen, unsigned int *reallen)
{
	int err = 0;
	struct key_item_t *unifykey;

	if (!keydata) {
		pr_err("%s:%d, keydata is NULL\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	unifykey = unifykey_find_item_by_name(keyname);
	if (unifykey == NULL) {
		pr_err("%s:%d,%s key name is not exist\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	if (unifykey_item_verify_check(unifykey)) {
		pr_err("%s:%d,%s key name is invalid\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	if (unifykey->permit & KEY_M_PERMIT_READ) {
		err = -EINVAL;

		switch (unifykey->dev) {
		case KEY_M_EFUSE:
			err = key_efuse_read(keyname, keydata,
				datalen, reallen);
			break;
		case KEY_M_SECURE:
			err = key_secure_read_hash(keyname, keydata,
				datalen, reallen, 1);
			break;
		case KEY_M_NORMAL:
			err = key_storage_read(keyname, keydata,
				datalen, reallen, 0);
			break;
		case KEY_M_UNKNOW_DEV:
		default:
			pr_err("%s:%d,%s key not know device\n",
				__func__, __LINE__, keyname);
			break;
		}
	}
	return err;
}
EXPORT_SYMBOL(key_unify_read);


/*
 *function name: key_unify_size
 * keyname : key name is ascii string
 * reallen : key real len, only valid while return ok.
 * return : <0 fail, >=0 ok
 * */
int key_unify_size(char *keyname, unsigned int *reallen)
{
	int err = 0;
	struct key_item_t *unifykey;

	unifykey = unifykey_find_item_by_name(keyname);
	if (unifykey == NULL) {
		pr_err("%s:%d,%s key name is not exist\n",
			__func__,
			__LINE__,
			keyname);
		return -EINVAL;
	}

	if (unifykey_item_verify_check(unifykey)) {
		pr_err("%s:%d,%s key name is invalid\n",
			__func__,
			__LINE__,
			keyname);
		return -EINVAL;
	}

	if (unifykey->permit & KEY_M_PERMIT_READ) {
		switch (unifykey->dev) {
		case KEY_M_EFUSE:
		{
			struct efusekey_info info;
			if (efuse_getinfo(keyname, &info) < 0)
				return -EINVAL;
			*reallen = info.size;
			break;
		}
		case KEY_M_SECURE:
			*reallen = key_storage_size(keyname);
			break;
		case KEY_M_NORMAL:
			*reallen = key_storage_size(keyname);
			break;
		case KEY_M_UNKNOW_DEV:
		default:
			pr_err("%s:%d,%s key not know device\n",
				__func__, __LINE__, keyname);
			break;
		}
	}

	return err;
}
EXPORT_SYMBOL(key_unify_size);

/*
*    key_unify_query - query whether key was burned.
*    @keyname : key name will be queried.
*    @keystate: query state value,
*		0: key was NOT burned;
*		1: key was burned; others: reserved.
*    keypermit:
*    return: 0: successful; others: failed.
*/
int key_unify_query(char *keyname, unsigned int *keystate,
	unsigned int *keypermit)
{
	int err = 0;
	struct key_item_t *unifykey;

	unifykey = unifykey_find_item_by_name(keyname);
	if (unifykey == NULL) {
		pr_err("%s:%d,%s key name is not exist\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	if (unifykey_item_verify_check(unifykey)) {
		pr_err("%s:%d,%s key name is invalid\n",
			__func__, __LINE__, keyname);
		return -EINVAL;
	}
	if (unifykey->permit & KEY_M_PERMIT_READ) {
		err = -EINVAL;
		switch (unifykey->dev) {
		case KEY_M_EFUSE:
			err = key_efuse_query(keyname, keystate);
			*keypermit = KEY_READ_PERMIT;
			if (err >= 0) {
				if (*keystate == KEY_BURNED)
					*keypermit |= KEY_WRITE_PROHIBIT;
				else if (*keystate == KEY_NO_EXIST)
					*keypermit |= KEY_WRITE_PERMIT;
			}
			break;
		case KEY_M_SECURE:
			err = key_storage_query(keyname, keystate);
			*keypermit = KEY_READ_PROHIBIT;
			*keypermit |= KEY_WRITE_PERMIT;
			break;
		case KEY_M_NORMAL:
			err = key_storage_query(keyname, keystate);
			*keypermit = KEY_READ_PERMIT;
			*keypermit |= KEY_WRITE_PERMIT;
			break;
		case KEY_M_UNKNOW_DEV:
			pr_err("%s:%d,%s key not know device\n",
				__func__, __LINE__, keyname);
		default:
			break;
		}
	}
	return err;
}
EXPORT_SYMBOL(key_unify_query);


/*
 *function name: key_unify_encrypt
 * keyname : key name is ascii string
 * encrypt : key encrypt status.
 * return : <0 fail, >=0 ok
 * */
int key_unify_encrypt(char *keyname, unsigned int *encrypt)
{
	int ret = 0;
	struct key_item_t *unifykey;
	unsigned int keystate, keypermit;

	unifykey = unifykey_find_item_by_name(keyname);
	if (unifykey == NULL) {
		pr_err("%s:%d,%s key name is not exist\n",
			__func__,
			__LINE__,
			keyname);
		return -EINVAL;
	}

	if (unifykey_item_verify_check(unifykey)) {
		pr_err("%s:%d,%s key name is invalid\n",
			__func__,
			__LINE__,
			keyname);
		return -EINVAL;
	}

	/* check key burned or not */
	ret = key_unify_query(unifykey->name, &keystate, &keypermit);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	*encrypt = 0;
	/* if burned, ask bl31, else using dts */
	if (keystate) {
		ret = amlkey_isencrypt(unifykey->name);
		if (ret < 0)
			goto _out;
		*encrypt = ret;
	} else {
		if (unifykey->attr & KEY_UNIFY_ATTR_ENCRYPT_MASK)
			*encrypt = 1;
	}

_out:
	return ret;
}
EXPORT_SYMBOL(key_unify_encrypt);

int key_unify_uninit(void)
{
	int bakerr, err =  -EINVAL;
	int i, cnt;
	key_unify_dev_uninit dev_uninit[] = {NULL};
	bakerr = 0;
	cnt = sizeof(dev_uninit)/sizeof(dev_uninit[0]);
	for (i = 0; i < cnt; i++) {
		if (dev_uninit[i]) {
			err = dev_uninit[i]();
			if (err)
				bakerr = err;
		}
	}
	return bakerr;
}
EXPORT_SYMBOL(key_unify_uninit);

int key_unify_get_init_flag(void)
{
	return module_init_flag;
}
EXPORT_SYMBOL(key_unify_get_init_flag);

static int unifykey_open(struct inode *inode, struct file *file)
{
	struct unifykey_dev_t *devp;
	devp = container_of(inode->i_cdev, struct unifykey_dev_t, cdev);
	file->private_data = devp;
	return 0;
}

static int unifykey_release(struct inode *inode, struct file *file)
{
	struct unifykey_dev_t *devp;
	devp = file->private_data;
	return 0;
}

static loff_t unifykey_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	switch (whence) {
	case 0: /* SEEK_SET (start postion)*/
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = (loff_t)unifykey_count_key() - 1;
		newpos = newpos - off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	if (newpos >= (loff_t)unifykey_count_key())
		return -EINVAL;
	filp->f_pos = newpos;

	return newpos;

}

static long unifykey_unlocked_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case KEYUNIFY_ATTACH:
		{	struct key_item_t *appitem;
			char initvalue[KEY_UNIFY_NAME_LEN];
			int ret;
			appitem = (struct key_item_t *)arg;
			memcpy(initvalue, appitem->name, KEY_UNIFY_NAME_LEN);
			ret = key_unify_init(initvalue, KEY_UNIFY_NAME_LEN);
			if (ret < 0) {
				pr_err("%s:%d,key unify init fail\n",
					__func__, __LINE__);
				return ret;
			}
		}
		break;
	case KEYUNIFY_GET_INFO:
		{
			/* struct key_item_info_t*/
			unsigned int index, reallen;
			unsigned int keypermit, keystate;
			struct key_item_t *kkey;
			struct key_item_info_t *key_item_info;
			char *keyname;
			int ret;

			key_item_info = (struct key_item_info_t *)arg;
			index = key_item_info->id;
			keyname = key_item_info->name;
			if (strlen(keyname))
				kkey = unifykey_find_item_by_name(keyname);
			else
				kkey = unifykey_find_item_by_id(index);
			if (NULL == kkey) {
				pr_err("%s() %d, find name fail\n",
					__func__, __LINE__);
				return -ENOTTY;
			}
			pr_err("%s() %d, %d, %s\n", __func__,
				__LINE__, kkey->id, kkey->name);

			ret = key_unify_query(kkey->name,
					&keystate, &keypermit);
			if (ret < 0) {
				pr_err("%s() %d, get size fail\n",
					__func__, __LINE__);
				return -EFAULT;
			}
			key_item_info->permit = keypermit;
			key_item_info->flag = keystate;
			key_item_info->id = kkey->id;
			strncpy(key_item_info->name,
					kkey->name, strlen(kkey->name));
			ret = key_unify_size(kkey->name, &reallen);
			if (ret < 0) {
				pr_err("%s() %d, get size fail\n",
					__func__, __LINE__);
				return -EFAULT;
			}
			/* set key info */
			key_item_info->size = reallen;

			return 0;
		}
		break;
	default:
		pr_err("%s() %d\n", __func__, __LINE__);
		return -ENOTTY;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long unifykey_compat_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	return unifykey_unlocked_ioctl(file, cmd,
				(unsigned long) compat_ptr(arg));
}
#endif

static ssize_t unifykey_read(struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int ret;
	int id;
	unsigned int reallen;
	struct key_item_t *item;
	char *local_buf = NULL;

	id = (int)(*ppos);
	item = unifykey_find_item_by_id(id);
	if (!item) {
		ret =  -EINVAL;
		goto exit;
	}

	if (item->dev == KEY_M_SECURE)
		count = SHA256_SUM_LEN;
	local_buf = kzalloc(count, GFP_KERNEL);
	if (!local_buf) {
		pr_err(KERN_INFO "memory not enough,%s:%d\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	ret = key_unify_read(item->name, local_buf, count, &reallen);
	if (ret < 0)
		goto exit;
	if (count > reallen)
		count = reallen;
	if (copy_to_user((void *)buf, (void *)local_buf, count)) {
		ret =  -EFAULT;
		goto exit;
	}
	ret = count;
exit:
	kfree(local_buf);
	return ret;
}
#if (DBG_DUMP_DATA)
static void _dump_data(char *buffer, unsigned int len, char *name)
{
	int i;
	pr_err("%s(%s, %d)\n", __func__, name, len);
	for (i = 0; i < len; i++)
		pr_err("%02x ", buffer[i]);
	pr_err("\n");
	return;
}
#endif

static ssize_t unifykey_write(struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int ret;
	int id;
	struct key_item_t *item;
	char *local_buf;
	local_buf = kzalloc(count, GFP_KERNEL);
	if (!local_buf) {
		pr_err(KERN_INFO "memory not enough,%s:%d\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	id = (int)(*ppos);
	item = unifykey_find_item_by_id(id);
	if (!item) {
		ret =  -EINVAL;
		goto exit;
	}
	if (copy_from_user(local_buf, buf, count)) {
		ret =  -EFAULT;
		goto exit;
	}
#if (DBG_DUMP_DATA)
	_dump_data(local_buf, count, item->name);
#endif
	ret = key_unify_write(item->name, local_buf, count);
	if (ret < 0)
		goto exit;
	ret = count;
exit:
	kfree(local_buf);
	return ret;
}

static ssize_t version_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	n += sprintf(&buf[n], "version:1.0\n");
	buf[n] = 0;
	return n;
}

static ssize_t list_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	int index, key_cnt;
	struct key_item_t *unifykey;
	char *keydev[] = {"unkown", "efuse", "normal", "secure"};
	key_cnt = unifykey_count_key();
	n += sprintf(&buf[n], "%d keys installed\n", key_cnt);
	/* show all the keys*/
	for (index = 0; index < key_cnt; index++) {
		unifykey = unifykey_find_item_by_id(index);
		n += sprintf(&buf[n], "%02d: %s, %s, %x\n",
			index, unifykey->name,
			keydev[unifykey->dev], unifykey->permit);
	}
	buf[n] = 0;
	return n;
}


static ssize_t exist_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	int ret;
	unsigned int keystate, keypermit;
	char *state[] = {"none", "exist", "unknown"};

	if (curkey == NULL) {
		pr_err("please set key name 1st, %s:%d\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	/* using current key*/
	ret = key_unify_query(curkey->name, &keystate, &keypermit);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
			__func__, __LINE__);
		return 0;
	}

	if (keystate > 2)
		keystate = 2;

	n += sprintf(&buf[n], "%s\n", state[keystate]);
	buf[n] = 0;
	return n;
}

static ssize_t encrypt_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	int ret;
	unsigned int encrypt;
	char *state[] = {"false", "true", "error"};

	if (curkey == NULL) {
		pr_err("please set key name 1st, %s:%d\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	/* using current key*/
	ret = key_unify_encrypt(curkey->name, &encrypt);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
			__func__, __LINE__);
		encrypt = 2;
		goto _out;
	}

	if (encrypt > 1)
		encrypt = 1;
_out:
	n += sprintf(&buf[n], "%s\n", state[encrypt]);
	buf[n] = 0;
	return n;
}

static ssize_t size_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	int ret;
	unsigned int reallen;

	if (curkey == NULL) {
		pr_err("please set key name 1st, %s:%d\n",
			__func__,
			__LINE__);
		return -EINVAL;
	}
	/* using current key*/
	ret = key_unify_size(curkey->name, &reallen);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
			__func__,
			__LINE__);
		return 0;
	}

	n += sprintf(&buf[n], "%d\n", reallen);
	buf[n] = 0;

	return n;
}

static ssize_t name_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;

	if (curkey == NULL) {
		pr_err("please set cur key name,%s:%d\n", __func__, __LINE__);
		return 0;
	}

	n += sprintf(&buf[n], "%s\n", curkey->name);
	buf[n] = 0;
	return n;
}

static ssize_t name_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *name;
	int index, key_cnt;
	struct key_item_t *unifykey = NULL;
	size_t query_name_len;
	size_t reval;

	if (count >= KEY_UNIFY_NAME_LEN)
		count = KEY_UNIFY_NAME_LEN - 1;

	key_cnt = unifykey_count_key();
	name = kzalloc(count, GFP_KERNEL);
	if (!name) {
		pr_err("can't kzalloc mem,%s:%d\n",
			__func__,
			__LINE__);
		return -EINVAL;
	}
	/* check '\n' and del */
	if (buf[count - 1] == '\n')
		memcpy(name, buf, count-1);
	else
		memcpy(name, buf, count);

	query_name_len = strlen(name);
	pr_info("%s() %d, name %s, %d\n",
		__func__,
		__LINE__,
		name,
		(int)query_name_len);

	curkey = NULL;

	for (index = 0; index < key_cnt; index++) {
		unifykey = unifykey_find_item_by_id(index);
		if (unifykey != NULL) {
			if (!strncmp(name, unifykey->name,
				((strlen(unifykey->name) > query_name_len)
				? strlen(unifykey->name) : query_name_len))) {
				pr_info("%s() %d\n", __func__, __LINE__);
				curkey = unifykey;
				break;
			}
		}
	}
	reval = count;
	if (curkey == NULL) {
		/* count = 0; */
		reval++;
		pr_err("could not found key %s\n", name);
	}
	if (!IS_ERR_OR_NULL(name))
		kfree(name);

	return reval;
}

static ssize_t read_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	unsigned int keysize, reallen;
	int ret;
	unsigned char *keydata = NULL;

	if (curkey != NULL) {
		/* get key value */
		ret = key_unify_size(curkey->name, &keysize);
		if (ret < 0) {
			pr_err("%s() %d: get key size fail\n",
				__func__,
				__LINE__);
			goto _out;
		}
		if (keysize == 0) {
			pr_err("%s() %d: key %s may not burned yet!\n",
				__func__,
				__LINE__,
				curkey->name);
			goto _out;
		}
		if (curkey->dev == KEY_M_SECURE)
			keysize = SHA256_SUM_LEN;
		pr_err("name: %s, size %d\n", curkey->name, keysize);
		keydata = kzalloc(keysize, GFP_KERNEL);
		if (keydata == NULL) {
			pr_err("%s() %d: no enough memory\n",
				__func__, __LINE__);
			goto _out;
		}
		ret = key_unify_read(curkey->name,
			keydata,
			keysize,
			&reallen);
		if (ret < 0) {
			pr_err("%s() %d: get key size fail\n",
				__func__,
				__LINE__);
			goto _out;
		}
		/* fixme */
		memcpy(buf, keydata, keysize);
		n += keysize;
		/* n += sprintf(&buf[n], "%s\n", keydata); */
		buf[n] = 0;
	}
_out:
	if (!IS_ERR_OR_NULL(keydata))
		kfree(keydata);

	return n;
}

static ssize_t write_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int ret;
	unsigned char *keydata = NULL;
	size_t key_len = 0;

	if (curkey != NULL) {
		keydata = kzalloc(count, GFP_KERNEL);

		if (keydata == NULL) {
			pr_err("%s() %d: no enough memory\n",
				__func__, __LINE__);
			goto _out;
		}
		/* check string */
		for (key_len = 0; key_len < count; key_len++)
			if (!isascii(buf[key_len]))
				break;
		/* check '\n' and del while string */
		if ((key_len == count) && (buf[count - 1] == '\n')) {
			pr_err("%s()  is a string\n", __func__);
			memcpy(keydata, buf, count-1);
			key_len = count - 1;
		} else {
			memcpy(keydata, buf, count);
			key_len = count;
		}
	#if (DBG_DUMP_DATA)
		_dump_data(keydata, key_len, curkey->name);
	#endif
		ret = key_unify_write(curkey->name, keydata, key_len);
		if (ret < 0) {
			pr_err("%s() %d: key write fail\n",
				__func__, __LINE__);
			goto _out;
		}

	}
_out:
	if (!IS_ERR_OR_NULL(keydata))
		kfree(keydata);

	return count;
}

static ssize_t attach_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	/* show attach status. */
	n += sprintf(&buf[n], "%s\n", (init_flag == 0 ? "no":"yes"));
	buf[n] = 0;

	return n;
}

static ssize_t attach_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	/* todo, do attach */
	key_unify_init(NULL, KEY_UNIFY_NAME_LEN);
	return count;
}

static ssize_t lock_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t n = 0;
	/* show lock state. */
	n += sprintf(&buf[n], "%d\n", lock_flag);
	buf[n] = 0;
	pr_info("%s\n", (lock_flag == 1 ? "locked":"unlocked"));

	return n;
}

static ssize_t lock_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf,
		size_t count)
{
	unsigned int state, len;

	/* check '\n' and del */
	if (buf[count - 1] == '\n')
		len = count - 1;
	else
		len = count;

	if (!strncmp(buf, "1", len))
		state = 1;
	else if (!strncmp(buf, "0", len))
		state = 0;
	else {
		pr_info("unifykey lock set fail\n");
		goto _out;
	}

	lock_flag = state;
_out:
	pr_info("unifykey is %s\n", (lock_flag == 1 ? "locked":"unlocked"));
	return count;
}


static const char *unifykeys_help_str = {
"Usage:\n"
"echo 1 > attach //initialise unifykeys\n"
"cat lock //get lock status\n"
"//if lock=1,you must wait, lock=0, you can go on\n"
"//so you must set unifykey lock first, then do other operations\n"
"echo 1 > lock //1:locked, 0:unlocked\n"
"echo \"key name\" > name //set current key name->\"key name\"\n"
"cat name //get current key name\n"
"echo \"key value\" > write //set current key value->\"key value\"\n"
"cat read //get current key value\n"
"cat size //get current key value\n"
"cat exist //get whether current key is exist or not\n"
"cat list //get all unifykeys\n"
"cat version //get unifykeys versions\n"
"//at last, you must set lock=0 when you has done all operations\n"
"echo 0 > lock //set unlock\n"
};

static ssize_t help_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t n = 0;
	n += sprintf(buf, "%s", unifykeys_help_str);
	buf[n] = 0;
	return n;
}

static const struct of_device_id unifykeys_dt_match[];

static char *get_unifykeys_drv_data(struct platform_device *pdev)
{
	char *key_dev = NULL;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(unifykeys_dt_match,
				pdev->dev.of_node);
		if (match)
			key_dev = (char *)match->data;
	}

	return key_dev;
}

static const struct file_operations unifykey_fops = {
	.owner      = THIS_MODULE,
	.llseek     = unifykey_llseek,
	.open       = unifykey_open,
	.release    = unifykey_release,
	.read       = unifykey_read,
	.write      = unifykey_write,
	.unlocked_ioctl  = unifykey_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= unifykey_compat_ioctl,
#endif
};


#define KEY_READ_ATTR  (S_IRUSR|S_IRGRP)
#define KEY_WRITE_ATTR (S_IWUSR|S_IWGRP)
#define KEY_RW_ATTR	(KEY_READ_ATTR | KEY_WRITE_ATTR)

static struct class_attribute unifykey_class_attrs[] = {
	__ATTR_RO(version),
	__ATTR_RO(list),
	__ATTR_RO(exist),
	__ATTR_RO(encrypt),
	__ATTR_RO(size),
	__ATTR_RO(help),
	__ATTR(name, KEY_RW_ATTR, name_show, name_store),
	__ATTR(write, KEY_WRITE_ATTR, NULL, write_store),
	__ATTR(read, KEY_READ_ATTR, read_show, NULL),
	__ATTR(attach, KEY_RW_ATTR, attach_show, attach_store),
	__ATTR(lock, KEY_RW_ATTR, lock_show, lock_store),
	__ATTR_NULL
};

static struct class unifykey_class = {
	.name = UNIFYKEYS_CLASS_NAME,
	.class_attrs = unifykey_class_attrs,
};

static int aml_unifykeys_probe(struct platform_device *pdev)
{
	int ret =  -1;
	struct device *devp;

	if (pdev->dev.of_node)
		ret = unifykey_dt_create(pdev);

	ret = alloc_chrdev_region(&unifykey_devno,
		0, 1,
		UNIFYKEYS_DEVICE_NAME);
	if (ret < 0) {
		pr_err("unifykey: failed to allocate major number\n ");
		ret = -ENODEV;
		goto out;
	}

	pr_info("%s:%d=============unifykey_devno:%x\n",
		__func__,
		__LINE__,
		unifykey_devno);

	ret = class_register(&unifykey_class);
	if (ret)
		goto error1;

	unifykey_devp = kzalloc(sizeof(struct unifykey_dev_t), GFP_KERNEL);
	if (!unifykey_devp) {
		pr_err("unifykey: failed to allocate memory\n ");
		ret = -ENOMEM;
		goto error2;
	}

	/* connect the file operations with cdev */
	cdev_init(&unifykey_devp->cdev, &unifykey_fops);
	unifykey_devp->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&unifykey_devp->cdev, unifykey_devno, 1);
	if (ret) {
		pr_err("unifykey: failed to add device\n");
		goto error3;
	}

	devp = device_create(&unifykey_class, NULL,
			unifykey_devno, NULL, UNIFYKEYS_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("unifykey: failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}
	devp->platform_data = get_unifykeys_drv_data(pdev);

	unifykey_device = devp;

	pr_info(KERN_INFO "unifykey: device %s created ok\n",
		UNIFYKEYS_DEVICE_NAME);

	return 0;
error4:
	cdev_del(&unifykey_devp->cdev);
error3:
	kfree(unifykey_devp);
error2:
	class_unregister(&unifykey_class);
error1:
	unregister_chrdev_region(unifykey_devno, 1);
out:
	return ret;
}

static int aml_unifykeys_remove(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		unifykey_dt_release(pdev);

	unregister_chrdev_region(unifykey_devno, 1);
	device_destroy(&unifykey_class, unifykey_devno);
	cdev_del(&unifykey_devp->cdev);
	kfree(unifykey_devp);
	class_unregister(&unifykey_class);

	return 0;
}

static const struct of_device_id unifykeys_dt_match[] = {
	{	.compatible = "amlogic, unifykey",
		.data		= NULL,
	},
	{},
};

static struct platform_driver unifykey_platform_driver = {
	.probe = aml_unifykeys_probe,
	.remove = aml_unifykeys_remove,
	.driver = {
		.name = UNIFYKEYS_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = unifykeys_dt_match,
	},
};

static int __init aml_unifykeys_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&unifykey_platform_driver);
	if (ret != 0) {
		pr_err("failed to register unifykey driver, error %d\n", ret);
		return -ENODEV;
	}
	module_init_flag = 1;
	pr_info(KERN_INFO "%s done!\n", __func__);

	return ret;
}

static void __exit aml_unifykeys_exit(void)
{
	key_unify_uninit();
	platform_driver_unregister(&unifykey_platform_driver);
}

module_init(aml_unifykeys_init);
module_exit(aml_unifykeys_exit);

MODULE_DESCRIPTION("Amlogic unifykeys management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bl zhou<benlong.zhou@amlogic.com>");


