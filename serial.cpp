// On Windows under cygwin, try /dev/comNN, where NN is 0, 1, 2, etc
// GUC-232A came up as /dev/com4 just now (I think)

// GCC and CLANG: g++ -std=c++11 -Wall -Wpedantic -Wextra serial.cpp -o serial

#include <map>   /* Standard input/output definitions */
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <stdlib.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/types.h>
#include <sys/time.h>

// This is needed with --std=c++11.  I haven't investigated further.
#if defined(__CYGWIN__)
#include <sys/select.h>
#endif

std::map<int, int> baudMapping = 
{
    {0, B0},
    {50, B50},
    {75, B75},
    {110, B110},
    {134, B134},
    {150, B150},
    {200, B200},
    {300, B300},
    {600, B600},
    {1200, B1200},
    {1800, B1800},
    {2400, B2400},
    {4800, B4800},
    {9600, B9600},
    {19200, B19200},
    {38400, B38400},
    {57600, B57600},
    {115200, B115200},

#ifdef B128000

    {128000, B128000},
    {230400, B230400},
    {256000, B256000},
    {460800, B460800 },
    {500000 , B500000},
    {576000, B576000},
    {921600, B921600},
    {1000000, B1000000},
    {1152000 , B1152000},
    {1500000, B1500000},
    {2000000, B2000000},
    {2500000, B2500000},
    {3000000, B3000000},

#ifndef __CYGWIN__
    {3500000, B3500000},
    {4000000, B4000000},
#endif /* B3500000 */

#else /* B128000 not defined */

    {250000, 250000},
    {266667, 266667},
    {285714, 285714},
    {307692, 307692},
    {333333, 333333},
    {363636, 363636},
    {400000, 400000},
    {444444, 444444},
    {500000, 500000},
    {471429, 471429},
    {666667, 666667},
    {800000, 800000},
    {1000000, 1000000},
    {1333333, 1333333},
    {2000000, 2000000},
    {4000000, 4000000},

#endif /* B128000 */
};

char presetNames[10][512];
char presetStrings[10][16384]; // should probably use std::string

const char* usageString = R"(
serial v1.0 by Brad Grantham, grantham@plunk.org

usage: %s serialportfile baud
e.g.: %s /dev/ttyS0 38400

The file $HOME/.serial (%s/.serial in your specific case) can also
contain 10 string presets which are emitted when pressing "~" (tilde)
followed by one of the keys "1" through "0".
This file contains one preset per line, of the format:

    name-of-preset preset-string-here

The preset string itself can contain spaces and also can contain embedded
newlines in the form "\n".  Here's a short example file:

    restart-device reboot\n
    initiate-connection telnet distant-machine\nexport DISPLAY=flebbenge:0\n

Pressing "~" then 1 will send "reboot" and a newline over the serial port.
Pressing "~" then 2 will send "telnet distant-machine" over the serial
port, then a newline, then "export DISPLAY=flebbenge:0", and then
another newline.

When running, press "~" (tilde) and then "h" for some help.
)";

void usage(char *progname)
{
    printf(usageString, progname, progname, getenv("HOME"));
}

const char* keyHelpString = R"(
key help:
    .   - exit
    d   - toggle duplex
    n   - toggle whether to send CR with NL
    0-9 - send preset strings from ~/.serial
    p   - print contents of presets
)";

int main(int argc, char **argv)
{
    int             speed;
    int             duplex = 0, crnl = 0;
    int             serial;
    int		    tty_in, tty_out;
    struct termios  options; 
    struct termios  old_stdin_termios; 
    unsigned int    baud;
    char            stringbuf[16384];
    bool	    done = false;
    fd_set   reads;
    struct timeval  timeout;

    char *programName = argv[0];
    argv++;
    argc--;

    while((argc > 0) && (argv[0][0] == '-'))
    {
        if(strcmp(argv[0], "--help") == 0)
        {
            argc--; argv++;
            usage(programName);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("unknown option \"%s\"\n", argv[0]);
            usage(programName);
            exit(EXIT_FAILURE);
        }
    }

    bool saw_tilde = false;

    FILE *presetFile;
    char presetName[512];

    for(int i = 0; i < 10; i++)
    {
        presetStrings[i][0] = '\0';
    }

    snprintf(presetName, sizeof(presetName), "%s/.serial", getenv("HOME"));
    presetFile = fopen(presetName, "r");

    if(presetFile == NULL)
    {

        fprintf(stderr, "couldn't open preset strings file \"%s\"\n", presetName);
        fprintf(stderr, "proceeding without preset strings.\n");

    }
    else
    {

        for(int i = 0; i < 10; i++)
        {
            int which = (i + 1) % 10;

            if(fscanf(presetFile, "%s ", presetNames[which]) != 1)
            {
                break;
	    }

            if(fgets(stringbuf, sizeof(stringbuf) - 1, presetFile) == NULL)
            {
                fprintf(stderr, "preset for %d (\"%s\") had a name but no string.  Ignored.\n", which, presetNames[which]);
                break;
            }
            stringbuf[strlen(stringbuf) - 1] = '\0';

            char *dst = presetStrings[which], *src = stringbuf;
            while(*src)
            {
                if(src[0] == '\\' && src[1] == 'n')
                {
                    *dst++ = '\n';
                    src += 2;
                }
                else
                {
                    *dst++ = *src++;
                }
            }
            *dst++ = '\0';
        }

        fclose(presetFile);
    }

    if(argc < 2)
    {
        usage(programName);
	exit(EXIT_FAILURE);
    }

    if(argv[1][0] < '0' || argv[1][0] > '9')
    {
        usage(programName);
	exit(EXIT_FAILURE);
    }

    baud = (unsigned int) atoi(argv[1]);
    auto found = baudMapping.find(baud);
    if(found == baudMapping.end())
    {
	fprintf(stderr, "Didn't understand baud rate \"%s\"\n", argv[1]);
	exit(EXIT_FAILURE);
    }
    baud = found->second;

    if(false) printf("baud mapping chosen: %d (0x%X)\n", baud, baud);

    tty_in = dup(0);
    if(tty_in == -1)
    {
	fprintf(stderr, "Can't open dup of stdin\n");
	exit(EXIT_FAILURE);
    }

    if(fcntl(tty_in, F_SETFL, O_NONBLOCK) == -1)
    {
	fprintf(stderr, "Failed to set nonblocking stdin\n");
	exit(EXIT_FAILURE);
    }

    tty_out = dup(1);
    if(tty_out == -1)
    {
	fprintf(stderr, "Can't open dup of stdout\n");
	exit(EXIT_FAILURE);
    }

    serial = open(argv[0], O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(serial == -1)
    {
	fprintf(stderr, "Can't open serial port \"%s\"\n", argv[0]);
	exit(EXIT_FAILURE);
    }

#if 0
    if(fcntl(serial, F_SETFL, 0) == -1)
    {
	fprintf(stderr, "Failed to reset fcntl on serial\n");
	exit(EXIT_FAILURE);
    }
#endif

    /*
     * get the current options 
     */
    tcgetattr(tty_in, &old_stdin_termios);
    tcgetattr(tty_in, &options);

    /*
     * set raw input, 1 second timeout 
     */
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~INLCR;
    options.c_iflag &= ~ICRNL;

    /*
     * Miscellaneous stuff 
     */
    options.c_cflag |= (CLOCAL | CREAD);	/* Enable receiver, set
						 * local */
    options.c_iflag |= (IXON | IXOFF);	/* Software flow control */
    options.c_lflag = 0;	/* no local flags */
    options.c_cflag |= HUPCL;	/* Drop DTR on close */

    /*
     * Clear the line 
     */
    tcflush(tty_in, TCIFLUSH);

    /*
     * Update the options synchronously 
     */
    if (tcsetattr(tty_in, TCSANOW, &options) != 0) {
	perror("setting stdin tc");
	goto restore;
    }

    /*
     * get the current options 
     */
    tcgetattr(serial, &options);

    /*
     * set raw input, 1 second timeout 
     */
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;


    options.c_iflag &= ~INPCK;	/* Enable parity checking */
    options.c_iflag |= IGNPAR;

    options.c_cflag &= ~PARENB;	/* Clear parity enable */
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    options.c_cflag &= ~CRTSCTS;

    options.c_oflag &= ~(IXON | IXOFF | IXANY);	/* no flow control */

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;	/* No output processing */
    options.c_iflag &= ~INLCR;	/* Don't convert linefeeds */
    options.c_iflag &= ~ICRNL;	/* Don't convert linefeeds */

    /*
     * Miscellaneous stuff 
     */
    options.c_cflag |= (CLOCAL | CREAD);	/* Enable receiver, set
						 * local */

    options.c_iflag |= (IXON | IXOFF);	/* Software flow control */
    options.c_lflag = 0;	/* no local flags */
    options.c_cflag |= HUPCL;	/* Drop DTR on close */

    cfsetispeed(&options, baud);
    speed = cfgetispeed(&options);
    printf("set tty input to speed %d, expected %d\n", speed, baud);
    cfsetospeed(&options, baud);
    speed = cfgetospeed(&options);
    printf("set tty output to speed %d, expected %d\n", speed, baud);

    /*
     * Clear the line 
     */
    tcflush(serial, TCIFLUSH);

    if (tcsetattr(serial, TCSANOW, &options) != 0)
    {
	perror("setting serial tc");
	goto restore;
    }
    tcflush(serial, TCIFLUSH);


    printf("press \"~\" (tilde) and then \"h\" for some help.\n");

    while(!done)
    {

	FD_ZERO(&reads);
	FD_SET(serial, &reads);
	FD_SET(tty_in, &reads);

        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

	int result = select(FD_SETSIZE, &reads, NULL, NULL, &timeout);

	if(result < 0)
        {

	    perror("select");
	    done = true;
	    continue;

	}
        else if(result == 0)
        {

	    if(false) printf("select timed out.\n");

	}
        else
        { 

	    for(int i = 0 ; i < FD_SETSIZE; i++)
            {
		if(FD_ISSET(i, &reads))
                {
		    if(false) printf("read on %d\n", i);
		}
	    }

	    if(FD_ISSET(serial, &reads))
            {
		if(false) printf("Read from serial\n");

                unsigned char buf[512];
		int byte_count = read(serial, buf, sizeof(buf));

                if(byte_count == -1) 
                {
                    if(errno == ENXIO) 
                    {
                        fprintf(stderr, "The device became unavailble.\n");
                        fprintf(stderr, "Maybe it was a USB adapter that was unplugged?\n");
                    }
                    else
                    {
                        fprintf(stderr, "unexpected return of -1 bytes from read: errno = %d\n", errno);
                    }
		    done = true;
		    continue;
                }

		if(byte_count == 0)
                {
		    fprintf(stderr, "unexpected read of 0 bytes from serial!\n");
		    done = true;
		    continue;
		}

		write(tty_out, buf, byte_count);
	    }

	    if(FD_ISSET(tty_in, &reads))
            {
		if(false) printf("Read from TTY\n");

                unsigned char buf[512];
		int byte_count = read(tty_in, buf, sizeof(buf));

                if(byte_count > 0)
                {

                    if(saw_tilde)
                    {
                        if(buf[0] == 'h' || buf[0] == '?')
                        {

                            printf("%s", keyHelpString);
                            int i;
                            for(i = 0; i < 10; i++)
                            {
                                int which = (i + 1) % 10;
                                if(presetStrings[which][0] == '\0')
                                    break;
                                printf("        %d : \"%s\"\n", which, presetNames[which]);
                            }
                            if(i == 0)
                            {
                                printf("        (no preset strings)\n");
                            }
                            saw_tilde = false;
                            continue;

                        }
                        else if(buf[0] >= '0' && buf[0] <= '9')
                        {
                            
                            int which = buf[0] - '0';
                            write(serial, presetStrings[which], strlen(presetStrings[which]));
                            saw_tilde = false;
                            continue;

                        }
                        else if(buf[0] == 'p')
                        {

                            printf("preset strings from ~/.serial:\n");
                            int i;
                            for(i = 0; i < 10; i++)
                            {
                                int which = (i + 1) % 10;
                                if(presetStrings[which][0] == '\0')
                                {
                                    break;
                                }
                                printf("  %d, \"%15s\",  : \"%s\"\n", which, presetNames[which], presetStrings[which]);
                            }
                            if(i == 0) 
                            {
                                printf("  (no preset strings)\n");
                            }
                            saw_tilde = false;
                            continue;

                        }
                        else if(buf[0] == '.')
                        {

                            done = true;
                            saw_tilde = false;
                            continue;

                        }
                        else if(buf[0] == 'd')
                        {

                            duplex = !duplex;
                            saw_tilde = false;
                            continue;

                        }
                        else if(buf[0] == 'n')
                        {

                            crnl = !crnl;

#if 1
                            tcgetattr(serial, &options);
                            if(crnl) 
                            {
                                options.c_iflag |= ICRNL;
                            }
                            else
                            {
                                options.c_iflag &= ~ICRNL;
                            }
                            tcsetattr(serial, TCSANOW, &options);
#endif

                            tcgetattr(tty_out, &options);
                            if(crnl)
                            {
                                options.c_oflag |= OCRNL;
                            }
                            else
                            {
                                options.c_oflag &= ~OCRNL;
                            }
                            tcsetattr(tty_out, TCSANOW, &options);
                            saw_tilde = false;

                            continue;
                        }

                    }
                    else if(buf[0] == '~')
                    {

                        saw_tilde = true;
                        continue; /* ick */

                    }
                    else
                    {

                        saw_tilde = false;
                    }

                    if(false) printf("writing %d bytes: '%c', %d\n", byte_count, buf[0], buf[0]);
                    write(serial, buf, byte_count);

                    if(duplex)
                    {
                        write(tty_out, buf, byte_count);
                    }

                }
                else
                {

		    fprintf(stderr, "unexpected read of 0 bytes from tty_in!\n");
		    done = true;
		    continue;

                }
	    }
	}
    }

restore:

    if (tcsetattr(tty_in, TCSANOW, &old_stdin_termios) != 0)
    {
	perror("restoring stdin");
	return (0);
    }

    close(serial);
    close(tty_in);
    close(tty_out);

    return 0;
}
