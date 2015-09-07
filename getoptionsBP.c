//<preamble>
/*
 * getoptions.c
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

#include "getoptions.h"

static const char helpmsg[] =
//</preamble>
//<fixedoptions>
  "\n\tOptions:\n"
  "\t-h, --help\n\tDisplays this help message, then quits.\n"
//</fixedoptions>

//<endoptions>
  ;

options_t
process_options(int argc, char **argv)
{

//</endoptions>
//<golongwshortpre>

	int opt;

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0,	0,	'h' },
//</golongwshortpre>
//<golongwshortpost>
			{0,	0,	0,	0 }
		};

		opt = getopt_long(argc, argv, optstr, long_options,
							&option_index);
		if (opt == -1)
			break;
//</golongwshortpost>
//<glongonlypre>
		switch (opt) {
			case 0:
				switch (option_index) {
//</glongonlypre>
//<glongonlypost>
				} // switch(option_index)
				break;
//</glongonlypost>
//<glshortspre>
			case 'h':
				dohelp(0);
				break;
//</glshortspre>
//<glshortspost>
			case ':':
				fprintf(stderr, "Option %s requires an argument\n",
							argv[this_option_optind]);
				dohelp(1);
				break;
			case '?':
				fprintf(stderr, "Unknown option: %s\n",
								argv[this_option_optind]);
				dohelp(1);
				break;
		}

	} // while(1)
	return opts;
} // process_options()
//</glshortspost>
//<tail>

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}
//</tail>
