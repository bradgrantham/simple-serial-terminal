// On Windows under cygwin, try /dev/comNN, where NN is 0, 1, 2, etc
// GUC-232A came up as /dev/com4 just now (I think)

// GCC and CLANG: g++ -std=c++11 -Wall -Wpedantic -Wextra serial.cpp -o serial
// Add -DDEBUG for debug output

#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>
#include <chrono>
#include <thread>

// This is needed with --std=c++11.  I haven't investigated further.
#if defined(__CYGWIN__)
#include <sys/select.h>
#endif

#ifdef DEBUG
#define dbprintf(...) printf(__VA_ARGS__)
#else
#define dbprintf(...) ((void)0)
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

    // On macOS, B128000 and friends are not defined. The termios
    // implementation accepts the raw integer baud rate directly
    // (e.g. 250000 means 250000 baud), unlike Linux which uses
    // symbolic constants.
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

struct Preset {
    std::string name;
    std::string value;
};

std::vector<Preset> presets;

// Signal handling: save terminal state so we can restore on SIGINT/SIGTERM.
static int saved_tty_in = -1;
static struct termios saved_termios;
static bool termios_saved = false;

static void restore_terminal()
{
    if(termios_saved && saved_tty_in != -1)
    {
        tcsetattr(saved_tty_in, TCSANOW, &saved_termios);
    }
}

static void signal_handler(int)
{
    restore_terminal();
    _exit(0);
}

const char* usageString = R"(
serial v1.2 by Brad Grantham, grantham@plunk.org

usage: %s [options] [serialportfile] [baud]
e.g.: %s /dev/ttyS0 38400
      %s --device /dev/ttyS0 --baud 38400

Options:

    --device <path>
        Serial port device path (alternative to positional argument).

    --baud <rate>
        Baud rate (alternative to positional argument).

    --monitor
        Only _read_ from the serial port. Keyboard presses are not sent
        and the ~ commands are not supported. Exit with Ctrl-C.

    --watch
        Keep trying to open (and re-open) the serial port until it succeeds.

    --expect-disconnect
        If the connection disconnects, don't print a warning message.

    --timestamp
        Prefix each line of received serial data with a timestamp.

The file $HOME/.serial (%s/.serial in your specific case) can also
contain string presets which are emitted when pressing "~" (tilde)
followed by one of the keys "1" through "0".
This file contains one preset per line, of the format:

    name-of-preset preset-string-here

The preset string itself can contain spaces and also can contain embedded
escape sequences: \n (newline), \r (carriage return), \t (tab), and
\\ (literal backslash).  Here's a short example file:

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
    const char *home = getenv("HOME");
    printf(usageString, progname, progname, progname, home ? home : "(HOME not set)");
}

const char* keyHelpString = R"(
key help:
    .   - exit
    d   - toggle duplex
    n   - toggle whether to send CR with NL
    0-9 - send preset strings from ~/.serial
    p   - print contents of presets
)";

// Open and configure the serial port, returning its file descriptor,
// or -1 if it can't be opened.
static int open_serial(char const *pathname, unsigned int baud)
{
    struct termios options;

    int serial = open(pathname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(serial == -1)
    {
        return -1;
    }

    tcgetattr(serial, &options);

    // Control flags: 8N1, no HW flow control, enable receiver, local mode, hangup on close
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    options.c_cflag |= (CS8 | CLOCAL | CREAD | HUPCL);

    // Input flags: ignore parity, SW flow control, no linefeed conversion
    options.c_iflag &= ~(INPCK | INLCR | ICRNL);
    options.c_iflag |= (IGNPAR | IXON | IXOFF);

    // Output flags: raw output
    options.c_oflag &= ~OPOST;

    // Local flags: raw mode (no canon, no echo, no signals)
    options.c_lflag = 0;

    // Read returns after 1 second or when data is available
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    cfsetispeed(&options, baud);
    speed_t speed = cfgetispeed(&options);
    if(speed != baud)
    {
        printf("set tty input to speed %lu, expected %u\n", (long unsigned int) speed, baud);
    }
    cfsetospeed(&options, baud);
    speed = cfgetospeed(&options);
    if(speed != baud)
    {
        printf("set tty output to speed %lu, expected %u\n", (long unsigned int) speed, baud);
    }

    tcflush(serial, TCIFLUSH);

    if(tcsetattr(serial, TCSANOW, &options) != 0)
    {
        perror("setting serial tc");
    }
    tcflush(serial, TCIFLUSH);

    return serial;
}

// Keep trying to open the serial port until it succeeds.
static int watch_serial(char const *pathname, unsigned int baud)
{
    int serial = -1;

    while(serial == -1)
    {
        serial = open_serial(pathname, baud);
        if(serial == -1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    fprintf(stderr, "[Connected]\n");
    return serial;
}

// Process escape sequences in a preset string: \n, \r, \t, and backslash.
static std::string unescape(const char *src)
{
    std::string result;
    while(*src)
    {
        if(src[0] == '\\' && src[1])
        {
            switch(src[1])
            {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                default:
                    result += src[0];
                    result += src[1];
                    break;
            }
            src += 2;
        }
        else
        {
            result += *src++;
        }
    }
    return result;
}

// Load preset strings from ~/.serial.
static void load_presets()
{
    const char *home = getenv("HOME");
    if(home == NULL)
    {
        fprintf(stderr, "HOME environment variable not set, skipping preset strings.\n");
        return;
    }

    char presetPath[512];
    snprintf(presetPath, sizeof(presetPath), "%s/.serial", home);
    FILE *presetFile = fopen(presetPath, "r");

    if(presetFile == NULL)
    {
        fprintf(stderr, "couldn't open preset strings file \"%s\"\n", presetPath);
        fprintf(stderr, "proceeding without preset strings.\n");
        return;
    }

    char namebuf[512];
    char stringbuf[16384];

    while(presets.size() < 10)
    {
        if(fscanf(presetFile, "%511s ", namebuf) != 1)
        {
            break;
        }

        if(fgets(stringbuf, sizeof(stringbuf) - 1, presetFile) == NULL)
        {
            fprintf(stderr, "preset \"%s\" had a name but no string.  Ignored.\n", namebuf);
            break;
        }
        // Strip trailing newline from fgets
        size_t len = strlen(stringbuf);
        if(len > 0 && stringbuf[len - 1] == '\n')
            stringbuf[len - 1] = '\0';

        presets.push_back({namebuf, unescape(stringbuf)});
    }

    fclose(presetFile);
}

// Handle a tilde command keystroke. Returns true if the main loop should
// exit, false otherwise.
static bool handle_tilde_command(unsigned char key, int serial, int tty_out,
                                 int &duplex, int &crnl)
{
    struct termios options;

    if(key == 'h' || key == '?')
    {
        printf("%s", keyHelpString);
        if(presets.empty())
        {
            printf("        (no preset strings)\n");
        }
        else
        {
            for(size_t i = 0; i < presets.size(); i++)
            {
                int which = (i + 1) % 10;
                printf("        %d : \"%s\"\n", which, presets[i].name.c_str());
            }
        }
    }
    else if(key >= '0' && key <= '9')
    {
        // Keys 1-9,0 map to presets 0-9
        int index = (key == '0') ? 9 : (key - '1');
        if(index >= 0 && (size_t)index < presets.size())
        {
            const std::string &val = presets[index].value;
            write(serial, val.c_str(), val.size());
        }
    }
    else if(key == 'p')
    {
        printf("preset strings from ~/.serial:\n");
        if(presets.empty())
        {
            printf("  (no preset strings)\n");
        }
        else
        {
            for(size_t i = 0; i < presets.size(); i++)
            {
                int which = (i + 1) % 10;
                printf("  %d, \"%s\" : \"%s\"\n", which, presets[i].name.c_str(), presets[i].value.c_str());
            }
        }
    }
    else if(key == '.')
    {
        return true;
    }
    else if(key == 'd')
    {
        duplex = !duplex;
    }
    else if(key == 'n')
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
    }

    return false;
}

// Print a timestamp to stdout in [HH:MM:SS.mmm] format.
static void print_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    printf("[%02d:%02d:%02d.%03d] ", tm->tm_hour, tm->tm_min, tm->tm_sec,
           (int)(tv.tv_usec / 1000));
}

// Write serial data to tty_out, optionally prefixing lines with timestamps.
// at_line_start tracks whether we're at the beginning of a new line.
static void write_serial_output(int tty_out, const unsigned char *buf,
                                int byte_count, bool timestamp,
                                bool &at_line_start)
{
    if(!timestamp)
    {
        write(tty_out, buf, byte_count);
        return;
    }

    for(int i = 0; i < byte_count; i++)
    {
        if(at_line_start)
        {
            print_timestamp();
            at_line_start = false;
        }
        write(tty_out, &buf[i], 1);
        if(buf[i] == '\n')
        {
            at_line_start = true;
        }
    }
}

// Main event loop. Returns when done.
static void run_loop(int serial, int tty_in, int tty_out,
                     char const *serial_pathname, unsigned int baud,
                     bool watch, bool expect_disconnect, bool timestamp)
{
    int duplex = 0, crnl = 0;
    bool saw_tilde = false;
    bool at_line_start = true;

    struct pollfd fds[2];
    int nfds = 0;

    // fds[0] is always the serial port
    fds[0].fd = serial;
    fds[0].events = POLLIN;
    nfds = 1;

    // fds[1] is tty_in, if we have one
    int tty_poll_index = -1;
    if(tty_in != -1)
    {
        tty_poll_index = nfds;
        fds[nfds].fd = tty_in;
        fds[nfds].events = POLLIN;
        nfds++;
    }

    bool done = false;

    while(!done)
    {
        int result = poll(fds, nfds, 500);

        if(result < 0)
        {
            perror("poll");
            break;
        }
        else if(result == 0)
        {
            dbprintf("poll timed out.\n");
            continue;
        }

        if(fds[0].revents & POLLIN)
        {
            dbprintf("Read from serial\n");

            unsigned char buf[512];
            int byte_count = read(serial, buf, sizeof(buf));

            if(byte_count == -1)
            {
                if(errno == ENXIO)
                {
                    if(watch)
                    {
                        fprintf(stderr, "[Device disconnected, waiting for it to come back]\n");
                        close(serial);
                        serial = watch_serial(serial_pathname, baud);
                        fds[0].fd = serial;
                    }
                    else
                    {
                        if(!expect_disconnect)
                        {
                            fprintf(stderr, "The device became unavailable.\n");
                            fprintf(stderr, "Maybe it was a USB adapter that was unplugged?\n");
                            fprintf(stderr, "Specify the --watch flag to retry automatically.\n");
                        }
                        done = true;
                    }
                }
                else
                {
                    fprintf(stderr, "unexpected return of -1 bytes from read: errno = %d\n", errno);
                    done = true;
                }
                continue;
            }

            if(byte_count == 0)
            {
                fprintf(stderr, "unexpected read of 0 bytes from serial!\n");
                done = true;
                continue;
            }

            write_serial_output(tty_out, buf, byte_count, timestamp, at_line_start);
        }

        if(tty_poll_index >= 0 && (fds[tty_poll_index].revents & POLLIN))
        {
            dbprintf("Read from TTY\n");

            unsigned char buf[512];
            int byte_count = read(tty_in, buf, sizeof(buf));

            if(byte_count > 0)
            {
                if(saw_tilde)
                {
                    saw_tilde = false;
                    done = handle_tilde_command(buf[0], serial, tty_out,
                                                duplex, crnl);
                    continue;
                }
                else if(buf[0] == '~')
                {
                    saw_tilde = true;
                    continue;
                }

                dbprintf("writing %d bytes: '%c', %d\n", byte_count, buf[0], buf[0]);
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
            }
        }
    }
}

int main(int argc, char **argv)
{
    int             serial;
    int             tty_in, tty_out;
    struct termios  options;
    unsigned int    baud;
    bool            monitor = false;
    bool            watch = false;
    bool            expect_disconnect = false;
    bool            timestamp = false;
    const char     *device_arg = NULL;
    const char     *baud_arg = NULL;

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
        else if(strcmp(argv[0], "--monitor") == 0)
        {
            argc--; argv++;
            monitor = true;
        }
        else if(strcmp(argv[0], "--watch") == 0)
        {
            argc--; argv++;
            watch = true;
        }
        else if(strcmp(argv[0], "--expect-disconnect") == 0)
        {
            argc--; argv++;
            expect_disconnect = true;
        }
        else if(strcmp(argv[0], "--timestamp") == 0)
        {
            argc--; argv++;
            timestamp = true;
        }
        else if(strcmp(argv[0], "--device") == 0)
        {
            argc--; argv++;
            if(argc < 1)
            {
                fprintf(stderr, "--device requires an argument\n");
                usage(programName);
                exit(EXIT_FAILURE);
            }
            device_arg = argv[0];
            argc--; argv++;
        }
        else if(strcmp(argv[0], "--baud") == 0)
        {
            argc--; argv++;
            if(argc < 1)
            {
                fprintf(stderr, "--baud requires an argument\n");
                usage(programName);
                exit(EXIT_FAILURE);
            }
            baud_arg = argv[0];
            argc--; argv++;
        }
        else
        {
            printf("unknown option \"%s\"\n", argv[0]);
            usage(programName);
            exit(EXIT_FAILURE);
        }
    }

    // Positional args override --device/--baud if both are given
    if(argc >= 1)
        device_arg = argv[0];
    if(argc >= 2)
        baud_arg = argv[1];

    if(device_arg == NULL || baud_arg == NULL)
    {
        usage(programName);
        exit(EXIT_FAILURE);
    }

    if(baud_arg[0] < '0' || baud_arg[0] > '9')
    {
        fprintf(stderr, "Baud rate must be a number, got \"%s\"\n", baud_arg);
        usage(programName);
        exit(EXIT_FAILURE);
    }

    baud = (unsigned int) atoi(baud_arg);
    auto found = baudMapping.find(baud);
    if(found == baudMapping.end())
    {
        fprintf(stderr, "Didn't understand baud rate \"%s\"\n", baud_arg);
        exit(EXIT_FAILURE);
    }
    baud = found->second;

    if(!monitor)
    {
        load_presets();
    }

    if(monitor)
    {
        tty_in = -1;
    }
    else
    {
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
    }

    tty_out = dup(1);
    if(tty_out == -1)
    {
        fprintf(stderr, "Can't open dup of stdout\n");
        exit(EXIT_FAILURE);
    }

    char const *serial_pathname = device_arg;
    serial = open_serial(serial_pathname, baud);
    if(serial == -1)
    {
        if(watch)
        {
            fprintf(stderr, "[Waiting for device to come online]\n");
            serial = watch_serial(serial_pathname, baud);
        }
        else
        {
            fprintf(stderr, "Can't open serial port \"%s\" (%s)\n", serial_pathname, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if(tty_in != -1)
    {
        tcgetattr(tty_in, &saved_termios);
        termios_saved = true;
        saved_tty_in = tty_in;

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        tcgetattr(tty_in, &options);

        // Raw input, 1 second timeout
        options.c_cflag |= (CLOCAL | CREAD | HUPCL);
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 10;

        options.c_iflag &= ~(INLCR | ICRNL);
        options.c_iflag |= (IXON | IXOFF);
        options.c_lflag = 0;

        tcflush(tty_in, TCIFLUSH);

        if(tcsetattr(tty_in, TCSANOW, &options) != 0)
        {
            perror("setting stdin tc");
            goto restore;
        }

        printf("press \"~\" (tilde) and then \"h\" for some help.\n");
    }

    run_loop(serial, tty_in, tty_out, serial_pathname, baud,
             watch, expect_disconnect, timestamp);

restore:

    restore_terminal();
    if(tty_in != -1)
        close(tty_in);

    close(serial);
    close(tty_out);

    return 0;
}
