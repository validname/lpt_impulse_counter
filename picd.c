#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>

#define PARPORT_CONTROL_ENABLE_IRQ	0x10		/* because this isn't defined in the linux/parport.h */
#define CONSUME_HOUR_FACTOR		3.2		/* 3200 impulses counted per 1 hour means 1 kWt of energy consumed */
#define FLUSH_INTERVAL 1

#define PP_DEV_NAME	"/dev/parport0"

#define STAT_FILE	"/var/log/picd.stats"
#define CACHE_FILE	"/var/run/picd.cache"

#define CACHE_CONSUMED_FORMAT	"%lf"

int running = 1;

void signalHandler(int sig) {
	switch(sig){
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
			/* gracefully shutdown the daemon */
			running = 0;
			break;
	}
}

// from http://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux
static void skeleton_daemon()
{
	pid_t pid;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* Success: Let the parent terminate */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* On success: The child process becomes session leader */
	if (setsid() < 0)
		exit(EXIT_FAILURE);

	/* Catch, ignore and handle signals */
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Fork off for the second time*/
	pid = fork();

	/* An error occurred */
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* Success: Let the parent terminate */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* Set new file permissions */
	umask(0);

	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	chdir("/");

	/* Close all open file descriptors */
	int x;
	for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
	{
		close (x);
	}
}

double read_cached_consumed(FILE *file) {
	unsigned char buffer[128], *ptr;
	double result;

	buffer[0] = 0x0;
	fseek(file, 0L, SEEK_SET);
	ptr = fgets(buffer, 128, file);
	if(!ptr) {
		syslog(LOG_WARNING, "Couldn't read cache file, buffer contains '%s' (%d bytes)", buffer, strlen(buffer));
	} else {
		if(sscanf(buffer, CACHE_CONSUMED_FORMAT, &result)!=1) {
			syslog(LOG_WARNING, "Couldn't read 'consumed' value from cache file");
		} else
			return result;
	}
	return 0.0;
}

int main() {
	FILE *statfile, *cachefile;
	unsigned char value, prev_value, first_interrupt, need_to_flush;
	unsigned int mode, missed_interrupts, was_missed;
	int ppfd, success;
	fd_set rfds;
	struct timespec time, prev_time, start_time, flush_time;
	unsigned long period, missed_period, impulses, seconds;
	double freq, consumed, elapsed;
	unsigned char buffer[1024], buffer2[1024], *ptr;

	// Set ctrl-c handler
	signal(SIGHUP, signalHandler);
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	skeleton_daemon();

	/* Open the log */
	openlog("picd", LOG_PID, LOG_DAEMON);

	/* Open the stat file */
	statfile = fopen(STAT_FILE, "w");
	if(!statfile) {
		syslog(LOG_ERR, "Unable to open stat file '%s': %s", STAT_FILE, strerror(errno));
		closelog();
		exit(EXIT_FAILURE);
	}

	/* Open the cache file */
	cachefile = fopen(CACHE_FILE, "r+");
	if(!cachefile) {
		syslog(LOG_ERR, "Unable to open stat file '%s': %s", CACHE_FILE, strerror(errno));
		fclose(statfile);
		closelog();
		exit(EXIT_FAILURE);
	}

	// Open parrallel port device
	ppfd = open(PP_DEV_NAME, O_RDWR);
	if(ppfd == -1) {
		syslog(LOG_ERR, "Unable to open parallel port: %s", strerror(errno));
		fclose(cachefile);
		fclose(statfile);
		closelog();
		exit(EXIT_FAILURE);
	}

	success = 0;
	// Instructs the kernel driver to forbid any sharing of the port
	if(ioctl(ppfd, PPEXCL) == -1)
		syslog(LOG_ERR, "Couldn't forbid sharing of parallel port: %s\n", strerror(errno));
	else {
		// Have to claim the device
		if(ioctl(ppfd, PPCLAIM) == -1)
			syslog(LOG_ERR, "Couldn't claim parallel port: %s", strerror(errno));
		else {
			// set ordinary compatible mode (with control and status register available)
			mode = IEEE1284_MODE_COMPAT;
			if(ioctl(ppfd, PPSETMODE, &mode) == -1)
				syslog(LOG_ERR, "Couldn't set IEEE1284_MODE_COMPAT mode: %s", strerror(errno));
			else {
				// but enable interrupts
				value = PARPORT_CONTROL_ENABLE_IRQ;
				if(ioctl(ppfd, PPWCONTROL, &value) == -1)
					syslog(LOG_ERR, "Couldn't enable interrupts for parallel port: %s", strerror(errno));
				else
					success = 1;
			}
		}
	}
	if(!success) {
		ioctl(ppfd, PPRELEASE);
		close(ppfd);
		fclose(cachefile);
		fclose(statfile);
		closelog();
		exit(EXIT_FAILURE);
	}

	syslog (LOG_NOTICE, "PICD daemon started.");

	impulses = 0;
	first_interrupt = 1;
	was_missed = 0;
	consumed = read_cached_consumed(cachefile);

	// set time for the worth case: missed interrupts immediately after start
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
	flush_time = start_time;
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
				// missed interrupts = next cycle will contain average values.
				syslog (LOG_WARNING, "Missed %i interrupts!", missed_interrupts - 1);
				was_missed = missed_interrupts;
				missed_period = (time.tv_sec - prev_time.tv_sec)*1000000 + (time.tv_nsec - prev_time.tv_nsec)/1000;	// in microseconds
			} else {
				if ( !first_interrupt ) {
					need_to_flush = 0;
					period = ((time.tv_sec - flush_time.tv_sec) + (time.tv_nsec - flush_time.tv_nsec)/1000000000);	// in seconds
					if(period>FLUSH_INTERVAL) {
						// get cached 'consumed' value from cache file
						need_to_flush = 1;
						flush_time = time;
						consumed = read_cached_consumed(cachefile);
					}
					period = (time.tv_sec - prev_time.tv_sec)*1000000 + (time.tv_nsec - prev_time.tv_nsec)/1000;	// in microseconds
					if(was_missed) { // there was missed interrupts on previos cycle, need to calculate average values
						period = (period + missed_period)/(was_missed+1);
						impulses += was_missed+1;
					}
					elapsed = (time.tv_sec - start_time.tv_sec) + (time.tv_nsec - start_time.tv_nsec)/1000000000.0;	// seconds from start time (float)
					seconds = elapsed;	// seconds from start time (integer)
					freq = 1000000.0/period;
					consumed += 1/CONSUME_HOUR_FACTOR;
					sprintf(buffer, "impulses\t-\t%ld\n", impulses);
					sprintf(buffer2, "elapsed_time\tsecond\t%ld\n", seconds);
					strcat(buffer, buffer2);
					sprintf(buffer2, "last_period\tmicrosecond\t%ld\n", period);
					strcat(buffer, buffer2);
					sprintf(buffer2, "instant_frequency\tHz\t%.2f\n", freq);
					strcat(buffer, buffer2);
					sprintf(buffer2, "instant_consumption\t-\t%.2f\n", freq/CONSUME_HOUR_FACTOR*3600);
					strcat(buffer, buffer2);
					sprintf(buffer2, "average_consumption\t-\t%.2f\n", impulses/CONSUME_HOUR_FACTOR*3600/elapsed);
					strcat(buffer, buffer2);
					sprintf(buffer2, "total_consumed\t-\t%lf\n", consumed);
					strcat(buffer, buffer2);
					fseek(statfile, 0L, SEEK_SET);
					fputs(buffer, statfile);

					sprintf(buffer, CACHE_CONSUMED_FORMAT, consumed);
					strcat(buffer, "\n");
					fseek(cachefile, 0L, SEEK_SET);
					fputs(buffer, cachefile);

					// flush output if elapsed time more or equal than 1 second
					if(need_to_flush) {
						fflush(statfile);
						fflush(cachefile);
					}
				} else {
					clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);	// correct start time - set it in the moment of first interrupt
				}
				was_missed = 0;
			}
			first_interrupt = 0;
			prev_time = time;
			impulses++;
		} else {
			syslog (LOG_WARNING, "Caught some signal?");
			continue;
		}
	}
	syslog (LOG_NOTICE, "Shutting down.");

	ioctl(ppfd, PPRELEASE);
	close(ppfd);
	fclose(cachefile);
	fclose(statfile);
	closelog();
	exit(EXIT_SUCCESS);
}
