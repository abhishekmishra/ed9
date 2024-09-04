/**
 * @file ed9t.c
 * @author Abhishek Mishra (abhishekmishra3@gmail.com
 * @brief Text editor based on kilo.c
 * @version 0.1
 * @date 2024-09-03
 *
 * @copyright Copyright (c) 2024
 *
 */

/*** INCLUDES ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/

/*
define a macro to create the effect of CTRL+key
using a bitmask with the value of the key
that sets the first 3 bits to 0
*/
#define CTRL_KEY(k) ((k) & 0x1f)

#define ED9T_WELCOME_MESSAGE "ED9T -- version %s"
#define ED9T_VERSION "0.1.0"

/* special editor keys enum */
typedef enum
{
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
} EditorKey;

/*** DATA ***/

/* type for one row of data */
typedef struct
{
  int size;
  char *chars;
} EditorRow;

/* type for global state of the editor */
typedef struct
{
  int cx, cy;
  int rowoff;
  int screenrows;
  int screencols;
  int numrows;
  EditorRow *row;
  struct termios orig_termios;
} EditorConfig;

/* global editor configuration */
EditorConfig E;

/*** TERMINAL ***/
void die(const char *s)
{
  /* clear the screen with the J command and argument 2 */
  write(STDOUT_FILENO, "\x1b[2J", 4);
  /* reposition the cursor to the top left with the H command */
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

/**
 * @brief Disable the raw mode for the terminal
 */
void disable_raw_mode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
  {
    die("tcsetattr");
  }
}

/**
 * @brief Enable the raw mode for the terminal
 */
void enable_raw_mode()
{
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
  {
    die("tcgetattr");
  }

  atexit(disable_raw_mode);

  struct termios raw = E.orig_termios;

  /* turn off ctrl-s and ctrl-q (IXON) */
  /* turn off enter to newline (ICRNL) */
  /* turn off miscellaneous flags*/
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  /* turn off output processing (OPOST) */
  raw.c_oflag &= ~(OPOST);

  raw.c_cflag |= (CS8);

  /* turn off echo (ECHO) and canonical mode (ICANON) */
  /* turn off ctrl-z and ctrl-c (ISIG) */
  /* turn off ctrl-v (IEXTEN) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  /* set timeout for read() */
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
  {
    die("tcsetattr");
  }
}

/**
 * @brief low-level keypress reading
 *
 * @return char character read from the keypress
 */
int editor_read_key()
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
    {
      die("read");
    }
  }

  /*
  read the arrow keys in the form of '\x1b[ABCD]'
  and convert them to wsad keys

  also read PGUP and PGDOWN keys in the form of Esc[5~ and Esc[6~

  The Home key could be sent as <esc>[1~, <esc>[7~, <esc>[H, or <esc>OH. Similarly, the End key could be sent as <esc>[4~, <esc>[8~, <esc>[F, or <esc>OF.

  The DEL key could be sent as <Esc>[3~.
  */
  if (c == '\x1b')
  {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
    {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
    {
      return '\x1b';
    }
    if (seq[0] == '[')
    {
      if (seq[1] >= '0' && seq[1] <= '9')
      {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
        {
          return '\x1b';
        }
        if (seq[2] == '~')
        {
          switch (seq[1])
          {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      }
      else
      {
        switch (seq[1])
        {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    }
    else if (seq[0] == 'O')
    {
      switch (seq[1])
      {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  }
  else
  {
    return c;
  }
}

/**
 * @brief Get the cursor position by using the value of the 'n' command
 *
 * @param rows pointer to the number of rows
 * @param cols pointer to the number of columns
 * @return int 0 if successful
 */
int get_cursor_position(int *rows, int *cols)
{
  /* buffer for the 'n' command return value */
  char buf[32];
  unsigned int i = 0;

  /* use the n command */
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
  {
    return -1;
  }

  /* read the response of the command, which is a control sequence */
  while (i < sizeof(buf) - 1)
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
    {
      break;
    }
    if (buf[i] == 'R')
    {
      break;
    }
    i++;
  }
  buf[i] = '\0';

  /* parse the control sequence to get the cursor position */
  if (buf[0] != '\x1b' || buf[1] != '[')
  {
    return -1;
  }
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
  {
    return -1;
  }

  return 0;
}

/**
 * @brief Get the window size of the terminal
 * and return the number of rows and columns
 * as values of the pointers passed to the function
 * and return 0 if successful
 *
 * This uses the ioctl system call TIOCGWINSZ to get the window size
 * and
 *
 * @param rows pointer to the number of rows
 * @param cols pointer to the number of columns
 * @return int 0 if successful
 */
int get_window_size(int *rows, int *cols)
{
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
  {
    /*
    use the B (down) and C (forward) cursor commands to move the cursor to the
    bottom right of the window and then read the cursor position to get the
    window size

    we use a large number to ensure that the cursor is moved to the bottom right
    but we don't use the H command as it is not documented to stay at the
    edge of the screen.
    */
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
    {
      return -1;
    }

    return get_cursor_position(rows, cols);
  }
  else
  {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** ROW OPERATIONS ***/

void editor_append_row(char *s, size_t len)
{
  E.row = realloc(E.row, sizeof(EditorRow) * (E.numrows + 1));
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/*** FILE I/O ***/

void editor_open(char *filename)
{
  FILE *fp = fopen(filename, "r");
  if (!fp)
  {
    die("fopen");
  }
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  linelen = getline(&line, &linecap, fp);
  while ((linelen = getline(&line, &linecap, fp)) != -1)
  {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
    {
      linelen--;
    }

    editor_append_row(line, linelen);
  }
  free(line);
  fclose(fp);
}

/*** APPEND BUFFER ***/
typedef struct
{
  char *b;
  int len;
} AppendBuffer;

#define ABUF_INIT {NULL, 0}

/**
 * @brief Append a string to the append buffer
 *
 * @param ab pointer to the append buffer
 * @param s string to append
 * @param len length of the string
 */
void ab_append(AppendBuffer *ab, const char *s, int len)
{
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
  {
    return;
  }

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

/**
 * @brief Free the memory used by the append buffer
 *
 * @param ab pointer to the append buffer
 */
void ab_free(AppendBuffer *ab)
{
  free(ab->b);
}

/*** INPUT ***/

/**
 * @brief Move the cursor based on which key is pressed
 *
 * @param
 */
void editor_move_cursor(int key)
{
  switch (key)
  {
  case ARROW_UP:
    if (E.cy != 0)
    {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows)
    {
      E.cy++;
    }
    break;
  case ARROW_LEFT:
    if (E.cx != 0)
    {
      E.cx--;
    }
    break;
  case ARROW_RIGHT:
    if (E.cx != E.screencols - 1)
    {
      E.cx++;
    }
    break;
  }
}

/**
 * @brief read the editor keypress and process it
 * to convert it to editor commands
 */
void editor_process_keypress()
{
  int c = editor_read_key();

  switch (c)
  {
  case CTRL_KEY('q'):
    /* clear the screen with the J command and argument 2 */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* reposition the cursor to the top left with the H command */
    write(STDOUT_FILENO, "\x1b[H", 3);

    exit(0);
    break;

    /*
    pageup and pagedown move the cursor to top and bottom
    of the screen
    */
  case PAGE_UP:
  case PAGE_DOWN:
  {
    int times = E.screenrows;
    while (times--)
    {
      editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
  }
  break;

  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    E.cx = E.screencols - 1;
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editor_move_cursor(c);
    break;
  }
}

/*** OUTPUT ***/

void editor_scroll()
{
  if (E.cy < E.rowoff)
  {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows)
  {
    E.rowoff = E.cy - E.screenrows + 1;
  }
}

/**
 * @brief Draw the rows of the editor
 *
 * @param ab pointer to the append buffer
 */
void editor_draw_rows(AppendBuffer *ab)
{
  for (int y = 0; y < E.screenrows; y++)
  {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows)
    {
      if (E.numrows == 0 && y == E.screenrows / 3)
      {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome), ED9T_WELCOME_MESSAGE, ED9T_VERSION);
        if (welcomelen > E.screencols)
        {
          welcomelen = E.screencols;
        }
        int padding = (E.screencols - welcomelen) / 2;
        if (padding)
        {
          ab_append(ab, "~", 1);
          padding--;
        }
        while (padding--)
        {
          ab_append(ab, " ", 1);
        }
        ab_append(ab, welcome, welcomelen);
      }
      else
      {
        ab_append(ab, "~", 1);
      }
    }
    else
    {
      int len = E.row[filerow].size;
      if (len > E.screencols)
      {
        len = E.screencols;
      }
      ab_append(ab, E.row[filerow].chars, len);
    }

    /* clear the line with the K command and argument 0 */
    ab_append(ab, "\x1b[K", 3);

    /* write the crlf for all but the last line */
    if (y < E.screenrows - 1)
    {
      ab_append(ab, "\r\n", 2);
    }
  }
}

/**
 * @brief Clear the screen so that the editor can be displayed
 */
void editor_refresh_screen()
{
  /* initialize the editor scroll */
  editor_scroll();

  AppendBuffer ab = ABUF_INIT;

  /* hide the cursor using ?25l */
  ab_append(&ab, "\x1b[?25l", 6);
  /* clear the screen with the J command and argument 2 */
  // ab_append(&ab, "\x1b[2J", 4);
  /* reposition the cursor to the top left with the H command */
  ab_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);

  /*
  move the cursor to position given by the editor config
  we use the H command with the column and row as arguments
  */
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.cx + 1);
  ab_append(&ab, buf, strlen(buf));

  /* reposition the cursor to the top left with the H command */
  // ab_append(&ab, "\x1b[H", 3);
  /* show the cursor using ?25h */
  ab_append(&ab, "\x1b[?25h", 6);

  /* write the buffer to the terminal */
  write(STDOUT_FILENO, ab.b, ab.len);
  ab_free(&ab);
}

/*** INIT ***/

/**
 * @brief Initialize the global editor state variable
 */
void init_editor()
{
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.numrows = 0;
  E.row = NULL;

  if (get_window_size(&E.screenrows, &E.screencols) == -1)
  {
    die("get_window_size");
  }
}

int main(int argc, char *argv[])
{
  /* enable the raw mode */
  enable_raw_mode();
  /* initialize the editor */
  init_editor();
  /* open a file */
  if (argc >= 2)
  {
    editor_open(argv[1]);
  }

  while (1)
  {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return 0;
}
