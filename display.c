/* Display manipulating module

Copyright (C) 2016 Arnie97

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <stdint.h>
#include <syscall.h>
#include <hpstring.h>
#include "s3c2410.h"
#include "display.h"


// {rom_address, code_point}
static const size_t offset[][2] = {
	{0x7FD10, 0},
	{0xBF640, 110},
	{0xE2920, 94 * 4},
	{0xE2BD0, 94 * 5},
	{0xE2C90, 502},
	{0xE2D50, 94 * 6},
	{0xE2E60, 94 * 7},
	{0xDEEA0, 612},
	{0xBFE50, 694},
	{0x3ABF0, 94 * 15},
	{0xF7020, 94 * 15 + 0x5400 / BYTES_PER_GLYPH},
};


void *
get_bitmap_font(const uint8_t **bytes)
{
	int page = 0[*bytes] - 0xA0;  // 区码
	int id   = 1[*bytes] - 0xA0;  // 位码
	if (page < 0) {               // ISO/IEC 2022 G0区
	    page = 3;                 // 用 GB 2312-1980 第3区中
	    id = 0[*bytes] - ' ';     // 相应的全角字符代替半角字符
	    *bytes += 1;              // 半角字符，指针移动一字节
	} else {                      // ISO/IEC 2022 GR区
	    *bytes += 2;              // 全角字符，指针移动两字节
	}

	size_t code_point = ((page - 1) * 94 + (id - 1));
	for (size_t i = sizeof(offset) / sizeof(*offset); i >= 0; i--) {
		if (code_point >= offset[i][1]) {
			code_point -= offset[i][1];
			code_point *= BYTES_PER_GLYPH;
			code_point += offset[i][0];
			return (void *)code_point;
		}
	}
}


int
font_not_found(void)
{
	if (*MAGIC == 0xC0DEBA5E) {
		return 0;
	}
	const uint8_t *msg = (
		"\x08\xa1\x40\x01\x82\x12\x08\x00\x20\x80\x00"
		"\xd0\xa7\xcf\xcf\x3f\x7f\x08\x10\xfc\xf3\x87"
		"\x00\x51\x2a\x41\xa0\x12\x7f\xfe\x04\x92\x80"
		"\xd8\x27\x07\x01\x82\x7f\x49\x44\xf8\xf0\x87"
		"\x90\xa2\xea\xdf\x3f\x04\x49\x28\x40\x50\x81"
		"\x90\x43\x80\x02\x09\x2b\x7f\x10\xfc\xf3\x87"
		"\xb0\xa2\x4a\x12\x86\x12\x08\x28\x20\x10\x01"
		"\x90\xd2\x2b\x9e\x19\x66\x08\xc6\x30\x08\x81"
	);
	for (int row = 0; row < 8; row++) {
		memcpy(&__display_buf[(row + 18) * BYTES_PER_ROW], &msg[row * 11], 11);
	}
	return 1;
}


const char *
bitmap_blit(const char *text)
{
	SysCall(ClearLcdEntry);
	if (font_not_found()) {
		return text;
	} else if (*text == '\n') {
		text++;  // omit line breaks between pages
	}
	int x = LEFT_MARGIN, y = TOP_MARGIN;
	while (*text) {
		if (*text == '\n') {
			text++;
			x = SCREEN_WIDTH;
			y += ROWS;
			goto next;
		}
		uint8_t pos = 7, *ptr = get_bitmap_font((const uint8_t **)&text);
		for (size_t row = 0; row < ROWS; row++, y++) {
			for (size_t col = 0; col < COLS_STORAGE; col++, x++) {
				__display_buf[y * BYTES_PER_ROW + (x >> 3)] |= ((*ptr >> pos) & 1) << (x & 7);
				if (pos) {
					pos--;
				} else {
					pos = 7;
					ptr++;
				}
			}
			x -= COLS_STORAGE; }

		next: if (x + COLS_REAL <= SCREEN_WIDTH - COLS_REAL) {  // next char
			x += COLS_REAL;
			y -= ROWS;
		} else if (y + LINE_SPACING + ROWS <= SCREEN_HEIGHT) {  // next line
			x = LEFT_MARGIN;
			y += LINE_SPACING;
		} else {  // next page
			break;
		}
	}
	set_indicator(INDICATOR_RSHIFT, *text);
	return text;
}
