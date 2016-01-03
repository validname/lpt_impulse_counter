#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <time.h>

#define PARPORT_CONTROL_ENABLE_IRQ	0x10		/* because this isn't defined in the linux/parport.h */
#define CONSUME_HOUR_FACTOR		3.2		/* 3200 impulses counted per 1 hour means 1 kWt of energy consumed */

#define PP_DEV_NAME "/dev/parport0"

int running = 1;

void signalHandler(int sig) {
	running = 0;
}

int main() {
	unsigned char value, prev_value, first_interrupt;
	unsigned int mode, missed_interrupts, was_missed;
	int ppfd, error;
	fd_set rfds;
	struct timespec time, prev_time, start_time;
	unsigned long period, missed_period, impulses, seconds;
	float freq, consumed, elapsed;

	// Set ctrl-c handler
	signal(SIGINT, signalHandler);

	ppfd = open(PP_DEV_NAME, O_RDWR);
	if(ppfd == -1) {
		printf("Unable to open parallel port: %s\n", strerror(error));
		exit(1);
	}

	// Have to claim the device
	error = ioctl(ppfd, PPCLAIM);
	if(error)
		{ printf("Couldn't claim parallel port: %s\n", strerror(error)); close(ppfd); exit(1); }

	// set ordinary compatible mode (with control and status register available)
	mode = IEEE1284_MODE_COMPAT;
	error = ioctl(ppfd, PPSETMODE, &mode);
	if(error)
		{ printf("Couldn't set IEEE1284_MODE_COMPAT mode: %s\n", strerror(error)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }

	// but enable interrupts
	value = PARPORT_CONTROL_ENABLE_IRQ;
	error = ioctl(ppfd, PPWCONTROL, &value);
	if(error)
		{ printf("Couldn't enable interrupts for parallel port: %s\n", strerror(error)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }

	impulses = 0;
	first_interrupt = 1;
	was_missed = 0;
	consumed = 0;

	// set time for the worth case: missed interrupts immediately after start
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
	printf("Waiting for interrupt on nACK pin state changes...\n");
	while(running) {
		// Wait for an interrupt
		FD_ZERO(&rfds);
		FD_SET(ppfd, &rfds);
		if(select(ppfd + 1, &rfds, NULL, NULL, NULL)) {
			// Received interrupt
			// Clear the interrupt
			ioctl(ppfd, PPCLRIRQ, &missed_interrupts);
			clock_gettime(CLOCK_MONOTONIC_RAW, &time);
			if(missed_interrupts > 1) {
				printf("Missed %i interrupts! Next cycle will contain average values.\n", missed_interrupts - 1);
				was_missed = missed_interrupts;
				missed_period = (time.tv_sec - prev_time.tv_sec)*1000000 + (time.tv_nsec - prev_time.tv_nsec)/1000;
			} else {
				if ( !first_interrupt ) {
					period = (time.tv_sec - prev_time.tv_sec)*1000000 + (time.tv_nsec - prev_time.tv_nsec)/1000;
					if(was_missed) { // there was missed interrupts on previos cycle, need to calculate average values
						period = (period + missed_period)/(was_missed+1);
					}
					elapsed = (time.tv_sec - start_time.tv_sec) + (time.tv_nsec - start_time.tv_nsec)/1000000000.0;	// seconds from start time (float)
					seconds = elapsed;	// seconds from start time (integer)
					freq = 1000000.0/period;
					consumed += 1/CONSUME_HOUR_FACTOR;
					printf("Impulses: %ld, ", impulses);
					printf("elapsed: %ld s, ", seconds);
					printf("last period: %ld us, ", period);
					printf("instant frequency: %f Hz, ", freq);
					printf("consumption (instant): %f, ", freq/CONSUME_HOUR_FACTOR*3600);
					printf("(avg): %f, ", impulses/CONSUME_HOUR_FACTOR*3600/elapsed);
					printf("total consumed: %f", consumed);
					printf("\n");
				} else {
					clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);	// correct start time - set it in the moment of first interrupt
				}
				was_missed = 0;
			}
			first_interrupt = 0;
			prev_time = time;
			impulses++;
		} else {
			printf("Caught some signal?\n");
			continue;
		}
	}
	printf("Shutting down.\n");
	ioctl(ppfd, PPRELEASE);
	close(ppfd);
	exit(0);
}
