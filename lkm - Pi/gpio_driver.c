//Driver for gpio pin manipulation through ioctl commands

//Import libraries
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#include "gpio_driver.h"

//Module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack Cavar");
MODULE_DESCRIPTION("ioctl custom GPIO driver");
MODULE_VERSION("1");

//Define the major number
#define MYMAJOR 64

//Allow user to choose the pin to interact with and the gpio base to get the exact pin number
static int gpio_pin = 0;  
static int gpio_base = 0; 

//Function to find the gpiochip with the label "pinctrl-bcm2835"
static int find_gpiochip(struct gpio_chip *chip, void *data) {
    if (strcmp(chip->label, "pinctrl-bcm2835") == 0) {
        return 1; // Return 1 if found
    }
    return 0; // Return 0 otherwise
}

//Function of unique commands to run gpio requests through
static long int my_ioctl(struct file *file, unsigned cmd, unsigned long arg) {
    int new_pin;
    int new_value;
    struct gpio_chip *chip; 

    switch (cmd) {

        //Get the value of pin to change to from the user
        case GPIO_SET_PIN:

            //Pin error checking
            if (copy_from_user(&new_pin, (int *)arg, sizeof(new_pin))) {
                printk("Failed to copy data from user.\n");
                return -EFAULT;
            }

            printk("About to set pin to %d\n", new_pin);

            gpio_pin = new_pin;  

            printk("Set pin to %d\n", new_pin);
            break;

        //Set the value of the pin selected to the value specified by the user
        case GPIO_SET_VALUE:

            //Chip/pin error checking
            if (gpio_pin == 0) {
                printk("No GPIO pin specified. Use GPIO_SET_PIN first.\n");
                return -EINVAL;
            }
            if (copy_from_user(&new_value, (int *)arg, sizeof(new_value))) {
                printk("Failed to copy data from user.\n");
                return -EFAULT;
            }

            chip = gpiochip_find(NULL, find_gpiochip);
            if (!chip) {
                printk("Could not find gpiochip with label pinctrl-bcm2835.\n");
                return -ENODEV;
            }

            gpio_base = chip->base;
            if (gpio_base < 0) {
                printk("Could not get GPIO base from gpiochip.\n");
                return -EINVAL;
            }
            if (gpio_request(gpio_pin, "sysfs")) {  
                printk("GPIO pin %d is not available. Use another pin.\n", gpio_pin);
                return -EBUSY;
            }

            printk("About to set pin %d to %d\n", gpio_pin, new_value);

            // Set the specified GPIO pin to the desired value using the GPIO base
            gpio_direction_output(gpio_pin, 0);
            gpio_set_value(gpio_pin, new_value); 
            printk("Set pin %d to %d\n", gpio_pin, new_value);
            break;

        //Free the pin selected by the user
        case GPIO_FREE_PIN:

            //Chip/pin error checking
            if (gpio_pin == 0) {
                printk("No GPIO pin specified. Use GPIO_SET_PIN first.\n");
                return -EINVAL;
            }

            chip = gpiochip_find(NULL, find_gpiochip);
            if (!chip) {
                printk("Could not find gpiochip with label pinctrl-bcm2835.\n");
                return -ENODEV;
            }
            gpio_base = chip->base;
            if (gpio_base < 0) {
                printk("Could not get GPIO base from gpiochip.\n");
                return -EINVAL;
            }

            printk("About to free pin %d\n", gpio_pin);

            // Free the specified GPIO pin
            if(gpio_pin != 0) {
                gpio_free(gpio_pin);
                printk("Freed pin %d\n", gpio_pin);
            }
            break;
    }
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = my_ioctl
};

//Functions to run upon lkm activation

//Function to run upon lkm activation
static int __init ModuleInit(void) {
    int retval;
    printk("Hello, Kernel!\n");

    printk("GPIO_SET_PIN: %u\n", GPIO_SET_PIN);
    printk("GPIO_SET_VALUE: %u\n", GPIO_SET_VALUE);
    printk("GPIO_FREE_PIN: %u\n", GPIO_FREE_PIN);
    
    //Register driver
    retval = register_chrdev(MYMAJOR, "my_gpio_driver", &fops);
    if (retval == 0) {
        printk("GPIO driver - registered Device number Major: %d, Minor: %d\n", MYMAJOR, 0);
    } else if (retval > 0) {
        printk("GPIO driver - registered Device number Major: %d, Minor: %d\n", retval >> 20, retval & 0xfffff);
    } else {
        printk("Could not register device number!\n");
        return -EINVAL;
    }

    return 0;
}

//Function to run upon lkm deactivation
static void __exit ModuleExit(void) {
    printk("GPIO pin on exit: %d\n", gpio_pin);
    if(gpio_pin != 0) {
        gpio_free(gpio_pin);
        printk("Freed pin %d\n", gpio_pin);
    }
    unregister_chrdev(MYMAJOR, "my_gpio_driver");
    printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);




