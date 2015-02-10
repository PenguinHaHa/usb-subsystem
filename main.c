//
// By Penguin, 2015.2
// This is Penguin's usb info based on linux devfs
// It is used to study linux usb subsystem, SAT(SCSI/ATA Translation)
//


#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>

#define SYSFS_DEVICE_PATH "/sys/bus/usb/devices"
#define USBFS_DEVICE_PATH_DEV "/dev/bus/usb"
#define USBFS_DEVICE_PATH_PRO "/proc/bus/usb"

///////////////
// PROTOTYPE 
///////////////
void check_sysfs_devices(void);
static int read_sysfs_attr(const char *devname, const char *attr);
void usb_device_info_sysfs(const char *devname);
static const char *find_usbfs_path(void);
void check_usbfs_devices(void);
void usbfs_scan_busdir(const char *busname);
void usb_device_info_usbfs(const char *busname, const char *devname);
static int usbfs_get_active_config(int fd);

///////////////
// LOCALS
///////////////
int lasterror;
static const char *usbfs_path = NULL;

///////////////
// FUNCTIONS
///////////////
int main(int argc, char* argv[])
{
  printf("This is Penguin's usb test\n");

  printf("sysfs:\n");
  check_sysfs_devices();

  printf("\nusbfs:\n");
  check_usbfs_devices();

  return 0;
}

void check_usbfs_devices(void)
{
  DIR *buses;
  struct dirent *entry;

  usbfs_path = find_usbfs_path();
  if (usbfs_path == NULL)
    return;

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
  printf("active_config %d\n", active_config);
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
  
  return active_config;
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
