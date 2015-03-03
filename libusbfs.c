//
// By Penguin, 2015.2
// This file include functions related to usbfs operation
// Possible file path include "/dev/bus/usb", "/proc/bus/usb" and "/dev/usbdev*.*"
// Open file node under these path will cause the device to be resumed
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include "storage.h"

#include "libusbfs.h"

///////////////
// PROTOTYPE
///////////////
void usbfs_get_descriptors(int fd);
void parse_csw(struct bulk_cs_wrap *csw);
static int bot_inquiry(int fd);

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
  bot_inquiry(fd);

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

// This function is used to send identify command to usb ata disk by BOT(Bulk only transport) protocol
// the device type, command sets and protocol should be check before call this function
static int bot_inquiry(int fd)
{
  int ret;
  struct usbdevfs_bulktransfer *bulk_ctrl;
  struct bulk_cb_wrap          *cbw;
  struct bulk_cs_wrap          *csw;
  char   *inquirydata;

  inquirydata = (char *)malloc(36);
  bulk_ctrl = (struct usbdevfs_bulktransfer *)malloc(sizeof(struct usbdevfs_bulktransfer));
  cbw = (struct bulk_cb_wrap *)malloc(sizeof(struct bulk_cb_wrap));
  csw = (struct bulk_cs_wrap *)malloc(sizeof(struct bulk_cs_wrap));

  memset(inquirydata, 0, 36);
  memset(bulk_ctrl, 0, sizeof(struct usbdevfs_bulktransfer));
  memset(cbw, 0, sizeof(struct bulk_cb_wrap));
  memset(csw, 0, sizeof(struct bulk_cs_wrap));

  printf("This is bot_inquiry\n");
  // Send command
  cbw->Signature = 0x43425355;
  cbw->Tag = 0x12;
  cbw->DataTransferLength = 36;
  cbw->Flags = 1 << 7;  // bit 7, 0: data-out from host to device, 1: data-in from device to host.
  cbw->Lun = 0;
  cbw->Length = 6;
 
  cbw->CDB[0] = 0x12;
  cbw->CDB[1] = 0;
  cbw->CDB[2] = 0;
  cbw->CDB[3] = 0;
  cbw->CDB[4] = 36;
  cbw->CDB[5] = 0;
  
  bulk_ctrl->ep = 0x2;        // bulk out endpoint addr, refer to descriptors info
  bulk_ctrl->len = sizeof(struct bulk_cb_wrap);        // transferred data length
  bulk_ctrl->timeout = 1000;  // 1 second
  bulk_ctrl->data = cbw;

  ret = ioctl(fd, USBDEVFS_BULK, &bulk_ctrl);
  if (ret < 0)
  {
    lasterror = errno;
    printf("bulk send command failed (%d) - %s\n", lasterror, strerror(lasterror));
    return -1;
  }

  // Transfer data
  bulk_ctrl->ep = 0x81;      // bulk in endpoint addr
  bulk_ctrl->len = 36;
  bulk_ctrl->timeout = 1000;
  bulk_ctrl->data = inquirydata;

  ret = ioctl(fd, USBDEVFS_BULK, &bulk_ctrl);
  if (ret < 0)
  {
    lasterror = errno;
    printf("bulk transfer data failed (%d) - %s\n", lasterror, strerror(lasterror));
  } 
  
  // get status
  bulk_ctrl->ep = 0x81;
  bulk_ctrl->len = sizeof(struct bulk_cs_wrap);
  bulk_ctrl->timeout = 1000;
  bulk_ctrl->data = csw;
  
  ret = ioctl(fd, USBDEVFS_BULK, &bulk_ctrl);
  if (ret < 0)
  {
    lasterror = errno;
    printf("bulk transfer data failed (%d) - %s\n", lasterror, strerror(lasterror));
    return -1;
  }

  parse_csw(csw);

  free(bulk_ctrl);
  free(cbw);
  free(csw);
  return 0;
}

void parse_csw(struct bulk_cs_wrap *csw)
{
  printf("Signature %x\n", csw->Signature);
  printf("Tag %x\n", csw->Tag);
  printf("Residue %x\n", csw->Residue);
  printf("Status %x\n", csw->Status);
}
