Rainbow HAT Library
*******************

Overview
========

In order to provide a contemporary approach of software developing an object-oriented approach was
chosen to develop an abstraction layer accessing the drivers directly. The approach is not ``checkpatch.pl``
conform due to the usage of a typedef. Since it does not compromise the kernel, I think this is a valid
approach.

.. code-block:: c
   :caption: rainbow_hat_device to ease access to the devices functionality. (Object oriented approach)

   typedef struct rainbow_hat_dev {
      int leds_fd, buttons_fd, buzzer_fd;
      // Function pointer for turning LEDs on
      int (*leds_on)(struct rainbow_hat_dev *dev, led_arg led_args, char err_buff[]);
      // Function pointer for turning LEDs off
      int (*leds_off)(struct rainbow_hat_dev *dev, char err_buff[]);
      // Function pointer for playing a tone
      int (*play_tone)(struct rainbow_hat_dev *dev, unsigned long frequency, char err_buff[]);
      // Function pointer for getting the active button
      int (*get_btn)(struct rainbow_hat_dev *dev, char *btn_active, char err_buff[]);
   } rainbow_hat_dev;

This allows for an object containing all resources required and provides (methods) functions
the driver / abstraction layer provides.

To create a rainbow_hat_device, the ``init_rainbow_hat()`` free needs to be called.

.. code-block:: c

   rainbow_hat_dev *init_rainbow_hat(char error_buffer[]);

To clean up any recourses created, the ``free_rainbow_hat()`` function needs to be called.

.. code-block:: c

   void free_rainbow_hat(rainbow_hat_dev *rainbowHatDev);

Implementation
==============

Global
------

The device files are pre-defined by the driver and are therefore defined as constants within the library.

.. code-block:: c

   static const char *BUTTONS_DEV = "/dev/rainbow_buttons";
   static const char *BUZZER_DEV = "/dev/rainbow_buzzer";
   static const char *LEDS_DEV = "/dev/rainbow_leds";

The Rainbow HAT has 3 Buttons (A, B and C) and 7 LEDs, therefore they can also be defined as constant.

.. code-block:: c

   const char BUTTON_NAMES[3] = {'A', 'B', 'C'};
   const int PIN_NUMBERS[MAX_AMOUNT_PINS] = {0, 1, 2, 3, 4, 5, 6};

The color Black turns any LED of when sent via SPI-Bus. Which is why it is also defined as a local Constant.

.. code-block:: c

   static const char *COL_BLACK = "000000";

The LED write function allows an argument like (0:ff00ff, .. , 6:00ff00), which is limited due to the
amount of LEDs the Rainbow HAT provides. Therefore a fixed size buffer can be pre-defined.

.. code-block:: c

   static char LED_ARRAY[LED_ARRAY_SIZE] = {0};

Light LEDs
----------

In order to make the user input as easy as possible a helper function to create the drivers
expected char array is defined. The function can handle two different settings. These function
are library functions and are not accessible from outer scope.

- One color + multiple LED-pins (sets all provided LED-pins to one color)
- Amount colors == Amount LEDs (Individually linking LED-pin with unique color)

.. code-block:: c

   static int create_led_array(const int pins[], int amount_leds, const char *hex_color[], int amount_colors)
   {
      // Input argument validation

      char tmp[20];

      // clear the global LED_ARRAY
      memset(LED_ARRAY, '\0', sizeof(LED_ARRAY));
      // concat the arguments to the expected char array
      for (int idx = 0; idx < amount_leds; ++idx) {
         snprintf(tmp, sizeof(tmp), "%s%d:%s", (idx > 0) ? "," : "", pins[idx],
                (amount_colors == 1 || amount_colors == amount_leds) ?
               hex_color[min(idx, amount_colors-1)] : hex_color[0]);

         // Check if concatenating tmp to LED_ARRAY would overflow
         // Error handling
         // Append tmp to Global LED_ARRAY
         strcat(LED_ARRAY, tmp);
      }
      return 0;
   }


To light any LED the file descriptor is used and the global LED_ARRAY is written to the device file.

.. code-block:: c

   static int set_leds(int led_fd, char err_buff[])
   {
   // set to -1 to indicate an error by default
   ssize_t bytes_written = -1;

   if (led_fd >= 0) {
      // Write LED_ARRAY to device file
      bytes_written = write(led_fd, LED_ARRAY, strlen(LED_ARRAY));
      // error handling
   }
   // Return -1 if error occurred (bytes_written < 0) or 0 for success
   return bytes_written < 0 ? -1 : 0;

LEDs on
-------

The function ``led_light_on`` is assigned to the ``rainbow_hat_dev`` as ``leds_on`` method to
light up one or more LEDs. In case an error occurs the error buffer contains an appropriate
message.

.. code-block:: c

   int led_light_on(rainbow_hat_dev *dev, led_arg ledArg, char err_buff[])
   {
      // Assign user input to global LED_ARRAY
      create_led_array(ledArg.pins, ledArg.amount_pins, ledArg.colors, ledArg.amount_colors);
      // Error handling

      // return set_leds value (-1: Error, 0: Success)
      return set_leds(dev->leds_fd, err_buff);
   }

LEDs off
--------

``led_light_off`` disables all LEDs at once. It is assigned to the ``rainbow_hat_dev`` as ``leds_off``.

.. code-block:: c

   int led_light_off(rainbow_hat_dev *dev, char err_buff[])
   {
      // create ledArg which is always the same for this functionality
      led_arg ledArg = {
         .amount_pins = MAX_AMOUNT_PINS,  // Amount of pins is all
         .pins = (int *)PIN_NUMBERS,   // All pins available
         .amount_colors = 1,        // only one color
         .colors = &COL_BLACK,      // set to black
      };
      return led_light_on(dev, ledArg, err_buff);


Get active Button
-----------------

The function ``get_active_button`` is assigned to the ``rainbow_hat_dev`` as ``get_btn`` method.
It returns the state of all buttons. The multiplexer of the Rainbow HAT device however only allows
one active button. Only the button pressed first will be acknowledged and returned.
If an error occurs the err_buff will contain an appropriate message.

.. code-block:: c

   int get_active_button(rainbow_hat_dev *dev, char *btn_active, char err_buff[])
   {
      // create small buffer to hold result, limited by null-byte
      char btn_state[4] = {'0', '0', '0', '\0'};
      ssize_t result = -1;
      // No matter what the provided char contains it will be reset
      *btn_active = '\0';

      if (dev->buttons_fd >= 0) {
         result = read(dev->buttons_fd, btn_state, 3);
         // Error handling
      } else {
         // check if button was pressed and return first result
         for (int i = 0; i < 3; i++) {
            if (btn_state[i] == '1') {
               *btn_active = BUTTON_NAMES[i];
               break;
         }
      }
      return result < 0 ? -1 : 0;
   }

Use Buzzer
----------

The function ``play_tone`` is assigned to the ``rainbow_hat_dev`` as ``play_tone`` method
to play a tone using the buzzer device. In case an error occurs the error buffer contains an
appropriate message.

.. code-block:: c

   int play_tone(rainbow_hat_dev *dev, unsigned long frequency, char err_buff[])
   {
      // initialize bytes_written with -1 to indicate error by default
      ssize_t bytes_written = -1;
      if (dev->buzzer_fd >= 0) {
         // write frequency to device file
         bytes_written = write(dev->buzzer_fd, &frequency, sizeof(frequency)) < 0;
         if (bytes_written < 0) {
         // Error handling
      }
      return bytes_written < 0 ? -1 : 0;
   }


Functionality
=============

Retrieve a rainbow_hat_dev using the init function.

.. code-block:: c

   char error_buffer[ERR_BUF_SIZE];
   rainbow_hat_dev *dev = init_rainbow_hat(error_buffer);

.. code-block:: c
   :caption: Example initializing LED args.

   int pin_numbers[7] = {0, 1, 2, 3, 4, 5, 6}
   const char *color = "1:FF00FF,3:00FF00"

   led_arg ledArg = {
      .amount_pins = amount_of_pins,   // amount of pins to be lit
      .pins = (int *)PIN_NUMBERS,      // LED pins to be lit
      .amount_colors = 1,              // amount of colors
      .colors = &color,                // color array
   };

Turn LED(s) on: |break|
``dev->leds_on(dev, led_arg, error_buffer);``

Turn all LEDs off: |break|
``dev->leds_off(dev, error_buffer);``

Play a tone using buzzer: |break|
``dev->play_tone(dev, int, error_buffer);``

Get pressed/active button: |break|
``dev->get_btn(dev, char*, error_buffer);``