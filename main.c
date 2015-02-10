//
// By Penguin, 2015.2
// This is Penguin's usb info based on linux devfs
// It is used to study linux usb subsystem, SAT(SCSI/ATA Translation)
//


#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

#include "libsysfs.h"
#include "libusbfs.h"

typedef enum _METHOD {
  sysfs,
  usbfs,
  both
} METHOD;

typedef struct _PARAMETERS {
  METHOD method;
} PARAMETERS;

///////////////
// PROTOTYPE 
///////////////
void parse_options(PARAMETERS *param, int argc, char** argv);
void print_usage(void);

///////////////
// LOCALS
///////////////
const char* const short_options = "hm:";
const struct option long_options[] = {
  {"help", 0, NULL, 'h'},
  {"method", 1, NULL, 'm'}
};

///////////////
// FUNCTIONS
///////////////
int main(int argc, char* argv[])
{
  PARAMETERS param;
  printf("This is Penguin's usb test\n");

  parse_options(&param, argc, argv);

  if (param.method == sysfs || param.method == both)
    check_sysfs_devices();

  if (param.method == usbfs || param.method == both)
    check_usbfs_devices();

  return 0;
}

void print_usage(void)
{
  printf("This is Penguin's linux-usb test\n");
  printf("   -h  --help                     Display usage information\n");
  printf("   -m  --method sysfs/usbfs/both  Specify how to get usb info\n");
}

void parse_options(PARAMETERS *param, int argc, char** argv)
{
  int option;
  char* opt_arg = NULL;

  // Default value
  param->method = both;

  if (argc == 1)
  {
    return;
  }

  while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)
  {
    switch (option)
    {
      case 'h':
        print_usage();
        exit(0);

      case 'm':
        opt_arg = optarg;
        if (!strcmp(opt_arg, "sysfs"))
          param->method = sysfs;
        else if (!strcmp(opt_arg, "usbfs"))
          param->method = usbfs;
        else if (!strcmp(opt_arg, "both"))
          param->method = both;
        else
        {
          printf("unknown parameter %s, use '-h' to get help info\n", opt_arg);
          exit(0);
        }
        break;

      default:
        printf("unknown option %c\n, use '-h' to get help info", option);
        exit(0);

    }
  }
}
