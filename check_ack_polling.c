#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <time.h>
#include <errno.h>

#define PP_DEV_NAME "/dev/parport0"

int running = 1;

void signalHandler(int sig) {
	running = 0;
}

int main() {
	unsigned char value, prev_value, first_value;
	unsigned int mode, changes;
	int ppfd;
	struct timespec time, prev_time_high, prev_time_low;
	unsigned long period_low_high, period_high_low;
	float freq;

	// Set ctrl-c handler
	signal(SIGINT, signalHandler);

	ppfd = open(PP_DEV_NAME, O_RDWR);
	if(ppfd == -1) {
		printf("Unable to open parallel port: %s\n", strerror(errno));
		exit(1);
	}

	// Instructs the kernel driver to forbid any sharing of the port
	if(ioctl(ppfd, PPEXCL) == -1)
		{ printf("Couldn't forbid sharing of parallel port: %s\n", strerror(errno)); close(ppfd); exit(1); }

	// Have to claim the device
	if(ioctl(ppfd, PPCLAIM) == -1)
		{ printf("Couldn't claim parallel port: %s\n", strerror(errno)); close(ppfd); exit(1); }

	// set ordinary compatible mode (with control and status register available)
	mode = IEEE1284_MODE_COMPAT;
	if(ioctl(ppfd, PPSETMODE, &mode) == -1)
		{ printf("Couldn't set IEEE1284_MODE_COMPAT mode: %s\n", strerror(errno)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }

	first_value = 1;
	changes = 0;
	period_low_high = 0;
	period_high_low = 0;

	printf("Waiting for nACK pin state changes...\n");
	while(running) {
		if (ioctl(ppfd, PPRSTATUS, &value) == -1)
			{ printf("Couldn't get parallel port status: %s\n", strerror(errno)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }
		value = (value&PARPORT_STATUS_ACK)>0;
		if( !first_value && prev_value!=value ) {
			clock_gettime(CLOCK_MONOTONIC_RAW, &time);
			printf("nACK pin signal is changed to %d. ", value);
			if ( value ) { // high level / rise
				if ( changes>3 ) {
					period_low_high = (time.tv_sec - prev_time_low.tv_sec)*1000000 + (time.tv_nsec - prev_time_low.tv_nsec)/1000;
					printf("Low-to-high period: %ld us. ", period_low_high);
				}
				prev_time_high = time;
			} else { // low level / fall
				if ( changes>3 ) {
					period_high_low = (time.tv_sec - prev_time_high.tv_sec)*1000000 + (time.tv_nsec - prev_time_high.tv_nsec)/1000;
					printf("High_to_low period: %ld us. ", period_high_low);
				}
				prev_time_low = time;
			}
			if ( period_low_high && period_high_low ) {
				freq = 1000000.0/(period_low_high+period_high_low);
				printf("Instant frequency is: %f Hz, magic is: %f\n", freq, freq*36/32*1000.0);
			} else {
				printf("\n");
			}
			changes++;
		}
		prev_value = value;
		first_value = 0;
	}
	printf("Shutting down.\n");
	ioctl(ppfd, PPRELEASE);
	close(ppfd);
	exit(0);
}
