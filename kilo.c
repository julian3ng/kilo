#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    // Save original flags
    tcgetattr(STDIN_FILENO, &orig_termios);

    // Happens on exit
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    
    // c_lflag handles local flags
    // there are also input, output, and control flags
    raw.c_lflag &= ~(ECHO);

    // Set attributes on stdin, after stdin is flushed
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int main(void) {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}
