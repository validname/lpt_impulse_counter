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

#define PP_DEV_NAME "/dev/parport0"

int running = 1;

void signalHandler(int sig) {
	running = 0;
}

int main() {
	int ppfd, error;
	unsigned int mode, missed_interrupts;
	unsigned char value;
	fd_set rfds;

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

	printf("Waiting for interrupt on nACK pin state changes...\n");
	while(running) {
		// Wait for an interrupt
		FD_ZERO(&rfds);
		FD_SET(ppfd, &rfds);
		if(select(ppfd + 1, &rfds, NULL, NULL, NULL)) {
			// Received interrupt
			// Clear the interrupt
			ioctl(ppfd, PPCLRIRQ, &missed_interrupts);
			if(missed_interrupts > 1) {
				printf("Missed %i interrupt(s)!\n", missed_interrupts - 1);
			} else {
				printf("Got interrupt.\n");
			}
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
