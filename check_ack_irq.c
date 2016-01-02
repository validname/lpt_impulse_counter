#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <time.h>

#define IRQ_ENABLE 0x10

#define PP_DEV_NAME "/dev/parport0"

int running = 1;

void signalHandler(int sig) {
	running = 0;
}

int main() {
	unsigned char value, prev_value, first_value;
	unsigned int mode, missed_interrupts;
	int ppfd, error;
	struct timespec time, prev_time;
	unsigned long period;
	float freq;

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
	value = IRQ_ENABLE;
	error = ioctl(ppfd, PPWCONTROL, &value);
	if(error)
		{ printf("Couldn't enable interrupts for parallel port: %s\n", strerror(error)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }

	first_value = 1;
	period = 0;

	printf("Waiting for interrupt on nACK pin state changes...\n");
	while(running) {
		// Wait for an interrupt
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(ppfd, &rfds);
		if(select(ppfd + 1, &rfds, NULL, NULL, NULL)) {
			// Received interrupt
			clock_gettime(CLOCK_MONOTONIC_RAW, &time);
			// Clear the interrupt
			ioctl(ppfd, PPCLRIRQ, &missed_interrupts);
			if(missed_interrupts > 1)
				printf("Missed %i interrupts!\n", missed_interrupts - 1);
			if ( !first_value ) {
				period = (time.tv_sec - prev_time.tv_sec)*1000000 + (time.tv_nsec - prev_time.tv_nsec)/1000;
				printf("Impulse period: %ld us. ", period);
				freq = 1000000.0/period;
				printf("Instant frequency is: %f Hz, magic is: %f\n", freq, freq*36/32*1000.0);
			}
			first_value = 0;
			prev_time = time;
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
