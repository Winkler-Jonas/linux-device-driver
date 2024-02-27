Platform Driver
***************

Data structure
==============

As specified in the project description one platform driver must be initiated, however two device files shall be
created. To enhance modularity and readability I decided to pack all relevant information about the platform driver
into one struct.

.. code-block:: c
   :caption: Platform driver struct (bundles information in one place)

   struct rainbow_gpio_dev {
      struct pwm_device *buzzer_pwm;
      struct gpio_descs *button_gpios;
      struct miscdevice misc_buzzer;
      struct miscdevice misc_buttons;
      struct mutex lock;               // required due to limited write operations to buzzer-file
   };

File operations
===============

The Misc-file operations where limited by the specifications. Therefore the buzzer only has a write operation and
also additionally prevents any reading operation in case the permissions are changed using ``chmod``. For the
buttons file only a read operation is added, because the state of the buttons are not supposed to change
using software.

.. code-block:: c

   static const struct file_operations rainbow_buzzer_fops = {
      .owner = THIS_MODULE,            // linking module and device
      .write = rainbow_buzzer_write,   // add device write operation
      .llseek = no_llseek,             // prevent seek/reading operations
   };

   static const struct file_operations rainbow_buttons_fops = {
      .owner = THIS_MODULE,            // linking module and device
      .read = rainbow_buttons_read,    // adding device read operation
   };

Probe function
==============

To keep everything in one place, I decided to initiate the misc devices inside the drivers probe function.

.. code-block:: c
   :caption: misc device init inside probe-function

   static int rainbow_hat_platform_driver_probe(struct platform_device *pdev)
   {

      // Acquire GPIOs using *devm_gpiod_get_array* make the kernel handle the required memory
      // GPIOD_ASIS-Flag leaves the state of the GPIOs as they are.
      // Property to access Button-GPIOs is *button* (not button-gpios as specified in .dts)
      gpio_dev->button_gpios = devm_gpiod_get_array(dev, "button", GPIOD_ASIS);

      gpio_dev->misc_buttons.minor = MISC_DYNAMIC_MINOR;    // get dynamic minor-nr
      gpio_dev->misc_buttons.name = DEVICE_NAME_BUTTON;     // Use pre-defined name as device file
      gpio_dev->misc_buttons.fops = &rainbow_buttons_fops;  // Add struct for file-operations
      gpio_dev->misc_buttons.mode = 0444;                   // Disable and write as specified

      ...
      // Acquire PWN-device using *devm_pwn_get* makes the kernel handle the required memory
      gpio_dev->buzzer_pwm = devm_pwm_get(dev, NULL);

      gpio_dev->misc_buzzer.minor = MISC_DYNAMIC_MINOR;
      gpio_dev->misc_buzzer.name = DEVICE_NAME_BUZZER;
      gpio_dev->misc_buzzer.fops = &rainbow_buzzer_fops;
      gpio_dev->misc_buzzer.mode = 0222;   // disable write operation as specified

      ...
   }

To create the device file the misc devices are registered. To ease access to these devices the
``platform_set_drvdata()`` function was used.


Remove
======

The remove function is quite straightforward. Since all devices are allocated using ``devm_*`` only
the misc-devices need to be unregistered.

.. code-block:: c

   static int rainbow_hat_platform_driver_remove(struct platform_device *pdev)
   {
      misc_deregister(&gpio_dev->misc_buttons);
      misc_deregister(&gpio_dev->misc_buzzer);
   }


Driver
======

``.of_match_table`` links the Driver with the device tree. However an SPI-Driver does not allways use
a device tree. Also some devices might not have a device tree, therefore ``of_match_ptr`` is used.
``of_match_ptr`` allows the kernel module to proceed, even if no device tree was found. It is not
necessary for this implementation. It however does allow modularity in case of future driver changes.

.. code-block:: c

   static struct platform_driver rainbow_hat_platform_driver = {
      .probe = rainbow_hat_platform_driver_probe,     // load the driver
      .remove = rainbow_hat_platform_driver_remove,   // remove the driver
      .driver = {
         .name = "rainbow_hat_platform_driver",                // driver name in e.g. dmesg
         .of_match_table = of_match_ptr(rainbow_hat_of_match), // of_match_ptr() used bc. continuity
         .owner = THIS_MODULE,   // connect driver to module
      },
   };

Read Buttons
============

Prototype
---------

.. code-block:: c

   static ssize_t rainbow_buttons_read(struct file *file,
                                       char __user *buf,
                                       size_t len,
                                       loff_t *offset)

Usage
-----

To access the driver's or device's read function, obtaining a file descriptor is essential. Once the file is open,
prepare a small buffer that is terminated with a null byte. This buffer should then be passed to the read function
along with the number of bytes you wish to read (amount of buttons), enabling you to capture the state of all buttons.

.. code-block:: c

   int fd = open("/dev/rainbow_buttons", O_RDONLY);

   char btn_state[4] = {'0', '0', '0', '\0'};
   read(dev->buttons_fd, btn_state, 3);

Implementation details
----------------------

The file operation function read for the button device is supposed to return the state of the buttons
(1: pressed, 0: release). To access the GPIO Device, ``container_of`` can be used to retrieve the
device earlier stored using ``platform_set_drvdata()``.

.. code-block:: c

   // Retrieving instance of stored gpio_dev using container_of
   struct rainbow_gpio_dev *gpio_dev = container_of(file->private_data, struct rainbow_gpio_dev, misc_buttons);

To retrieve the state of all three buttons a small buffer is initiated and then filled using a loop and
``gpiod_get_value()``. The returned value is then inverted to allow the specifications to be fulfilled.
Once the state is aquired and inverted the buffer can be moved to the user-space using ``copy_to_user()``.

.. code-block:: c

   char button_status[3]; // Buffer to hold the status of the 3 buttons
   int i;

   for (i = 0; i < 3; i++) {
      int gpio_val = gpiod_get_value(gpio_dev->button_gpios->desc[i]);

      button_status[i] = gpio_val ? '0' : '1'; // Invert status for specifications
   }

   copy_to_user(buf, button_status, sizeof(button_status)

Write Buzzer
============

Prototype
---------

.. code-block:: c

   static ssize_t rainbow_buzzer_write(struct file *file,
                                       const char __user *buf,
                                       size_t count,
                                       loff_t *ppos)

Usage
-----

To utilize the driver's or device's write function, a file descriptor is necessary. After opening the
file, you can write an integer to it, which represents the frequency to be played.

.. code-block:: c

   int frequency = 440;
   int fd = open("/dev/rainbow_buzzer", O_WRONLY);
   write(fd, frequency, sizeof(frequency);

Implementation details
----------------------

Before writing to the buzzer, the mutex must be checked to ensure it is currently not used by another process or thread.
Then the user-space data (frequency) is copied into a local variable.

.. code-block:: c

   mutex_trylock(&gpio_dev->lock)
   copy_from_user(&freq, buf, sizeof(freq))

Once successfully copied. If the provided frequency is 0, the buzzer is disabled.
Otherwise frequency is converted into a signal the PWN device is able to interpret.
If everything is successful, the signal is send to the device which plays the frequency.

.. code-block:: c

   if (freq == 0) {
      // Turn off the buzzer
      pwm_disable(gpio_dev->buzzer_pwm);
      ret = count;
   } else {
      // Calculate the period (in nanoseconds) for the PWM signal
      period = 1000000000UL / freq;

   // send data to PWN device
   pwm_config(gpio_dev->buzzer_pwm, (int) (period / 2), (int) period);
   pwm_enable(gpio_dev->buzzer_pwm);

Once finished or if an error occurred the mutex will be unlocked. And either the written amount
of bytes or an error-code will be returned to the caller.

.. code-block:: c

   mutex_unlock(&gpio_dev->lock);
   return ret;