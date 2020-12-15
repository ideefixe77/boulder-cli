#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

struct termios org_termios, game_termios, raw_termios;

void make_beep()
{
    printf("\x07");
}

void init_drawing()
{
    printf("\033[H");
}

void clear_screen()
{
    printf("\033[2J");
}

void hide_cursor(int hide)
{
    if (hide)
        printf("\033[?25l");
    else
        printf("\033[?25h");
}

void restore_terminal()
{
    tcsetattr(0, TCSANOW, &org_termios);
    clear_screen();
    hide_cursor(0);
}

void init_game_terminal()
{
    tcgetattr(0, &org_termios);
    memcpy(&game_termios, &org_termios, sizeof(game_termios));

    game_termios.c_lflag &= ~(ECHO|ICANON);
    tcsetattr(0, TCSANOW, &game_termios);

    memcpy(&raw_termios, &game_termios, sizeof(raw_termios));
    raw_termios.c_cc[VTIME] = 0;
    raw_termios.c_cc[VMIN] = 0;

    atexit(restore_terminal);

    clear_screen();
    hide_cursor(1);
}

void reset_raw_terminal()
{
    tcsetattr(0, TCSANOW, &game_termios);
}

void set_raw_terminal()
{
    cfmakeraw(&raw_termios);
    tcsetattr(0, TCSANOW, &raw_termios);
}

int kbhit()
{
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

int getkey()
{
    int key = -1;

    if (kbhit())
    {
        set_raw_terminal();
        key = getch();
        reset_raw_terminal();
    }

    return key;
}

int nsleep(const struct timespec *req, struct timespec *rem)
{
    struct timespec temp_rem;
    if (nanosleep(req, rem) == -1)
        return nsleep(rem, &temp_rem);
    else
        return 1;
}

int Sleep(unsigned long milisec)
{
    struct timespec req = { 0 }, rem = { 0 };
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    nsleep(&req, &rem);
    return 1;
}

