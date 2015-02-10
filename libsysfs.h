#ifndef _LIB_SYSFS_H
#define _LIB_SYSFS_H

#define SYSFS_DEVICE_PATH "/sys/bus/usb/devices"

void check_sysfs_devices(void);
static int read_sysfs_attr(const char *devname, const char *attr);
void usb_device_info_sysfs(const char *devname);

#endif
