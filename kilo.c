#include <termios.h>
#include <unistd.h>

void enableRawMode() {
    struct termios raw;
    
    // Get current terminal attributes    
    tcgetattr(STDIN_FILENO, &raw);

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
