#include <stdio.h>
#include <string.h>
#define G_LOG_DOMAIN "cusbfx2"
#include <glib.h>
#include <libusb.h>

#include "cusbfx2.h"


#define CUSBFX2_TRANSFER_TIMEOUT 1000


/* private */
struct cusbfx2_handle {
	libusb_device_handle *usb_handle;
};

struct cusbfx2_transfer {
	const gchar *name;
	GSList *usb_transfers;
	cusbfx2_transfer_cb_fn callback;
	gpointer user_data;
};

typedef struct {
	guint8 bLength;
	guint8 bDescriptorType;
	guint16 wData[255];
} string_descriptor;

static gchar *
cusbfx2_get_manufacturer(libusb_device_handle *handle)
{
	string_descriptor *desc;
	guint8 buf[256], result[256];
	gint i;
	struct libusb_device_descriptor *device = libusb_get_device_descriptor(libusb_get_device(handle));
	libusb_control_transfer(handle, LIBUSB_RECIPIENT_DEVICE|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_ENDPOINT_IN,
							LIBUSB_REQUEST_GET_DESCRIPTOR,
							(LIBUSB_DT_STRING << 8) | device->iManufacturer,
							0x0000, buf, sizeof(buf) - 1, 1000);
	desc = (string_descriptor *)buf;

	for (i = 0; i < (desc->bLength - 2) / 2; ++i) {
		result[i] = desc->wData[i] & 0xFF;
	}
	result[i] = '\0';
	return g_strdup(result);
}

/**
 * @param id
 * @return デバイスが見付かった場合はデバイスへのハンドル. それ以外では NULL.
 */
static libusb_device_handle *
cusbfx2_find_open(guint8 id)
{
	libusb_device *found = NULL;
	libusb_device **devices;
	libusb_device *device;
	libusb_device_handle *handle = NULL;
	gint ndevices;
	gint i = 0;

	ndevices = libusb_get_device_list(&devices);
	if (ndevices < 0) {
		g_message("cusbfx2_find_open: no devices found (%d)", ndevices);
		return NULL;
	}

	/* 接続されている全てのUSBデバイスを調べる */
	while ((device = devices[i++])) {
		struct libusb_device_descriptor *desc = libusb_get_device_descriptor(device);

		g_debug("cusbfx2_find_open: devices[%d] idVendor=0x%04x, idProduct=0x%04x, bcdDevice=0x%04x",
				i - 1, desc->idVendor, desc->idProduct, desc->bcdDevice);

		/* bcdDevice = FX2LP:0xA0nn FX2:0x00nn */
		if (((id == 0) && ((desc->bcdDevice & 0x0F00) == 0x0000)) ||
			(desc->bcdDevice == 0xFF00 + id)) {
			if ((desc->idVendor == 0x04B4) &&
				((desc->idProduct == 0x8613) || (desc->idProduct == 0x1004))) {
				g_message("cusbfx2_find_open: found cusbfx2 idVendor=0x%04x, idProduct=0x%04x, bcdDevice=0x%04x",
						  desc->idVendor, desc->idProduct, desc->bcdDevice);
				found = device;
				break;
			}
		}
	}

	/* デバイスをオープンする */
	if (found) {
		handle = libusb_open(found);
		if (!handle) {
			g_critical("cusbfx2_find_open: libusb_open failed");
		} else {
			gint ret = libusb_claim_interface(handle, 0);
			if (ret) {
				g_critical("cusbfx2_find_open: libusb_claim_interface failed (%d)", ret);
			}
		}
	}

	/* デバイスリストと見つかったデバイス以外の参照を解放する */
	libusb_free_device_list(devices, 1);

	return handle;
}


static gint
cusbfx2_load_firmware(libusb_device_handle *dev, guint8 id, guint8 *firmware, const gchar *firmware_id)
{
	static const guint8 descriptor_pattern[] = { 0xB4, 0x04, 0x04, 0x10, 0x00, 0x00 };
	guint8 *patch_ptr = NULL;
	guint16 len, ofs;

	/* 8051 を停止 */
	guint8 stop_buf[] = { 1 };
	libusb_control_transfer(dev, LIBUSB_RECIPIENT_DEVICE|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_ENDPOINT_OUT,
							0xA0, 0xE600, 0x0000, stop_buf, 1, 1000);

	/* skip header ? */
	firmware += 8;

	for (;;) {
		gint i;
		len = (firmware[0] << 8) | firmware[1];
		ofs = (firmware[2] << 8) | firmware[3];
		firmware += 4;

		/* ファームウェアの Device Descript VID=04B4 PID=1004 BCD=0000 を見付け出し、
		   idに適合するBCD(FFxx)に書き換える */
		for (i = 0; i < (len & 0x7FFF); ++i) {
			guint8 *p = firmware + i;
			if (!memcmp(p, descriptor_pattern, G_N_ELEMENTS(descriptor_pattern))) {
				/* パターンが見付かったので、BCDを書き換える */
				patch_ptr = p + 4;
				patch_ptr[0] = id;
				patch_ptr[1] = 0xFF;
			}
		}

		/* ベンダリクエスト'A0'を使用して8051に書き込む */
		libusb_control_transfer(dev, LIBUSB_RECIPIENT_DEVICE|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_ENDPOINT_OUT,
								0xA0, ofs, 0x0000, firmware, len & 0x7FFF, 1000);

		if (len & 0x8000)
			break;				/* 最終(リセット) */

		firmware += len;
	}

	/* パッチを当てていた場合は元に戻す */
	if (patch_ptr) {
		patch_ptr[0] = patch_ptr[1] = 0x00;
	}

	return TRUE;
}

/* Exposed functions */

/**
 * Initialize cusbfx2.
 *
 * This function must be called before calling any other cusbfx2 functions.
 *
 * @return 0 on success, non-zero on error
 */
gint
cusbfx2_init(void)
{
	return libusb_init();
}


/**
 * Deinitialize cusbfx2.
 *
 * Should be called after closing all open devices and before your application terminates.
 */
void
cusbfx2_exit(void)
{
	libusb_exit();
}


/**
 * ID が @a id である FX2 をオープンします。
 *
 * @a firmware が NULL であれば、ファームウェアのロードを行いません。
 *
 * @param[in]	id	FX2のID
 * @param[in]	firmware	ファームウェア
 * @param[in]	firmware_id	ファームウェアのID
 * @param[in]	is_force_load	TRUEなら強制的にファームウェアをロードする
 * @return handle
 */
cusbfx2_handle *
cusbfx2_open(guint8 id, guint8 *firmware, const gchar *firmware_id, gboolean is_force_load)
{
#define CUSBFX2_REOPEN_RETRY_MAX 10
#define CUSBFX2_REOPEN_RETRY_WAIT (500 * 1000)

	libusb_device_handle *usb_handle;
	cusbfx2_handle *h = NULL;
	gint ret;

	usb_handle = cusbfx2_find_open(id);
	if (!usb_handle) {
		g_critical("cusbfx2_open: cusbfx2_find_open(id=%d) failed", id);
		return NULL;
	}

	if (firmware) {
		gint i;
		gchar *manufacturer;

		manufacturer = cusbfx2_get_manufacturer(usb_handle);
		if (!is_force_load && !strcmp(manufacturer, firmware_id)) {
			g_message("Firmware <%s> already loaded", manufacturer);
			g_free(manufacturer);
		} else {
			g_free(manufacturer);
			g_message("cusbfx2_open: loading firmware");
			cusbfx2_load_firmware(usb_handle, id, firmware, firmware_id);

			/* 再起動後のファームウェアに接続 */
			g_message("cusbfx2_open: reload device");
			ret = libusb_release_interface(usb_handle, 0);
			if (ret) {
				g_critical("cusbfx2_open: libusb_release_interface failed (%d)", ret);
			}

			libusb_close(usb_handle);

			for (i = 0; i < CUSBFX2_REOPEN_RETRY_MAX; ++i) {
				usb_handle = cusbfx2_find_open(id);
				if (usb_handle)
					break;
				g_usleep(CUSBFX2_REOPEN_RETRY_WAIT);
			}

			if (!usb_handle) {
				g_critical("cusbfx2_open: cusbfx2_find_open(id=%d) failed", id);
				return NULL;
			}
		}
	}

	h = g_malloc0(sizeof(*h));
	h->usb_handle = usb_handle;

	return h;
}

void
cusbfx2_close(cusbfx2_handle *h)
{
	if (!h)
		return;

	g_assert(h->usb_handle);

	if (h->usb_handle) {
		gint ret = libusb_release_interface(h->usb_handle, 0);
		if (ret) {
			g_critical("cusbfx2_close: libusb_release_interface failed (%d)", ret);
		}

		libusb_close(h->usb_handle);
	}

	g_free(h);
}


gint
cusbfx2_bulk_transfer(cusbfx2_handle *h, guint8 endpoint, guint8 *data, gint length)
{
	gint transferred;

	g_assert(h);

	return libusb_bulk_transfer(h->usb_handle, endpoint, data, length, &transferred, CUSBFX2_TRANSFER_TIMEOUT);
}


static void
cusbfx2_transfer_callback(struct libusb_transfer *usb_transfer)
{
	cusbfx2_transfer *transfer = (cusbfx2_transfer *)usb_transfer->user_data;
	gint ret;

	g_assert(transfer);

	switch (usb_transfer->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		if (usb_transfer->actual_length != usb_transfer->length) {
			g_warning("cusbfx2_transfer_callback: [%s] length mismatch req=%d, actual=%d",
					  transfer->name, usb_transfer->length, usb_transfer->actual_length);
		}

		if (transfer->callback) {
			transfer->callback(usb_transfer->buffer, usb_transfer->actual_length, transfer->user_data);
		}
		break;

	case LIBUSB_TRANSFER_ERROR:
		g_warning("cusbfx2_transfer_callback: [%s] TRANSFER_ERROR", transfer->name);
		break;

	case LIBUSB_TRANSFER_TIMED_OUT:
		g_warning("cusbfx2_transfer_callback: [%s] TRANSFER_TIMED_OUT", transfer->name);
		break;

	case LIBUSB_TRANSFER_CANCELLED:
		g_warning("cusbfx2_transfer_callback: [%s] TRANSFER_CANCELLED", transfer->name);
		break;

	default:
		g_assert_not_reached();
	}

	ret = libusb_submit_transfer(usb_transfer);
	if (ret) {
		g_critical("cusbfx2_transfer_callback: [%s] libusb_submit_transfer failed (%d)", transfer->name, ret);
	}
}


/**
 * Setup asynchronus bulk transfer.
 *
 * @param size 転送サイズ
 * @param nqueues キュー数
 */
cusbfx2_transfer *
cusbfx2_init_bulk_transfer(cusbfx2_handle *h, const gchar *name, guint8 endpoint,
						   gint length, gint nqueues,
						   cusbfx2_transfer_cb_fn callback, gpointer user_data)
{
	gint i;
	cusbfx2_transfer *transfer;

	g_assert(h);
	g_assert(h->usb_handle);
	g_assert(length > 0);
	g_assert(nqueues > 0);
	g_assert(callback);

	transfer = g_malloc0(sizeof(*transfer));
	transfer->name = name;
	transfer->usb_transfers = NULL;
	transfer->callback = callback;
	transfer->user_data = user_data;

	for (i = 0; i < nqueues; ++i) {
		struct libusb_transfer *usb_transfer;
		gpointer buffer;
		gint ret;

		usb_transfer = libusb_alloc_transfer();
		if (!usb_transfer) {
			g_critical("cusbfx2_init_bulk_transfer: libusb_alloc_transfer failed");
			continue;
		}

		transfer->usb_transfers = g_slist_append(transfer->usb_transfers, usb_transfer);

		buffer = g_malloc(length);
		libusb_fill_bulk_transfer(usb_transfer, h->usb_handle, endpoint, buffer, length,
								  cusbfx2_transfer_callback, transfer, CUSBFX2_TRANSFER_TIMEOUT);

		ret = libusb_submit_transfer(usb_transfer);
		if (ret) {
			g_critical("cusbfx2_init_bulk_transfer: libusb_submit_transfer failed (%d)", ret);
		}
	}

	return transfer;
}

void
cusbfx2_start_transfer(cusbfx2_transfer *transfer)
{
	GSList *p;
	for (p = transfer->usb_transfers; p; p = g_slist_next(p)) {
		struct libusb_transfer *usb_transfer = (struct libusb_transfer *)p->data;
		gint ret = libusb_submit_transfer(usb_transfer);
		if (ret) {
			g_critical("cusbfx2_start_transfer: libusb_submit_transfer failed (%d)", ret);
		}
	}
}

void
cusbfx2_cancel_transfer_sync(cusbfx2_transfer *transfer)
{
	GSList *p;
	for (p = transfer->usb_transfers; p; p = g_slist_next(p)) {
		struct libusb_transfer *usb_transfer = (struct libusb_transfer *)p->data;
		gint ret = libusb_cancel_transfer_sync(usb_transfer);
		if (ret) {
			g_warning("cusbfx2_cancel_transfer_sync: libusb_cancel_transfer_sync failed (%d)", ret);
		}
	}
}

void
cusbfx2_free_transfer(cusbfx2_transfer *transfer)
{
	GSList *p;

	if (!transfer)
		return;

	for (p = transfer->usb_transfers; p; p = g_slist_next(p)) {
		struct libusb_transfer *usb_transfer = (struct libusb_transfer *)p->data;
		gint ret = libusb_cancel_transfer_sync(usb_transfer);
		if (ret) {
			g_warning("cusbfx2_cancel_transfer_sync: libusb_cancel_transfer_sync failed (%d)", ret);
		}
		g_free(usb_transfer->buffer);
		libusb_free_transfer(usb_transfer);
	}
	g_slist_free(transfer->usb_transfers);
}


/**
 * Handle any pending events in blocking mode with a sensible timeout.
 *
 * This timeout is currently hardcoded at 2 seconds but we may change this
 * if we decide other values are more sensible. For finer control over whether
 * this function is blocking or non-blocking, or the maximum timeout,
 * use libusb_poll_timeout() instead.
 *
 * @return 0 on success, non-zero on error
 */
int
cusbfx2_poll(void)
{
	return libusb_poll();
}
