/*
 * getoptions.h
 * Copyright 2015 Bob Parker <rlp1938@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#ifndef _GETOPTIONS_H
#define _GETOPTIONS_H
int opt;
/* declarations */

/* helpmsg */
char *helpmsg =
/* usage */
  "\n\tOptions:\n"
  "\t-h\n\toutputs this help message.\n"
/* options */
  ;

void dohelp(int forced);
void process_options(int argc, char **argv);

#endif
