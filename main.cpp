#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "include/freeglut.h"
#include <iostream>
#include <fstream>
#include <Windows.h>
#include <mmsystem.h>
#include <mciapi.h>
#include <vector>
#include <thread>
#include <chrono>
#include "Textures.h"

constexpr auto PI = 3.14159;
constexpr auto FPS = 60.0;
constexpr auto g = 9.8;
using namespace std;

Textures textures;

ma_engine engine;
ma_sound coin_sound, jump_sound, break_sound, kick_sound, ground_theme, over_sound, stage_clear;

class Mouse {
public:
	int x, y;
	bool left, right, middle;
	Mouse() :x{ 0 }, y{ 0 }, left{ false }, right{ false }, middle{ false } {}
};
bool keyStates[256]{};
bool specialKeyStates[256]{};
Mouse mouse;
int ScreenWidth = 1280, ScreenHeight = 800;
float globalDx = 0;

class Object {
public:
	float x0, y0, xc = 0, yc = 0, X, Y, Xinc, Yinc; //for Hitbox
	Object() {
		this->x0 = 0; this->y0 = 0; this->xc = 0; this->yc = 0;
		this->X = 0; this->Y = 0; this->Xinc = 0; this->Yinc = 0;
	}
	Object(float x0, float y0, float X, float Y, float Xinc, float Yinc) {
		this->x0 = x0; this->y0 = y0; this->yc = 0; this->xc = 0;
		this->X = X; this->Y = Y; this->Xinc = Xinc; this->Yinc = Yinc;
	}
	int get_x0() { return this->x0; }
	int get_y0() { return this->y0; }
	int get_xc() { return this->xc; }
	int get_yc() { return this->yc; }
	int get_X() { return this->X; }
	int get_Y() { return this->Y; }
	int get_Xinc() { return this->Xinc; }
	int get_Yinc() { return this->Yinc; }

	void set_x0(int x0) { this->x0 = x0; }
	void set_y0(int y0) { this->y0 = y0; }
	void set_xc(int xc) { this->xc = xc; }
	void set_yc(int yc) { this->yc = yc; }
	void set_X(int X) { this->X = X; }
	void set_Y(int Y) { this->Y = Y; }
	void set_Xinc(int Xinc) { this->Xinc = Xinc; }
	void set_Yinc(int Yinc) { this->Yinc = Yinc; }
};
class Static_Object : public Object {
public:
	GLuint tex_id;
	bool isDestroyable;
	Static_Object(float x0, float y0, float X, float Y, GLuint tex_Id, bool is_Destroyable) :
		Object(x0, y0, X, Y, 0, 0), tex_id{ tex_Id }, isDestroyable{ is_Destroyable } {};
};
class Lucky_block :public Static_Object {
public:
	short buff_id;  //1 - mushroom/flower, 2 - coin
	bool buffTaken = false;
	Lucky_block(float x0, float y0, float X, float Y, GLuint tex_Id,  bool is_Destroyable, short buff_Id) :
		Static_Object(x0, y0, X, Y, tex_Id, is_Destroyable), buff_id{ buff_Id } {};
};
class Buff :public Object {
public:
	GLuint tex_id;
	unsigned short buff_id;   // 1 - mushroom, 2 - flower, 3 - star
	bool hasPotentialEnergy = false;
	bool hasBottomCollision = false;
	float V0 = 0; //starting speed for jump
	float t = 0; //time elapsed while moving in Y
	float YC; //current starting Y level
	float yc = 0; //difference between starting and current Y
	Buff(float x0, float y0, float X, float Y, float Xinc, float Yinc, GLuint tex_Id, unsigned short buff_Id) :
		Object(x0, y0, X, Y, Xinc, Yinc), tex_id{ tex_Id }, buff_id{ buff_Id } {
		this->YC = y0;
	};
};
class Enemy :public Object {
public:
	GLuint tex_id;
	short turtle_level = 1;
	unsigned short enemy_id;  // 1 - mushroom, 2 - turtle, ...
	bool hasPotentialEnergy = false;
	bool hasBottomCollision = false;
	float V0 = 0; //starting speed for jump
	float t = 0; //time elapsed while moving in Y
	float YC; //current starting Y level
	float yc = 0; //difference between starting and current Y
	Enemy(float x0, float y0, float X, float Y, float Xinc, float Yinc, GLuint tex_Id, unsigned short enemy_Id) :
		Object(x0, y0, X, Y, Xinc, Yinc), tex_id{ tex_Id }, enemy_id{ enemy_Id } {
		this->YC = y0;
	};
	void move() {
		xc += Xinc;
		/*if (enemy_id == 2) {
			if (xc + Xinc - globalDx < -2000.0) Xinc = -Xinc;
			if (xc + Xinc - globalDx > 50.0) Xinc = -Xinc;
		}*/
	}
};
class Turtle : public Enemy {
public:

};
class Player :public Object {
public:
	GLuint tex_id;
	bool hasPotentialEnergy = false;
	bool hasBottomCollision = false;
	unsigned short playerLevel = 1;
	float V0 = 0; //starting speed for jump
	float t = 0; //time elapsed while moving in Y
	float Ystart = 0; //current starting Y level
	float dy = 0; //difference between starting and current Y
	Player(float x0, float y0, float X, float Y, float Xinc, float Yinc, GLuint tex_Id) :
		Object(x0, y0, X, Y, Xinc, Yinc), tex_id{ tex_Id } {};
};

enum CollisionType { BOTTOM, TOP, LEFT, RIGHT, NONE };
template<typename T, typename T2>
CollisionType checkCollisionType(T& rect1, T2& rect2) {  //basic collision func where rect1 is main
	float left1 = rect1.x0 + rect1.xc;
	float left2 = rect2.x0 + rect2.xc;
	float right1 = rect1.x0 + rect1.xc + rect1.X;
	float right2 = rect2.x0 + rect2.xc + rect2.X;
	if (right1 >= left2 && left1 <= right2) {
		float top1 = rect1.y0 + rect1.yc + rect1.Y;
		float top2 = rect2.y0 + rect2.yc + rect2.Y;
		float bottom1 = rect1.y0 + rect1.yc;
		float bottom2 = rect2.y0 + rect2.yc;
		if (top1 >= bottom2 && bottom1 <= top2) {
			float dx, dy;
			if (left1 > left2 && right1 < right2) dx = rect1.X;
			else if (left2 > left1 && right2 < right1) dx = rect2.X;
			else dx = right1 - left2 < right2 - left1 ? right1 - left2 : right2 - left1;
			if (top1 > top2 && bottom1 < bottom2) dy = rect2.Y;
			else if (top2 > top1 && bottom2 < bottom1) dy = rect1.Y;
			else dy = top2 - bottom1 < top1 - bottom2 ? top2 - bottom1 : top1 - bottom2;
			if (dx >= dy) {
				if (bottom1 < bottom2)
					return TOP;
				else
					return BOTTOM;
			}
			else {
				if (right1 < right2)
					return LEFT;
				else
					return RIGHT;
			}
		}
	}
	return NONE;
}

template<typename T> void draw_Rectangle_Hit_box(T& rect, float r, float g, float b) {
	glColor3f(r, g, b);
	glBegin(GL_LINES);
	glVertex2i(rect.x0 + rect.xc, rect.y0 + rect.yc);
	glVertex2i(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);

	glVertex2i(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);
	glVertex2i(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);

	glVertex2i(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);
	glVertex2i(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);

	glVertex2i(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);
	glVertex2i(rect.x0 + rect.xc, rect.y0 + rect.yc);
	glEnd();
}
template<typename T> void drawRectangle(T& rect) {
	glBegin(GL_QUADS);
	glVertex2i(rect.x0 + rect.xc, rect.y0 + rect.yc);
	glVertex2i(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);
	glVertex2i(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);
	glVertex2i(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);
	glEnd();
}
template<typename T> void drawRectangleTexture(T& rect, float coords[], bool isNotMirroredOnX) {
	glBegin(GL_QUADS);
	if (!isNotMirroredOnX) {
		glTexCoord2f(coords[6], coords[7]); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc);
		glTexCoord2f(coords[4], coords[5]); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(coords[2], coords[3]); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(coords[0], coords[1]); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);
	}
	else {
		glTexCoord2f(coords[0], coords[1]); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc);
		glTexCoord2f(coords[2], coords[3]); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(coords[4], coords[5]); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(coords[6], coords[7]); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);
	}
	glEnd();
}
template<typename T> void drawRectangleTexture2(T& rect) {
	glBegin(GL_QUADS);
	float ratio;
	if (rect.X > rect.Y) {
		ratio = rect.X / rect.Y;
		glTexCoord2f(0.0, 0.0); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc);
		glTexCoord2f(0.0, 1.0); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(ratio, 1.0); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(ratio, 0.0); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);
	}
	else {
		ratio = rect.Y / rect.X;
		glTexCoord2f(0.0, 0.0); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc);
		glTexCoord2f(0.0, ratio); glVertex2f(rect.x0 + rect.xc, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(1.0, ratio); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc + rect.Y);
		glTexCoord2f(1.0, 0.0); glVertex2f(rect.x0 + rect.xc + rect.X, rect.y0 + rect.yc);
	}
	
	glEnd();
}


Player playerHitbox(20, 200, 40, 40, 0, 0, 0);

vector<Static_Object> Rectangles;
vector<Lucky_block> lucky_blocks;
vector<Enemy> enemy_Hit_boxes;
vector<Buff> buff_Hit_boxes;
vector<Static_Object> coins;

Object flag_bottom(8310, 80, 40, 40, 0, 0);
Object flag_stick(8325, 120, 10, 340, 0, 0);
Object flag(8335, 420, 60, 40, 0, 0);

//texture coordinates
float lvl1_staying[] = { 0.0,0.5, 0.0,0.625, 0.125,0.625, 0.125,0.5 };
float lvl1_jump[] = { 0.125,0.5, 0.125,0.625, 0.25,0.625, 0.25,0.5 };
float lvl1_moving1[] = { 0.25,0.5, 0.25,0.625, 0.375,0.625, 0.375,0.5 };
float lvl1_moving2[] = { 0.375,0.5, 0.375,0.625, 0.5,0.625, 0.5,0.5 };
float lvl1_moving3[] = { 0.5,0.5, 0.5,0.625, 0.625,0.625, 0.625,0.5 };
float lvl1_over[] = { 0.625,0.5, 0.625,0.625, 0.75,0.625, 0.75,0.5 };

float lvl2_staying[] = { 0.0,0.0, 0.0,0.25, 0.125,0.25, 0.125,0.0 };
float lvl2_jump[] = { 0.125,0.0, 0.125,0.25, 0.25,0.25, 0.25,0.0 };
float lvl2_moving1[] = {0.25,0.0, 0.25,0.25, 0.375,0.25, 0.375,0.0};
float lvl2_moving2[] = { 0.375,0.0, 0.375,0.25, 0.5,0.25, 0.5,0.0 };
float lvl2_moving3[] = { 0.5,0.0, 0.5,0.25, 0.625,0.25, 0.625,0.0 };

float lvl3_staying[] = { 0.0,0.25, 0.0,0.5, 0.125,0.5, 0.125,0.25 };
float lvl3_jump[] = { 0.125,0.25, 0.125,0.5, 0.25,0.5, 0.25,0.25 };
float lvl3_moving1[] = { 0.25,0.25, 0.25,0.5, 0.375,0.5, 0.375,0.25 };
float lvl3_moving2[] = { 0.375,0.25, 0.375,0.5, 0.5,0.5, 0.5,0.25 };
float lvl3_moving3[] = { 0.5,0.25, 0.5,0.5, 0.625,0.5, 0.625,0.25 };

float tex_coords[] = { 0.0,0.0, 0.0,1.0, 1.0,1.0, 1.0,0.0 };
float turtle_coords1[] = { 0.5,0.0, 0.5,0.6875, 0.0,0.6875, 0.0,0.0 };
float turtle_coords2[] = { 0.5,0.0, 0.5,0.4375, 1.0,0.4375, 1.0,0.0 };
float pipe_coords[] = { 0.0,0.0, 0.0,0.9375, 0.375,0.9375, 0.375,0.0 };
float lucky_coords1[] = { 0.0,0.0, 0.0,0.5, 0.5,0.5, 0.5,0.0 };
float lucky_coords2[] = { 0.0,0.5, 0.0,1.0, 0.5,1.0, 0.5, 0.5 };
float lucky_coords3[] = { 0.5,0.0, 0.5,0.5, 1.0,0.5, 1.0,0.0 };
//
bool movingRight = true;
bool isInvulnerable = false;
bool isGameOver = false;
bool isWin = false;
bool use_first_texture = true; //for changing block's textures
bool allowSpawn;  //to spawn enemies at certain places
short distanceSpawn; //distance to spawn enemies
Object game_over(0, 0, ScreenWidth, ScreenHeight, 0, 0);
bool showGameOver = false;
chrono::steady_clock::time_point begin_point, end_point;
chrono::steady_clock::time_point animation_begin, animation_end; //for lucky block's texture changes
chrono::steady_clock::time_point moving_begin, moving_end; //for mario moving textures
short animation_counter = 0;
short animationInc = 1;
bool isMoving = false;
float* move_ptr, *move_ptr2, *move_ptr3;

void setup_mario_game() {
	Static_Object pipe(800, 20, 90, 200, 8, 0);
	Static_Object pipe2(1400, 80, 90, 200, 8, 0);
	Static_Object pipe3(2100, 80, 90, 200, 8, 0);
	Static_Object pipe4(6800, 0, 90, 200, 8, 0);
	Static_Object pipe5(7400, 0, 90, 200, 8, 0);

	Static_Object floor1(0, 0, 2500, 80, 0, false);
	Static_Object floor2(2700, 0, 800, 80, 0, false);
	Static_Object floor3(3700, 0, 2500, 80, 0, false);
	Static_Object floor4(6400, 0, 2400, 80, 0, false);

	Static_Object bricks1(200, 240, 40, 40, 0, true);
	Static_Object bricks2(280, 240, 40, 40, 0, true);
	Static_Object bricks3(360, 240, 40, 40, 0, true);
	Static_Object bricks4(3040, 240, 40, 40, 0, true);
	Static_Object bricks5(3120, 240, 40, 40, 0, true);

	Static_Object bricks6(5440, 80, 160, 40, 0, false);
	Static_Object bricks7(5480, 120, 120, 40, 0, false);
	Static_Object bricks8(5520, 160, 80, 40, 0, false);
	Static_Object bricks9(5560, 200, 40, 40, 0, false);

	Static_Object bricks10(5680, 80, 160, 40, 0, false);
	Static_Object bricks11(5680, 120, 120, 40, 0, false);
	Static_Object bricks12(5680, 160, 80, 40, 0, false);
	Static_Object bricks13(5680, 200, 40, 40, 0, false);

	Static_Object bricks14(6000, 80, 200, 40, 0, false);
	Static_Object bricks15(6040, 120, 160, 40, 0, false);
	Static_Object bricks16(6080, 160, 120, 40, 0, false);
	Static_Object bricks17(6120, 200, 80, 40, 0, false);

	Static_Object bricks18(6400, 80, 160, 40, 0, false);
	Static_Object bricks19(6400, 120, 120, 40, 0, false);
	Static_Object bricks20(6400, 160, 80, 40, 0, false);
	Static_Object bricks21(6400, 200, 40, 40, 0, false);

	Static_Object bricks22(3160, 440, 40, 40, 0, true);
	Static_Object bricks23(3200, 440, 360, 40, 0, false);
	Static_Object bricks24(3680, 440, 80, 40, 0, false);
	Static_Object bricks25(3760, 440, 40, 40, 0, true);
	Static_Object bricks26(3800, 240, 40, 40, 0, true);
	Static_Object bricks27(5090, 440, 40, 40, 0, true);
	Static_Object bricks28(5210, 440, 40, 40, 0, true);
	Static_Object bricks29(5130, 240, 40, 40, 0, true);
	Static_Object bricks30(5170, 240, 40, 40, 0, true);
	Static_Object bricks31(7000, 240, 40, 40, 0, true);
	Static_Object bricks32(7040, 240, 40, 40, 0, true);
	Static_Object bricks33(7120, 240, 40, 40, 0, true);

	Static_Object bricks34(7600, 80, 360, 40, 0, false);
	Static_Object bricks35(7640, 120, 320, 40, 0, false);
	Static_Object bricks36(7680, 160, 280, 40, 0, false);
	Static_Object bricks37(7720, 200, 240, 40, 0, false);
	Static_Object bricks38(7760, 240, 200, 40, 0, false);
	Static_Object bricks39(7800, 280, 160, 40, 0, false);
	Static_Object bricks40(7840, 320, 120, 40, 0, false);
	Static_Object bricks41(7880, 360, 80, 40, 0, false);

	Lucky_block lucky_block1(240, 240, 40, 40, 1, false, 2);
	Lucky_block lucky_block2(320, 240, 40, 40, 1, false, 1);
	Lucky_block lucky_block3(280, 440, 40, 40, 1, false, 2);
	Lucky_block lucky_block4(3080, 240, 40, 40, 1, false, 1);
	Lucky_block lucky_block5(4450, 240, 40, 40, 1, false, 2);
	Lucky_block lucky_block6(4610, 240, 40, 40, 1, false, 2);
	Lucky_block lucky_block7(4770, 240, 40, 40, 1, false, 2);
	Lucky_block lucky_block8(4610, 440, 40, 40, 1, false, 1);
	Lucky_block lucky_block9(3800, 440, 40, 40, 1, false, 2);
	Lucky_block lucky_block10(5130, 440, 40, 40, 1, false, 2);
	Lucky_block lucky_block11(5170, 440, 40, 40, 1, false, 2);
	Lucky_block lucky_block12(7080, 240, 40, 40, 1, false, 2);

	Enemy enemy1(650, 80, 40, 40, -0.25, 0, 3, 1);
	enemy_Hit_boxes.push_back(enemy1);
	Enemy enemy12(1700, 80, 40, 40, -0.25, 0, 3, 1);
	enemy_Hit_boxes.push_back(enemy12);
	Enemy enemy13(1760, 80, 40, 40, -0.25, 0, 3, 1);
	enemy_Hit_boxes.push_back(enemy13);

	playerHitbox = Player(20, 200, 38, 40, 0, 0, 0);
	Rectangles = { pipe, pipe2, pipe3, pipe4, pipe5, floor1, floor2, floor3, floor4, bricks1, bricks2, bricks3, bricks4,
		bricks5,bricks6, bricks7, bricks8, bricks9, bricks10, bricks11, bricks12, bricks13, bricks14, bricks15, bricks16,
		bricks17,bricks18, bricks19, bricks20, bricks21, bricks22, bricks23, bricks24, bricks25, bricks26, bricks27,
		bricks28, bricks29, bricks30, bricks31, bricks32, bricks33, bricks34, bricks35, bricks36, bricks37, bricks38,
		bricks39, bricks40, bricks41
	};
	lucky_blocks = { lucky_block1, lucky_block2, lucky_block3, lucky_block4, lucky_block5, lucky_block6,
	lucky_block7, lucky_block8, lucky_block9, lucky_block10, lucky_block11, lucky_block12 };
	movingRight = true;
	isGameOver = false;
	isWin = false;
	showGameOver = false;
	allowSpawn = true;
	distanceSpawn = -1850;
	flag_bottom.xc = 0;
	flag_stick.xc = 0;
	flag.xc = 0;
	globalDx = 0;

	move_ptr = lvl1_moving1;
	move_ptr2 = lvl2_moving1;
	move_ptr3 = lvl3_moving1;

	glClearColor(0.0, 0.7, 1.0, 0.0);

	ma_sound_start(&ground_theme);
	ma_sound_set_looping(&ground_theme, true);
}

void update_func() {
	if (keyStates[27]) {
		ma_sound_uninit(&coin_sound);
		ma_sound_uninit(&jump_sound);
		ma_sound_uninit(&break_sound);
		ma_sound_uninit(&kick_sound);
		ma_sound_uninit(&ground_theme);
		ma_sound_uninit(&over_sound);
		ma_sound_uninit(&stage_clear);
		ma_engine_uninit(&engine);
		exit(0);
	}
	/// 1 CONTROLLING
	playerHitbox.hasBottomCollision = false;
	playerHitbox.Xinc = 0;
	isMoving = false;
	if (!isGameOver && !isWin) {
		if (keyStates['a']) {
			playerHitbox.Xinc = -5;
			movingRight = false;
			isMoving = true;
		}
		if (keyStates['d']) {
			playerHitbox.Xinc = 5;
			movingRight = true;
			isMoving = true;
		}
	}
	if (playerHitbox.xc == 0 && playerHitbox.Xinc < 0) playerHitbox.Xinc = 0;
	if (playerHitbox.xc == ScreenWidth - playerHitbox.X && playerHitbox.Xinc > 0) playerHitbox.Xinc = 0;

	for (int i = 0; i < coins.size(); i++) {
		if (coins[i].yc >= 90) {
			coins.erase(coins.begin() + i);
			i--;
			continue;
		}
		coins[i].yc += 5;
	}

	for (int i = 0; i < 1; i++) {
		/// 2 LOGIC AND PHYSICS
		if (isInvulnerable) {
			end_point = chrono::steady_clock::now();
			if (chrono::duration_cast<chrono::seconds>(end_point - begin_point).count() >= 1) isInvulnerable = false;
		}

		if (keyStates[32] && !playerHitbox.hasPotentialEnergy && !isGameOver && !isWin) { //start jump
			ma_sound_start(&jump_sound);
			if (playerHitbox.playerLevel < 2) playerHitbox.V0 = 65;
			else playerHitbox.V0 = 65;
			playerHitbox.hasPotentialEnergy = true;
		}
		if (playerHitbox.hasPotentialEnergy) {
			playerHitbox.t += 1 / FPS;
			playerHitbox.yc = playerHitbox.V0 * playerHitbox.t * 10 - 100 * g * playerHitbox.t * playerHitbox.t / 2;   //current dy
		}
		//playerHitbox.yc = playerHitbox.dy;

		for (int h = 0; h < buff_Hit_boxes.size(); h++) {                //for buffs with moves (right-left and fall)
			if (buff_Hit_boxes[h].buff_id == 2) continue;
			buff_Hit_boxes[h].hasBottomCollision = false;
			if (buff_Hit_boxes[h].hasPotentialEnergy) {
				buff_Hit_boxes[h].t += 1 / FPS;
				buff_Hit_boxes[h].yc = -100 * g * buff_Hit_boxes[h].t * buff_Hit_boxes[h].t / 2;
			}
			for (unsigned int i = 0; i < Rectangles.size(); i++) {         //check if buff collides with static objects
				switch (checkCollisionType(buff_Hit_boxes[h], Rectangles[i])) {
				case TOP:
					break;
				case BOTTOM:
					buff_Hit_boxes[h].t = 0;
					buff_Hit_boxes[h].y0 = Rectangles[i].Y + Rectangles[i].y0 + Rectangles[i].yc;
					buff_Hit_boxes[h].yc = 0;
					buff_Hit_boxes[h].hasPotentialEnergy = false;
					buff_Hit_boxes[h].hasBottomCollision = true;
					break;
				case LEFT:
					buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
					break;
				case RIGHT:
					buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
					break;
				default:
					break;
				}
			}
			for (unsigned int i = 0; i < lucky_blocks.size(); i++) {         //check if buff collides with lucky blocks
				switch (checkCollisionType(buff_Hit_boxes[h], lucky_blocks[i])) {
				case TOP:
					break;
				case BOTTOM:
					buff_Hit_boxes[h].t = 0;
					buff_Hit_boxes[h].y0 = lucky_blocks[i].Y + lucky_blocks[i].y0 + lucky_blocks[i].yc;
					buff_Hit_boxes[h].yc = 0;
					buff_Hit_boxes[h].hasPotentialEnergy = false;
					buff_Hit_boxes[h].hasBottomCollision = true;
					break;
				case LEFT:
					buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
					break;
				case RIGHT:
					buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
					break;
				default:
					break;
				}
			}
			if (!buff_Hit_boxes[h].hasBottomCollision) buff_Hit_boxes[h].hasPotentialEnergy = true;
			buff_Hit_boxes[h].xc += buff_Hit_boxes[h].Xinc;
			if (buff_Hit_boxes[h].y0 + buff_Hit_boxes[h].yc < 0)
				buff_Hit_boxes.erase(buff_Hit_boxes.begin() + h);
		}

		for (int i = 0; i < enemy_Hit_boxes.size(); i++) {     //for enemies
			if (enemy_Hit_boxes[i].turtle_level == 2) continue;
			enemy_Hit_boxes[i].hasBottomCollision = false;
			if (enemy_Hit_boxes[i].hasPotentialEnergy) {
				enemy_Hit_boxes[i].t += 1 / FPS;
				enemy_Hit_boxes[i].yc = -100 * g * enemy_Hit_boxes[i].t * enemy_Hit_boxes[i].t / 2;
			}
			for (unsigned int h = 0; h < Rectangles.size(); h++) {         //check if enemy collides with static objects
				switch (checkCollisionType(enemy_Hit_boxes[i], Rectangles[h])) {
				case TOP:
					break;
				case BOTTOM:
					enemy_Hit_boxes[i].t = 0;
					enemy_Hit_boxes[i].y0 = Rectangles[h].Y + Rectangles[h].y0 + Rectangles[h].yc;
					enemy_Hit_boxes[i].yc = 0;
					enemy_Hit_boxes[i].hasPotentialEnergy = false;
					enemy_Hit_boxes[i].hasBottomCollision = true;
					break;
				case LEFT:
					enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
					break;
				case RIGHT:
					enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
					break;
				default:
					break;
				}
			}
			if (enemy_Hit_boxes[i].enemy_id == 2 && enemy_Hit_boxes[i].turtle_level == 3)
				for (unsigned int h = 0; h < enemy_Hit_boxes.size(); h++) {  //check if turtle lvl3 collides with enemies
					if (i == h) continue;
					switch (checkCollisionType(enemy_Hit_boxes[i], enemy_Hit_boxes[h])) {
					case TOP:
						break;
					case BOTTOM:
						break;
					case LEFT:
						enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + h);
						i--;
						h--;
						break;
					case RIGHT:
						enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + h);
						i--;
						h--;
						break;
					default:
						break;
					}
				}
			for (unsigned int h = 0; h < lucky_blocks.size(); h++) {         //check if enemy collides with lucky blocks
				switch (checkCollisionType(enemy_Hit_boxes[i], lucky_blocks[h])) {
				case TOP:
					break;
				case BOTTOM:
					enemy_Hit_boxes[i].t = 0;
					enemy_Hit_boxes[i].y0 = lucky_blocks[h].Y + lucky_blocks[h].y0 + lucky_blocks[h].yc;
					enemy_Hit_boxes[i].yc = 0;
					enemy_Hit_boxes[i].hasPotentialEnergy = false;
					enemy_Hit_boxes[i].hasBottomCollision = true;
					break;
				case LEFT:
					enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
					break;
				case RIGHT:
					enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
					break;
				default:
					break;
				}
			}
			if (!enemy_Hit_boxes[i].hasBottomCollision) enemy_Hit_boxes[i].hasPotentialEnergy = true;
			enemy_Hit_boxes[i].move();
			if (enemy_Hit_boxes[i].y0 + enemy_Hit_boxes[i].yc < 0)
				enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + i);
		}

		/// 3 CHECK COLLISION / CHANGE PHYSICS
		if (!isGameOver && !isWin) {
			for (unsigned int i = 0; i < Rectangles.size(); i++) {         //check with static objects
				switch (checkCollisionType(playerHitbox, Rectangles[i])) {
				case TOP:
					playerHitbox.V0 = 0;
					playerHitbox.t = 0;
					playerHitbox.y0 = Rectangles[i].y0 - playerHitbox.Y;
					playerHitbox.yc = 0;
					if (Rectangles[i].isDestroyable && playerHitbox.playerLevel > 1) {
						ma_sound_start(&break_sound);
						Rectangles.erase(Rectangles.begin() + i);
					}
					break;
				case BOTTOM:
					playerHitbox.y0 = Rectangles[i].Y + Rectangles[i].y0 + Rectangles[i].yc;
					playerHitbox.V0 = 0;
					playerHitbox.t = 0;
					playerHitbox.yc = 0;
					playerHitbox.hasPotentialEnergy = false;
					playerHitbox.hasBottomCollision = true;
					break;
				case LEFT:
					if (playerHitbox.Xinc > 0) playerHitbox.Xinc = 0;
					break;
				case RIGHT:
					if (playerHitbox.Xinc < 0) playerHitbox.Xinc = 0;
					break;
				default:
					break;
				}
			}
			for (unsigned int i = 0; i < lucky_blocks.size(); i++) {         //check with lucky blocks
				switch (checkCollisionType(playerHitbox, lucky_blocks[i])) {
				case TOP:
					playerHitbox.V0 = 0;
					playerHitbox.t = 0;
					playerHitbox.y0 = lucky_blocks[i].y0 - playerHitbox.Y;
					playerHitbox.yc = 0;
					if (!lucky_blocks[i].buffTaken) {
						if (lucky_blocks[i].buff_id == 1)
							if (playerHitbox.playerLevel < 2) {
								Buff buff1(lucky_blocks[i].x0 + lucky_blocks[i].xc, lucky_blocks[i].y0 + lucky_blocks[i].Y + 1, 40, 40, 1, 0, 2, 1);
								buff_Hit_boxes.push_back(buff1);
							}
							else {
								Buff buff2(lucky_blocks[i].x0 + lucky_blocks[i].xc, lucky_blocks[i].y0 + lucky_blocks[i].Y + 1, 40, 40, 1, 0, 5, 2);
								buff_Hit_boxes.push_back(buff2);
							}
						else {
							//PlaySound(L"smb_coin.wav", NULL, SND_ASYNC);
							//sndPlaySound(L"smb_coin.wav", SND_ASYNC);
							//PlaySound((LPCWSTR)audioBytes1, NULL, SND_MEMORY | SND_ASYNC);
							if (ma_sound_is_playing(&coin_sound)) ma_sound_stop(&coin_sound);
							ma_sound_start(&coin_sound);
							Static_Object coin(lucky_blocks[i].x0 + lucky_blocks[i].xc + 5, lucky_blocks[i].y0 + 20, 30, 40, 9, 0);
							coins.push_back(coin);
						}
						lucky_blocks[i].buffTaken = true;
					}
					break;
				case BOTTOM:
					playerHitbox.y0 = lucky_blocks[i].Y + lucky_blocks[i].y0 + lucky_blocks[i].yc;
					playerHitbox.V0 = 0;
					playerHitbox.t = 0;
					playerHitbox.yc = 0;
					playerHitbox.hasPotentialEnergy = false;
					playerHitbox.hasBottomCollision = true;
					break;
				case LEFT:
					if (playerHitbox.Xinc > 0) playerHitbox.Xinc = 0;
					break;
				case RIGHT:
					if (playerHitbox.Xinc < 0) playerHitbox.Xinc = 0;
					break;
				default:
					break;
				}
			}
			for (unsigned int i = 0; i < buff_Hit_boxes.size(); i++) {    //check with buff elements
				CollisionType type = checkCollisionType(playerHitbox, buff_Hit_boxes[i]);
				if (type != NONE) {
					if (playerHitbox.playerLevel == 1) playerHitbox.Y *= 2;
					playerHitbox.playerLevel += 1;
					buff_Hit_boxes.erase(buff_Hit_boxes.begin() + i);
				}
			}
			for (unsigned int i = 0; i < enemy_Hit_boxes.size(); i++) {    //check with enemies
				if (isInvulnerable || isGameOver) break;
				switch (checkCollisionType(playerHitbox, enemy_Hit_boxes[i])) {
				case TOP:
					playerHitbox.playerLevel--;
					if (playerHitbox.playerLevel == 0) {
						isGameOver = true;
						playerHitbox.hasPotentialEnergy = true;
						playerHitbox.V0 = 50;
						playerHitbox.t = 0;
						playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
						playerHitbox.yc = 0;
						break;
					}
					if (playerHitbox.playerLevel == 1) playerHitbox.Y /= 2;
					isInvulnerable = true;
					begin_point = chrono::steady_clock::now();
					break;
				case BOTTOM:
					playerHitbox.y0 = enemy_Hit_boxes[i].Y + enemy_Hit_boxes[i].y0 + enemy_Hit_boxes[i].yc;
					playerHitbox.V0 = 20;
					playerHitbox.t = 0;
					//playerHitbox.dy = playerHitbox.Ystart;
					playerHitbox.yc = 0;
					//playerHitbox.hasBottomCollision = true;
					ma_sound_start(&kick_sound);
					if (enemy_Hit_boxes[i].enemy_id == 2) {
						enemy_Hit_boxes[i].turtle_level++;
						if (enemy_Hit_boxes[i].turtle_level == 2) enemy_Hit_boxes[i].Y = 38;
						if (enemy_Hit_boxes[i].turtle_level == 3)
							if (playerHitbox.x0 + playerHitbox.xc + playerHitbox.X / 2 < enemy_Hit_boxes[i].x0 + enemy_Hit_boxes[i].xc + enemy_Hit_boxes[i].X / 2)
								if (enemy_Hit_boxes[i].Xinc > 0)
									enemy_Hit_boxes[i].Xinc *= 10;
								else enemy_Hit_boxes[i].Xinc *= -10;
							else
								if (enemy_Hit_boxes[i].Xinc > 0)
								enemy_Hit_boxes[i].Xinc *= -10;
								else enemy_Hit_boxes[i].Xinc *= 10;
						break;
					}
					enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + i);
					break;
				case LEFT:
					playerHitbox.playerLevel--;
					if (playerHitbox.playerLevel == 0) {
						isGameOver = true;
						playerHitbox.hasPotentialEnergy = true;
						playerHitbox.V0 = 50;
						playerHitbox.t = 0;
						playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
						playerHitbox.yc = 0;
						break;
					}
					if (playerHitbox.playerLevel == 1) playerHitbox.Y /= 2;
					isInvulnerable = true;
					begin_point = chrono::steady_clock::now();
					break;
				case RIGHT:
					playerHitbox.playerLevel--;
					if (playerHitbox.playerLevel == 0) {
						isGameOver = true;
						playerHitbox.hasPotentialEnergy = true;
						playerHitbox.V0 = 50;
						playerHitbox.t = 0;
						playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
						playerHitbox.yc = 0;
						break;
					}
					if (playerHitbox.playerLevel == 1) playerHitbox.Y /= 2;
					isInvulnerable = true;
					begin_point = chrono::steady_clock::now();
					break;
				default:
					break;
				}
			}
			if (!playerHitbox.hasBottomCollision) playerHitbox.hasPotentialEnergy = true; //if no bottom collisions, then falling
		}
		//playerHitbox.yc = playerHitbox.dy;
		if (playerHitbox.y0 + playerHitbox.yc <= 0.0 && !isGameOver) {
			if (playerHitbox.playerLevel > 1) playerHitbox.Y /= 2;
			isGameOver = true;
			playerHitbox.hasPotentialEnergy = true;
			playerHitbox.V0 = 50;
			playerHitbox.t = 0;
			playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
			playerHitbox.yc = 0;
		}
		if (isGameOver && playerHitbox.y0 + playerHitbox.yc <= -2000.0) {
			glClearColor(0.0, 0.0, 0.0, 0.0);
			showGameOver = true;
			Rectangles.clear();
			lucky_blocks.clear();
			buff_Hit_boxes.clear();
			enemy_Hit_boxes.clear();
			ma_sound_stop(&ground_theme);
			ma_sound_seek_to_pcm_frame(&ground_theme, 0);
			ma_sound_start(&over_sound);
			return;
		}
	}

	if (!isGameOver && !isWin) {
		//move global objects or move the player
		if (playerHitbox.xc == ScreenWidth / 2 - 40) {
			if ((playerHitbox.Xinc < 0 && Rectangles[0].xc == 0) || (playerHitbox.Xinc > 0 && Rectangles[0].xc == -(8800 - ScreenWidth)))
				playerHitbox.xc += playerHitbox.Xinc;
			else {
				for (int i = 0; i < Rectangles.size(); i++)
					Rectangles[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < lucky_blocks.size(); i++)
					lucky_blocks[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < enemy_Hit_boxes.size(); i++)
					enemy_Hit_boxes[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < buff_Hit_boxes.size(); i++)
					buff_Hit_boxes[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < coins.size(); i++)
					coins[i].xc -= playerHitbox.Xinc;
				flag_bottom.xc -= playerHitbox.Xinc;
				flag_stick.xc -= playerHitbox.Xinc;
				flag.xc -= playerHitbox.Xinc;
				globalDx -= playerHitbox.Xinc;
			}
		}
		else
			playerHitbox.xc += playerHitbox.Xinc;
		//

		if (checkCollisionType(playerHitbox, flag_stick) == RIGHT) {
			isWin = true;
			playerHitbox.x0 = flag_stick.x0 + flag_stick.xc - playerHitbox.X;
			playerHitbox.xc = 0;
			playerHitbox.hasPotentialEnergy = false;
			playerHitbox.t = 0.0;
			playerHitbox.V0 = 0.0;
		}

		if (allowSpawn) {
			switch (int(globalDx)) {
			case -1850: {
				if (distanceSpawn != -1850) break;
				Enemy mushroom(3300 + globalDx, 480, 40, 40, -1, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom);
				Enemy mushroom2(3400 + globalDx, 480, 40, 40, -1, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom2);
				distanceSpawn = -3000;
				break;
			}
			case -3000: {
				if (distanceSpawn != -3000) break;
				Enemy mushroom(4600 + globalDx, 80, 40, 40, -1, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom);
				Enemy mushroom2(4680 + globalDx, 80, 40, 40, -1, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom2);
				Enemy turtle(4450 + globalDx, 80, 40, 60, -1, 0, 7, 2);
				enemy_Hit_boxes.push_back(turtle);
				distanceSpawn = -5700;
				break;
			}
			case -5700: {
				if (distanceSpawn != -5700) break;
				Enemy mushroom(7100 + globalDx, 80, 40, 40, -1, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom);
				Enemy turtle(7300 + globalDx, 80, 40, 60, -1, 0, 7, 2);
				enemy_Hit_boxes.push_back(turtle);
				allowSpawn = false;
				break;
			}
			}
		}
	}

	if (isWin) {
		if (flag.yc != -300.0) flag.yc -= 4;
		if (playerHitbox.y0 + playerHitbox.yc > 120.0) {
			playerHitbox.yc -= 4;
			if (playerHitbox.y0 + playerHitbox.yc <= 120) {
				playerHitbox.y0 = 120;
				playerHitbox.yc = 0;
			}
		}
		if (flag.yc == -300) {
			if (playerHitbox.y0 != 80.0) {
				playerHitbox.hasPotentialEnergy = true;
				if (playerHitbox.y0 + playerHitbox.yc <= 80) {
					playerHitbox.hasPotentialEnergy = false;
					playerHitbox.y0 = 80.0;
					playerHitbox.yc = 0.0;
				}
				return;
			}
			isMoving = false;
			playerHitbox.hasBottomCollision = true;
			if (playerHitbox.xc < 300.0) {
				isMoving = true;
				playerHitbox.xc += 2.0;
				if (playerHitbox.xc == 300.0) {
					ma_sound_stop(&ground_theme);
					ma_sound_seek_to_pcm_frame(&ground_theme, 0);
					ma_sound_start(&stage_clear);
				}
			}
		}
	}
	animation_end = chrono::steady_clock::now();
	moving_end = chrono::steady_clock::now();
	if (chrono::duration_cast<chrono::milliseconds>(animation_end - animation_begin).count() >= 500) {
		if (use_first_texture) use_first_texture = false;
		else use_first_texture = true;
		animation_begin = chrono::steady_clock::now();
	}
	if (chrono::duration_cast<chrono::milliseconds>(moving_end - moving_begin).count() >= 100) {
		if (isMoving) {
			switch (animation_counter) {
			case 0:
				move_ptr = lvl1_moving1;
				move_ptr2 = lvl2_moving1;
				move_ptr3 = lvl3_moving1;
				break;
			case 1:
				move_ptr = lvl1_moving2;
				move_ptr2 = lvl2_moving2;
				move_ptr3 = lvl3_moving2;
				break;
			case 2:
				move_ptr = lvl1_moving3;
				move_ptr2 = lvl2_moving3;
				move_ptr3 = lvl3_moving3;
				break;
			}
			if (animation_counter == 0) animationInc = 1;
			if (animation_counter == 2) animationInc = -1;
			animation_counter += animationInc;
		}
		else {
			move_ptr = lvl1_moving1;
			move_ptr2 = lvl2_moving1;
			move_ptr3 = lvl3_moving1;
		}

		moving_begin = chrono::steady_clock::now();
	}
}

void update_thread_func() {
	if (keyStates[27]) {
		ma_sound_uninit(&coin_sound);
		ma_sound_uninit(&jump_sound);
		ma_sound_uninit(&break_sound);
		ma_sound_uninit(&kick_sound);
		ma_sound_uninit(&ground_theme);
		ma_sound_uninit(&over_sound);
		ma_sound_uninit(&stage_clear);
		ma_engine_uninit(&engine);
		exit(0);
	}
	/// 1 CONTROLLING
	playerHitbox.hasBottomCollision = false;
	playerHitbox.Xinc = 0;
	isMoving = false;
	if (!isGameOver && !isWin) {
		if (keyStates['a']) {
			playerHitbox.Xinc = -1;
			movingRight = false;
			isMoving = true;
		}
		if (keyStates['d']) {
			playerHitbox.Xinc = 1;
			movingRight = true;
			isMoving = true;
		}
	}
	if (playerHitbox.xc == 0 && playerHitbox.Xinc < 0) playerHitbox.Xinc = 0;
	if (playerHitbox.xc == ScreenWidth - playerHitbox.X && playerHitbox.Xinc > 0) playerHitbox.Xinc = 0;

	for (int i = 0; i < coins.size(); i++) {
		if (coins[i].yc >= 90) {
			coins.erase(coins.begin() + i);
			i--;
			continue;
		}
		coins[i].yc += 1.25;
	}

	/// 2 LOGIC AND PHYSICS
	if (isInvulnerable) {
		end_point = chrono::steady_clock::now();
		if (chrono::duration_cast<chrono::seconds>(end_point - begin_point).count() >= 1) isInvulnerable = false;
	}

	if (keyStates[32] && !playerHitbox.hasPotentialEnergy && !isGameOver && !isWin) { //start jump
		ma_sound_start(&jump_sound);
		if (playerHitbox.playerLevel < 2) playerHitbox.V0 = 65;
		else playerHitbox.V0 = 65;
		playerHitbox.hasPotentialEnergy = true;
	}
	if (playerHitbox.hasPotentialEnergy) {
		playerHitbox.t += 1 / 240.0;
		playerHitbox.yc = playerHitbox.V0 * playerHitbox.t * 10 - 100 * g * playerHitbox.t * playerHitbox.t / 2;   //current dy
	}
	//playerHitbox.yc = playerHitbox.dy;

	for (int h = 0; h < buff_Hit_boxes.size(); h++) {                //for buffs with moves (right-left and fall)
		if (buff_Hit_boxes[h].buff_id == 2) continue;
		buff_Hit_boxes[h].hasBottomCollision = false;
		if (buff_Hit_boxes[h].hasPotentialEnergy) {
			buff_Hit_boxes[h].t += 1 / 240.0;
			buff_Hit_boxes[h].yc = -100 * g * buff_Hit_boxes[h].t * buff_Hit_boxes[h].t / 2;
		}
		for (unsigned int i = 0; i < Rectangles.size(); i++) {         //check if buff collides with static objects
			switch (checkCollisionType(buff_Hit_boxes[h], Rectangles[i])) {
			case TOP:
				break;
			case BOTTOM:
				buff_Hit_boxes[h].t = 0;
				buff_Hit_boxes[h].y0 = Rectangles[i].Y + Rectangles[i].y0 + Rectangles[i].yc;
				buff_Hit_boxes[h].yc = 0;
				buff_Hit_boxes[h].hasPotentialEnergy = false;
				buff_Hit_boxes[h].hasBottomCollision = true;
				break;
			case LEFT:
				buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
				break;
			case RIGHT:
				buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
				break;
			default:
				break;
			}
		}
		for (unsigned int i = 0; i < lucky_blocks.size(); i++) {         //check if buff collides with lucky blocks
			switch (checkCollisionType(buff_Hit_boxes[h], lucky_blocks[i])) {
			case TOP:
				break;
			case BOTTOM:
				buff_Hit_boxes[h].t = 0;
				buff_Hit_boxes[h].y0 = lucky_blocks[i].Y + lucky_blocks[i].y0 + lucky_blocks[i].yc;
				buff_Hit_boxes[h].yc = 0;
				buff_Hit_boxes[h].hasPotentialEnergy = false;
				buff_Hit_boxes[h].hasBottomCollision = true;
				break;
			case LEFT:
				buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
				break;
			case RIGHT:
				buff_Hit_boxes[h].Xinc = -buff_Hit_boxes[h].Xinc;
				break;
			default:
				break;
			}
		}
		if (!buff_Hit_boxes[h].hasBottomCollision) buff_Hit_boxes[h].hasPotentialEnergy = true;
		buff_Hit_boxes[h].xc += buff_Hit_boxes[h].Xinc;
		if (buff_Hit_boxes[h].y0 + buff_Hit_boxes[h].yc < 0)
			buff_Hit_boxes.erase(buff_Hit_boxes.begin() + h);
	}

	for (int i = 0; i < enemy_Hit_boxes.size(); i++) {     //for enemies
		if (enemy_Hit_boxes[i].turtle_level == 2) continue;
		enemy_Hit_boxes[i].hasBottomCollision = false;
		if (enemy_Hit_boxes[i].hasPotentialEnergy) {
			enemy_Hit_boxes[i].t += 1 / 240.0;
			enemy_Hit_boxes[i].yc = -100 * g * enemy_Hit_boxes[i].t * enemy_Hit_boxes[i].t / 2;
		}
		for (unsigned int h = 0; h < Rectangles.size(); h++) {         //check if enemy collides with static objects
			switch (checkCollisionType(enemy_Hit_boxes[i], Rectangles[h])) {
			case TOP:
				break;
			case BOTTOM:
				enemy_Hit_boxes[i].t = 0;
				enemy_Hit_boxes[i].y0 = Rectangles[h].Y + Rectangles[h].y0 + Rectangles[h].yc;
				enemy_Hit_boxes[i].yc = 0;
				enemy_Hit_boxes[i].hasPotentialEnergy = false;
				enemy_Hit_boxes[i].hasBottomCollision = true;
				break;
			case LEFT:
				enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
				break;
			case RIGHT:
				enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
				break;
			default:
				break;
			}
		}
		if (enemy_Hit_boxes[i].enemy_id == 2 && enemy_Hit_boxes[i].turtle_level == 3)
			for (unsigned int h = 0; h < enemy_Hit_boxes.size(); h++) {  //check if turtle lvl3 collides with enemies
				if (i == h) continue;
				switch (checkCollisionType(enemy_Hit_boxes[i], enemy_Hit_boxes[h])) {
				case TOP:
					break;
				case BOTTOM:
					break;
				case LEFT:
					enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + h);
					i--;
					h--;
					break;
				case RIGHT:
					enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + h);
					i--;
					h--;
					break;
				default:
					break;
				}
			}
		for (unsigned int h = 0; h < lucky_blocks.size(); h++) {         //check if enemy collides with lucky blocks
			switch (checkCollisionType(enemy_Hit_boxes[i], lucky_blocks[h])) {
			case TOP:
				break;
			case BOTTOM:
				enemy_Hit_boxes[i].t = 0;
				enemy_Hit_boxes[i].y0 = lucky_blocks[h].Y + lucky_blocks[h].y0 + lucky_blocks[h].yc;
				enemy_Hit_boxes[i].yc = 0;
				enemy_Hit_boxes[i].hasPotentialEnergy = false;
				enemy_Hit_boxes[i].hasBottomCollision = true;
				break;
			case LEFT:
				enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
				break;
			case RIGHT:
				enemy_Hit_boxes[i].Xinc = -enemy_Hit_boxes[i].Xinc;
				break;
			default:
				break;
			}
		}
		if (!enemy_Hit_boxes[i].hasBottomCollision) enemy_Hit_boxes[i].hasPotentialEnergy = true;
		enemy_Hit_boxes[i].move();
		if (enemy_Hit_boxes[i].y0 + enemy_Hit_boxes[i].yc < 0)
			enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + i);
	}

	/// 3 CHECK COLLISION / CHANGE PHYSICS
	if (!isGameOver && !isWin) {
		for (unsigned int i = 0; i < Rectangles.size(); i++) {         //check with static objects
			switch (checkCollisionType(playerHitbox, Rectangles[i])) {
			case TOP:
				playerHitbox.V0 = 0;
				playerHitbox.t = 0;
				playerHitbox.y0 = Rectangles[i].y0 - playerHitbox.Y;
				playerHitbox.yc = 0;
				if (Rectangles[i].isDestroyable && playerHitbox.playerLevel > 1) {
					ma_sound_start(&break_sound);
					Rectangles.erase(Rectangles.begin() + i);
				}
				break;
			case BOTTOM:
				playerHitbox.y0 = Rectangles[i].Y + Rectangles[i].y0 + Rectangles[i].yc;
				playerHitbox.V0 = 0;
				playerHitbox.t = 0;
				playerHitbox.yc = 0;
				playerHitbox.hasPotentialEnergy = false;
				playerHitbox.hasBottomCollision = true;
				break;
			case LEFT:
				if (playerHitbox.Xinc > 0) playerHitbox.Xinc = 0;
				break;
			case RIGHT:
				if (playerHitbox.Xinc < 0) playerHitbox.Xinc = 0;
				break;
			default:
				break;
			}
		}
		for (unsigned int i = 0; i < lucky_blocks.size(); i++) {         //check with lucky blocks
			switch (checkCollisionType(playerHitbox, lucky_blocks[i])) {
			case TOP:
				playerHitbox.V0 = 0;
				playerHitbox.t = 0;
				playerHitbox.y0 = lucky_blocks[i].y0 - playerHitbox.Y;
				playerHitbox.yc = 0;
				if (!lucky_blocks[i].buffTaken) {
					if (lucky_blocks[i].buff_id == 1)
						if (playerHitbox.playerLevel < 2) {
							Buff buff1(lucky_blocks[i].x0 + lucky_blocks[i].xc, lucky_blocks[i].y0 + lucky_blocks[i].Y + 1, 40, 40, 0.25, 0, 2, 1);
							buff_Hit_boxes.push_back(buff1);
						}
						else {
							Buff buff2(lucky_blocks[i].x0 + lucky_blocks[i].xc, lucky_blocks[i].y0 + lucky_blocks[i].Y + 1, 40, 40, 0.25, 0, 5, 2);
							buff_Hit_boxes.push_back(buff2);
						}
					else {
						if (ma_sound_is_playing(&coin_sound)) ma_sound_stop(&coin_sound);
						ma_sound_start(&coin_sound);
						Static_Object coin(lucky_blocks[i].x0 + lucky_blocks[i].xc + 5, lucky_blocks[i].y0 + 20, 30, 40, 9, 0);
						coins.push_back(coin);
					}
					lucky_blocks[i].buffTaken = true;
				}
				break;
			case BOTTOM:
				playerHitbox.y0 = lucky_blocks[i].Y + lucky_blocks[i].y0 + lucky_blocks[i].yc;
				playerHitbox.V0 = 0;
				playerHitbox.t = 0;
				playerHitbox.yc = 0;
				playerHitbox.hasPotentialEnergy = false;
				playerHitbox.hasBottomCollision = true;
				break;
			case LEFT:
				if (playerHitbox.Xinc > 0) playerHitbox.Xinc = 0;
				break;
			case RIGHT:
				if (playerHitbox.Xinc < 0) playerHitbox.Xinc = 0;
				break;
			default:
				break;
			}
		}
		for (unsigned int i = 0; i < buff_Hit_boxes.size(); i++) {    //check with buff elements
			CollisionType type = checkCollisionType(playerHitbox, buff_Hit_boxes[i]);
			if (type != NONE) {
				if (playerHitbox.playerLevel == 1) playerHitbox.Y *= 2;
				if (playerHitbox.playerLevel < 3) playerHitbox.playerLevel += 1;
				buff_Hit_boxes.erase(buff_Hit_boxes.begin() + i);
			}
		}
		for (unsigned int i = 0; i < enemy_Hit_boxes.size(); i++) {    //check with enemies
			if (isInvulnerable || isGameOver) break;
			switch (checkCollisionType(playerHitbox, enemy_Hit_boxes[i])) {
			case TOP:
				playerHitbox.playerLevel--;
				if (playerHitbox.playerLevel == 0) {
					isGameOver = true;
					playerHitbox.hasPotentialEnergy = true;
					playerHitbox.V0 = 50;
					playerHitbox.t = 0;
					playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
					playerHitbox.yc = 0;
					break;
				}
				if (playerHitbox.playerLevel == 1) playerHitbox.Y /= 2;
				isInvulnerable = true;
				begin_point = chrono::steady_clock::now();
				break;
			case BOTTOM:
				playerHitbox.y0 = enemy_Hit_boxes[i].Y + enemy_Hit_boxes[i].y0 + enemy_Hit_boxes[i].yc;
				playerHitbox.V0 = 20;
				playerHitbox.t = 0;
				//playerHitbox.dy = playerHitbox.Ystart;
				playerHitbox.yc = 0;
				//playerHitbox.hasBottomCollision = true;
				ma_sound_start(&kick_sound);
				if (enemy_Hit_boxes[i].enemy_id == 2) {
					enemy_Hit_boxes[i].turtle_level++;
					if (enemy_Hit_boxes[i].turtle_level == 2) enemy_Hit_boxes[i].Y = 38;
					if (enemy_Hit_boxes[i].turtle_level == 3)
						if (playerHitbox.x0 + playerHitbox.xc + playerHitbox.X / 2 < enemy_Hit_boxes[i].x0 + enemy_Hit_boxes[i].xc + enemy_Hit_boxes[i].X / 2)
							if (enemy_Hit_boxes[i].Xinc > 0)
								enemy_Hit_boxes[i].Xinc *= 10;
							else enemy_Hit_boxes[i].Xinc *= -10;
						else
							if (enemy_Hit_boxes[i].Xinc > 0)
								enemy_Hit_boxes[i].Xinc *= -10;
							else enemy_Hit_boxes[i].Xinc *= 10;
					break;
				}
				enemy_Hit_boxes.erase(enemy_Hit_boxes.begin() + i);
				break;
			case LEFT:
				playerHitbox.playerLevel--;
				if (playerHitbox.playerLevel == 0) {
					isGameOver = true;
					playerHitbox.hasPotentialEnergy = true;
					playerHitbox.V0 = 50;
					playerHitbox.t = 0;
					playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
					playerHitbox.yc = 0;
					break;
				}
				if (playerHitbox.playerLevel == 1) playerHitbox.Y /= 2;
				isInvulnerable = true;
				begin_point = chrono::steady_clock::now();
				break;
			case RIGHT:
				playerHitbox.playerLevel--;
				if (playerHitbox.playerLevel == 0) {
					isGameOver = true;
					playerHitbox.hasPotentialEnergy = true;
					playerHitbox.V0 = 50;
					playerHitbox.t = 0;
					playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
					playerHitbox.yc = 0;
					break;
				}
				if (playerHitbox.playerLevel == 1) playerHitbox.Y /= 2;
				isInvulnerable = true;
				begin_point = chrono::steady_clock::now();
				break;
			default:
				break;
			}
		}
		if (!playerHitbox.hasBottomCollision) playerHitbox.hasPotentialEnergy = true; //if no bottom collisions, then falling
	}
	//playerHitbox.yc = playerHitbox.dy;
	if (playerHitbox.y0 + playerHitbox.yc <= 0.0 && !isGameOver) {
		if (playerHitbox.playerLevel > 1) playerHitbox.Y /= 2;
		isGameOver = true;
		playerHitbox.hasPotentialEnergy = true;
		playerHitbox.V0 = 50;
		playerHitbox.t = 0;
		playerHitbox.y0 = playerHitbox.y0 + playerHitbox.yc;
		playerHitbox.yc = 0;
	}
	if (isGameOver && playerHitbox.y0 + playerHitbox.yc <= -2000.0) {
		showGameOver = true;
		Rectangles.clear();
		lucky_blocks.clear();
		buff_Hit_boxes.clear();
		enemy_Hit_boxes.clear();
		ma_sound_stop(&ground_theme);
		ma_sound_seek_to_pcm_frame(&ground_theme, 0);
		ma_sound_start(&over_sound);
		//glClearColor(0.0, 0.0, 0.0, 0.0);
		return;
	}

	if (!isGameOver && !isWin) {
		//move global objects or move the player
		if (playerHitbox.xc == ScreenWidth / 2 - 40) {
			if ((playerHitbox.Xinc < 0 && Rectangles[0].xc == 0) || (playerHitbox.Xinc > 0 && Rectangles[0].xc == -(8800 - ScreenWidth)))
				playerHitbox.xc += playerHitbox.Xinc;
			else {
				for (int i = 0; i < Rectangles.size(); i++)
					Rectangles[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < lucky_blocks.size(); i++)
					lucky_blocks[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < enemy_Hit_boxes.size(); i++)
					enemy_Hit_boxes[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < buff_Hit_boxes.size(); i++)
					buff_Hit_boxes[i].xc -= playerHitbox.Xinc;
				for (int i = 0; i < coins.size(); i++)
					coins[i].xc -= playerHitbox.Xinc;
				flag_bottom.xc -= playerHitbox.Xinc;
				flag_stick.xc -= playerHitbox.Xinc;
				flag.xc -= playerHitbox.Xinc;
				globalDx -= playerHitbox.Xinc;
			}
		}
		else
			playerHitbox.xc += playerHitbox.Xinc;
		//

		if (checkCollisionType(playerHitbox, flag_stick) == RIGHT) {
			isWin = true;
			playerHitbox.x0 = flag_stick.x0 + flag_stick.xc - playerHitbox.X;
			playerHitbox.xc = 0;
			playerHitbox.hasPotentialEnergy = false;
			playerHitbox.t = 0.0;
			playerHitbox.V0 = 0.0;
		}

		if (allowSpawn) {
			switch (int(globalDx)) {
			case -1850: {
				if (distanceSpawn != -1850) break;
				Enemy mushroom(3300 + globalDx, 480, 40, 40, -0.25, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom);
				Enemy mushroom2(3400 + globalDx, 480, 40, 40, -0.25, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom2);
				distanceSpawn = -3000;
				break;
			}
			case -3000: {
				if (distanceSpawn != -3000) break;
				Enemy mushroom(4600 + globalDx, 80, 40, 40, -0.25, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom);
				Enemy mushroom2(4680 + globalDx, 80, 40, 40, -0.25, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom2);
				Enemy turtle(4450 + globalDx, 80, 40, 60, -0.25, 0, 7, 2);
				enemy_Hit_boxes.push_back(turtle);
				distanceSpawn = -5700;
				break;
			}
			case -5700: {
				if (distanceSpawn != -5700) break;
				Enemy mushroom(7100 + globalDx, 80, 40, 40, -0.25, 0, 3, 1);
				enemy_Hit_boxes.push_back(mushroom);
				Enemy turtle(7300 + globalDx, 80, 40, 60, -0.25, 0, 7, 2);
				enemy_Hit_boxes.push_back(turtle);
				allowSpawn = false;
				break;
			}
			}
		}
	}

	if (isWin) {
		if (flag.yc != -300.0) flag.yc -= 1;
		if (playerHitbox.y0 + playerHitbox.yc > 120.0) {
			playerHitbox.yc -= 1;
			if (playerHitbox.y0 + playerHitbox.yc <= 120) {
				playerHitbox.y0 = 120;
				playerHitbox.yc = 0;
			}
		}
		if (flag.yc == -300) {
			if (playerHitbox.y0 != 80.0) {
				playerHitbox.hasPotentialEnergy = true;
				if (playerHitbox.y0 + playerHitbox.yc <= 80) {
					playerHitbox.hasPotentialEnergy = false;
					playerHitbox.y0 = 80.0;
					playerHitbox.yc = 0.0;
				}
				return;
			}
			isMoving = false;
			playerHitbox.hasBottomCollision = true;
			if (playerHitbox.xc < 300.0) {
				isMoving = true;
				playerHitbox.xc += 0.5;
				if (playerHitbox.xc == 300.0) {
					ma_sound_stop(&ground_theme);
					ma_sound_seek_to_pcm_frame(&ground_theme, 0);
					ma_sound_start(&stage_clear);
				}
			}
		}
	}
	animation_end = chrono::steady_clock::now();
	moving_end = chrono::steady_clock::now();
	if (chrono::duration_cast<chrono::milliseconds>(animation_end - animation_begin).count() >= 500) {
		if (use_first_texture) use_first_texture = false;
		else use_first_texture = true;
		animation_begin = chrono::steady_clock::now();
	}
	if (chrono::duration_cast<chrono::milliseconds>(moving_end - moving_begin).count() >= 100) {
		if (isMoving) {
			switch (animation_counter) {
			case 0:
				move_ptr = lvl1_moving1;
				move_ptr2 = lvl2_moving1;
				move_ptr3 = lvl3_moving1;
				break;
			case 1:
				move_ptr = lvl1_moving2;
				move_ptr2 = lvl2_moving2;
				move_ptr3 = lvl3_moving2;
				break;
			case 2:
				move_ptr = lvl1_moving3;
				move_ptr2 = lvl2_moving3;
				move_ptr3 = lvl3_moving3;
				break;
			}
			if (animation_counter == 0) animationInc = 1;
			if (animation_counter == 2) animationInc = -1;
			animation_counter += animationInc;
		}
		else {
			move_ptr = lvl1_moving1;
			move_ptr2 = lvl2_moving1;
			move_ptr3 = lvl3_moving1;
		}

		moving_begin = chrono::steady_clock::now();
	}
}

void update_Thread() {
	while (true) {
		if (!showGameOver) update_thread_func();
		this_thread::sleep_for(chrono::milliseconds(4));
	}
}

void draw_func() {
	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0, 1.0, 1.0);
	if (!showGameOver) {
		//update_func();
		for (int i = 0; i < Rectangles.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, textures.getTexture(Rectangles[i].tex_id));
			if (Rectangles[i].tex_id == 8) drawRectangleTexture(Rectangles[i], pipe_coords, true);
			else drawRectangleTexture2(Rectangles[i]);
		}
		for (int i = 0; i < coins.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, textures.getTexture(coins[i].tex_id));
			drawRectangleTexture(coins[i], tex_coords, true);
		}
		for (int i = 0; i < lucky_blocks.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, textures.getTexture(lucky_blocks[i].tex_id));
			if (lucky_blocks[i].buffTaken)
				drawRectangleTexture(lucky_blocks[i], lucky_coords3, true);
			else if (use_first_texture)
				drawRectangleTexture(lucky_blocks[i], lucky_coords1, true);
			else drawRectangleTexture(lucky_blocks[i], lucky_coords2, true);
		}
		for (int i = 0; i < enemy_Hit_boxes.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, textures.getTexture(enemy_Hit_boxes[i].tex_id));
			if (enemy_Hit_boxes[i].enemy_id == 2) {
				if (enemy_Hit_boxes[i].turtle_level == 1)
					drawRectangleTexture(enemy_Hit_boxes[i], turtle_coords1, enemy_Hit_boxes[i].Xinc > 0 ? false : true);
				else
					drawRectangleTexture(enemy_Hit_boxes[i], turtle_coords2, true);
			}
			else
				drawRectangleTexture(enemy_Hit_boxes[i], tex_coords, true);
		}
		for (int i = 0; i < buff_Hit_boxes.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, textures.getTexture(buff_Hit_boxes[i].tex_id));
			drawRectangleTexture(buff_Hit_boxes[i], tex_coords, true);
		}
		glBindTexture(GL_TEXTURE_2D, textures.getTexture(4));
		if (!isGameOver) {
			if (playerHitbox.playerLevel == 1)
				if (playerHitbox.hasBottomCollision) {
					if (isMoving)
						drawRectangleTexture(playerHitbox, move_ptr, movingRight);
					else
						drawRectangleTexture(playerHitbox, lvl1_staying, movingRight);
				}
				else {
					drawRectangleTexture(playerHitbox, lvl1_jump, movingRight);
				}
			else if (playerHitbox.playerLevel == 2)
				if (playerHitbox.hasBottomCollision)
					if (isMoving)
						drawRectangleTexture(playerHitbox, move_ptr2, movingRight);
					else
						drawRectangleTexture(playerHitbox, lvl2_staying, movingRight);
				else {
					drawRectangleTexture(playerHitbox, lvl2_jump, movingRight);
				}
			else
				if (playerHitbox.hasBottomCollision) {
					if (isMoving)
						drawRectangleTexture(playerHitbox, move_ptr3, movingRight);
					else
						drawRectangleTexture(playerHitbox, lvl3_staying, movingRight);
				}
				else {
					drawRectangleTexture(playerHitbox, lvl3_jump, movingRight);
				}
		}
		else
			drawRectangleTexture(playerHitbox, lvl1_over, true);
		glBindTexture(GL_TEXTURE_2D, 0);
		drawRectangle(flag_bottom);
		drawRectangle(flag_stick);
		glColor3f(0.0, 1.0, 0.0);
		drawRectangle(flag);
	}
	else {
		glClearColor(0.0, 0.0, 0.0, 0.0);
		if (keyStates[27]) exit(0);
		if (keyStates['r']) setup_mario_game();
		glBindTexture(GL_TEXTURE_2D, textures.getTexture(6));
		drawRectangleTexture(game_over, tex_coords, true);
	}

	glutSwapBuffers();
}

float timeElapsed = 0;
int frames = 0;
void Timer(int = 0) {
	glutPostRedisplay();
	/*timeElapsed += 1 / 60.0;
	frames++;
	if (timeElapsed >= 1) {
		cout << "FPS: " << frames / timeElapsed << endl;
		timeElapsed = 0;
		frames = 0;
	}*/
	glutTimerFunc(1000 / FPS, Timer, 0);
}

//RESHAPE
void customReshape(int width, int height) {
	ScreenWidth = width;
	ScreenHeight = height;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
}

//KEYBOARD
void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 27: //escape
		keyStates[27] = true;
		break;
	case 32: //spacebar
		keyStates[32] = true;
		break;
	case 'w':
		keyStates['w'] = true;
		break;
	case 'a':
		keyStates['a'] = true;
		break;
	case 's':
		keyStates['s'] = true;
		break;
	case 'd':
		keyStates['d'] = true;
		break;
	case 'z':
		keyStates['z'] = true;
		break;
	case 'r':
		keyStates['r'] = true;
		break;
	}
}

void keyboardUp(unsigned char key, int x, int y) {
	switch (key) {
	case 27:
		keyStates[27] = false;
		break;
	case 32:
		keyStates[32] = false;
		break;
	case 'w':
		keyStates['w'] = false;
		break;
	case 'a':
		keyStates['a'] = false;
		break;
	case 's':
		keyStates['s'] = false;
		break;
	case 'd':
		keyStates['d'] = false;
		break;
	case 'z':
		keyStates['z'] = false;
		break;
	case 'r':
		keyStates['r'] = false;
		break;
	}
}

void keyboardSpecial(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP:
		specialKeyStates[GLUT_KEY_UP] = true;
		break;
	case GLUT_KEY_DOWN:
		specialKeyStates[GLUT_KEY_DOWN] = true;
		break;
	}
}

void keyboardSpecialUp(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP:
		specialKeyStates[GLUT_KEY_UP] = false;
		break;
	case GLUT_KEY_DOWN:
		specialKeyStates[GLUT_KEY_DOWN] = false;
		break;
	}
}

//MOUSE
void mouseMove(int X, int Y) {
	mouse.x = X;
	mouse.y = Y;
}

void mousePress(int button, int state, int X, int Y) {
	switch (button) {
	case GLUT_LEFT_BUTTON:
		if (state == GLUT_DOWN) mouse.left = true;
		else mouse.left = false;
		break;
	case GLUT_RIGHT_BUTTON:
		if (state == GLUT_DOWN) mouse.right = true;
		else mouse.right = false;
		break;
	case GLUT_MIDDLE_BUTTON:
		if (state == GLUT_DOWN) mouse.middle = true;
		else mouse.middle = false;
		break;
	}
}

void mouseMoveAndPress(int X, int Y) {
	mouse.x = X;
	mouse.y = Y;
	/*stringstream ss;
	ss << "Mouse position [X: " << mouse.x << ", Y: " << mouse.y << "]";
	glutSetWindowTitle(ss.str().c_str());*/
}

void initialize() {
	textures.loadTexture2D("bricks.png", 1); //0
	textures.loadTexture2D("lucky_block.png", 1); //1
	textures.loadTexture2D("mushroom_buff.png", 1); //2
	textures.loadTexture2D("mushroom_enemy.png", 1); //3
	textures.loadTexture2D("mario.png", 1); //4
	textures.loadTexture2D("flower.png", 1); //5
	textures.loadTexture2D("game_over.png", 1); //6
	textures.loadTexture2D("turtle.png", 1); //7
	textures.loadTexture2D("pipe.png", 1); //8
	textures.loadTexture2D("coin.png", 1); //9
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.99);
	glClearColor(0.0, 0.7, 1.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(0.0, 800.0, 0.0, 800.0);
	glMatrixMode(GL_MODELVIEW);

	animation_begin = chrono::steady_clock::now();
	moving_begin = chrono::steady_clock::now();

	/*file.open("smb_coin.wav", ios::binary);
	file.seekg(0, ios::end);
	int size = file.tellg();
	audioBytes1 = new BYTE[size];
	file.seekg(0, ios::beg);
	file.read((char*)audioBytes1, size);
	file.close();*/

	ma_engine_init(NULL, &engine);
	ma_sound_init_from_file(&engine, "smb_coin.wav", 0, NULL, NULL, &coin_sound);
	ma_sound_init_from_file(&engine, "smb_breakblock.wav", 0, NULL, NULL, &break_sound);
	ma_sound_init_from_file(&engine, "smb_jump-super.wav", 0, NULL, NULL, &jump_sound);
	ma_sound_init_from_file(&engine, "smb_kick.wav", 0, NULL, NULL, &kick_sound);
	//ma_engine_play_sound(&engine, "01. Ground Theme.mp3", NULL);
	ma_sound_init_from_file(&engine, "01. Ground Theme.mp3", 0, NULL, NULL, &ground_theme);
	ma_sound_init_from_file(&engine, "smb_gameover.wav", 0, NULL, NULL, &over_sound);
	ma_sound_init_from_file(&engine, "smb_stage_clear.wav", 0, NULL, NULL, &stage_clear);
}

int main(int argc, char** argv) {
	////window creation
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(ScreenWidth, ScreenHeight);
	glutCreateWindow("Main Window");
	////initialization for options
	initialize();
	setup_mario_game();
	thread thrd(update_Thread);
	thrd.detach();
	////display and timer functions
	glutDisplayFunc(draw_func);
	glutTimerFunc(0, Timer, 0);
	////reshape
	glutReshapeFunc(customReshape);
	////keyboard
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);
	glutSpecialFunc(keyboardSpecial);
	glutSpecialUpFunc(keyboardSpecialUp);
	////Mouse
	glutPassiveMotionFunc(mouseMove);
	glutMouseFunc(mousePress);
	glutMotionFunc(mouseMoveAndPress);
	////main loop
	glutMainLoop();
}