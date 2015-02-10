//
// By Penguin, 2015.2
// This file include functions related to usbfs operation
// The device path is "/sys/bus/usb/devices"
// It's a in-memory copied of devicce descriptors, using it can avioding the need to open device
//

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>

#include "libsysfs.h"
///////////////
// LOCALS
///////////////
int lasterror;

///////////////
// FUNCTIONS
///////////////
void check_sysfs_devices(void)
{
  DIR *devices;
  struct dirent *entry;

  devices = opendir(SYSFS_DEVICE_PATH);

  if (devices == NULL)
  {
    lasterror = errno;
    printf("opendir %s failed (%d) - %s\n", SYSFS_DEVICE_PATH, lasterror, strerror(lasterror));
    return;
  }

  printf("SYSFS %s :\n", SYSFS_DEVICE_PATH);

  while ((entry = readdir(devices)))
  {
    if ((!isdigit(entry->d_name[0]) && strncmp(entry->d_name, "usb", 3)) || strchr(entry->d_name, ':'))
      continue;

    usb_device_info_sysfs(entry->d_name);
   
  }
}

void usb_device_info_sysfs(const char *devname)
{

  printf("%s :", devname);
  printf("bus number %d ", read_sysfs_attr(devname, "busnum"));
  printf("dev number %d ", read_sysfs_attr(devname, "devnum"));
  printf("speed %d\n", read_sysfs_attr(devname, "speed"));
}

static int read_sysfs_attr(const char *devname, const char *attr)
{
  char filename[PATH_MAX];
  FILE *f;
  int value;

  snprintf(filename, PATH_MAX, "%s/%s/%s", SYSFS_DEVICE_PATH, devname, attr);
  f = fopen(filename, "r");
  if (f == NULL)
  {
    lasterror = errno;
    if (lasterror == ENOENT)
    {
      printf("file %s not found, the device may be disconnected\n", filename);
      return -1;
    }
    printf("open file %s failed (%d) - %s\n", filename, lasterror, strerror(lasterror));
    return -1;
  }

  if (fscanf(f, "%d", &value))
  {
    if (value > 0)
      return value;

    printf("file %s contains a negative value\n", filename);
    return -1;
  }
  lasterror = errno;
  printf("fscanf %s failed (%d) - %s\n", filename, lasterror, strerror(lasterror));

  return -1;
}
