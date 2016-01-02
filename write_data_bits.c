#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <libgen.h>

#define PP_DEV_NAME "/dev/parport0"

int main(int argc, char **argv) {
	unsigned char value, symbol;
	unsigned char buf[128];
	unsigned int mode, i;
	int ppfd, error;

	if(argc<2) {
		printf("Usage: %s <hexbyte>\n", basename(argv[0]));
		exit(1);
	}
/*
	if(strlen(argv[1])!=2) {
		printf("Invalid hexbyte length!\n");
		exit(1);
	}
*/
	value = 0;
	error = sscanf(argv[1], "%2hhx", &value);
//	sprintf(buf, "%hhx", value);
//	if(!error || strlen(buf)!=2)
	if(!error)
		{ printf("Couldn't convert hexbyte to byte: %s\n", strerror(error)); exit(1); }

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

	error = ioctl(ppfd, PPWDATA, &value);
	if(error)
		{ printf("Couldn't write in parallel port: %s\n", strerror(error)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }

	ioctl(ppfd, PPRELEASE);
	close(ppfd);
	exit(0);
}
