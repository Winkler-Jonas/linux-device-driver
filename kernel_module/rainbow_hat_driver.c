// SPDX-License-Identifier: GPL-2.0+
/*
 * This kernel-module implements:
 *      - Platform-Driver-Buzzer:
 *          User-space application provides frequency <int> in Hz which
 *          is played on Rainbow HAT (file: /dev/SEE-DEVICE_NAME_BUZZER)
 *      - Platform-Driver-Buttons:
 *          One User-space application can read status of Rainbow HAT
 *          Buttons. (1: Pressed, 0: Released) (file: /dev/SEE-DEVICE_NAME_BUTTONS)
 *      - SPI-Driver-LED-ARC:
 *          One User-space application can set Color for Rainbow HAT ARC-LEDs.
 *          Data Type expected <char*> -> "LEDnr:hex-color-code, ... ,LEDnr:hex-color-code"
 *          (file: /dev/SEE-DEVICE_NAME_LED)
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/string.h>

/* --------------------------------------------------- */
/* ------------------- PROLOGUE ---------------------- */
/* --------------------------------------------------- */

#define MODULE_NAME "rainbow_hat_driver"

// Device names
#define DEVICE_NAME_BUZZER "rainbow_buzzer"
#define DEVICE_NAME_BUTTON "rainbow_buttons"
#define DEVICE_NAME_LED "rainbow_leds"

// LED-Arc Setup
#define NUMBER_LEDS 7
#define NUMBER_START_BYTES 4
#define NUMBER_BYTES_PER_LED 4
#define NUMBER_STOP_BYTES 4
#define MAX_EXPECTED_INPUT_LENGTH 70 // +6 for user input-error like (\n)
#define LED_BRIGHTNESS 0xE0
#define MAX_BRIGHTNESS 0x1F

struct rgb_led {
	uint8_t r, g, b; // RGB values
};

struct rainbow_spi_dev {
	struct spi_device *spi;
	struct mutex lock;
	struct miscdevice misc_leds;
	struct rgb_led leds[NUMBER_LEDS];
};

struct rainbow_gpio_dev {
	struct pwm_device *buzzer_pwm;
	struct gpio_descs *button_gpios;
	struct miscdevice misc_buzzer;
	struct miscdevice misc_buttons;
	struct mutex lock;
};

/* --------------------------------------------------- */
/* --------------- Module functionality -------------- */
/* --------------------------------------------------- */

// Helper function to convert a hex character to its integer value
static int hex_to_int(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	return -1;
}

// Function to parse a hex color string into an RGB structure
static int parse_hex_color(const char *buf, unsigned int *color)
{
	int value;
	*color = 0;

	for (int i = 0; i < 6; i++) {
		value = hex_to_int(buf[i]);
		if (value < 0)
			return -EINVAL;
		*color = (*color << 4) | value;
	}
	return 0;
}

// Function to update LEDs using SPI-BUS
static int update_leds(struct rainbow_spi_dev *dev)
{
	if (!dev->spi) {
		pr_warn("SPI device not available\n");
		return -ENODEV;
	}

	uint8_t buffer[NUMBER_START_BYTES + NUMBER_LEDS * NUMBER_BYTES_PER_LED +
		       NUMBER_STOP_BYTES] = {0};
	int i;
	struct spi_message msg;
	struct spi_transfer spi_xfer = {
		.tx_buf = buffer,
		.len = sizeof(buffer),
		.bits_per_word = 8,
	};

	// Start frame
	memset(buffer, 0x00, NUMBER_START_BYTES);

	// LED frames

	for (i = 0; i < NUMBER_LEDS; i++) {
		int offset = NUMBER_START_BYTES + i * NUMBER_BYTES_PER_LED;

		buffer[offset] = LED_BRIGHTNESS | MAX_BRIGHTNESS;
		buffer[offset + 1] = dev->leds[i].b; // Blue
		buffer[offset + 2] = dev->leds[i].g; // Green
		buffer[offset + 3] = dev->leds[i].r; // Red
	}

	// End frame - Ensures the SPI command is latched.
	memset(&buffer[sizeof(buffer) - NUMBER_STOP_BYTES], 0xFF,
	       NUMBER_STOP_BYTES);

	spi_message_init(&msg);
	spi_message_add_tail(&spi_xfer, &msg);
	int ret = spi_sync(dev->spi, &msg);

	if (ret < 0) {
		pr_info("Failed to update LEDs via SPI: %d\n", ret);
		return ret;
	}
	return 0;
}

// Function to parse user-space input args
static int parse_led_colors(struct rainbow_spi_dev *dev,
			    const char *buf, size_t count)
{
	/*
	 * User-space-app input must look like
	 *      -> "LEDnr:color-hex-value"
	 *          --- or as array ---
	 *      -> "LEDnr:color-hex-value, .. ,LEDnr:color-hex-value"
	 * LED data-format:
	 * Byte 1: Blue value
	 * Byte 2: Green value
	 * Byte 3: Red value
	 */
	const char *curr = buf;
	const char *end = buf + count;
	unsigned int color;
	char *next;
	int ret;
	long led_num; // Use long to ensure compatibility with kstrtol

	while (curr < end && *curr != '\0') {
		char *colon = strchr(curr, ':');

		if (!colon) {
			pr_info("Colon not found\n");
			return -EINVAL;
		}

		// Temporarily create a string for the LED number
		char led_num_str[10] = {0}; // Large enough for any LED number
		int len = colon - curr;

		if (len >= sizeof(led_num_str)) {
			pr_info("LED number string too long\n");
			return -EINVAL;
		}
		strncpy(led_num_str, curr, len);
		led_num_str[len] = '\0'; // Ensure null-termination

		// Parse the LED number
		ret = kstrtol(led_num_str, 10, &led_num);
		if (ret) {
			pr_info("Error parsing LED number: %d\n", ret);
			return -EINVAL;
		}
		if (led_num < 0 || led_num >= NUMBER_LEDS) {
			pr_info("LED number %ld out of range\n", led_num);
			return -EINVAL;
		}

		// Advance curr past the colon to the color code
		curr = colon + 1;

		// Now curr should point at the start of the color code
		if (parse_hex_color(curr, &color) < 0) {
			pr_info("RGB value parsing failed\n");
			return -EINVAL;
		}

		// Apply the color to the specified LED
		dev->leds[led_num].r = (color >> 16) & 0xFF;
		dev->leds[led_num].g = (color >> 8) & 0xFF;
		dev->leds[led_num].b = color & 0xFF;

		// Advance curr past the color code
		curr += 6; // Assuming the color code is always 6 characters long

		// Look for a comma indicating another LED-color pair; skip it if found
		next = strchr(curr, ',');
		if (next && next < end) {
			curr = next + 1; // Move past the comma
		} else {
			// If no more pairs are found, break out of the loop
			break;
		}
	}
	return 0;
}

static ssize_t rainbow_leds_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	if (count > MAX_EXPECTED_INPUT_LENGTH) {
		pr_info("Input to large, max %d bytes", MAX_EXPECTED_INPUT_LENGTH);
		return -EINVAL;
	}


	struct rainbow_spi_dev *spi_dev = container_of(file->private_data,
						       struct rainbow_spi_dev,
						       misc_leds);
	char *kbuf;
	int ret = 0;

	// Allocate buffer for user-space data
	kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM; // logged by kernel

	// Ensure exclusive access (interruptable to break if needed)
	if (mutex_lock_interruptible(&spi_dev->lock)) {
		ret = -ERESTARTSYS;
		pr_info("Device already in use! ERR-(%d)\n",
		       -ERESTARTSYS);
		goto out;
	}

	if (copy_from_user(kbuf, buf, count)) {
		ret = -EFAULT;
		pr_warn("Failed data transfer from user-space! ERR-(%d)\n",
		       -EFAULT);
		goto unlock;
	}
	kbuf[count] = '\0'; // Ensure string is null-terminated

	if (parse_led_colors(spi_dev, kbuf, count) != 0) {
		ret = -EINVAL;
		pr_info("Parsing input unsuccessful! ERR-(%d)\n",
		       -EINVAL);
		goto unlock;
	}

	// Update the LEDs via SPI
	if (update_leds(spi_dev) != 0) {
		ret = -EIO;
		pr_info("Error updating LED(s)! ERR-(%d)\n", -EIO);
		goto unlock;
	}

	ret = count;

unlock:
	mutex_unlock(&spi_dev->lock);
out:
	kfree(kbuf);
	return ret;
}

static ssize_t rainbow_buttons_read(struct file *file, char __user *buf,
				    size_t len, loff_t *offset)
{
	struct rainbow_gpio_dev *gpio_dev = container_of(file->private_data,
							 struct rainbow_gpio_dev,
							 misc_buttons);
	char button_status[3]; // Buffer to hold the status of the 3 buttons
	int i;

	// Check if the user-space buffer is large enough to hold the status of all buttons
	if (len < sizeof(button_status)) {
		pr_info("Provided Buffer not sufficient in size! ERR-(%d)\n",
		       -EINVAL);
		return -EINVAL; // Return invalid argument error
	}

	// Read each button's GPIO value, invert it, and store it in the buffer
	for (i = 0; i < 3; i++) {
		int gpio_val = gpiod_get_value(gpio_dev->button_gpios->desc[i]);

		if (gpio_val < 0) {
			pr_info("Error retrieving button value! BTN-VAL-(%d)\n",
			       gpio_val);
			return gpio_val;
		}
		button_status[i] = gpio_val ? '0' : '1'; // Invert logic here
	}

	// Copy the button status to user-space
	if (copy_to_user(buf, button_status, sizeof(button_status))) {
		pr_info("Failed data transfer from user-space! ERR-(%d)\n",
		       -EFAULT);
		return -EFAULT; // Return bad address error if copy fails
	}

	// Update the offset to indicate that the data has been read
	*offset += sizeof(button_status);

	// Return the number of bytes read
	return sizeof(button_status);
}

static ssize_t rainbow_buzzer_write(struct file *file, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	struct rainbow_gpio_dev *gpio_dev = container_of(file->private_data,
							 struct rainbow_gpio_dev,
							 misc_buzzer);
	unsigned long freq;
	unsigned long period;
	int ret;

	// Ensure that the input size is correct
	if (count != sizeof(freq)) {
		pr_info("Invalid frequency! ERR-(%d)\n", -EINVAL);
		return -EINVAL;
	}

	if (!mutex_trylock(&gpio_dev->lock)) {
		pr_info("Buzzer is busy! ERR-(%d)\n", -EINVAL);
		return -EBUSY;
	}

	// Copy the frequency value from user space
	if (copy_from_user(&freq, buf, sizeof(freq))) {
		mutex_unlock(&gpio_dev->lock);
		pr_info("Failed data transfer from user-space! ERR-(%d)\n",
		       -EFAULT);
		return -EFAULT;
	}

	if (freq == 0) {
		// Turn off the buzzer
		pwm_disable(gpio_dev->buzzer_pwm);
		ret = count;
	} else {
		// Calculate the period (in nanoseconds) for the PWM signal
		period = 1000000000UL / freq;

		// Ensure period does not exceed INT_MAX before calling pwm_config
		if (period > INT_MAX) {
			mutex_unlock(&gpio_dev->lock);
			pr_info("Invalid frequency - Out of range! ERR-(%d)\n",
			       -ERANGE);
			return -ERANGE;
		}

		// Configure the PWM device
		ret = pwm_config(gpio_dev->buzzer_pwm, (int) (period / 2),
				 (int) period);
		if (ret == 0) {
			pwm_enable(gpio_dev->buzzer_pwm);
			ret = count;
		} else {
			pr_info("Unexpected Error occurred during buzzer config! ERR-(%d)\n",
			       ret);
			mutex_unlock(&gpio_dev->lock);
			return ret;
		}
	}

	mutex_unlock(&gpio_dev->lock);
	return ret;
}

/* --------------------------------------------------- */
/* ----------------- File operations ----------------- */
/* --------------------------------------------------- */

static const struct file_operations rainbow_leds_fops = {
	.owner = THIS_MODULE,
	.write = rainbow_leds_write,
};

static const struct file_operations rainbow_buzzer_fops = {
	.owner = THIS_MODULE,
	.write = rainbow_buzzer_write,
	.llseek = no_llseek, // prevent seek/reading operations
};

static const struct file_operations rainbow_buttons_fops = {
	.owner = THIS_MODULE,
	.read = rainbow_buttons_read,
};

/* --------------------------------------------------- */
/* ----------------- Driver Setup -------------------- */
/* --------------------------------------------------- */

static int rainbow_hat_spi_driver_probe(struct spi_device *spi)
{
	struct rainbow_spi_dev *dev;
	int err;

	// Allocate memory for device (devm* is automatically freed)
	dev = devm_kzalloc(&spi->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM; // logged by kernel

	dev->spi = spi;
	mutex_init(&dev->lock);

	// Initialize the misc-led device
	dev->misc_leds.minor = MISC_DYNAMIC_MINOR;
	dev->misc_leds.name = DEVICE_NAME_LED;
	dev->misc_leds.fops = &rainbow_leds_fops;
	dev->misc_leds.mode = 0222;

	// Register misc-led device
	err = misc_register(&dev->misc_leds);
	if (err) {
		dev_err(&spi->dev, "Failed to register misc device\n");
		return err;
	}
	// Successfully created and registered led-misc
	dev_info(&spi->dev, "/dev/%s created.\n", DEVICE_NAME_LED);
	// Save the device driver data for later use in (read/write) operations.
	spi_set_drvdata(spi, dev);
	return 0;
}

static void rainbow_hat_spi_driver_remove(struct spi_device *spi)
{
	struct rainbow_spi_dev *dev = spi_get_drvdata(spi);

	// Deregister the misc device
	if (dev) {
		misc_deregister(&dev->misc_leds);
		dev_info(&spi->dev, "/dev/%s removed.\n", DEVICE_NAME_LED);
	}
}

static int rainbow_hat_platform_driver_probe(struct platform_device *pdev)
{
	struct rainbow_gpio_dev *gpio_dev;
	struct device *dev = &pdev->dev;
	int ret;

	// Allocate memory for devices (devm* is automatically freed)
	gpio_dev = devm_kzalloc(dev, sizeof(*gpio_dev), GFP_KERNEL);
	if (!gpio_dev)
		return -ENOMEM; // logged by kernel


	/* ----------------------- Probe Buttons ------------------------- */

	// Retrieve the array of GPIOs for the buttons
	gpio_dev->button_gpios = devm_gpiod_get_array(dev, "button",
						      GPIOD_ASIS);
	if (IS_ERR(gpio_dev->button_gpios)) {
		dev_err(dev, "Failed to get GPIO array for buttons\n");
		return PTR_ERR(gpio_dev->button_gpios);
	}

	// Check we have the expected number of GPIOs for the buttons
	if (!gpio_dev->button_gpios || gpio_dev->button_gpios->ndescs != 3) {
		dev_err(dev, "Incorrect number of button GPIOs found\n");
		return -EINVAL;
	}

	// Initialize the misc-buttons device
	gpio_dev->misc_buttons.minor = MISC_DYNAMIC_MINOR;
	gpio_dev->misc_buttons.name = DEVICE_NAME_BUTTON;
	gpio_dev->misc_buttons.fops = &rainbow_buttons_fops;
	gpio_dev->misc_buttons.mode = 0444; // Allow read access

	// Register the misc-buttons device
	ret = misc_register(&gpio_dev->misc_buttons);
	if (ret) {
		dev_err(dev, "Could not register the buttons-misc device\n");
		return ret;
	}
	// Successfully created and registered buttons-misc
	dev_info(dev, "/dev/%s created.\n", DEVICE_NAME_BUTTON);

	/* ------------------------ Probe Buzzer ------------------------- */

	// Retrieve PWN GPIO
	gpio_dev->buzzer_pwm = devm_pwm_get(dev, NULL);
	if (IS_ERR(gpio_dev->buzzer_pwm)) {
		dev_err(dev, "Failed to acquire PWM device\n");
		return PTR_ERR(gpio_dev->buzzer_pwm);
	}

	// Initialize mutex (only one user-app is allowed to write)
	mutex_init(&gpio_dev->lock);

	// Initiate misc-buzzer device
	gpio_dev->misc_buzzer.minor = MISC_DYNAMIC_MINOR;
	gpio_dev->misc_buzzer.name = DEVICE_NAME_BUZZER;
	gpio_dev->misc_buzzer.fops = &rainbow_buzzer_fops;
	gpio_dev->misc_buzzer.mode = 0222; // set mode to only write

	// register misc-buzzer device
	ret = misc_register(&gpio_dev->misc_buzzer);
	if (ret) {
		dev_err(dev, "Failed to register misc device\n");
		return ret;
	}
	// Successfully created and registered buzzer-misc
	dev_info(dev, "/dev/%s created.\n", DEVICE_NAME_BUZZER);

	// Save the device driver data for later use in (read/write) operations.
	platform_set_drvdata(pdev, gpio_dev);
	return 0;
}

static int rainbow_hat_platform_driver_remove(struct platform_device *pdev)
{
	struct rainbow_gpio_dev *gpio_dev = platform_get_drvdata(pdev);

	// Deregister the misc-devices
	if (gpio_dev) {
		misc_deregister(&gpio_dev->misc_buttons);
		dev_info(&pdev->dev, "/dev/%s removed.\n", DEVICE_NAME_BUTTON);
		misc_deregister(&gpio_dev->misc_buzzer);
		dev_info(&pdev->dev, "/dev/%s removed.\n", DEVICE_NAME_BUZZER);
	}
	return 0;
}

// Device tree table with compatible strings for gpios and spi
static const struct of_device_id rainbow_hat_of_match[] = {
	{.compatible = "tha,rainbow-arc",},
	{.compatible = "tha,rainbow-hat",},
	{}
};

MODULE_DEVICE_TABLE(of, rainbow_hat_of_match);

// Setup SPI-Driver
static struct spi_driver rainbow_hat_spi_driver = {
	.driver = {
		.name = "rainbow_hat_spi_driver", // driver name in e.g. dmesg
		// of_match_ptr() used due to prev. manual impl.
		.of_match_table = of_match_ptr(rainbow_hat_of_match),
		.owner = THIS_MODULE, // connect driver to module
	},
	.probe = rainbow_hat_spi_driver_probe,    // init()
	.remove = rainbow_hat_spi_driver_remove,  // del()
};

// Setup Platform-Driver
static struct platform_driver rainbow_hat_platform_driver = {
	.probe = rainbow_hat_platform_driver_probe,   // init()
	.remove = rainbow_hat_platform_driver_remove, // del()
	.driver = {
		.name = "rainbow_hat_platform_driver", // driver name in e.g. dmesg
		// of_match_ptr() used bc. continuity
		.of_match_table = of_match_ptr(rainbow_hat_of_match),
		.owner = THIS_MODULE, // connect driver to module
	},
};

/* --------------------------------------------------- */
/* --------------- Driver Initialization ------------- */
/* --------------------------------------------------- */

// Init SPI+Platform Drivers
static int __init rainbow_init(void)
{
	int err;

	// Attempt to register the SPI driver
	err = spi_register_driver(&rainbow_hat_spi_driver);
	if (err) {
		pr_err("Failed to register SPI driver\n");
		return err; // Return the error, no resources to clean up yet
	}

	// Attempt to register the platform driver
	err = platform_driver_register(&rainbow_hat_platform_driver);
	if (err) {
		pr_err("Failed to register platform driver\n");
		spi_unregister_driver(&rainbow_hat_spi_driver);
		return err;
	}

	return 0; // Success
}

// Cleanup SPI+Platform Drivers
static void __exit rainbow_exit(void)
{
	spi_unregister_driver(&rainbow_hat_spi_driver);
	platform_driver_unregister(&rainbow_hat_platform_driver);
}

// No macros used due to inevitable duplicate call to __init/__exit
module_init(rainbow_init);
module_exit(rainbow_exit);

/* --------------------------------------------------- */
/* ------------------- EPILOGUE ---------------------- */
/* --------------------------------------------------- */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonas Winkler");
MODULE_DESCRIPTION(
	"Platform driver for Rainbow HAT buttons/buzzer and SPI-Driver for LEDs");
MODULE_VERSION("1.0");
