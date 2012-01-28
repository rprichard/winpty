#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pconsole.h>

static int signalWriteFd;


// Put the input terminal into non-blocking non-canonical mode.
static termios setRawTerminalMode()
{
    if (!isatty(STDIN_FILENO)) {
	fprintf(stderr, "input is not a tty\n");
	exit(1);
    }
    if (!isatty(STDOUT_FILENO)) {
        fprintf(stderr, "output is not a tty\n");
	exit(1);
    }

    // This code makes the terminal output non-blocking.
    int flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    if (flags == -1) {
	perror("fcntl F_GETFL on stdout failed");
	exit(1);
    }
    if (fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL on stdout failed");
	exit(1);
    }

    termios buf;
    if (tcgetattr(STDIN_FILENO, &buf) < 0) {
	perror("tcgetattr failed");
	exit(1);
    }
    termios saved = buf;
    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    buf.c_cflag &= ~(CSIZE | PARENB);
    buf.c_cflag |= CS8;
    buf.c_oflag &= ~OPOST;
    buf.c_cc[VMIN] = 0;
    buf.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0) {
	fprintf(stderr, "tcsetattr failed\n");
	exit(1);
    }
    return saved;
}

static void restoreTerminalMode(termios original)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) < 0) {
	perror("error restoring terminal mode");
	exit(1);
    }
}

static void terminalResized(int signo)
{
    char dummy = 0;
    write(signalWriteFd, &dummy, 1);
}

static void pconsoleIo(pconsole_t *pconsole)
{
    char dummy = 0;
    write(signalWriteFd, &dummy, 1);
}

int main()
{
    winsize sz;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &sz);

    pconsole_t *pconsole = pconsole_open(sz.ws_col, sz.ws_row);
    if (pconsole == NULL) {
	fprintf(stderr, "Error creating pconsole.\n");
	exit(1);
    }

    pconsole_set_io_cb(pconsole, pconsoleIo);

    {
	struct sigaction resizeSigAct;
	memset(&resizeSigAct, 0, sizeof(resizeSigAct));
	resizeSigAct.sa_handler = terminalResized;
	resizeSigAct.sa_flags = SA_RESTART;
	sigaction(SIGWINCH, &resizeSigAct, NULL);
    }

    termios mode = setRawTerminalMode();
    int signalReadFd;

    const int bufSize = 4096;
    char writeBuf[bufSize];
    int writeBufAmount;
    char buf[bufSize];
    int amount;

    {
	int pipeFd[2];
	if (pipe2(pipeFd, O_NONBLOCK) != 0) {
	    perror("Could not create pipe");
	    exit(1);
	}
	signalReadFd = pipeFd[0];
	signalWriteFd = pipeFd[1];
    }

    while (true) {
	fd_set readfds;
	fd_set writefds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(signalReadFd, &readfds);
	if (pconsole_get_output_queue_size(pconsole) < bufSize)
	    FD_SET(STDIN_FILENO, &readfds);
	if (writeBufAmount > 0)
	    FD_SET(STDOUT_FILENO, &writefds);

	if (select(signalReadFd + 1, &readfds, &writefds, NULL, NULL) < 0) {
	    perror("select failed");
	    exit(1);
	}

	// Check for terminal resize.
	{
	    winsize sz2;
	    ioctl(STDIN_FILENO, TIOCGWINSZ, &sz2);
	    if (memcmp(&sz, &sz2, sizeof(sz)) != 0) {
		sz = sz2;
		pconsole_set_size(pconsole, sz.ws_col, sz.ws_row);
	    }
	}

	// Discard any data in the signal pipe.
	amount = read(signalReadFd, buf, bufSize);
	if (amount == 0 || amount < 0 && errno != EAGAIN) {
	    perror("error reading internal signal fd");
	    exit(1);
	}

	// Read from the pty and write to the pconsole.
	amount = bufSize - pconsole_get_output_queue_size(pconsole);
	if (amount > 0) {
	    amount = read(STDIN_FILENO, buf, amount);
	    if (amount == 0) {
		break;
	    } else if (amount < 0 && errno != EAGAIN) {
		perror("error reading from pty");
		exit(1);
	    } else if (amount > 0) {
		pconsole_write(pconsole, buf, amount);
	    }
	}

	// Read from the pconsole.
	amount = bufSize - writeBufAmount;
	if (amount > 0) {
	    amount = pconsole_read(pconsole, buf + writeBufAmount, amount);
	    if (amount == 0)
		break;
	    else if (amount > 0)
		writeBufAmount += amount;
	}

	// Write to the pty.
	if (writeBufAmount > 0) {
	    amount = write(STDOUT_FILENO, writeBuf, writeBufAmount);
	    if (amount == 0) {
		break;
	    } else if (amount < 0 && errno != EAGAIN) {
		perror("error writing to pty");
	    } else if (amount > 0) {
		int remaining = writeBufAmount - amount;
		if (amount < writeBufAmount)
		    memmove(writeBuf, writeBuf + amount, remaining);
		writeBufAmount = remaining;
	    }
	}
    }

    // TODO: Get the pconsole child exit code and exit with it.

    restoreTerminalMode(mode);
    return 0;
}
