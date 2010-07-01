/*******************************************************
 Windows HID simplification

 Alan Ott
 Signal 11 Software

 8/22/2009
 Linux Version - 6/2/2009

 Copyright 2009, All Rights Reserved.
 
 This software may be used by anyone for any reason so
 long as this copyright notice remains intact.
********************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libudev.h"

#include "hidapi.h"

struct Device {
	int valid;
	int device_handle;
	int blocking;
};


#define MAX_DEVICES 64
static struct Device devices[MAX_DEVICES];
static int devices_initialized = 0;

static void register_error(struct Device *device, const char *op)
{

}

/* Get an attribute value from a udev_device and return it as a whar_t
   string. The returned string must be freed with free() when done.*/
static wchar_t *copy_udev_string(struct udev_device *dev, const char *udev_name)
{
	const char *str;
	wchar_t *ret = NULL;
	str = udev_device_get_sysattr_value(dev, udev_name);
	if (str) {
		/* Convert the string from UTF-8 to wchar_t */
		size_t wlen = mbstowcs(NULL, str, 0);
		ret = calloc(wlen+1, sizeof(wchar_t));
		mbstowcs(ret, str, wlen+1);
		ret[wlen] = 0x0000;
	}
	
	return ret;
}

struct hid_device  HID_API_EXPORT *hid_enumerate(unsigned short vendor_id, unsigned short product_id)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	
	struct hid_device *root = NULL; // return object
	struct hid_device *cur_dev = NULL;
	
	setlocale(LC_ALL,"");

	/* Create the udev object */
	udev = udev_new();
	if (!udev) {
		printf("Can't create udev\n");
		return NULL;
	}

	/* Create a list of the devices in the 'hidraw' subsystem. */
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "hidraw");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);
	/* For each item, see if it matches the vid/pid, and if so
	   create a udev_device record for it */
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *sysfs_path;
		const char *dev_path;
		const char *str;
		unsigned short dev_vid;
		unsigned short dev_pid;
		
		/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it */
		sysfs_path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, sysfs_path);
		dev_path = udev_device_get_devnode(dev);
		
		/* The device pointed to by dev contains information about
		   the hidraw device. In order to get information about the
		   USB device, get the parent device with the
		   subsystem/devtype pair of "usb"/"usb_device". This will
		   be several levels up the tree, but the function will find
		   it.*/
		dev = udev_device_get_parent_with_subsystem_devtype(
		       dev,
		       "usb",
		       "usb_device");
		if (!dev) {
			/* Unable to find parent usb device. */
			goto next;
		}

		/* Get the VID/PID of the device */
		str = udev_device_get_sysattr_value(dev,"idVendor");
		dev_vid = (str)? strtol(str, NULL, 16): 0x0;
		str = udev_device_get_sysattr_value(dev, "idProduct");
		dev_pid = (str)? strtol(str, NULL, 16): 0x0;

		/* Check the VID/PID against the arguments */
		if ((vendor_id == 0x0 && product_id == 0x0) ||
		    (vendor_id == dev_vid && product_id == dev_pid)) {
			struct hid_device *tmp;
			size_t len;

		    	/* VID/PID match. Create the record. */
			tmp = malloc(sizeof(struct hid_device));
			if (cur_dev) {
				cur_dev->next = tmp;
			}
			else {
				root = tmp;
			}
			cur_dev = tmp;
			
			/* Fill out the record */
			cur_dev->next = NULL;
			str = dev_path;
			if (str) {
				len = strlen(str);
				cur_dev->path = calloc(len+1, sizeof(char));
				strncpy(cur_dev->path, str, len+1);
				cur_dev->path[len] = '\0';
			}
			else
				cur_dev->path = NULL;
			
			/* Serial Number */
			cur_dev->serial_number
				= copy_udev_string(dev, "serial");

			/* Manufacturer and Product strings */
			cur_dev->manufacturer_string
				= copy_udev_string(dev, "manufacturer");
			cur_dev->product_string
				= copy_udev_string(dev, "product");
			
			/* VID/PID */
			cur_dev->vendor_id = dev_vid;
			cur_dev->product_id = dev_pid;
		}
		else
			goto next;

	next:
		udev_device_unref(dev);
	}
	/* Free the enumerator and udev objects. */
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	
	return root;
}

void  HID_API_EXPORT hid_free_enumeration(struct hid_device *devs)
{
	struct hid_device *d = devs;
	while (d) {
		struct hid_device *next = d->next;
		free(d->path);
		free(d->serial_number);
		free(d->manufacturer_string);
		free(d->product_string);
		free(d);
		d = next;
	}
}

int HID_API_EXPORT hid_open(unsigned short vendor_id, unsigned short product_id, wchar_t *serial_number)
{
	struct hid_device *devs, *cur_dev;
	const char *path_to_open = NULL;
	int handle = -1;
	
	devs = hid_enumerate(vendor_id, product_id);
	cur_dev = devs;
	while (cur_dev) {
		if (cur_dev->vendor_id == vendor_id &&
		    cur_dev->product_id == product_id) {
			if (serial_number) {
				if (wcscmp(serial_number, cur_dev->serial_number) == 0) {
					path_to_open = cur_dev->path;
					break;
				}
			}
			else {
				path_to_open = cur_dev->path;
				break;
			}
		}
		cur_dev = cur_dev->next;
	}

	if (path_to_open) {
		/* Open the device */
		handle = hid_open_path(path_to_open);
	}

	hid_free_enumeration(devs);
	
	return handle;
}

int HID_API_EXPORT hid_open_path(const char *path)
{
  	int i;
	int handle = -1;
	struct Device *dev = NULL;

	// Initialize the Device array if it hasn't been done.
	if (!devices_initialized) {
		int i;
		for (i = 0; i < MAX_DEVICES; i++) {
			devices[i].valid = 0;
			devices[i].device_handle = -1;
			devices[i].blocking = 1;
		}
		devices_initialized = 1;
	}

	// Find an available handle to use;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (!devices[i].valid) {
			devices[i].valid = 1;
			handle = i;
			dev = &devices[i];
			break;
		}
	}

	if (handle < 0) {
		return -1;
	}

	// OPEN HERE //
	dev->device_handle = open(path, O_RDWR);

	// If we have a good handle, return it.
	if (dev->device_handle > 0) {
		return handle;
	}
	else {
		// Unable to open any devices.
		dev->valid = 0;
		return -1;
	}

}


int HID_API_EXPORT hid_write(int device, const unsigned char *data, size_t length)
{
	struct Device *dev = NULL;
	int bytes_written;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];
	
	bytes_written = write(dev->device_handle, data, length);

	return bytes_written;
}


int HID_API_EXPORT hid_read(int device, unsigned char *data, size_t length)
{
	struct Device *dev = NULL;
	int bytes_read;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];

	bytes_read = read(dev->device_handle, data, length);

	return bytes_read;
}

int HID_API_EXPORT hid_set_nonblocking(int device, int nonblock)
{
	int flags, res;
	struct Device *dev = NULL;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];
	
	flags = fcntl(dev->device_handle, F_GETFL, 0);
	if (flags >= 0) {
		if (nonblock)
			res = fcntl(dev->device_handle, F_SETFL, flags | O_NONBLOCK);
		else
			res = fcntl(dev->device_handle, F_SETFL, flags & ~O_NONBLOCK);
	}
	else
		return -1;

	if (res < 0) {
		return -1;
	}
	else {
		dev->blocking = !nonblock;
		return 0; /* Success */
	}
}

void HID_API_EXPORT hid_close(int device)
{
	struct Device *dev = NULL;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return;
	if (devices[device].valid == 0)
		return;
	dev = &devices[device];

	close(dev->device_handle);

	dev->valid = 0;
}

int HID_API_EXPORT_CALL hid_get_manufacturer_string(int device, wchar_t *string, size_t maxlen)
{
	struct Device *dev = NULL;
	int res;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];

	// TODO:

	return 0;
}

int HID_API_EXPORT_CALL hid_get_product_string(int device, wchar_t *string, size_t maxlen)
{
	struct Device *dev = NULL;
	int res;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];

	// TODO:

	return 0;
}

int HID_API_EXPORT_CALL hid_get_serial_number_string(int device, wchar_t *string, size_t maxlen)
{
	struct Device *dev = NULL;
	int res;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];

	// TODO:

	return 0;
}

int HID_API_EXPORT_CALL hid_get_indexed_string(int device, int string_index, wchar_t *string, size_t maxlen)
{
	struct Device *dev = NULL;
	int res;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return -1;
	if (devices[device].valid == 0)
		return -1;
	dev = &devices[device];

	// TODO:

	return 0;
}


HID_API_EXPORT const char * HID_API_CALL  hid_error(int device)
{
	struct Device *dev = NULL;

	// Get the handle 
	if (device < 0 || device >= MAX_DEVICES)
		return NULL;
	if (devices[device].valid == 0)
		return NULL;
	dev = &devices[device];

	// TODO:

	return NULL;
}
