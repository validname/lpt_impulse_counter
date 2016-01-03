#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <libgen.h>
#include <errno.h>

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

	value = 0;
	error = sscanf(argv[1], "%2hhx", &value);
	if(error<=0)
		{ printf("Couldn't convert hexbyte to byte: %s\n", strerror(errno)); exit(1); }

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

	if(ioctl(ppfd, PPWDATA, &value) == -1)
		{ printf("Couldn't write in parallel port: %s\n", strerror(errno)); ioctl(ppfd, PPRELEASE); close(ppfd); exit(2); }

	ioctl(ppfd, PPRELEASE);
	close(ppfd);
	exit(0);
}
