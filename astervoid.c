/* ---------------------------------------------------------------
 *
 * astervoid.c
 *
 * Copyright (C) 2017-2018, 2021 Matthew Love <matthew.love@colorado.edu>
 *
 * This file is liscensed under the GPL v.2 or later and
 * is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <http://www.gnu.org/licenses/>
 *
 * --------------------------------------------------------------*/

#include <ncurses.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <math.h>

#define _version 0.1.2

#define DELAY 50000
#define FPS 8

#define MAX_ASTEROIDS 500
#define MAX_MISSLES 20
#define MAX_CHESTS 20

#define RED 1
#define GREEN 2
#define YELLOW 3
#define BLUE 4
#define CYAN 5
#define MAGENTA 6
#define WHITE 7

#define GAME_PLAY 0
#define GAME_PAUSED 1
#define GAME_OVER 2
#define GAME_TITLE 3
#define GAME_RESET 4

#define SHIP 0
#define UFO 1
#define ASTEROID 2
#define MISSLE 3
#define CHEST 4

WINDOW *wEmpty;
WINDOW *wBattleField;
WINDOW *wStarField;
WINDOW *wStatus;
WINDOW *wGameOver;
WINDOW *wGamePaused;
WINDOW *wTitleScreen;

time_t t;

int max_y = 0, max_x = 0;
int scrmax_y = 0, scrmax_x = 0;
int lAst = 0, lMiss = 0, lChest = 0;

char* ranks[6]={"Sprite","Novice","Ensign","Captain","Expert","Elite"};

char* dShips[8] = {"<!","\\~","/\\","~//","!>","_\\","\\/","/_"};
int dxShips[8] = {-1,-1,0,1,1,1,0,-1};
int dyShips[8] = {0,-1,-1,-1,0,1,1,1};

//char* dUfo[4][6] = {"(* *)", "(** )", "(* *)", "( **)"};
//char* dUfo[4] = {"(* *)", "(** )", "(* *)", "( **)"};
char* dUfo[3] = {"(* *)", "(** )", "( **)"};
int dxUfo[3] = {0, -1, 1};

char dAst2[2][3][4] = {
  {" __ ",
   "|, :",
   " ~~"},
  {" __ ",
   "|; :",
   " ~~"},
};

char dAst5[3][5][10] = {
  {"  ,_--_.  ",
   "/o    __ \\",
   "|    (__)|",
   "\\       / ",
   "  `-~~-`  "},
  {"   ____.  ",
   " _/(_)  \\ ",
   "|        |",
   "\\   o   / ",
   "  `----`  "},
  {"  ,~~__.  ",
   "/      ()\\",
   "\\ .  __  |",
   " \\  (__)/ ",
   "  `-~--`  "},
};

typedef struct gStats gStats;
struct gStats {
  int astSpeed;
  int ufoSpeed;
  int level;
  int astLevel;
  char rank[20];
  int status;
};

gStats stats;

typedef struct spOb spOb;
struct spOb {
  int type; // type of space object; 0-4
  int subtype; // varies by obj // missles(0/1) asteroids(0/1)
  int x; // current ob x
  int y; // current ob y
  int max_x; // current ob max_x
  int min_x; // current ob min_x (same as x)
  int max_y; // current ob max_y
  int min_y; // current ob min_y
  int dx; // current x direction
  int dy; // current y direction
  int dS; // current obj dir: 0-7
  int draw; // 0/1 draw or not
  int color; // color of object
  int speed; // current obj speed: 1=fast +1=slower
  int score; // how many other objects has this one destroyed
  int drift; //0/1 does this object drift?
  int iter; // number of this space object
  int mvcnt; // move count
  int lives; // number of lives
  char* dOb; // space object char
  WINDOW *spWin; // space object window
};

spOb ship;
spOb ufo;
spOb asts[MAX_ASTEROIDS];
spOb chests[MAX_CHESTS];
spOb missles[MAX_MISSLES];

int mod (int a, int b) {
  if (b < 0) {
    return mod(a, -b);
  }
  int ret = a % b;
  if (ret < 0) {
    ret+=b;
  }
  return ret;
}

void displayOnBattleField(WINDOW *wElem, int x, int y, int xx, int yy) {
  copywin(wElem, wBattleField, 0, 0, y, x, yy, xx, 0);
  wrefresh(wBattleField);
}

void clearFromBattleField(int x, int y, int xx, int yy) {
  copywin(wEmpty, wBattleField, y, x, y, x, yy, xx, 0);
}

void bonusDisplay(int x, int y, int width, int height, char* db){
  WINDOW* wShipBonus;
  int t;

  wShipBonus=newpad(height,width);
  for(t=0;t<10;t++){ 			// 5 frames
    wattrset(wShipBonus,COLOR_PAIR(mod(t,6)));	// set color
    wclear(wShipBonus);	// clear pad
    waddstr(wShipBonus, db);
    displayOnBattleField(wShipBonus,x,y,x+width-1,y+height-1);
    usleep(50000);		// play animation not too fast
  }
}

void breakDisplay(int nAst){
  WINDOW* wAstBreak;
  int t;

  wAstBreak=newpad(5,7);
  for(t=0;t<10;t++){
    wattrset(wAstBreak,COLOR_PAIR(mod(t,6)));
    wclear(wAstBreak);
    if (t % 2 == 0) {
      waddstr(wAstBreak, asts[nAst].dOb);
    } else {
      waddstr(wAstBreak, dAst2[0][0]);
    }
    displayOnBattleField(wAstBreak,asts[nAst].x,asts[nAst].y,asts[nAst].max_x,asts[nAst].max_y);
    usleep(5000);
  }
}

void explosionDisplay(int x, int y, int width, int height) {
  WINDOW* wShipExplosion;
  char explosionChars[18+1]="@~`.,^#*-_=\\/%{}  ";
  int t,s,r;
  
  wShipExplosion=newpad(height,width);
  for(t=0;t<6;t++){
    wclear(wShipExplosion);
    wattrset(wShipExplosion,COLOR_PAIR(mod(t,6)));
    for(s=0;s<width;s++){
      for(r=0;r<height;r++){
    	waddch(wShipExplosion,explosionChars[rand()%18]);
      }
    }
    displayOnBattleField(wShipExplosion,x,y,x+width-1,y+height-1);
    //usleep(50000);
    usleep(10000);
  }
} // todo: kann man bestimmt noch besser machen.

/* collisions */

int collisionP(int sx, int sy, int sxx, int syy, int tx, int ty, int txx, int tyy) {

  if ((sy >= ty && sy <= tyy) || syy >= ty && syy <= tyy) {
    if ((sx >= tx && sx <= txx) || sxx >= tx && sxx <= txx) {
      return 1;
    }
  }
  return 0;
}

int spObCollision(spOb* st0, spOb* st1) {
  return collisionP(st0->x,st0->y,st0->max_x,st0->max_y,st1->x,st1->y,st1->max_x,st1->max_y);
}

void collisionMonitor() {
  int i, j;
  
  // ship and ufo collide
  if (spObCollision(&ship, &ufo)) {
    explosionDisplay(ship.x,ship.y,2,1);
    ship.lives-=1;
    stats.status = GAME_RESET;
  }

  // ship and chest collide
  for (i = 0; i < lChest; i++) {
    if (spObCollision(&ship, &chests[i])) {
      bonusDisplay(ship.x,ship.y,2,1,ship.dOb);
      ship.score+=10;
      ship.lives+=1;
      chests[i].draw=0;
    } else if (spObCollision(&ufo, &chests[i])) {
      bonusDisplay(ufo.x,ufo.y,5,1,ufo.dOb);
      ufo.score+=10;
      chests[i].draw=0;
    }
  }

  // missle hits something
  for (i = 0; i < lMiss; i++) {
    if (missles[i].subtype == 1) {
      if (spObCollision(&missles[i],&ship)) {
	explosionDisplay(ship.x,ship.y,2,1);
	ship.lives-=1;
	stats.status = GAME_RESET;
      }
    } else if (missles[i].subtype == 0) {
      if (spObCollision(&missles[i],&ufo)) {
	explosionDisplay(ufo.x,ufo.y,5,1);
	ufo.draw = 0;
	missles[i].draw = 0;
	ship.score+=2;
      }
    }
    for (j = 0; j < lAst; j++) {
      if (spObCollision(&missles[i],&asts[j])) {
	breakDisplay(j);
	asts[j].draw = 0;
	if (missles[i].subtype == 1) {
	  ufo.score+=1;
	} else {
	  ship.score+=1;
	}
	missles[i].draw = 0;
	
      }
    }
  }
  
  // asteroid hits something
  for (i = 0; i <= lAst; i++) {
    if (spObCollision(&ship, &asts[i])) {
      explosionDisplay(ship.x,ship.y,2,1);
      ship.lives-=1;
      asts[i].draw = 0;
      stats.status = GAME_RESET;
    }
    if (spObCollision(&ufo, &asts[i])) {
      explosionDisplay(ufo.x,ufo.y,5,1);
      ufo.draw = 0;
    }
    for (j = 0; j <= lAst; j++) {
      if (j != i) {
	if (spObCollision(&asts[i], &asts[j])) {
	    asts[i].draw = 0;
	    asts[j].draw = 0;
	}
      }
    }
  }
}

void spObOnBattleField(spOb* spaceThing) {
  copywin(spaceThing->spWin, wBattleField, 0, 0, spaceThing->y, spaceThing->x, spaceThing->max_y, spaceThing->max_x, 0);
  wrefresh(wBattleField);
}

void spObFromBattleField(spOb* spaceThing) {
  copywin(wEmpty, wBattleField, spaceThing->y, spaceThing->x, spaceThing->y, spaceThing->x, spaceThing->max_y, spaceThing->max_x, 0);
}

void spObRefresh(spOb* spaceThing) {
  wclear(spaceThing->spWin);
  wattrset(spaceThing->spWin,COLOR_PAIR(spaceThing->color));
  waddstr(spaceThing->spWin, spaceThing->dOb);
}

int spObVoid(spOb* spaceThing) {
  if (spaceThing->x >= max_x || spaceThing->x <= 0 || spaceThing->y >= max_y || spaceThing->y <= 0) {
    return 1;
  }
  return 0;
}

void asteroidRemove(int nAst) {
  int j, i;
  spObFromBattleField(&asts[nAst]);
  for (i = nAst; i < lAst; i++) {
    asts[i] = asts[i+1];
  }
  lAst--;
}

void missleRemove(int nMiss) {
  int i;
  spObFromBattleField(&missles[nMiss]);
  for (i = nMiss; i < lMiss; i++) {
    missles[i] = missles[i+1];
    missles[i].iter=i;
  }
  lMiss--;
}

void chestRemove(int nChest) {
  int i;
  spObFromBattleField(&chests[nChest]);
  for (i = nChest; i < lChest; i++) {
    chests[i] = chests[i+1];
  }
  lChest--;
}

void spObMove(spOb* spaceThing) {
  
  spObFromBattleField(spaceThing);
  spObRefresh(spaceThing);
  spaceThing->mvcnt++;
  
  if ((spaceThing->mvcnt % spaceThing->speed) == 0) {  
    spaceThing->x = mod((spaceThing->x+spaceThing->dx), max_x);
    spaceThing->y = mod((spaceThing->y+spaceThing->dy), max_y);
    spaceThing->min_x = mod((spaceThing->min_x+spaceThing->dx), max_x);
    spaceThing->min_y = mod((spaceThing->min_y+spaceThing->dy), max_y);
    spaceThing->max_x = mod((spaceThing->max_x+spaceThing->dx), max_x);
    spaceThing->max_y = mod((spaceThing->max_y+spaceThing->dy), max_y);
  }

  if (spaceThing->type == MISSLE) {
    if (spObVoid(spaceThing)==1) {
      missleRemove(spaceThing->iter);
    }
  }
  
  spObOnBattleField(spaceThing);
}

/*
 * Initialize Spacebound Objects
 */

static void shipInit() {
  ship.type = SHIP;
  ship.iter = 0;
  ship.mvcnt = 0;
  ship.dx = -1;
  ship.dy = 0;
  ship.dS = 0;
  ship.x = max_x/2;
  ship.y = max_y/2;
  ship.max_x = ship.x+1;
  ship.min_x = ship.x;
  ship.max_y = ship.y;
  ship.min_y = ship.y;
  ship.drift = 0;
  ship.speed = 3;
  ship.color = CYAN;
  ship.score = 0;
  ship.lives = 3;
  ship.dOb = dShips[0];
  ship.spWin = newpad(1, 2);
  wattrset(ship.spWin,COLOR_PAIR(ship.color));
  wclear(ship.spWin);
}

static void ufoInit() {
  ufo.type = UFO;
  ufo.iter = 0;
  ufo.mvcnt = 0;
  ufo.speed = stats.ufoSpeed;
  if ((random() % 2) == 0) {
    ufo.color = RED;
  } else {
    ufo.color = GREEN;
  }
  //ufo.score = 0;
  ufo.lives = 3;
  ufo.dy = 0;
  if ((random() % 2) == 0) {
    ufo.x = max_x-4;
    ufo.dx = -1;
  } else {
    ufo.x = 1;
    ufo.dx = 1;
  }
  ufo.y = (random() % max_y-1)+3;
  ufo.max_x = ufo.x+5;
  ufo.min_x = ufo.x;
  ufo.max_y = ufo.y;
  ufo.min_y = ufo.y;
  ufo.draw = 1;
  ufo.dOb = dUfo[0];
  
  ufo.spWin = newpad(1, ufo.max_x-ufo.min_x);
  wclear(ufo.spWin);
  wattrset(ufo.spWin, COLOR_PAIR(MAGENTA));
}

static void asteroidInit(int nAst) {
  
  asts[nAst].type = ASTEROID;
  asts[nAst].iter = nAst;
  asts[nAst].mvcnt = 0;
  asts[nAst].speed = stats.astSpeed;
  asts[nAst].subtype = 5;
  asts[nAst].draw = 1;
  int tmp = 0;
  tmp = (random() % 5);

  if (tmp == 4) {
    asts[nAst].x = (random() % max_x-4)+1;
    asts[nAst].y = 2;
    asts[nAst].dy = 1;
    asts[nAst].dx = -1;
  } else if (tmp == 3) {
    asts[nAst].x = 2;
    asts[nAst].y = (random() % max_y-4)+1;
    asts[nAst].dx = 1;
    asts[nAst].dy = -1;
  } else if (tmp == 2) {
    asts[nAst].x = max_x-4;
    asts[nAst].y = (random() % max_y-4)+1;
    asts[nAst].dx = -1;
    asts[nAst].dy = 1;
  } else {
    asts[nAst].x = (random() % max_x-4)+1;
    asts[nAst].y = max_y-4;
    asts[nAst].dy = -1;
    asts[nAst].dx = 1;
  }

  asts[nAst].max_x = asts[nAst].x+9;
  asts[nAst].max_y = asts[nAst].y+4;
  //asts[nAst].dOb = dAst5[random() % 2][random() % 2];
  asts[nAst].dOb = dAst5[random() % 3][0];
  asts[nAst].color = YELLOW;

  asts[nAst].spWin = newpad(5, 10);
  wattrset(asts[nAst].spWin,COLOR_PAIR(asts[nAst].color)); 
  wclear(asts[nAst].spWin);
}

void asteroidSplit(int nAst) {
  
  asts[lAst].speed = asts[nAst].speed;
  asts[lAst].mvcnt = 0;
  asts[lAst].subtype = 2;
  asts[lAst].iter = lAst;
  asts[lAst].draw = 1;
  asts[lAst].x = asts[nAst].x+(random() % 6);
  asts[lAst].y = asts[nAst].y+(random() % 6);
  asts[lAst].max_x = asts[lAst].x+3;
  asts[lAst].max_y = asts[lAst].y+2;
  asts[lAst].color = YELLOW;
  asts[lAst].dx = asts[nAst].dx;
  asts[lAst].dy = asts[nAst].dy;
  asts[lAst].dOb = dAst2[random() % 2][0];
  
  asts[lAst].spWin = newpad(3, 4);
  wattrset(asts[lAst].spWin,COLOR_PAIR(asts[lAst].color));
  wclear(asts[lAst].spWin);
  spObRefresh(&asts[lAst]);

  spObOnBattleField(&asts[lAst]);
  lAst++;

  asteroidRemove(nAst);
}

void chestInit(int nChest) {
  chests[nChest].dOb = "$";
  chests[nChest].mvcnt = 0;
  chests[nChest].iter = nChest;
  chests[nChest].type = CHEST;
  chests[nChest].color = BLUE;
  chests[nChest].speed = 3;
  chests[nChest].draw=1;

  int tmp = 0;
  tmp = (random() % 5);
  
  if (tmp == 4) {
    chests[nChest].x = (random() % max_x-2)+1;
    chests[nChest].y = 1;
    chests[nChest].dx = -1;
    chests[nChest].dy = 1;
  } else if (tmp == 3) {
    chests[nChest].x = 1;
    chests[nChest].y = (random() % max_y-2)+1;
    chests[nChest].dx = 1;
    chests[nChest].dy = -1;
  } else if (tmp == 2) {
    chests[nChest].x = max_x-1;
    chests[nChest].y = (random() % max_y-2)+1;
    chests[nChest].dx = -1;
    chests[nChest].dy = 1;
  } else {
    chests[nChest].x = (random() % max_x-2)+1;
    chests[nChest].y = max_y - 1;
    chests[nChest].dx = 1;
    chests[nChest].dy = -1;
  }

  chests[nChest].max_x = chests[nChest].x;
  chests[nChest].max_y = chests[nChest].y;

  chests[nChest].spWin = newpad(1, 1);
  wattrset(chests[nChest].spWin,chests[nChest].color);
  wclear(chests[nChest].spWin);
}

static void ufoMissleInit(int nMiss) {

  int dfx, dfy, dist, dist1, i;
  int astNear = 0;

  missles[nMiss].type = MISSLE;
  missles[nMiss].dOb = "+";
  missles[nMiss].iter = nMiss;
  missles[nMiss].mvcnt = 0;
  missles[nMiss].x = ufo.x+2;
  missles[nMiss].y = ufo.y;
  missles[nMiss].max_x = missles[nMiss].x;
  missles[nMiss].max_y = missles[nMiss].y;
  missles[nMiss].subtype = 1;
  missles[nMiss].draw = 1;
  missles[nMiss].speed = 1;

  // UFO Aims for Asteroid
  dfx = fabs(ufo.x - asts[0].x);
  dfy = fabs(ufo.y - asts[0].y);
  dist = sqrt((dfx*dfx)+(dfy*dfy));
  for (i = 1; i < lAst; i++) {
    asts[i].color = YELLOW;
    //asts[astNear].color = WHITE;
    dfx = fabs(ufo.x - asts[i].x);
    dfy = fabs(ufo.y - asts[i].y);
    dist1 = sqrt((dfx*dfx)+(dfy*dfy));
    if (dist1 < dist) {
      dist = dist1;
      astNear = i;
    }
  }
  asts[astNear].color = RED;
  spObRefresh(&asts[astNear]);
  spObOnBattleField(&asts[astNear]);

  if ((ufo.x == asts[astNear].x) || (ufo.x >= asts[astNear].x && ufo.x <= asts[astNear].max_x)) {
    missles[nMiss].dx = 0;
    ufo.dS = 0;
  } else if (ufo.x > asts[astNear].x) {
    missles[nMiss].dx = -1;
    ufo.dS = 1;
  } else {
    missles[nMiss].dx = 1;
    ufo.dS = 2;
  }

  if ((ufo.y == asts[astNear].y) || (ufo.y >= asts[astNear].y && ufo.y <= asts[astNear].max_y)) {
    missles[nMiss].dy = 0;
    
  } else if (ufo.y > asts[astNear].y) {
    missles[nMiss].dy = -1;
  } else {
    missles[nMiss].dy = 1;
  }
  
  missles[nMiss].spWin = newpad(1, 1);
  wattrset(missles[nMiss].spWin,missles[nMiss].color);
  wclear(missles[nMiss].spWin);
}

static void missleInit(int nMiss) {
  missles[nMiss].dOb = "+";
  missles[nMiss].type = MISSLE;
  missles[nMiss].iter = nMiss;
  missles[nMiss].mvcnt = 0;
  missles[nMiss].subtype = 0;
  missles[nMiss].draw = 1;
  missles[nMiss].speed = 1;
  missles[nMiss].x = ship.x;
  missles[nMiss].y = ship.y;
  missles[nMiss].max_x = missles[nMiss].x;
  missles[nMiss].max_y = missles[nMiss].y;
  missles[nMiss].dx = dxShips[ship.dS];
  missles[nMiss].dy = dyShips[ship.dS];
  missles[nMiss].color = GREEN;
  missles[nMiss].spWin = newpad(1, 1);
  wattrset(missles[nMiss].spWin,missles[nMiss].color);
  wclear(missles[nMiss].spWin);
}

/* BATTLEFIELD */

static void starFieldInit() {
  int i,j;

  wEmpty = newpad(max_y, max_x);
  wclear(wEmpty);

  wStarField = newpad(max_y, max_x);
  wattrset(wStarField, COLOR_PAIR(YELLOW));
  waddstr(wStarField, "*");
  
  for (i = 0; i<max_x; i++) {
    j = random() % max_y;
    copywin(wStarField, wEmpty,0,0,j,i,j,i,0);
  }
  box(wEmpty,0,0);
}

static void battleFieldInit() {
  wBattleField = newwin(max_y, max_x, 0, 0);
  wclear(wBattleField);
  clearFromBattleField(0,0,max_x-1,max_y-1);
}

void battleFieldClear() {
  wclear(wBattleField);
  clearFromBattleField(0,0,max_x-1,max_y-1);
}

/* title screen */

static void titleScreenInit() {  
  wTitleScreen = newpad(max_y, max_x);
  wclear(wTitleScreen);
}

void titleScreenDisplay() {

  int x, y;
  WINDOW *wTitleText;
  WINDOW *wStartText;

  /* big title */
  wTitleText = newpad(3, 45);
  wclear(wTitleText);
  wattrset(wTitleText, COLOR_PAIR(YELLOW));
  waddstr(wTitleText, "  //_\\/ __|_   _| __| _ \\\\ \\ / / _ \\_ _|   \\ ");
  waddstr(wTitleText, " // _ \\__ \\ | | | _||   / \\ V / (_) | || |) |");
  waddstr(wTitleText, "//_/ \\____/ |_| |___|_|_\\  \\_/ \\___/___|___/ "); 

  x = (max_x / 2) - (45 / 2);
  y = 0;
  displayOnBattleField(wTitleText,x,y,x+44,y+2);  

  /* info text */
  wStartText = newpad(1, 20);
  wclear(wStartText);
  wattrset(wStartText, COLOR_PAIR(RED));
  waddstr(wStartText, "Press SPACE to start");

  x = (max_x / 2) - (20 / 2);
  y = max_y - 2;
  displayOnBattleField(wStartText,x,y,x+19,y);
}

void titleScreenClear() {
  battleFieldClear();
  battleFieldInit();
}

/* gameover  */

static void gameOverInit() {
  wGameOver = newpad(13, 31);
  wclear(wGameOver);
  wattrset(wGameOver, COLOR_PAIR(GREEN));
  waddstr(wGameOver, "                               ");
  waddstr(wGameOver, "  #####   ####  ##   ## ###### ");
  waddstr(wGameOver, " ##      ##  ## ####### ##     ");
  waddstr(wGameOver, " ## ###  ###### ## # ## #####  ");
  waddstr(wGameOver, " ##  ##  ##  ## ##   ## ##     ");
  waddstr(wGameOver, "  #####  ##  ## ##   ## ###### ");
  waddstr(wGameOver, "                               ");
  waddstr(wGameOver, "  ####  ##   ## ###### ######  ");
  waddstr(wGameOver, " ##  ## ##   ## ##     ##   ## ");
  waddstr(wGameOver, " ##  ##  ## ##  #####  ######  ");
  waddstr(wGameOver, " ##  ##  ## ##  ##     ##  ##  ");
  waddstr(wGameOver, "  ####    ###   ###### ##   ## ");
  waddstr(wGameOver, "                               ");
}

void gameOverDisplay() {
  WINDOW *wStartText;

  int x = (max_x / 2) - (31 / 2);
  int y = (max_y / 2) - (13 / 2);
  displayOnBattleField(wGameOver,x,y,x+30,y+12);

  /* info text */
  wStartText = newpad(1, 22);
  wclear(wStartText);
  wattrset(wStartText, COLOR_PAIR(RED));
  waddstr(wStartText, "Press SPACE to restart");
  x = (max_x / 2) - (22 / 2);
  y = max_y - 2;

  displayOnBattleField(wStartText,x,y,x+21,y);
}

void gameOverClear()  {
  int x = (max_x / 2) - (31 / 2);
  int y = (max_y / 2) - (13 / 2);
  clearFromBattleField(x,y,x+30,y+12);
}

/* paused */

static void gamePausedInit() {
  wGamePaused = newpad(10, 41);
  wclear(wGamePaused);
  wattrset(wGamePaused, COLOR_PAIR(GREEN));
  waddstr(wGamePaused, "###### ###### ##  ##  ##### ###### ##### ");
  waddstr(wGamePaused, "###### ###### ##  ## ###### ###### ######");
  waddstr(wGamePaused, "##  ## ##  ## ##  ## ##     ##     ##  ##");
  waddstr(wGamePaused, "##  ## ##  ## ##  ## ##     ##     ##  ##");
  waddstr(wGamePaused, "###### ###### ##  ## ###### ###### ##  ##");
  waddstr(wGamePaused, "###### ###### ##  ## ###### ###### ##  ##");
  waddstr(wGamePaused, "##     ##  ## ##  ##     ## ##     ##  ##");
  waddstr(wGamePaused, "##     ##  ## ##  ##     ## ##     ##  ##");
  waddstr(wGamePaused, "##     ##  ## ###### ###### ###### ######");
  waddstr(wGamePaused, "##     ##  ## ###### #####  ###### ##### ");
}

void gamePausedDisplay() {
  int x = (max_x / 2) - (41 / 2);
  int y = (max_y / 2) - (10 / 2);
  displayOnBattleField(wGamePaused,x,y,x+40,y+9);
}

void gamePausedClear()  {
  int x = (max_x / 2) - (41 / 2);
  int y = (max_y / 2) - (10 / 2);
  clearFromBattleField(x,y,x+40,y+9);
}

/* Status Bar  */

void statusInit() {
  wStatus = newpad(1, 70);
  wclear(wStatus);
}

void statusDisplay() {
  char strStatus[70];
  
  sprintf (strStatus, "Score: %2.7d/%2.7d Asteroids: %2.7d Rank: %s Ships: %d", ship.score, ufo.score, lAst, stats.rank, ship.lives);
  
  wclear(wStatus);
  wattrset(wStatus, COLOR_PAIR(RED));
  waddstr(wStatus, strStatus);

  displayOnBattleField(wStatus,2,1,70,1);
}

void statusClear(){
  clearFromBattleField(2,0,70,0);
}

static void finish(int sig) {
  endwin();

  fprintf(stderr,"Thank you for playing Astervoid, come back soon\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"=========================================================================\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Final score: %7.7ld\nFinal rank: %s \n",ship.score,stats.rank);
  exit(sig);
}

/* initializations */

void gameLevel() {
  if (ship.score > 0 && ship.score > stats.level*100) {
    // level up
    if (stats.astSpeed > 1) {
      stats.astSpeed-=1;
    }
    if (stats.ufoSpeed > 1) {
      stats.ufoSpeed-=1;
    }
    ship.score+=1;
    stats.level+=1;
    stats.astLevel+=1;
    strcpy(stats.rank,ranks[mod(stats.level, 6)]);
    if (lChest<MAX_CHESTS) {
      chestInit(lChest);
      lChest++;
    }
  }
}

void resetStats() {
  stats.astSpeed = 6;
  stats.ufoSpeed = 7;
  stats.level=1;
  stats.astLevel=6;
  strcpy(stats.rank,ranks[0]);
}

void initAll() {
  statusInit();
  gameOverInit();
  gamePausedInit();
  titleScreenInit();
  starFieldInit();
  battleFieldInit();
  shipInit();
  asteroidInit(0);
  lAst=1;
  lMiss=0;
  ufoInit();
  ufo.score = 0;
  resetStats();
}

void gamePlay() {
  initscr();
  clear();
  keypad(stdscr, TRUE);
  nonl();	
  cbreak();	
  noecho();	

  start_color();
  init_color(COLOR_BLUE,240,248,255);
  init_pair(RED, COLOR_RED, COLOR_BLACK);
  init_pair(GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);

  getmaxyx(stdscr, scrmax_y, scrmax_x);
  max_x = scrmax_x;// + 100;
  max_y = scrmax_y;// + 100;

  initAll();
}

void gameReset() {
  battleFieldClear();
  //initAll();
  ship.x=max_x/2;
  ship.y=max_y/2;
  ship.max_x=ship.x+1;
  ship.max_y=ship.y;
  ship.drift=0;
  spObRefresh(&ship);
  spObOnBattleField(&ship);
}

void gameReplay() {
  initAll();
  spObOnBattleField(&ship);
}

void readInput() {

  int ch;
  int m=0;
  ch = getch();

  switch (stats.status) {

  case GAME_PAUSED:

    if (ch == 'p') {
      gamePausedClear();
      stats.status = GAME_PLAY;
    }
    break;

  case GAME_OVER:
    if (ch == ' ') {
      gameOverClear();
      stats.status = GAME_PLAY;
      //battleFieldClear();
      gameReplay();
    }
    if (ch == 't') {
      gameOverClear();
      stats.status = GAME_TITLE;
      gameReplay();
    }
    if (ch == 'q') {
      finish(0);
    }
    break;

  case GAME_TITLE:
    if (ch == ' ') {
      titleScreenClear();
      stats.status = GAME_PLAY;
      gameReplay();
      battleFieldClear();
    }
    if (ch == 'q') {
      finish(0);
    }
    break;
    
  case GAME_PLAY:
    if (ch == 'q') {
      battleFieldClear();
      stats.status = GAME_TITLE;
    } else if (ch == 'p') { 
      stats.status = GAME_PAUSED;
    } else if (ch == 'd' || ch == KEY_RIGHT) {
      if (ship.dS == 7) {
	ship.dS = 0;
      } else {
	ship.dS+=1;
      }
      ship.dOb = dShips[ship.dS];
    } else if (ch == 'a' || ch == KEY_LEFT) {
      if (ship.dS == 0) {
	ship.dS = 7;
      } else {
	ship.dS-=1;
      }
      ship.dOb = dShips[ship.dS];
    } else if (ch == 'w' || ch == KEY_UP) {
      ship.dx = dxShips[ship.dS];
      ship.dy = dyShips[ship.dS];
      spObRefresh(&ship);
      spObMove(&ship);
      ship.drift = 1;
    } else if (ch == 's' || ch == KEY_DOWN) {
      ship.drift = 0;
    } else if (ch == ' ') {
      if (lMiss < MAX_MISSLES) {
	missleInit(lMiss);
	lMiss+=1;
      }
    } else if (ch == 'c') {
      chestInit(lChest);
      lChest+=1;
    } else if (lAst < MAX_ASTEROIDS) {
      asteroidInit(lAst);
      lAst+=1;
    }   
    wrefresh(wBattleField);
  }
}

/* game handler */

void handleTimer() {
  int i;
  
  switch (stats.status) {
    
  case GAME_PAUSED:
    if (stats.status == GAME_PAUSED) {
      gamePausedDisplay();
    }
    break;

  case GAME_TITLE:
    if (stats.status == GAME_TITLE) {
      titleScreenDisplay();
    }
    break;

  case GAME_RESET:
    gameReset();
    stats.status=GAME_PLAY;
    
  case GAME_PLAY:

    collisionMonitor();
    gameLevel();

    if (ship.lives == 0) {
      stats.status = GAME_OVER;
    }

    // chests
    if (lChest<MAX_CHESTS && (random() % 1000) == 0) {
      chestInit(lChest);
      lChest++;
    }

    if (lChest > 0) {
      for (i = 0; i< lChest; i++) {
	if (chests[i].draw) {
	  chests[i].color=mod(chests[i].mvcnt, 6);
	  spObRefresh(&chests[i]);
	  spObMove(&chests[i]);
	} else {
	  chestRemove(i);
	}
      }
    }
    
    // asteroids
    if (lAst < stats.astLevel && lAst < MAX_ASTEROIDS) {
      asteroidInit(lAst);
      lAst++;
    }
    for (i = 0; i < lAst; i++) {
      if (asts[i].draw) {
	spObMove(&asts[i]);
      } else {
	if (asts[i].subtype == 2) {
	  asteroidRemove(i);
	} else {
	  asteroidSplit(i);
	}
      }
    }
    
    // missles
    for (i = 0; i < lMiss; i++) {
      if (missles[i].draw) {
	spObRefresh(&missles[i]);
	spObMove(&missles[i]);
      } else {
	missleRemove(i);
      }
    }
    
    // ship
    if (ship.drift == 1) {
      spObMove(&ship);
    }
    spObRefresh(&ship);
    spObOnBattleField(&ship);

    // ufo
    if (ufo.draw) {
      spObMove(&ufo);
      if (lMiss < MAX_MISSLES && (random() % ufo.speed) == 0) {
	ufoMissleInit(lMiss);
	lMiss+=1;
      }
      //if ((random() % ufo.speed) == 0) {
      ufo.dOb = dUfo[ufo.dS];
      //}
    } else {
      spObFromBattleField(&ufo);
      ufoInit();
    }
    // status
    statusDisplay();

  case GAME_OVER:
    if (stats.status == GAME_OVER) {
      gameOverDisplay();
    }
    break;
  }
}

void setUpTimer() {

  struct itimerval myTimer;
  struct sigaction myAction;
  myTimer.it_value.tv_sec = 0;
  myTimer.it_value.tv_usec = 1000000 / FPS;
  myTimer.it_interval.tv_sec = 0;
  myTimer.it_interval.tv_usec = 1000000 / FPS;
  setitimer(ITIMER_REAL, &myTimer, NULL);
  
  myAction.sa_handler = &handleTimer;
  myAction.sa_flags = SA_RESTART;
  sigaction(SIGALRM, &myAction, NULL);
}

int main(int argc, char *argv[]) {
  
  srand((unsigned) time(&t));
  stats.status = GAME_TITLE;
  gamePlay();
  setUpTimer();

  while(1) {
    usleep(DELAY*1);
    readInput();
    }
  endwin();
}
