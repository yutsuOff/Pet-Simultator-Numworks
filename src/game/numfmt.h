#ifndef GAME_NUMFMT_H
#define GAME_NUMFMT_H

/* Readable formatting of large amounts: 999, 1.23K, 45.6M, 789B, 1.00T, 2.5Qa...
 * Values are truncated (never rounded up), so the display never shows more than
 * the player actually has. Beyond the last suffix, scientific notation is used. */

/* Writes into buf (at least 16 bytes). Returns buf. */
char *numfmt(double value, char *buf);

#endif
