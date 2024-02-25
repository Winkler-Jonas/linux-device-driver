// SPDX-License-Identifier: GPL-2.0+
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "rainbow_hat.h"

// macro to find the smaller of two values
#define min(a, b) ((a) < (b) ? (a) : (b))

// Constants
static const char *BUTTONS_DEV = "/dev/rainbow_buttons";
static const char *BUZZER_DEV = "/dev/rainbow_buzzer";
static const char *LEDS_DEV = "/dev/rainbow_leds";

static const char *COL_BLACK = "000000";
const int PIN_NUMBERS[MAX_AMOUNT_PINS] = {0, 1, 2, 3, 4, 5, 6};

const char BUTTON_NAMES[3] = {'A', 'B', 'C'};

// Module variables
#define LED_ARRAY_SIZE 70
static char LED_ARRAY[LED_ARRAY_SIZE] = {0};

static int write_error(char err_buff[], char *err_str)
{
	/*
	 * Function populates error_buffer provided by user.
	 * New-line character will always be attached.
	 *
	 * Return-values:
	 *	- 0	-> Success
	 *	- 1	-> Severe Error
	 *	- 2	-> Error message to long
	 */
	int result;

	result = snprintf(err_buff, ERR_BUF_SIZE, "%s\n", err_str);

	if (result < 0) {
		// Handle formatting error
		fprintf(stderr, "Error occurred during snprintf formatting.\n");
		return 1;
	} else if (result >= ERR_BUF_SIZE) {
		// Handle truncation (not all characters were written)
		fprintf(stderr, "Warning: snprintf output truncated. Needed size: %d\n", result);
		return 2;
	}
	return 0;
}

static int set_leds(int led_fd, char err_buff[])
{
	/*
	 * Function writes provided data to misc-device to change LEDs-color.
	 * If the driver encounters any error, appropriate err-msg will be stored in err_buff.
	 *
	 * Args:
	 *	- led_fd	-> LED-misc-file-descriptor
	 *	- err_buff	-> buffer to store error
	 *
	 * Return-values:
	 *	- 0	-> Success
	 *	- -1	-> Failure (See err_buff for info)
	 */
	ssize_t bytes_written = -1;

	if (led_fd >= 0) {
		bytes_written = write(led_fd, LED_ARRAY, strlen(LED_ARRAY));
		if (bytes_written < 0) {
			debug_print("Error writing to %s Error: %d\n", LEDS_DEV, bytes_written);
			switch (bytes_written) {
			case -ENOMEM:
				write_error(err_buff, "LED-Device: Insufficient memory!");
				break;
			case -ERESTART:
				write_error(err_buff, "LED-Device: Device busy!");
				break;
			case -EFAULT:
				write_error(err_buff, "LED-Device: Segmentation fault!");
				break;
			case -EINVAL:
				write_error(err_buff, "LED-Device: Invalid argument!");
				break;
			case -EIO:
				write_error(err_buff, "LED-Device: Device error!");
				break;
			default:
				write_error(err_buff, "LED-Device: Unexpected error occurred!");
			}
		}
	}
	return bytes_written < 0 ? -1 : 0;
}

static int create_led_array(const int pins[], int amount_leds, const char *hex_color[], int amount_colors)
{
	/*
	 * Function tries to zip the tow provided arrays.
	 *
	 * Args:
	 *	- pins		-> Array containing pin-values (0-6)
	 *	- amount_leds	-> length of pins-array
	 *	- hex_color	-> Array containing hex-color-codes
	 *	- amount-colors	-> length of hex_color-array
	 *
	 * Result is stored in module-var LED_ARRAY
	 *
	 * Return-values:
	 *	-  0 -> Success
	 *	- -1 -> Error (Invalid argument, different length of arrays)
	 *	- -2 -> Unexpected buffer overflow.
	 */
	char tmp[20];

	if (amount_leds > MAX_AMOUNT_PINS || amount_colors > MAX_AMOUNT_PINS ||
		amount_leds <= 0 || amount_colors <= 0) {

		printf("LEDS: %d, Colors: %d", amount_leds, amount_colors);
		return -1; // Invalid argument
	}

	memset(LED_ARRAY, '\0', sizeof(LED_ARRAY));
	for (int idx = 0; idx < amount_leds; ++idx) {
		snprintf(tmp, sizeof(tmp), "%s%d:%s", (idx > 0) ? "," : "", pins[idx],
				 (amount_colors == 1 || amount_colors == amount_leds) ?
				hex_color[min(idx, amount_colors-1)] : hex_color[0]);

		// Check if concatenating tmp to LED_ARRAY would overflow
		if (strlen(LED_ARRAY) + strlen(tmp) >= LED_ARRAY_SIZE)
			return -2; // Buffer overflow
		strcat(LED_ARRAY, tmp);
	}
	return 0;
}

int led_light_on(rainbow_hat_dev *dev, led_arg ledArg, char err_buff[])
{
	/*
	 * Function lights multiple LEDs of the Rainbow HAT device.
	 *
	 * Args:
	 *	- dev		-> pointer to rainbow-hat-dev
	 *	- pin_nbrs	-> value of pin to be lit (will be zipped with hex_color)
	 *	- hex_color	-> hex-values of colors to be applied (will be zipped with pin_nbrs)
	 *
	 * Return-value:
	 *	-  0	-> Success
	 *	- <0	-> Error (Check err-buff for info)
	 */
	if (dev == NULL) {
		write_error(err_buff, "LED-LIGHT-ON: Device-Error (Null-Pointer)!");
		return -1;
	}
	if (create_led_array(ledArg.pins, ledArg.amount_pins, ledArg.colors, ledArg.amount_colors)) {
		write_error(err_buff, "LED-LIGHT-ON: Invalid argument!");
		return -1;
	}
	return set_leds(dev->leds_fd, err_buff);
}

int led_light_off(rainbow_hat_dev *dev, char err_buff[])
{
	/*
	 * Function tuns off all LEDs.
	 *
	 * Args:
	 *	- dev		-> rainbow hat device
	 *	- err_buff	-> buffer to store err-msg
	 *
	 * Return-value:
	 *	-  0	-> Success
	 *	- <0	-> Error (check err_buff for info)
	 */
	if (dev == NULL) {
		write_error(err_buff, "LED-LIGHT-OFF: Device Error (Null-Pointer)!");
		return -1;
	}
	led_arg ledArg = {
		.amount_pins = MAX_AMOUNT_PINS,
		.pins = (int *)PIN_NUMBERS,
		.amount_colors = 1,
		.colors = &COL_BLACK,
	};
	return led_light_on(dev, ledArg, err_buff);
}

int play_tone(rainbow_hat_dev *dev, unsigned long frequency, char err_buff[])
{
	/*
	 * Function plays a tone depending on the frequency provided.
	 *
	 * Args:
	 *	- dev		-> rainbow hat device.
	 *	- frequency	-> The frequency to be played by the buzzer.
	 *	- err_buff	-> buffer to store potential err-msg.
	 *
	 * Return-value:
	 *	-  0	-> Success
	 *	- -1	-> Error (see err_buff for info)
	 */
	if (dev == NULL) {
		write_error(err_buff, "Buzzer-Device: Device Error (Null-Pointer)!");
		return -1;
	}

	ssize_t bytes_written = -1;

	if (dev->buzzer_fd >= 0) {
		bytes_written = write(dev->buzzer_fd, &frequency, sizeof(frequency)) < 0;
		if (bytes_written < 0) {
			debug_print("Error writing to %s Error: %dn", BUZZER_DEV, bytes_written);
			switch (bytes_written) {
			case -EINVAL:
				write_error(err_buff, "Buzzer-Device: Invalid arguments!");
				break;
			case -EBUSY:
				write_error(err_buff, "Buzzer-Device: Device busy!");
				break;
			case -EFAULT:
				write_error(err_buff, "Buzzer-Device: Segmentation fault!");
				break;
			case -ERANGE:
				write_error(err_buff, "Buzzer-Device: Invalid argument - validate frequency!");
				break;
			default:
				write_error(err_buff, "Buzzer-Device: Unexpected error occurred!");
			}
		}
	}
	return bytes_written < 0 ? -1 : 0;
}

int get_active_button(rainbow_hat_dev *dev, char *btn_active, char err_buff[])
{
	/*
	 * Function retrieves button state.
	 * (The exam specifies to check every button concurrently - even
	 * though the rainbow hat's multiplexer is only able to recognise
	 * one button press at time - more specific the first button that
	 * applies the voltage is recognised any following press cannot be
	 * identified by the hardware. The driver does implement a potential
	 * hardware update - this function however does not.)
	 *
	 * Args:
	 *	- dev		-> Rainbow hat device
	 *	- btn_active	-> Char pointer to retrieve pressed button
	 *	- err_buff	-> err_buff to store potential err-msg
	 *
	 * Return-value:
	 *	-  0	-> Success
	 *	- -1	-> Error (see err_buff for info)
	 */
	if (dev == NULL) {
		write_error(err_buff, "Button-Device: Device Error (Null-Pointer)!");
		return -1;
	}

	char btn_state[4] = {'0', '0', '0', '\0'};
	ssize_t result = -1;
	*btn_active = '\0';

	if (dev->buttons_fd >= 0) {
		result = read(dev->buttons_fd, btn_state, 3);
		if (result < 0) {
			switch (result) {
			case -EINVAL:
				write_error(err_buff, "Button-Device: Invalid argument!");
				break;
			case -EFAULT:
				write_error(err_buff, "Button-Device: Segmentation Fault!");
				break;
			default:
				write_error(err_buff, "Button-Device: Unexpected Error occurred!");
				break;
			}
		} else {
			for (int i = 0; i < 3; i++) {
				if (btn_state[i] == '1') {
					*btn_active = BUTTON_NAMES[i];
					break;
				}
			}
		}
	}
	return result < 0 ? -1 : 0;
}

rainbow_hat_dev *init_rainbow_hat(char error_buffer[])
{
	/*
	 * Function create rainbow_hat_device instance to interact with the device.
	 *
	 * Returned device must be freed by user! **See free_rainbow_hat()**
	 *
	 * Return-values:
	 *	- NULL			-> Error occurred (check error_buffer for details)
	 *	- rainbow_hat_dev	-> Device containing functions and file-descriptors.
	 */
	rainbow_hat_dev *dev = (rainbow_hat_dev *)malloc(sizeof(rainbow_hat_dev));

	if (dev == NULL) {
		debug_print("Failed to allocate memory for rainbow_hat_device\n");
		write_error(error_buffer, "INIT: Insufficient memory!");
		return NULL;
	}

	dev->leds_fd = open(LEDS_DEV, O_WRONLY);
	dev->buttons_fd = open(BUTTONS_DEV, O_RDONLY);
	dev->buzzer_fd = open(BUZZER_DEV, O_WRONLY);

	if (dev->leds_fd < 0) {
		debug_print("LED device open failed\n");
		write_error(error_buffer, "LED device open failed");
		goto cleanup;
	}

	if (dev->buttons_fd < 0) {
		debug_print("Buttons device open failed\n");
		write_error(error_buffer, "Buttons device open failed");
		goto cleanup;
	}

	if (dev->buzzer_fd < 0) {
		debug_print("Buzzer device open failed\n");
		write_error(error_buffer, "Buzzer device open failed");
		goto cleanup;
	}

	dev->leds_on = led_light_on;
	dev->leds_off = led_light_off;
	dev->play_tone = play_tone;
	dev->get_btn = get_active_button;

	return dev;

cleanup:
	free_rainbow_hat(dev);
	return NULL;
}

void free_rainbow_hat(rainbow_hat_dev *rainbowHatDev)
{
	/*
	 * Function frees resources of provided rainbow_hat_device
	 */
	if (rainbowHatDev) {
		if (rainbowHatDev->leds_fd >= 0) {
			close(rainbowHatDev->leds_fd);
			rainbowHatDev->leds_fd = -1;
			debug_print("LED device closed\n");
		}
		if (rainbowHatDev->buttons_fd >= 0) {
			close(rainbowHatDev->buttons_fd);
			rainbowHatDev->buttons_fd = -1;
			debug_print("Buttons device closed\n");
		}
		if (rainbowHatDev->buzzer_fd >= 0) {
			close(rainbowHatDev->buzzer_fd);
			rainbowHatDev->buzzer_fd = -1;
			debug_print("Buzzer device closed\n");
		}

		free(rainbowHatDev);
		rainbowHatDev = NULL;
		debug_print("Rainbow HAT device closed\n");
	}
}

