SPI Driver
**********

Data structure
==============

To have all relevant data of the driver in one place a struct is used.
The SPI driver requires a mutex because only one process or thread is allowed to change
the RGB values at a time. Additionally an array is initialized with amount of LEDs available
on the Rainbow HAT.

.. code-block:: c
   :caption: SPI driver struct (bundles information in one place)

   // Struct to hold RGB values
   struct rgb_led {
      uint8_t r, g, b;
   };

   struct rainbow_spi_dev {
      struct spi_device *spi;
      struct mutex lock;
      struct miscdevice misc_leds;
      struct rgb_led leds[NUMBER_LEDS];
   };

File operations
===============

The Misc-file operations are limited by the specifications. Therefore it only allows a write operation.
Read operations could additionally be prevented with ``.llseek = no_llseek`` to prevent read operations
even if file permissions change.

.. code-block:: c

   static const struct file_operations rainbow_leds_fops = {
      .owner = THIS_MODULE,
      .write = rainbow_leds_write,
   };

Probe function
==============

To keep the program structure similar the misc device for the SPI-driver is also initiated within the
probe function.

.. code-block:: c
   :caption: misc device init inside probe function

   static int rainbow_hat_spi_driver_probe(struct spi_device *spi)
   {

      // using devm_* to make the kernel handle memory
      dev = devm_kzalloc(&spi->dev, sizeof(*dev), GFP_KERNEL);

      dev->misc_leds.minor = MISC_DYNAMIC_MINOR;   // dynamic minor Nr.
      dev->misc_leds.name = DEVICE_NAME_LED;
      dev->misc_leds.fops = &rainbow_leds_fops;
      dev->misc_leds.mode = 0222;   // set permission to write

      misc_register(&dev->misc_leds);  // Register misc device
      spi_set_drvdata(spi, dev);       // Set SPI-device to later user
   }

``spi_set_drvdata()`` eases access to the device in the write function.

Remove
======

The drivers remove function only needs to deregister the misc device due to
devm_* usage in the probe function.

.. code-block:: c

   static void rainbow_hat_spi_driver_remove(struct spi_device *spi)
   {
      misc_deregister(&dev->misc_leds);
   }

Driver
======

SPI drivers usually have an ID attached. The specification however did not allow any.
Therefore only a device tree is attached with ``.of_match_table``.

.. code-block:: c

   static struct spi_driver rainbow_hat_spi_driver = {
      .driver = {
         .name = "rainbow_hat_spi_driver", // driver name in e.g. dmesg
         .of_match_table = of_match_ptr(rainbow_hat_of_match),
         .owner = THIS_MODULE, // connect driver to module
      },
      .probe = rainbow_hat_spi_driver_probe,    // load the driver
      .remove = rainbow_hat_spi_driver_remove,  // remove the driver
   };

Read LEDs
=========

Prototype
---------

.. code-block:: c

   static ssize_t rainbow_leds_write(struct file *file,
                                     const char __user *buf,
                                     size_t count,
                                     loff_t *ppos)

Usage
-----

To access the driver write function, obtaining a file descriptor is essential. Once the file
is opened any LED can be lit using a string argument. The drivers write function allows
either one LED like (LEDnr:color-hex-code) e.g. (0:00FF00) or multiple LEDs in an array like
string like in the example bellow.

The specification suggested an argument like (255, 0, 255) but, this would drastically increase
the parsing process due to its whitespaces, separators and brackets especially if multiple LEDs
should be lit.

.. code-block:: c

   char *led_array = "1:abcdef,2:fedcba,3:ff00ff,4:00ff00";
   int fd = open("/dev/rainbow_leds", O_WRONLY);
   bytes_written = write(led_fd, led_array, strlen(led_array));

Implementation details
----------------------

The SPI-device can be retrieves just like the platform driver using the ``container_of`` function.

.. code-block:: c

   struct rainbow_spi_dev *spi_dev = container_of(file->private_data, struct rainbow_spi_dev, misc_leds);

Afterwards the user-argument is being handled.

.. code-block:: c
   :caption: write function handling user argument

   // small buffer required to hold user argument (plus one for null-byte)
   kbuf = kmalloc(count + 1, GFP_KERNEL);

   // lock the mutex (can be interrupted in case of failure)
   mutex_lock_interruptible(&spi_dev->lock)

   // copy the user argument to the local buffer
   copy_from_user(kbuf, buf, count)

   // apply a null-byte to signal the end of the buffer
   kbuf[count] = '\0';

   // parse the local buffer containing (e.g. 1:ff00ff,200ff00...)
   parse_led_colors(spi_dev, kbuf, count)

   // when successfully parsed the LED strip can be updated
   update_leds(spi_dev)

   // before returning to the user-space. The local buffer needs to be freed and the mutex unlocked
   mutex_unlock(&spi_dev->lock);
   kfree(kbuf);

.. code-block:: c
   :caption: parsing the user argument

   const char *curr = buf;
   const char *end = buf + count;
   long led_num;
   char *next;

   // loop over every byte provided. Buffer ends with null-byte.
   while (curr < end && *curr != '\0') {
		char *colon = strchr(curr, ':'); // looking for colon (required for arg-parse)

      // create small buffer any single argument can fit (e.g. 1:ff00ff)
      char led_num_str[10] = {0};
		int len = colon - curr;

      // copy current buffer to led_num buffer
      strncpy(led_num_str, curr, len);
      led_num_str[len] = '\0'; // Ensure null-termination

      // Parse char to long value
      ret = kstrtol(led_num_str, 10, &led_num);

      // Advance pointer past colon
      curr = colon + 1;

      //parse the color string
      parse_hex_color(curr, &color)

      // If successfully parsed, attach values to device buffer
      dev->leds[led_num].r = (color >> 16) & 0xFF;
      dev->leds[led_num].g = (color >> 8) & 0xFF;
      dev->leds[led_num].b = color & 0xFF;

      // Advance pointer past color-string
      curr += 6;

      // Check if next character is a comma
      next = strchr(curr, ',');

      // if comma was found, continue loop, else break and end parsing
      if (next && next < end) {
         curr = next + 1; // Move past the comma
      } else {
         break;
      }

Parsing the Hex-values can be achieved in multiple ways. An example can we viewed in the source code.

The SPI-Bus requires certain settings to work as expected. These values can be pre-defined in the
Pre-Processor. Afterwards the can be used to create an argument for the Bus.

.. code-block:: c

   #define NUMBER_LEDS 7
   #define NUMBER_START_BYTES 4
   #define NUMBER_BYTES_PER_LED 4
   #define NUMBER_STOP_BYTES 4
   #define LED_BRIGHTNESS 0xE0
   #define MAX_BRIGHTNESS 0x1F


   static int update_leds(struct rainbow_spi_dev *dev)
   {
      // Create buffer with expected length for SPI-Buffer
      uint8_t buffer[NUMBER_START_BYTES + NUMBER_LEDS * NUMBER_BYTES_PER_LED + NUMBER_STOP_BYTES] = {0};

      // initialize the SPI-Bus arguments
      struct spi_message msg;
      struct spi_transfer spi_xfer = {
         .tx_buf = buffer,
         .len = sizeof(buffer),
         .bits_per_word = 8,
      };

      // Initialize the buffer for the SPI-Bus
      memset(buffer, 0x00, NUMBER_START_BYTES);

      // Write the color codes to the buffer
      for (int i = 0; i < NUMBER_LEDS; i++) {
         int offset = NUMBER_START_BYTES + i * NUMBER_BYTES_PER_LED;

         buffer[offset] = LED_BRIGHTNESS | MAX_BRIGHTNESS;
         buffer[offset + 1] = dev->leds[i].b; // Blue
         buffer[offset + 2] = dev->leds[i].g; // Green
         buffer[offset + 3] = dev->leds[i].r; // Red
      }

      // Write end to buffer (expected from SPI-Bus)
      memset(&buffer[sizeof(buffer) - NUMBER_STOP_BYTES], 0xFF, NUMBER_STOP_BYTES);

      // send message via SPI-Bus
      spi_message_init(&msg);
      spi_message_add_tail(&spi_xfer, &msg);
      spi_sync(dev->spi, &msg);
   }
