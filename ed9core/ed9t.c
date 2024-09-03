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
#include <termios.h>
#include <unistd.h>

/*** DATA ***/
struct termios orig_termios;

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

void disable_raw_mode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
  {
    die("tcsetattr");
  }
}

void enable_raw_mode()
{
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
  {
    die("tcgetattr");
  }

  atexit(disable_raw_mode);

  struct termios raw = orig_termios;

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
 * @brief Clear the screen so that the editor can be displayed
 */
void editor_refresh_screen()
{
  /* clear the screen with the J command and argument 2 */
  write(STDOUT_FILENO, "\x1b[2J", 4);
  /* reposition the cursor to the top left with the H command */
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** INIT ***/

int main()
{
  enable_raw_mode();

  while (1)
  {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return 0;
}
