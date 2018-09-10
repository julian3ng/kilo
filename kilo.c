/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h> // Terminal I/O Control
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
    int screenrows, screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
    // clear screen and move cursor to 1, 1 on exit
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(s);
    exit(1);
}

char editorReadKey() {
    int nread;
    char c;
    // until a char is read, read
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    unsigned int i = 0;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}



void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    // Save original flags
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    // Happens on exit
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    
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



/*** input ***/

void editorProcessKeypress() {
    char c = editorReadKey();
    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}
        
/*** output ***/

void editorDrawRows() {
    for (int y=0; y<E.screenrows; y++) {
        write(STDOUT_FILENO, "~", 1);
        if (y < E.screenrows - 1) {
            write(STDOUT_FILENO, "\r\n", 2);
        }
            
    }
}

void editorRefreshScreen() {
    // 0x1b = 27 = ESC
    // [ -> start escape sequence
    // 2J -> J command with 2 as argument (clear entire screen)
    // 1J -> clear up to cursor
    // 0J -> clear cursor to end
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}


/*** init ***/

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main(void) {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
