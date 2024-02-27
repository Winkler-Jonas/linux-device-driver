Metronome
*********

.. code-block:: c
   :caption: prototype metronome_led_sequence

   int metronome_led_sequence(rainbow_hat_dev *dev,
                              int bpm,                   // beats per minute
                              const char *color_beat_1,  // color for beat 1
			                     const char *color_beat_2,  // color for beat 2 + 3
                              char err_buff[])

To make sure the user input is valid, firstly the rainbow_hat_dev must be none-null! In theory
the err_buff could be null, however if it is, no error_messages can be retrieved. Therefore an
immediate function return is executed.

.. code-block:: c
   :caption: Argument validation

   if (!dev || !err_buff) {
      fprintf(stderr, "Metronome LED sequence - Invalid arguments!\n");
      return -1;
   }

The function requires a couple local variables to ease processing.

.. code-block:: c

   const int beat_duration_us = (int) (60.0 / bpm * 1000000);
   const int sleep_duration_us = 100000; // 100 ms
   int second_third_beat[3];        // buffer for 2. + 3. beat LED-pins
   const char *current_color;       // all pins are lit with the same color
   int current_amount_pins;         // 1. beat has 7 pins, 2. + 3. only 3
   int *current_pins;               // helping array

.. code-block:: c
   :caption: initiating led_arg (it is always just one color used)

   led_arg beat_arg = {
      .amount_colors = 1,
   };

The metronome is supposed to run indefinitely, therefor an infinity loop is initiated.
Here ``keep_running`` an atomic value is used, because both the piano and the metronome
run simultaneously. Otherwise ``while(1)`` would be sufficient.

.. code-block:: c

   while (keep_running) {
      for (int beat = 1; beat <= 3; beat++) {
         // Adjust color depending on the beat
         current_color = (beat == 1) ? color_beat_1 : color_beat_2;

         // switch pins and color between beats
      }
   }

To switch the colors and pins between the beats. The led_arg needs to be altered.
To simplify slicing an array like in python, a small helper macro is defined in
the preprocessor.

.. code-block:: c
   :caption: slice macro in pre-processor

   #define slice(buf, src, start, end) (memcpy((buf), &(src)[(start)], sizeof((src)[0]) * ((end) - (start))))

.. code-block:: c
   :caption: Switching pins and color between beats

   switch (beat) {
   case 1:
      current_pins = (int *) PIN_NUMBERS;       // 1. Beat requires all Pins
      current_amount_pins = MAX_AMOUNT_PINS;    // MAX_AMOUNT is 7
      break;
   case 2:
      // slice PIN_NUMBERS (containing all pins) like [4:] in python assigning result to second_third_beat
      slice(second_third_beat, PIN_NUMBERS, 4, MAX_AMOUNT_PINS);
      current_pins = second_third_beat;         // assign sliced array to current_pins used for led_args
      current_amount_pins = 3;                  // amount of pins is 3
      break;
   case 3:
      slice(second_third_beat, PIN_NUMBERS, 0, 3);
      current_pins = second_third_beat;
      current_amount_pins = 3;
      break;
   }

   beat_arg.colors = &current_color;      // assign the current color
   beat_arg.pins = current_pins;          // assign the pins
   beat_arg.amount_pins = current_amount_pins;  // assign the amount of pins

Before changing any LED colors a start time is stored. This allows to calculate
the execution time and keep the metronome at sync due to execution delay.

.. code-block:: c

   clock_gettime(CLOCK_MONOTONIC, &start_time);

Now the LEDs can be lit using the prior processed arguments, using the devices method
``leds_on()``. After lighting the LEDs a short sleep is initiated to keep them lit.

.. code-block:: c
   :caption: Turning LEDs on

   if (dev->leds_on(dev, beat_arg, err_buff) < 0) {
      fprintf(stderr, "LEDs On Error: %s\n", err_buff);
      goto err_exit;
   }

   usleep(sleep_duration_us); // Wait a little bit (100 ms)

Once the thread wakes up again, the LEDs need to be turned off.

.. code-block:: c
   :caption: Turning LEDs off

   if (dev->leds_off(dev, err_buff) < 0) {
      fprintf(stderr, "LEDs Off Error: %s\n", err_buff);
      goto err_exit;
   }

When the execution is complete, the time-span it took is calculated and
the sleep adjusted before restarting the loop.

For simplification and readability a macro to calculate the elapsed time is
define in the pre-processor.

.. code-block:: c

   #define elapsed_time_us(start, end) \
	(((end)->tv_sec - (start)->tv_sec) * 1000000L + ((end)->tv_nsec - (start)->tv_nsec) / 1000L)

.. code-block:: c

   clock_gettime(CLOCK_MONOTONIC, &end_time);

   // adjust for led-on/off process time to keep metronome at sync
   long operation_time_us = elapsed_time_us(&start_time, &end_time);
   long adjusted_sleep_duration_us = beat_duration_us - operation_time_us;

   if (adjusted_sleep_duration_us > 0)
      usleep(adjusted_sleep_duration_us);

If an error occurs during execution, the ``err_exit`` label is jumped to.
It tries to send a ``SIGTERM`` signal. If successful, the ``signal_handler`` stops
the infinity loop by setting ``keep_running = 0;``. If signal for any reason cannot
be send, the program exits prematurely.

.. code-block:: c

   err_exit: // err occurred -> cleanup - or - hard exit
   if (keep_running) {
      if (kill(getpid(), SIGTERM) == -1) {
         fprintf(stderr, "Proc: %d - Error sending SIGTERM\n", getpid());
         exit(EXIT_FAILURE);
      }
   }