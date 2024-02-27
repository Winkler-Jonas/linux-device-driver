Piano
*****

.. code-block:: c
   :caption: Prototype piano_simulation

   int piano_simulation(rainbow_hat_dev *dev, int freq_a, int freq_b, int freq_c, char err_buff[]);

To make sure the user input is valid, firstly the rainbow_hat_dev must be none-null! In theory
the err_buff could be null, however if it is, no error_messages can be retrieved. Therefore an
immediate function return is executed.

.. code-block:: c
   :caption: Argument validation

   if (!dev || !err_buff) {
      fprintf(stderr, "Piano simulator - Invalid arguments!\n");
      return -1;
   }

A single char buffer is required to store the active button. It can be initialized
like in the example bellow, but must not. Buffer will be reset by library.

.. code-block:: c

   char active_btn = '\0';

The piano simulation is specified to run indefinitely, therefore an infinity loop is
initiated. The ``keep_running`` variable is of ``atomic`` nature for thread safety.
If the piano_simulation is run by it self, this is not required. ``while(1)`` in this
case would be sufficient.

.. code-block:: c

   while (keep_running) {
      // retrieve button state
      if (dev->get_btn(dev, &active_btn, err_buff) < 0) {
         // print Error stored in err_buff
         fprintf(stderr, "Error reading Button data: %s\n", err_buff);
         goto err_exit;
		}
      .
      .
      .
   }

If a button was pressed a switch case is run trough to decide which tone should be played.
The frequencies in this case were pre-defined. The simulation could however play any tones.

.. code-block:: c
   :caption: pre-defined tones

   #define FREQ_C 262
   #define FREQ_E 330
   #define FREQ_G 392

.. code-block:: c
   :caption: Switching tone for button press action

   switch (active_btn) {
   case 'A':
      // play adequate tone
      if (dev->play_tone(dev, freq_a, err_buff) < 0) {
         fprintf(stderr, "Error playing tone associated with button A: %s\n", err_buff);
         goto err_exit;
      }
      break;
   case 'B':
      if (dev->play_tone(dev, freq_b, err_buff) < 0) {
         fprintf(stderr, "Error playing tone associated with button B: %s\n", err_buff);
         goto err_exit;
      }
      break;
   case 'C':
      if (dev->play_tone(dev, freq_c, err_buff) < 0) {
         fprintf(stderr, "Error playing tone associated with button C: %s\n", err_buff);
         goto err_exit;
      }
      break;
   }

If there is no active button - no button was pressed or the active button was released the
tone should stop playing.

.. code-block:: c

   // stop playing sound if no button is pressed
   if (dev->play_tone(dev, 0, err_buff) < 0) {
      fprintf(stderr, "Error muting buzzer: %s\n", err_buff);
      goto err_exit;
   }

Before continuing the infinity loop a short sleep is initiated to lessen the CPU usage.

.. code-block:: c

   usleep(10000); // short sleep to reduce CPU usage

Whenever an error occures the function jumps to the label ``err_exit`` which tries
to signal ``SIGTERM``. Enabling the ``signal_handler()`` to set ``keep_running`` to 0.
Effectively stopping the infinity loop. If ``SIGTERM`` fail for any reason the program
exits prematurely.

.. code-block:: c

   err_exit: // err occurred -> cleanup - or - hard exit
      if (keep_running) {
         if (kill(getpid(), SIGTERM) == -1) {
            fprintf(stderr, "Proc: %d - Error sending SIGTERM\n", getpid());
            exit(EXIT_FAILURE);
         }
      }