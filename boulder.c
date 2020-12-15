/* 
 * Boulder Palm
 * Copyright (C) 2001-2020 by Wojciech Martusewicz <martusewicz@interia.pl> 
 */

#include "tools.h"
#include "levels.h"

#define SCREEN_SIZE_X       40
#define SCREEN_SIZE_Y       22

#define X_MARGIN            0
#define Y_MARGIN            0

#define TILE_SIZE           1
#define BITMAP_MAX          14  /* Number of bitmaps to load */

#if (SCREEN_SIZE_X / TILE_SIZE) < LEVELS_WIDTH
    #define BOARD_WIDTH     (SCREEN_SIZE_X / TILE_SIZE)
#else
    #define BOARD_WIDTH     LEVELS_WIDTH
#endif
#if (SCREEN_SIZE_Y / TILE_SIZE) < LEVELS_HIGH - 1 // -1 for bottom margin
    #define BOARD_HIGH      ((SCREEN_SIZE_Y / TILE_SIZE) - 1)
#else
    #define BOARD_HIGH      LEVELS_HIGH
#endif

#define STANDARD_DELAY      1000
#define INTER_TIME          60

enum tile {TUNNEL, WALL, HERO, ROCK, DIAMOND, GROUND, METAL, BOX, DOOR, FLY, 
           CRASH};
enum hero {KILLED, FACE1, FACE2, RIGHT, LEFT};
enum sound {SOUND_NONE, SOUND_MOVE, SOUND_DIAMOND, SOUND_EXPLOSION};
enum direction {NORTH, EAST, SOUTH, WEST};
enum move {REAL, GHOST};
enum box_state {STILL, MOVING};
enum side {FALL_LEFT = -1, FALL_RIGHT = 1};

struct game
{
    int current_level;
    int level_diamonds;   // Total number of diamonds to pick up
    int level_time;       // Total time to pass the board
    int diamonds;         // Diamonds left
    int time;             // Time left
    enum hero hero_state; // Direction of player
    enum move move_mode;  // Move mode (real move or action without move)
    int lastposx, lastposy;
    int move_time;        // Time of last move (impatience feature)
    int sound_mode;
    enum sound sound_to_play;
};

struct board_mem
{
    unsigned char board:4;
    unsigned char rock_move:1;
    unsigned char box_move:1;
    unsigned char box_dir:2;
};

/********************
 * Global variables *
 ********************/
struct game Game;
unsigned char Mem[LEVELS_HIGH][LEVELS_WIDTH];


/*********************************************
 * Access (get/set) to game board properties *
 *********************************************/
int GetBoard(int h, int w)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    return b->board;
}

void SetBoard(int h, int w, int v)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    b->board = v;
}

int GetRockMove(int h, int w)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    return b->rock_move;
}

void SetRockMove(int h, int w, int v)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    b->rock_move = v;
}

int GetBoxMove(int h, int w)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    return b->box_move;
}

void SetBoxMove(int h, int w, int v)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    b->box_move = v;
}

int GetBoxDir(int h, int w)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    return b->box_dir;
}

void SetBoxDir(int h, int w, int v)
{
    struct board_mem *b = (struct board_mem*)&(Mem[h][w]);
    b->box_dir = v;
}


/*****************
 * Loading level *
 *****************/
int LoadLevel(int level)
{
    int j, i;

    if (level >= LEVELS_NUMBERS || level < 0)
        return -1;

    for (j = 0; j <= LEVELS_HIGH - 1; j++)
    {
        for (i = 0; i <= LEVELS_WIDTH - 1; i++)
        {
            char t = (levels[level][j][i]);
            SetBoard(j, i, t - 48);
        }
    }

    Game.level_diamonds = levels_diamonds[level];
    Game.level_time = levels_time[level];
   
    return 0;
}


/**************************************
 * This function starts the new board *
 **************************************/
void StartLevel(int new_level)
{
    if (LoadLevel(new_level) < 0)
        if (Game.current_level != 0)
        {
            Game.current_level = 0;
            StartLevel(Game.current_level);
        }
    Game.time = Game.level_time;
    Game.move_time = Game.level_time;
    Game.diamonds = Game.level_diamonds;
    Game.hero_state = FACE1;
}


/******************
 * Play the sound *
 ******************/
void SoundPlay(void)
{
    if (Game.sound_mode && Game.sound_to_play != SOUND_NONE)
    {
        // TODO: Play the sound...
        switch (Game.sound_to_play)
        {
            case SOUND_MOVE:
                break;
            case SOUND_DIAMOND:
                make_beep(); 
                break;
            case SOUND_EXPLOSION:
                break;
            case SOUND_NONE:
                break;
        }

        Game.sound_to_play = SOUND_NONE;
    }
}


/****************************
 * Set the sound to be play *
 ****************************/
void SoundRequest(int sound)
{
    Game.sound_to_play = sound;
}


/******************
 * Make the crash *
 ******************/
void MakeCrash(int object, int y, int x)
{
    int j, i;

    for (j = y - 1; j <= y + 1; j++)
        for (i = x - 1; i <= x + 1; i++)
            if (GetBoard(j, i) != METAL) 
                SetBoard(j, i, object);

    SoundRequest(SOUND_EXPLOSION);
}


/********************
 * Remove the crash *
 ********************/
void CrashRemove(void)
{
    int j, i;

    for (j = LEVELS_HIGH - 2; j > 0; j--)
        for (i = 0; i <= LEVELS_WIDTH - 1; i++)
            if (GetBoard(j, i) == CRASH)
                SetBoard(j, i, TUNNEL);
}


/***************************************************
 * This function control each other box and fly AI *
 ***************************************************/
int MoveBox(int j, int i, int d)
{
    int dj = j, di = i;

    if (d > WEST)
        d -= (WEST + 1);
    if (d < NORTH)
        d = WEST;

    switch (d)
    {
        case NORTH: dj -= 1; break;
        case EAST:  di += 1; break;
        case SOUTH: dj += 1; break;
        case WEST:  di -= 1; break;
    }

    if (GetBoard(dj, di) == HERO)
    {
        if (GetBoard(j, i) == BOX)
            MakeCrash(CRASH, dj, di);
        else
            MakeCrash(DIAMOND, dj, di);
        return 1;
    }

    if (GetBoard(dj, di) == TUNNEL)
    {
        if (GetBoard(j, i) == BOX)
            SetBoard(dj, di, BOX);
        else
            SetBoard(dj, di, FLY);
        SetBoard(j, i, TUNNEL);
        SetBoxMove(dj, di, MOVING);
        SetBoxDir(dj, di, d);
        return 1;
    }

    return 0;
}


/**********************************************
 * This function control boxs's and flys's AI *
 **********************************************/
void MoveBoxes(void)
{
    int j, i, d;

    for (j = LEVELS_HIGH - 2; j > 0; j--)
        for (i = 1; i < LEVELS_WIDTH - 1; i++)
            SetBoxMove(j, i, STILL);

    for (j = LEVELS_HIGH - 2; j > 0; j--)
        for (i = 1; i < LEVELS_WIDTH - 1; i++)
            if ((GetBoard(j, i) == BOX || GetBoard(j, i) == FLY) 
                && GetBoxMove(j, i) == STILL)
            {
                for (d = GetBoxDir(j, i) - 1; d <= GetBoxDir(j, i) + 2; d++)
                    if (MoveBox(j, i, d))
                        break;
            }
}


/*******************************************
 * Falling rock and diamonds on given side *
 *******************************************/
void FallingOnSide(int j, int i, int side)
{
    if (GetBoard(j, i + side) == TUNNEL 
        && GetBoard(j + 1, i + side) == TUNNEL)
    {
        SetBoard(j, i + side, GetBoard(j, i));
        SetBoard(j, i, TUNNEL);
        SetRockMove(j, i + side, MOVING);
    }
}


/***************************************************
 * This function control rock and diamonds falling *
 ***************************************************/
void MoveRocks(void)
{
    int j, i;

    for (j = LEVELS_HIGH - 2; j > 0; j--)
        for (i = (j % 2) ? LEVELS_WIDTH - 2 : 1;
             (j % 2) ? i > 0 : i < LEVELS_WIDTH - 1;
             (j % 2) ? i-- : i++)
        {
            if (GetBoard(j, i) == ROCK || GetBoard(j, i) == DIAMOND)
            {
                // Falling rock or diamond on right or left
                if (GetBoard(j + 1, i) == ROCK
                    || GetBoard(j + 1, i) == DIAMOND
                    || GetBoard(j + 1, i) == WALL
                    || GetBoard(j + 1, i) == DOOR
                    || GetBoard(j + 1, i) == METAL)
                {
                    if (rand() & 1)
                        FallingOnSide(j, i, FALL_RIGHT);                        
                    else
                        FallingOnSide(j, i, FALL_LEFT);
                }

                // Falling down
                if (GetBoard(j + 1, i) == TUNNEL)
                {
                    SetBoard(j + 1, i, GetBoard(j, i));         
                    SetBoard(j, i, TUNNEL);
                    SetRockMove(j + 1, i, MOVING);
                }

                // Rock or diamond kills the player
                if (GetBoard(j + 1, i) == HERO && GetRockMove(j, i) == MOVING)
                    MakeCrash(CRASH, j + 1, i);
                
                // Rock or diamond kills the BOX
                if (GetBoard(j + 1, i) == BOX)
                    MakeCrash(CRASH, j + 1, i);
                if (GetBoard(j + 1, i) == FLY)
                    MakeCrash(DIAMOND, j + 1, i);

                SetRockMove(j, i, STILL);
            }
        }
}


/**********************************
 * This function finds the object *
 **********************************/
int FindObject(int object, int *y, int *x)
{
    int j, i;

    for (j = 1; j < LEVELS_HIGH - 1; j++)
        for (i = 1; i < LEVELS_WIDTH - 1; i++)
            if (GetBoard(j, i) == object)
            {
                if (y != 0)
                    *y = j;
                if (x != 0)
                    *x = i;
                return object; // Object found
            }
    return (-1); // Object not found
}


/**********************************
 * This function moves the player *
 **********************************/
void MoveHero(int y, int x)
{
    int j, i, o;

    if (FindObject(HERO, &j, &i) != HERO)
        return;

    o = GetBoard(j + y, i + x);

    switch (o)
    {
        case DIAMOND: // Get the diamond
            if (Game.diamonds)
                Game.diamonds--;
            SoundRequest(SOUND_DIAMOND);
            break;      
        case ROCK: // Push the rock
            if (x > 0)
                if (GetBoard(j, i + x + 1) == TUNNEL)
                {
                    SetBoard(j, i + x, TUNNEL);
                    SetBoard(j, i + x + 1, ROCK);
                }
            if (x < 0)
                if (GetBoard(j, i + x - 1) == TUNNEL)
                {
                    SetBoard(j, i + x, TUNNEL);
                    SetBoard(j, i + x - 1, ROCK);
                }
            o = GetBoard(j + y, i + x);
            break;
        case BOX:
            MakeCrash(CRASH, j + y, i + x);
            return;
        case FLY:
            MakeCrash(DIAMOND, j + y, i + x);
            return;
    }

    // Move player if it's possible
    if (o != WALL && o != ROCK && o != METAL
         && j + y >= 0 && i + x >= 0 
         && j + y < LEVELS_HIGH && i + x < LEVELS_WIDTH
         && (o != DOOR || !Game.diamonds))
    {
        if (Game.move_mode == REAL)
        {
            SetBoard(j, i, TUNNEL);
            SetBoard(j + y, i + x, HERO);
        } else
        {
            SetBoard(j + y, i + x, TUNNEL);
        }
        if (Game.sound_to_play == SOUND_NONE)
            SoundRequest(SOUND_MOVE);
    }

    Game.move_mode = REAL;
    Game.move_time = Game.time;
    return;
}


/***************
 * Kill player *
 ***************/
void KillHero(void)
{
    int y, x;

    if (FindObject(HERO, &y, &x) == HERO)
        MakeCrash(CRASH, y, x);
}


char SelectTile(int item, int posx, int posy)
{
    char t;

    switch (item)
    {
        case 0: t = ' '; break; // TUNNEL
        case 1: t = '='; break; // WALL
        case 2: t = 'R'; break; // HERO
        case 3: t = 'o'; break; // ROCK
        case 4: t = '*'; break; // DIAMOND
        case 5: t = '~'; break; // GROUND
        case 6: t = '#'; break; // METAL
        case 7: t = '@'; break; // BOX
        case 8: t = '>'; break; // DOOR
        case 9: t = '%'; break; // FLY
        case 10: t = '^'; break; //CRASH
        default: t = 'R';
    }

    return t;
}

/**********************************************************
 * This function draw currently visable part of the board *
 **********************************************************/
void ShowView(void)
{
    int starty, startx, posy, posx, y, x;

    /* Find the player */
    if (FindObject(HERO, &starty, &startx) < 0)
    {
        startx = Game.lastposx;
        starty = Game.lastposy;
        Game.hero_state = KILLED;
    } else
    {
        Game.lastposx = startx;
        Game.lastposy = starty;
    }

    // Scrolling the board
    startx -= BOARD_WIDTH / 2;
    if (startx < 0)
        startx = 0;
    if (startx > LEVELS_WIDTH - BOARD_WIDTH)
        startx = LEVELS_WIDTH - BOARD_WIDTH;

    starty -= BOARD_HIGH / 2;
    if (starty < 0)
        starty = 0;
    if (starty > LEVELS_HIGH - BOARD_HIGH)
        starty = LEVELS_HIGH - BOARD_HIGH;

    // Draw the board
    init_drawing();
    posy = starty;
    for (y = 0; y < BOARD_HIGH; y++)
    {
        posx = startx;
        for (x = 0; x < BOARD_WIDTH; x++)
        {
            int t = SelectTile(GetBoard(posy, posx), x, y);
            printf("%c", t);
            posx++;
        }
        printf("\n");
        posy++;
    }
}


/*******************************************************
 * Write time, score, etc and end of the Game checking *
 *******************************************************/
void ShowStatus(void)
{
    char txt[21];

    if (!Game.time)
    {
        KillHero();
        printf("   * Game Over *    \n");
    } else
    if (!Game.diamonds && FindObject(DOOR, 0, 0) < 0)
    {
        Sleep(STANDARD_DELAY);
        sprintf(txt, "    * Level %02d *    \n", Game.current_level + 2);
        printf("%s", txt);
        Sleep(STANDARD_DELAY);
        StartLevel(++Game.current_level);
    } else
    {
        sprintf(txt, "L:%02d,D:%03d,T:%03d,M:%d\n", Game.current_level + 1, 
            Game.diamonds, Game.time, Game.sound_mode);
        printf("%s", txt);
    }
}


/**********************
 * Refreash the Board *
 **********************/
void RefreashBoard(void)
{
    static int t = 0;

    if (!t--)
    {
        CrashRemove();
        MoveRocks();
        MoveBoxes();
        ShowStatus();
        ShowView();
        SoundPlay();
        t = INTER_TIME / 5; // The speed of moving objects
    }
}


/*********************
 * Time decrementing *
 *********************/
void DecrementTime(void)
{
    static int t = INTER_TIME;

    if (Game.time <= 0 || Game.hero_state == KILLED)
        return;

    switch (t--)
    {
        case 0:
            Game.time--;
            t = INTER_TIME;
        case INTER_TIME / 2:
            if (Game.hero_state == FACE1 && Game.move_time - Game.time > 5)
                Game.hero_state = FACE2;
            else
                Game.hero_state = FACE1;
    }
}


/******************
 * Show the intro *
 ******************/
void ShowIntro(void)
{
//    printf("Boulder start");
//    Sleep(STANDARD_DELAY);
}


/********************
 * Start aplication *
 ********************/
void StartAplication(void)
{
    init_game_terminal();

    Game.current_level = 0;
    Game.diamonds      = 0;
    Game.move_mode     = REAL;
    Game.sound_mode    = 1;
    Game.sound_to_play = SOUND_NONE;

    ShowIntro();
    StartLevel(Game.current_level);
}


/*************************************
* Handle a key press from the player *
 *************************************/
int KeyDown(void)
{
    int key = getkey();

    switch (key)
    {
        case 'a':
        case 68:
            MoveHero(0, -1);
            Game.hero_state = LEFT;
            return 1;
        case 'd':
        case 67:
            MoveHero(0, 1);
            Game.hero_state = RIGHT;
            return 1;
        case 'w':
        case 65:
            MoveHero(-1, 0);
            return 1;
        case 's':
        case 66:
            MoveHero(1, 0);
            return 1;
        case 32: case 13: // Spacebar, Return
            if (Game.hero_state == KILLED)
                StartLevel(Game.current_level);
            else
                Game.move_mode = GHOST;
            break;
        case 'm':
            Game.sound_mode ^= 1;
            break;
        case 'n':
            StartLevel(++Game.current_level);
            break;
        case 'p':
            if (Game.current_level > 0)
                StartLevel(--Game.current_level);
            break;
        case 'r':
            KillHero();
            break;
        case 'j': // Respawn cheat
            SetBoard(Game.lastposy, Game.lastposx, HERO);
            Game.hero_state = FACE1;
            break;
        case 't': // Time cheat
            Game.time = Game.level_time;
            break;
        case 'q':
            exit(0);
    }
    return 0;
}


int main()
{
    StartAplication();

    while (1)
    {
        if (KeyDown())
        {
            ShowView();
            SoundPlay();
        }

        DecrementTime();
        RefreashBoard();

        Sleep(1000 / 60);
    }
}

