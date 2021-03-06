/*** includes ***/

// These tell us what features are available, must come before includes.
// 
#define _DEFAULT_SOURCE
#define _BSD_SOURCE // all features
#define _GNU_SOURCE // and bsd ones

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> // Terminal I/O Control
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_LEFT  = 1000,
    ARROW_DOWN,
    ARROW_UP,
    ARROW_RIGHT,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    DEL_KEY,
};

/*** data ***/

typedef struct erow {
    int size;
    char *chars;
} erow;

struct editorConfig {
    int cx, cy;                  // cursor position
    int rowoff;                  // current scrolled position
    int coloff;                  // current horizontal scrolled position
    int screenrows, screencols;  // screen height / width
    int numrows;                 // number of rows in buffer
    erow *row;                   // buffer rows
    struct termios orig_termios; /// original terminal configuration
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

int editorReadKey() {
    int nread;
    char c;
    // until a char is read, read
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '3': return DEL_KEY;
                    case '1':
                    case '7': return HOME_KEY;
                    case '4':
                    case '8': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                case 'D': return ARROW_LEFT;
                case 'B': return ARROW_DOWN;
                case 'A': return ARROW_UP;                
                case 'C': return ARROW_RIGHT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
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

/*** row operations ***/

void editorAppendRow(char *s, size_t len) {
    // Add a row
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    // Consider the last row, index 'at'
    int at = E.numrows;
    // Size = len arg
    E.row[at].size = len;
    // Allocate the line's pointer
    E.row[at].chars = malloc(len + 1);
    // Copy the string into the pointer
    memcpy(E.row[at].chars, s, len);
    // Null out the last char
    E.row[at].chars[len] = '\0';
    E.numrows++;
}

/*** file i/o ***/

void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while(linelen > 0 && (line[linelen - 1] == '\n' ||
                              line[linelen - 1] == '\r')) {
            linelen--;
        }
        editorAppendRow(line, linelen);

    }
    free(line);
    fclose(fp);
}

/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}


/*** input ***/

void editorMoveCursor(int key) {
    switch (key) {
    case ARROW_LEFT:
        if (E.cx != 0) { E.cx--; }
        break;
    case ARROW_RIGHT:
        E.cx++;
        break;
    case ARROW_DOWN:
        if (E.cy < E.numrows) { E.cy++; }
        break;
    case ARROW_UP:
        if (E.cy != 0) { E.cy--; }
        break;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }

    break;
        
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}
        
/*** output ***/

void editorScroll() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }

    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }

    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }

    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
    
    
}

void editorDrawRows(struct abuf *ab) {
    for (int y=0; y<E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                                          "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }

                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        }  else {
            int len = E.row[filerow].size - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            abAppend(ab,& E.row[filerow].chars[E.coloff], len);
        }

        
        // clear each line during redraw
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
    
}

void editorRefreshScreen() {
    editorScroll();
    
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
             E.cy - E.rowoff + 1, E.cx - E.coloff + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/*** init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    } 

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
