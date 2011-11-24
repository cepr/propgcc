/**
 * @file osint_linux.c
 *
 * Serial I/O functions used by PLoadLib.c
 *
 * Copyright (c) 2009 by John Steven Denson
 * Modified in 2011 by David Michael Betz
 *
 * MIT License                                                           
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>
#include <sys/select.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>

#include "osint.h"

typedef int HANDLE;
static HANDLE hSerial;
static struct termios old_sparm;

static void chk(char *fun, int sts)
{
    if (sts != 0)
        printf("%s failed\n", fun);
}

int serial_find(const char* prefix, int (*check)(const char* port, void* data), void* data)
{
    char path[PATH_MAX];
    int prefixlen = strlen(prefix);
    struct dirent *entry;
    DIR *dirp;
    
    if (!(dirp = opendir("/dev")))
        return -1;
    
    while ((entry = readdir(dirp)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefixlen) == 0) {
            sprintf(path, "/dev/%s", entry->d_name);
            if ((*check)(path, data) == 0) {
                closedir(dirp);
                return 0;
            }
        }
    }
    
    closedir(dirp);
    return -1;
}

/**
 * open serial port
 * @param port - COMn port name
 * @returns hSerial - file handle to serial port
 */
int serial_init(const char* port, unsigned long baud)
{
    struct termios sparm;
    int tbaud = 0;

    /* open the port */
    hSerial = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(hSerial == -1) {
        //printf("Invalid file handle for serial port '%s'\n",port);
        //printf("Error '%s'\n",strerror(errno));
        return 0;
    }

    switch(baud) {
        case 0:
            tbaud = B115200;
            break;
        case 115200:
            tbaud = B115200;
            break;
        case 57600:
            tbaud = B57600;
            break;
        case 38400:
            tbaud = B38400;
            break;
        default:
            printf("Unsupported baudrate. Use /dev/tty*:115200, /dev/tty*:57600, or /dev/tty*:38400\n");
            serial_done();
            exit(2);
            break;
    }

    fcntl(hSerial, F_SETFL, 0);

    /* get the current options */
    chk("tcgetattr", tcgetattr(hSerial, &old_sparm));
    sparm = old_sparm;
    
    /* set raw input */
    memset(&sparm, 0, sizeof(sparm));
    sparm.c_cflag     = CS8 | CLOCAL | CREAD;
    sparm.c_lflag     = 0; // &= ~(ICANON | ECHO | ECHOE | ISIG);
    sparm.c_oflag     = 0; // &= ~OPOST;

    sparm.c_iflag     = IGNPAR | IGNBRK;
    sparm.c_cc[VTIME] = 0;
    sparm.c_cc[VMIN] = 1;
    chk("cfsetispeed", cfsetispeed(&sparm, tbaud));
    chk("cfsetospeed", cfsetospeed(&sparm, tbaud));

    /* set the options */
    chk("tcflush", tcflush(hSerial, TCIFLUSH));
    chk("tcsetattr", tcsetattr(hSerial, TCSANOW, &sparm));

    return hSerial;
}

/**
 * close serial port
 */
void serial_done(void)
{
    tcsetattr(hSerial, TCSANOW, &old_sparm);
    close(hSerial);
}

/**
 * receive a buffer
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to read
 * @returns number of bytes read
 */
int rx(uint8_t* buff, int n)
{
    ssize_t bytes = read(hSerial, buff, n);
    if(bytes < 1) {
        printf("Error reading port: %d\n", (int)bytes);
        return 0;
    }
    return (int)bytes;
}

/**
 * transmit a buffer
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to send
 * @returns zero on failure
 */
int tx(uint8_t* buff, int n)
{
    ssize_t bytes;
#if 0
    int j = 0;
    while(j < n) {
        printf("%02x ",buff[j++]);
    }
    printf("tx %d byte(s)\n",n);
#endif
    bytes = write(hSerial, buff, n);
    if(bytes != n) {
        printf("Error writing port\n");
        return 0;
    }
    return (int)bytes;
}

/**
 * receive a buffer with a timeout
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to read
 * @param timeout - timeout in milliseconds
 * @returns number of bytes read or SERIAL_TIMEOUT
 */
int rx_timeout(uint8_t* buff, int n, int timeout)
{
    ssize_t bytes = 0;
    struct timeval toval;
    fd_set set;

    FD_ZERO(&set);
    FD_SET(hSerial, &set);

    toval.tv_sec = timeout / 1000;
    toval.tv_usec = (timeout % 1000) * 1000;

    if (select(hSerial + 1, &set, NULL, NULL, &toval) > 0) {
        if (FD_ISSET(hSerial, &set))
            bytes = read(hSerial, buff, n);
    }

    return (int)(bytes > 0 ? bytes : SERIAL_TIMEOUT);
}

/**
 * hwreset ... resets Propeller hardware using DTR
 * @param sparm - pointer to DCB serial control struct
 * @returns void
 */
void hwreset(void)
{
    int cmd = TIOCM_DTR;
    ioctl(hSerial, TIOCMBIS, &cmd); // assert DTR pin
    msleep(10);
    ioctl(hSerial, TIOCMBIC, &cmd); // deassert DTR pin
    msleep(100);
    tcflush(hSerial, TCIFLUSH);
}

/**
 * sleep for ms milliseconds
 * @param ms - time to wait in milliseconds
 */
void msleep(int ms)
{
#if 0
    volatile struct timeb t0, t1;
    do {
        ftime((struct timeb*)&t0);
        do {
            ftime((struct timeb*)&t1);
        } while (t1.millitm == t0.millitm);
    } while(ms-- > 0);
#else
    usleep(ms * 1000);
#endif
}

#define ESC     0x1b    /* escape from terminal mode */

/**
 * simple terminal emulator
 */
extern int check_for_exit;

void terminal_mode(void)
{
    struct termios oldt, newt;
    char buf[128];
    ssize_t cnt;
    fd_set set;
    int exit_char = 0xdead; /* not a valid character */
    int sawexit_char = 0;
    int sawexit_valid = 0; 
    int exitcode = 0;
    int continue_terminal = 1;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (check_for_exit)
      {
	exit_char = 0xff;
      }

#if 0
    /* make it possible to detect breaks */
    tcgetattr(hSerial, &newt);
    newt.c_iflag &= ~IGNBRK;
    newt.c_iflag |= PARMRK;
    tcsetattr(hSerial, TCSANOW, &newt);
#endif

    do {
        FD_ZERO(&set);
        FD_SET(hSerial, &set);
        FD_SET(STDIN_FILENO, &set);
        if (select(hSerial + 1, &set, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(hSerial, &set)) {
	        if ((cnt = read(hSerial, buf, sizeof(buf))) > 0) {
		    int i;
		    // check for breaks
		    ssize_t realbytes = 0;
		    for (i = 0; i < cnt; i++) {
		      if (sawexit_valid)
			{
			  exitcode = buf[i];
			  continue_terminal = 0;
			}
		      else if (sawexit_char) {
			if (buf[i] == 0) {
			  sawexit_valid = 1;
			} else {
			  buf[realbytes++] = exit_char;
			  buf[realbytes++] = buf[i];
			  sawexit_char = 0;
			}
		      } else if (((int)buf[i] & 0xff) == exit_char) {
			sawexit_char = 1;
		      } else {
			buf[realbytes++] = buf[i];
		      }
		    }
		    write(fileno(stdout), buf, realbytes);
		}
            }
            if (FD_ISSET(STDIN_FILENO, &set)) {
                if ((cnt = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                    int i;
                    for (i = 0; i < cnt; ++i)
                        if (buf[i] == ESC)
                            goto done;
                    write(hSerial, buf, cnt);
                }
            }
        }
    } while (continue_terminal);

done:
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (sawexit_valid)
      {
	exit(exitcode);
      }
    
}

