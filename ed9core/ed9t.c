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

/*** DEFINES ***/

/*
define a macro to create the effect of CTRL+key
using a bitmask with the value of the key
that sets the first 3 bits to 0
*/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** INCLUDES ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** DATA ***/

/* type for global state of the editor */
typedef struct
{
  int screenrows;
  int screencols;
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
char editor_read_key()
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
  return c;
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

  // TODO remove the 1 || condition
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

/*** INPUT ***/

/**
 * @brief read the editor keypress and process it
 * to convert it to editor commands
 */
void editor_process_keypress()
{
  char c = editor_read_key();

  switch (c)
  {
  case CTRL_KEY('q'):
    /* clear the screen with the J command and argument 2 */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* reposition the cursor to the top left with the H command */
    write(STDOUT_FILENO, "\x1b[H", 3);

    exit(0);
    break;
  }
}

/*** OUTPUT ***/

/**
 * @brief Draw the rows of the editor
 */
void editor_draw_rows()
{
  for (int y = 0; y < E.screenrows; y++)
  {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

/**
 * @brief Clear the screen so that the editor can be displayed
 */
void editor_refresh_screen()
{
  /* clear the screen with the J command and argument 2 */
  write(STDOUT_FILENO, "\x1b[2J", 4);
  /* reposition the cursor to the top left with the H command */
  write(STDOUT_FILENO, "\x1b[H", 3);

  editor_draw_rows();

  /* reposition the cursor to the top left with the H command */
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** INIT ***/

/**
 * @brief Initialize the global editor state variable
 */
void init_editor()
{
  if (get_window_size(&E.screenrows, &E.screencols) == -1)
  {
    die("get_window_size");
  }
}

int main()
{
  /* enable the raw mode */
  enable_raw_mode();
  /* initialize the editor */
  init_editor();

  while (1)
  {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return 0;
}
