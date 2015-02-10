#ifndef _LIB_USBFS_H
#define _LIB_USBFS_H

#define USBFS_DEVICE_PATH_DEV "/dev/bus/usb"
#define USBFS_DEVICE_PATH_PRO "/proc/bus/usb"

void check_usbfs_devices(void);
void usbfs_scan_busdir(const char *busname);
void usb_device_info_usbfs(const char *busname, const char *devname);
static int usbfs_get_active_config(int fd);
static const char *find_usbfs_path(void);

#endif
