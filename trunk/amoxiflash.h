/*
amoxiflash -- NAND Flash chip programmer utility, using the Infectus 1 / 2 chip
Copyright (C) 2008  bushing

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

For more information, contact bushing@gmail.com, or see http://code.google.com/p/amoxiflash
*/

typedef unsigned char u8;
typedef unsigned int u32;

#define ECC_OK 0
#define ECC_WRONG 1
#define ECC_INVALID 2
#define ECC_BLANK 3

typedef unsigned long long int u64;

u8 * calc_page_ecc(u8 *data);
int check_ecc(u8 *page);

