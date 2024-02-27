Project
#######

Description
***********

Overview
========

This project is designed to demystify the process of developing a driver for Linux platforms, specifically focusing
on Miscellaneous Devices. Our case study involves creating a driver for the Raspberry Pi Zero, equipped with a Rainbow HAT.

The core objective is to develop a driver that enables control over the Rainbow HAT's features: the LED strip (Arc),
its three tactile buttons, and the buzzer for tone generation. Successful completion of this driver will be followed
by the development of a comprehensive library. This library will offer simplified access to the driver's functionalities,
facilitating the creation of user-space applications.

One such application we aim to develop is a metronome, which utilizes the LED strip for visual tempo indication.
Additionally, we plan to implement a feature that simulates piano sounds when the buttons are pressed, enhancing
the interactive experience.

Below is a schematic representation of the project's architecture, providing a visual guide to how the components
interconnect and function together.

Architecture
============

.. figure:: /_static/architecture.svg
   :alt: Image of project architecture


Driver
======

Platform Driver
---------------

Device tree snippet relevant to developing the driver. The property ``buttons-gpios`` must be referenced as
``button`` inside the driver.

.. code-block:: none
   :caption: rainbow-hat.dts device tree overlay fragment.
   :emphasize-lines: 5, 13, 14, 15

   fragment@3 {
		target-path = "/";
		__overlay__ {
			rainbow_hat {
				compatible = "tha,rainbow-hat";
				status = "okay";

				/* LEDs */
				led-gpios = <&gpio  6 1>, /* red */
					    <&gpio 19 1>, /* green */
					    <&gpio 26 1>; /* blue */
				/* Buttons */
				button-gpios = <&gpio 21 0>, /* A */
					       <&gpio 20 0>, /* B */
					       <&gpio 16 0>; /* C */

				pwms = <&pwm 1 100000000>;
			};
		};
	};

Buttons
_______

The Rainbow HAT features three buttons (A, B, and C), which are accessible via three GPIOs. To access multiple
GPIOs simultaneously, the function ``gpiod_get_array`` can be utilized. To allow the kernel to manage memory for
these GPIOs, the ``devm_*`` functions can be employed.

.. code-block:: c

   devm_gpiod_get_array(platform_device, "property", FLAG);

Specifications
~~~~~~~~~~~~~~

   - When the driver is loaded using the ``insmod`` command, a device file named ``/dev/rainbow_buttons`` will be created.
   - Any number of applications will be able to read the file.
   - Write operations to the file are permitted.
   - The ``read`` function of the driver is designed to report the status of the three buttons |break| (1: pressed, 0: released).
   - Error handling and return values must be managed properly.
   - ``checkpatch.pl`` affirmation required!


Buzzer
______

The Buzzer is attached to a PWM pin. Accessing it can be achieved with ``pwm_get()``. Again, it is recommended to use
`devm_*` functions and allow the kernel to manage the memory.

.. code-block:: c

   devm_pwm_get(platform_device, NULL);

Specifications
~~~~~~~~~~~~~~

   - When the driver is loaded using ``insmod``, an additional device file called ``/dev/rainbow_buzzer`` will be created.
   - The device file does not provide read access.
   - Only one user-space application at a time is allowed to write to this file.
   - Error handling and return values must be managed properly.
   - ``checkpatch.pl`` affirmation required!


SPI Driver
----------

Device tree snippet relevant to developing the driver.

.. code-block:: none
   :caption: rainbow-hat.dts device tree overlay fragment.
   :emphasize-lines: 8

   fragment@6 {
		target = <&spi0>;
		__overlay__ {
			/* needed to avoid dtc warning */
			#address-cells = <1>;
			#size-cells = <0>;
			spirainbow: spirainbow@0 {
				compatible = "tha,rainbow-arc";
				reg = <0>;	/* CE1 */
				#address-cells = <0>;
				#size-cells = <0>;
				spi-max-frequency = <1000000>;
				status = "okay";
			};
		};
	};

LEDs
____

The LEDs can be controlled using the SPI-Bus, therefore a SPI-Driver is required.

Specifications
~~~~~~~~~~~~~~

   - On loading the driver a device file called ``/dev/rainbow_leds`` shall be created.
   - Only one user-space-application at a time is allowed to change the RGB values at a time (write the file).
   - Error handling and return values must be managed properly.
   - ``checkpatch.pl`` affirmation required!

User-Space-Application
======================

Piano
-----

Specifications
______________

   - Application must be written using C-programming language
   - Pressing a button shall play a tone using the devices buzzer
   - On button release the tone must stop
   - The following tones/frequencies shall be associated with the buttons.

.. list-table:: Button tones
   :widths: 30 30 40
   :header-rows: 1

   * - Button
     - Tone
     - Frequency
   * - A
     - C
     - 262 Hz
   * - B
     - E
     - 330 Hz
   * - C
     - G
     - 392 Hz

Metronome
---------

The metronome is supposed to visually display a three-four time in a certain speed with certain colors, defined
by the user.

Specifications
______________

   - Application must be written in C.
   - bpm is variable depending on user input.
   - color for first, second and third beat are variable depending on user input.
   - The first beat has a unique color.
   - The second and third beat share the same color.
   - Error handling must be properly implemented.
   - ``checkpatch.pl`` affirmation required!

.. code-block:: none
   :caption: Pseudo code example

   while True:
      switch on all LEDs in color_1
      wait
      switch off all LEDs
      wait
      switch on LED 4, 5 and 6 display color_2
      wait
      switch off all LEDs
      wait
      switch on LED 0, 1 and 2 display color_2
      wait
      switch off all LEDs
      wait
