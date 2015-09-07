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
#include "fileops.h"

static const char helpmsg[] =
"\tUsage: gengo \n"

"\n\tOptions:\n"
"\t-h, --help\n\tDisplays this help message, then quits.\n"
"\t-i, --interactive\n"
"\tgenerate intermediate files from the user's answers to questions"
" about\n\tthe options. \n"
"\t-g, --generate\n"
"\tgenerate the main.c, getoptions.c and getoptions.h from the\n "
"\tintermediate text files. "
"\t  Generate a Makefile to make program_name. \n"
"\t-c, --columns\n"
"\tnumber of columns in display. Default is 80 columns. Number input"
" must\n"
"\tbe in range 72..132. If outside range the number will be adjusted"
" to the\n"
"\tsmaller or larger number of this range. \n"
"\t-d, --delete\n"
"\tdeletes any workfiles found in the current directory. \n"
;


options_t
process_options(int argc, char **argv)
{
	static const char optstr[] = ":higc:d";

	options_t opts = { 0 };
	opts.cols = 80;

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0,	0,	'h' },
			{"interactive",	0,	0,	'i'},
			{"generate",	0,	0,	'g'},
			{"columns",	1,	0,	'c'},
			{"delete",	0,	0,	'd'},
			{0,	0,	0,	0 }
		};

		int opt = getopt_long(argc, argv, optstr, long_options,
							&option_index);
		if (opt == -1)
			break;
		switch (opt) {
			case 0:
				switch (option_index) {
				} // switch(option_index)
				break;
			case 'h':
				dohelp(0);
				break;
			case 'i':
				opts.inter = 1;
				break;
			case 'g':
				opts.gen = 1;
				break;
			case 'c':
				opts.cols = strtol(optarg, NULL, 10);
				break;
			case 'd':
				if (fileexists("helpTXT.c") == 0) unlink("helpTXT.c");
				if (fileexists("usageTXT.c") == 0) unlink("usageTXT.c");
				if (fileexists("declTXT.h") == 0) unlink("declTXT.h");
				if (fileexists("defltTXT.c") == 0) unlink("defltTXT.c");
				if (fileexists("socodeTXT.c") == 0)
					unlink("socodeTXT.c");
				if (fileexists("locodeTXT.c") == 0)
					unlink("locodeTXT.c");
				if (fileexists("lostructTXT.c") == 0)
					unlink("lostructTXT.c");
				if (fileexists("noargsTXT.c") == 0)
					unlink("noargsTXT.c");
				exit(EXIT_SUCCESS);
				break;
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
		} // switch(opt)
	} // while(1)

	return opts;
} // process_options()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}
