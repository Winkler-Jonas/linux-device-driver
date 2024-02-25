// SPDX-License-Identifier: GPL-2.0+
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include "rainbow_hat.h"

// helper-macro to slice an array
#define slice(buf, src, start, end) \
	(memcpy((buf), &(src)[(start)], sizeof((src)[0]) * ((end) - (start))))
#define elapsed_time_us(start, end) \
	(((end)->tv_sec - (start)->tv_sec) * 1000000L + ((end)->tv_nsec - (start)->tv_nsec) / 1000L)

// Predefined color-codes (Any other hex-value can be used as well)
#define COLOR_RED "FF0000"
#define COLOR_PURPLE "FF00FF"

// Predefined frequencies (Any other frequencies are possible to)
#define FREQ_C 262
#define FREQ_E 330
#define FREQ_G 392

// Metronome runs for undefined time.
// Use keep_running to halt operations and start cleanup.
sig_atomic_t keep_running = 1;
// Used for keeping metronome at sync
struct timespec start_time, end_time;

// Arguments for piano simulation thread
struct piano_simulation_args {
	rainbow_hat_dev *dev;
	int freq_a;
	int freq_b;
	int freq_c;
	char *err_buff;
};

// Handle basic signals (SIGTERM/SIGINT)
void signal_handler(int sig)
{
	// halt loops -> start cleanup / exit
	keep_running = 0;
}

int metronome_led_sequence(rainbow_hat_dev *dev, int bpm,
			   const char *color_beat_1,
			   const char *color_beat_2, char err_buff[])
{
	/*
	 * Function simulates a metronome using the Rainbow HAT Arc LEDs. Beat one
	 * lights up all available LEDs, beat two lights 3 LEDs on the left and beat
	 * three lights 3 LEDs on the right with the colors defined by the user.
	 *
	 * Args:
	 *	- dev		-> Rainbow HAT device
	 *	- bpm		-> integer beats per minute
	 *	- color_beat_1	-> HEX-VALUE of the first beat e.g.(FFFFFF)
	 *	- color_beat_2	-> HEX-VALUE of the second beat e.g.(00FF00)
	 *	- err_buff	-> buffer to store potential err-msg
	 *
	 * Returns:
	 *	-  0	-> Normal exit
	 *	- -1	-> Unexpected exit (Error occurred)
	 */
	if (!dev || !err_buff) {
		fprintf(stderr,
			"Metronome LED sequence - Invalid arguments!\n");
		return -1;
	}

	const int beat_duration_us = (int) (60.0 / bpm * 1000000);
	const int sleep_duration_us = 100000; // 100 ms
	int second_third_beat[3];
	const char *current_color;
	int current_amount_pins;
	int *current_pins;

	led_arg beat_arg = {
		.amount_colors = 1,
	};

	while (keep_running) {
		for (int beat = 1; beat <= 3; beat++) {
			// Adjust color depending on the beat
			current_color = (beat == 1) ? color_beat_1
						    : color_beat_2;

			// Change pins and amount depending on the beat
			switch (beat) {
			case 1:
				current_pins = (int *) PIN_NUMBERS;
				current_amount_pins = MAX_AMOUNT_PINS;
				break;
			case 2:
				slice(second_third_beat, PIN_NUMBERS, 4,
				      MAX_AMOUNT_PINS);
				current_pins = second_third_beat;
				current_amount_pins = 3;
				break;
			case 3:
				slice(second_third_beat, PIN_NUMBERS, 0,
				      3);
				current_pins = second_third_beat;
				current_amount_pins = 3;
				break;
			}

			beat_arg.colors = &current_color;
			beat_arg.pins = current_pins;
			beat_arg.amount_pins = current_amount_pins;

			// time before led-on/off
			clock_gettime(CLOCK_MONOTONIC, &start_time);

			if (dev->leds_on(dev, beat_arg, err_buff) < 0) {
				fprintf(stderr, "LEDs On Error: %s\n",
					err_buff);
				goto err_exit;
			}

			usleep(sleep_duration_us); // Wait a little bit (100 ms)

			if (dev->leds_off(dev, err_buff) < 0) {
				fprintf(stderr, "LEDs Off Error: %s\n",
					err_buff);
				goto err_exit;
			}

			// time after executing led-on/off
			clock_gettime(CLOCK_MONOTONIC, &end_time);

			// adjust for led-on/off process time to keep metronome at sync
			long operation_time_us = elapsed_time_us(&start_time,
								 &end_time);
			long adjusted_sleep_duration_us =
				beat_duration_us - operation_time_us;

			if (adjusted_sleep_duration_us > 0)
				usleep(adjusted_sleep_duration_us);
		}
	}
	return 0;
err_exit: // err occurred -> cleanup - or - hard exit
	if (keep_running) {
		if (kill(getpid(), SIGTERM) == -1) {
			fprintf(stderr, "Proc: %d - Error sending SIGTERM\n",
				getpid());
			exit(EXIT_FAILURE);
		}
	}
	return -1;
}

int piano_simulation(rainbow_hat_dev *dev, int freq_a, int freq_b, int freq_c,
		     char err_buff[])
{
	/*
	 * Function allows to use Rainbow HAT buttons to play frequencies provided
	 * by user with the integrated buzzer. If multiple buttons are pressed, only
	 * the first one touched is recognized and sound is played accordingly.
	 *
	 * Args:
	 *	- dev		-> Rainbow HAT device
	 *	- freq_a	-> Frequency for Button A
	 *	- freq_b	-> Frequency for Button B
	 *	- freq_c	-> Frequency for Button C
	 *	- err_buff	-> buffer for potential err-msg
	 *
	 * Returns:
	 *	-  0	-> Normal exit
	 *	- -1	-> Unexpected exit (Error occurred)
	 */
	if (!dev || !err_buff) {
		fprintf(stderr, "Piano simulator - Invalid arguments!\n");
		return -1;
	}

	char active_btn = '\0';

	while (keep_running) {
		if (dev->get_btn(dev, &active_btn, err_buff) < 0) {
			fprintf(stderr, "Error reading Button data: %s\n",
				err_buff);
			goto err_exit;
		}
		// if no button is pressed active_btn is '\0'
		if (active_btn) {
			switch (active_btn) {
			case 'A':
				// play adequate tone
				if (dev->play_tone(dev, freq_a, err_buff) < 0) {
					fprintf(stderr,
						"Error playing tone associated with button A: %s\n",
						err_buff);
					goto err_exit;
				}
				break;
			case 'B':
				if (dev->play_tone(dev, freq_b, err_buff) < 0) {
					fprintf(stderr,
						"Error playing tone associated with button B: %s\n",
						err_buff);
					goto err_exit;
				}
				break;
			case 'C':
				if (dev->play_tone(dev, freq_c, err_buff) < 0) {
					fprintf(stderr,
						"Error playing tone associated with button C: %s\n",
						err_buff);
					goto err_exit;
				}
				break;
			}
		} else {
			// stop playing sound if no button is pressed
			if (dev->play_tone(dev, 0, err_buff) < 0) {
				fprintf(stderr, "Error muting buzzer: %s\n",
					err_buff);
				goto err_exit;
			}
		}
		usleep(10000); // short sleep to reduce CPU usage
	}
	return 0;
err_exit: // err occurred -> cleanup - or - hard exit
	if (keep_running) {
		if (kill(getpid(), SIGTERM) == -1) {
			fprintf(stderr, "Proc: %d - Error sending SIGTERM\n",
				getpid());
			exit(EXIT_FAILURE);
		}
	}
	return -1;
}

void *piano_simulation_thread_wrapper(void *args)
{
	struct piano_simulation_args *actual_args = (struct piano_simulation_args *)args;
	int result = piano_simulation(actual_args->dev, actual_args->freq_a,
				      actual_args->freq_b, actual_args->freq_c,
				      actual_args->err_buff);
	return (void *)(intptr_t)result;
}


int main(void)
{
	char error_buffer[ERR_BUF_SIZE];
	int ret = 0;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	rainbow_hat_dev *dev = init_rainbow_hat(error_buffer);

	if (dev == NULL) {
		fprintf(stderr, "%s\n", error_buffer);
		return -1;
	}

	struct piano_simulation_args thrd_args;

	thrd_args.dev = dev;
	thrd_args.freq_a = FREQ_C;
	thrd_args.freq_b = FREQ_E;
	thrd_args.freq_c = FREQ_G;
	thrd_args.err_buff = error_buffer;

	// Create the button thread
	pthread_t piano_thread;

	if (pthread_create(&piano_thread, NULL,
			   piano_simulation_thread_wrapper,
			   &thrd_args)) {
		perror("Failed to create button thread");
		exit(EXIT_FAILURE);
	}

	ret = metronome_led_sequence(dev, 90, COLOR_RED,
				     COLOR_PURPLE, error_buffer);
	void *piano_ret_val;

	pthread_join(piano_thread, &piano_ret_val);

	if (ret)
		fprintf(stderr,
			"Severe Error occurred during execution of metronome!\n");
	else if (piano_ret_val)
		fprintf(stderr,
			"Severe error occurred during execution of piano simulation!\n");

	free_rainbow_hat(dev);
	return ret;
}
