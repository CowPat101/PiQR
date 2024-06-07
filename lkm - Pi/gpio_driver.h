#ifndef IOCTL_GPIO_H
#define IOCTL_GPIO_H

#include <linux/ioctl.h>

#define GPIO_IOC_MAGIC 'a'

#define GPIO_SET_PIN   _IOW(GPIO_IOC_MAGIC, 0x0A, int)  
#define GPIO_SET_VALUE _IOW(GPIO_IOC_MAGIC, 0x0B, int)  
#define GPIO_FREE_PIN _IOR(GPIO_IOC_MAGIC, 0x0C, int)  

#endif