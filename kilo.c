#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    // Save original flags
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");

    // Happens on exit
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    
    // c_[iocl]flag handles input, output, control, and local (misc) flags

    // turn off break conditions sending SIGINT
    // turn off carriage return newline (C-m is now 13, not 10)
    // turn off parity checking?
    // turn off setting 8th bit of every byte to 0
    // turn off transmission stuff (IXON C-s, C-q)
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // turn off output newline carriage return
    // ("\n" doesn't translate to "\r\n" anymore)
    raw.c_oflag &= ~(OPOST);

    // set character size to 8 bits per byte
    raw.c_cflag |= (CS8);
    
    // turn off echoing and canonical (\n to actually enter input)
    // turn off C-v sending next literal
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    

    // Set attributes on stdin, after stdin is flushed
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int main(void) {
    enableRawMode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q') break;
        
    }
    return 0;
}
