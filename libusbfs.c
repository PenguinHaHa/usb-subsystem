//
// By Penguin, 2015.2
// This file include functions related to usbfs operation
// Possible file path include "/dev/bus/usb", "/proc/bus/usb" and "/dev/usbdev*.*"
// Open file node under these path will cause the device to be resumed
//

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>

#include "libusbfs.h"

///////////////
// PROTOTYPE
///////////////
void usbfs_get_descriptors(int fd);

///////////////
// LOCALS
///////////////
int lasterror;
static const char *usbfs_path = NULL;

///////////////
// FUNCTIONS 
///////////////
void check_usbfs_devices(void)
{
  DIR *buses;
  struct dirent *entry;

  usbfs_path = find_usbfs_path();
  if (usbfs_path == NULL)
    return;

  printf("USBFS %s :\n", usbfs_path);
  buses = opendir(usbfs_path);
  if (buses == NULL)
  {
    lasterror = errno;
    printf("opendir %s failed (%d) - %s\n", usbfs_path, lasterror, strerror(lasterror));
    return;
  }

  while (entry = readdir(buses))
  {
    if (entry->d_name[0] == '.')
      continue;

    usbfs_scan_busdir(entry->d_name);
  }
}

static const char *find_usbfs_path(void)
{
  const char *path;
  int found = 0;

  DIR *dir;
  struct dirent *entry;

  path = USBFS_DEVICE_PATH_DEV;
  dir = opendir(path);
  if (dir)
  {
    while (entry = readdir(dir))
    {
      if (entry->d_name[0] == '.')
        continue;
      return path;
      break;
    }
  }

  path = USBFS_DEVICE_PATH_PRO;
  dir = opendir(path);
  if (dir)
  {
    while (entry = readdir(dir))
    {
      if (entry->d_name[0] == '.')
        continue;
      return path;
      break;
    }
  }

  printf("find usbfs failed\n");
  return NULL;
}

void usbfs_scan_busdir(const char *busname)
{
  DIR *dir;
  char dirpath[PATH_MAX];
  struct dirent *entry;

  snprintf(dirpath, PATH_MAX, "%s/%s", usbfs_path, busname);
  dir = opendir(dirpath);
  if (dir == NULL)
  {
    lasterror = errno;
    printf("open %s failed (%d) - %s\n", dirpath, lasterror, strerror(lasterror));
    printf("this may caused by an hub unplugged\n");
    return;
  }

  while (entry = readdir(dir))
  {
    if (entry->d_name[0] == '.')
      continue;

    usb_device_info_usbfs(busname, entry->d_name);
  }
}

void usb_device_info_usbfs(const char *busname, const char *devname)
{
  char devpath[PATH_MAX];
  int fd;
  int active_config;

  printf("bus %s dev %s: \n", busname, devname);
  snprintf(devpath, PATH_MAX, "%s/%s/%s", usbfs_path, busname, devname);
  fd = open(devpath, O_RDWR);
  if (fd < 0)
  {
    lasterror = errno;
    if (lasterror == EACCES)
    {
      fd = open(devpath, O_RDONLY);
      if (fd < 0)
      {
        lasterror = errno;
        printf("open %s failed with O_RDONLY (%d) - %s\n", devpath, lasterror, strerror(lasterror));
        return;
      }
      printf("only have read-only access to device %s\n", devpath);
      return;
    }
    printf("open %s failed with O_RDWR (%d) - %s\n", devpath, lasterror, strerror(lasterror));
    return;
  }

  active_config = usbfs_get_active_config(fd);
  usbfs_get_descriptors(fd);

  close(fd);
}

void usbfs_get_descriptors(int fd)
{
  int ret;
  struct usb_device_descriptor devdesc;
  
  // Get device descriptor firstly
  ret = read(fd, &devdesc, sizeof(devdesc));
  if (ret < 0)
  {
    lasterror = errno;
    printf("line %d, read file failed (%d) -%s\n", __LINE__, lasterror, strerror(lasterror));
    return;
  }
  printf("bLength %d | ", devdesc.bLength);
  printf("bDescriptorType %d | ", devdesc.bDescriptorType);
  printf("bcdUSB %04x | ", devdesc.bcdUSB);
  printf("bDeviceClass %02x | ", devdesc.bDeviceClass);
  printf("bDeviceSubClass %02x | ", devdesc.bDeviceSubClass);
  printf("bDeviceProtocol %02x | ", devdesc.bDeviceProtocol);
  printf("idVendor %04x | ", devdesc.idVendor);
  printf("idProduct %04x | ", devdesc.idProduct);
  printf("\n");
}

static int usbfs_get_active_config(int fd)
{
  unsigned char active_config = 0;
  int ret;

  struct usbdevfs_ctrltransfer ctrl = 
  {
    .bRequestType = 0x80, // In: device-to-host
    .bRequest = 0x08,     // Get the current device configuration value
    .wValue = 0,
    .wIndex = 0,
    .wLength = 1,
    .timeout = 1000,
    .data = &active_config
  };

  ret = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
  if (ret < 0)
  {
    lasterror = errno;
    printf("send control command failed (%d) - %s\n", lasterror, strerror(lasterror));
    return -1;
  }
  
  printf("active_config %d\n", active_config);
  return active_config;
}

