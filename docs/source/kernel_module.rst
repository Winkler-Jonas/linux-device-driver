Kernel Module
#############

Device files
************

The misc device files are pre-defined are set up at the top after import statements.
Defining the file names globally simplifies access and useful error messages.

.. code-block:: c

   #define DEVICE_NAME_BUZZER "rainbow_buzzer"   // device file for the Buzzer
   #define DEVICE_NAME_BUTTON "rainbow_buttons"  // device file for the Buttons
   #define DEVICE_NAME_LED "rainbow_leds"        // device file for the LEDs

Device tree overlay
*******************

To allow the usage of the device tree file, it has to be properly initiated using its compatible stings.

.. code-block:: c

   static const struct of_device_id rainbow_hat_of_match[] = {
      {.compatible = "tha,rainbow-arc",}, // compatible string for LEDs
      {.compatible = "tha,rainbow-hat",}, // compatible string for GPIO and PWN
      {}
   };

The compatible string must match perfectly, an additional whitespace or other character will result in an error.
For the kernel to find the last entry, an empty entry is appended to the ``of_device_id struct``.

.. include:: platform_driver.rst

.. include:: spi_driver.rst

Module init and exit
********************

Due to creating multiple drivers and devices in one module macros cannot be used. Therefore
the module ``__init()`` and ``__exit()`` function are used instead.

.. code-block:: c
   :caption: module init and exit

   // Initialize SPI + Platform Driver
   static int __init rainbow_init(void)
   {
      // Register SPI driver
      spi_register_driver(&rainbow_hat_spi_driver);

      // Register Platform driver
      platform_driver_register(&rainbow_hat_platform_driver);
   }

   // Cleanup SPI + Platform Drivers
   static void __exit rainbow_exit(void)
   {
      // Unregister SPI driver
      spi_unregister_driver(&rainbow_hat_spi_driver);
      // Unregister Platform driver
      platform_driver_unregister(&rainbow_hat_platform_driver);
   }

   // No macros used due to inevitable duplicate call to __init/__exit
   module_init(rainbow_init);
   module_exit(rainbow_exit);