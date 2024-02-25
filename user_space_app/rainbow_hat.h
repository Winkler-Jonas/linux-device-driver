/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef LINUX_KERNEL_MODULE_WITH_CLION_IDE_SUPPORT_CMAKE_RAINBOW_HAT_H
#define LINUX_KERNEL_MODULE_WITH_CLION_IDE_SUPPORT_CMAKE_RAINBOW_HAT_H
#include <unistd.h>
#include <string.h>

// For debugging, set to 1 to enable debug prints
#define DEBUG 0

#if DEBUG
	#include <stdio.h> // Ensure printf is available
	#define debug_print(fmt, ...) printf("DEBUG: " fmt, ##__VA_ARGS__)
#else
	#define debug_print(fmt, ...) // Define as empty in non-debug builds
#endif

// The maximum amount of LEDs present on the Rainbow HAT Arc
#define MAX_AMOUNT_PINS 7
// Size the error_buffer should at least have
#define ERR_BUF_SIZE 256

extern const int PIN_NUMBERS[MAX_AMOUNT_PINS];
extern const char BUTTON_NAMES[3];

/*
 * For checkpatch.pl - yes these are typedefs and I will keep them
 * because they enhance readability of the code and do not enter
 * any kernel-space. Sole purpose is easing use for user-space
 * applications.
*/
typedef struct led_arg {
	int amount_colors;
	const char **colors;
	int amount_pins;
	int *pins;
} led_arg;

// The rainbow_hat_dev stores all relevant information the driver requires and offers
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

// Call init to create a new rainbow_hat_dev
rainbow_hat_dev *init_rainbow_hat(char error_buffer[]);
// free to free any devices created with init function
void free_rainbow_hat(rainbow_hat_dev *rainbowHatDev);

#endif
