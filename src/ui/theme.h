#ifndef UI_THEME_H
#define UI_THEME_H

#include "../gfx/gfx.h"

/* Shared palette: every screen uses these so the whole game stays consistent. */
#define COL_WHITE       RGB(255, 255, 255)
#define COL_BAR         RGB(28, 36, 72)    /* top and tab bars */
#define COL_BAR_LIGHT   RGB(48, 62, 118)
#define COL_OUTLINE     RGB(30, 40, 80)    /* thick outline of cards and panels */
#define COL_PANEL_TOP   RGB(255, 255, 255)
#define COL_PANEL_BOT   RGB(222, 232, 250)
#define COL_TEXT        RGB(30, 36, 64)
#define COL_TEXT_DIM    RGB(110, 120, 150)
#define COL_SHADOW      RGB(10, 14, 34)
#define COL_GOLD        RGB(255, 200, 40)
#define COL_GOLD_DARK   RGB(200, 130, 0)
#define COL_GEM         RGB(60, 180, 255)
#define COL_BLUE        RGB(50, 130, 250)
#define COL_BLUE_DARK   RGB(25, 80, 200)
#define COL_GREEN       RGB(70, 205, 90)
#define COL_GREEN_DARK  RGB(30, 140, 50)
#define COL_RED         RGB(240, 70, 70)
#define COL_HP_BACK     RGB(60, 66, 96)
#define COL_LOCKED      RGB(150, 155, 170)

/* Rarity colors (PS99 style). */
#define COL_RARITY_BASIC     RGB(170, 175, 185)
#define COL_RARITY_RARE      RGB(60, 150, 255)
#define COL_RARITY_EPIC      RGB(170, 80, 255)
#define COL_RARITY_LEGENDARY RGB(255, 150, 20)
#define COL_RARITY_MYTHICAL  RGB(255, 50, 80)
#define COL_RARITY_SECRET    RGB(20, 20, 20)

/* Layout */
#define TOPBAR_H 22
#define TABBAR_H 24
#define CONTENT_Y TOPBAR_H
#define CONTENT_H (240 - TOPBAR_H - TABBAR_H)

#endif
