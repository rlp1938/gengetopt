/* gengo.c
 *
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

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>
#include "fileops.h"
#include "firstrun.h"

void *memmem(const void *haystack, size_t haystacklen,
                    const void *needle, size_t needlelen);

char *helpmsg = "\n\tUsage:  gengo [option] -i option_string\n"
  "\t\tgengo [option] -g program_name\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n\n"
  "\t-i generate intermediate files from the user's answers to "
  "questions\n\t   about the options.\n\n"
  "\t-l ask for long option names and generate code to process them.\n"
  "\t   This is meaningful only with -i option.\n\n"
  "\t-g generate the main.c, getoptions.c and getoptions.h from the\n"
  "\t    above named txt files. Generate a Makefile to make"
  " program_name.\n"
  "\tThe next options are meaningful only with the -g option\n\n"
  "\t-c number of columns in display. Default is 80 columns.\n\n"
  "\t-s starting column for help text. Default is 12, ie 12 spaces"
  " from the \n\t   left.\n\n"
  "\t-u starting column for usage text. Default is 16, ie 2 tabs from"
  " the\n\t   left.\n\n"
  ;

char *getoptionsBP_C, *getoptionsBP_H, *mainBP_C;


static void dohelp(int forced);
static void getoptdata(char *useroptstring);
static void getmultilines(char *multi, char *display, unsigned maxlen,
			int wanteol);
static void getuserinput(const char *prompt, char *reply);
static void generatecode(const char *progname, int cols);
static void fatal(const char *msg);
static fdata fmtusagelines(const char *progname, char *from, char *to);
static fdata fmthelplines(char *from, char *to, int cols);
static fdata bracketsearch(char *from, char *to,
							const char *srchfrom, const char *fromemsg,
							const char *srchto, const char *toemsg);

int main(int argc, char **argv)
{
	int opt;

	// declare option flags, vars.
	int inter, longopts, gen, cols;

	// defaults.
	inter = longopts = gen = 0;
	cols = 80;

	char *pn = strdup(basename(argv[0]));
	if (checkfirstrun(pn) == -1) {
		fputs("firstrun\n", stdout);
		firstrun(pn, "getoptionsBP.c", "getoptionsBP.h", "mainBP.c",
					NULL);
	}

	// name the boiler plate files.
	{
		char buf[PATH_MAX];
		char *home = getenv("HOME");
		sprintf(buf, "%s/.config/%s/%s", home, pn, "getoptionsBP.c");
		getoptionsBP_C = strdup(buf);
		sprintf(buf, "%s/.config/%s/%s", home, pn, "getoptionsBP.h");
		getoptionsBP_H = strdup(buf);
		sprintf(buf, "%s/.config/%s/%s", home, pn, "mainBP.c");
		mainBP_C = strdup(buf);
	}
	free(pn);

	while((opt = getopt(argc, argv, ":hilgc:s:u:")) != -1) {
		switch(opt){
		case 'h':
			dohelp(0);
		break;
		case 'i': // intermediate files to be written.
			inter = 1;
		break;
		case 'l': // want long option names.
			longopts = 1;
		break;
		case 'g': // generate program files from intermediates.
			gen = 1;
		break;
		case 'c': // display columns.
			cols = strtol(optarg, NULL, 10);
			if (cols < 80) cols = 80;
			if (cols > 132) cols = 132;
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			dohelp(1);
		break;
		} //switch()
	}//while()

	// make sure that I have set inter or gen but not both.
	if ((inter == 0 && gen == 0) || (inter == 1 && gen == 1)) {
		fprintf(stderr,
		"\tYou must select -i or -g option but not both.\n");
		dohelp(EXIT_FAILURE);
	}

	// now process the non-option argument which must exist.

	if (inter == 1) {	// gathering options data
		if (!argv[optind]) {
			fputs("No options string provided.\n", stderr);
			dohelp(EXIT_FAILURE);
		}
		getoptdata(argv[optind]);
	} else {	// writing program files.
		if (!argv[optind]) {
			fputs("No program name provided.\n", stderr);
			dohelp(EXIT_FAILURE);
		}
		char *progname = strdup(argv[optind]);
		generatecode(progname, cols);
		free(progname);
	}

	free(mainBP_C);
	free(getoptionsBP_H);
	free(getoptionsBP_C);
	return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

void getoptdata(char *useroptstring)
{
	/* Creates work files from users provided data.*/
	char *optstringout;
	size_t len = strlen(useroptstring);
	unsigned idx;
	char *eols = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

	optstringout = malloc(len + 3);	// user input leading ":h" or not.

	if (strncmp(":h", useroptstring, 2) == 0) {
		strcpy(optstringout, useroptstring);
	} else {
		strcpy(optstringout, ":h");
		strcat(optstringout, useroptstring);
	}

	// open work files
	FILE *fphelp = dofopen("helpTXT.h", "w");	// .h to header file.
	FILE *fpdecl = dofopen("declTXT.h", "w");
	FILE *fpdeflt = dofopen("defltTXT.c", "w");	// .c to C file.
	FILE *fpcode = dofopen("codeTXT.c", "w");

	// Must initialise some stuff independently of user later input.
	fputs("char *optstring;\n", fpdecl);
	fprintf(fpdeflt, "\toptstring = \"%s\";\n", optstringout);
	fputs("\n/* helptext */\n", fphelp);

	len = strlen(optstringout);

	for (idx = 2; idx < len; idx++)  {	// ignore ":h"
		unsigned char c = optstringout[idx];
		if (c == ':') continue;	// user is responsible for any optarg.
		char outbuf[PATH_MAX];
		char namebuf[NAME_MAX];
		char typebuf[NAME_MAX];
		char defltbuf[NAME_MAX];
		char codebuf[NAME_MAX];
		char codeopt[4];	// '%c' for alpha, %c for numeric.
		if (!isalnum(c)) {
			fprintf(stderr, "Option char: %c is not in [a-zA-Z0-9]\n"
					, c);
			exit(EXIT_FAILURE);
		}
		if (isalpha(c)) {
			sprintf(codeopt, "'%c'", c);
		} else {
			sprintf(codeopt, "%c", c);
		}
		fputs(eols, stdout);	// clear terminal window
		fprintf(stdout, "For option: %c\n", c);
		fputs("For this option will you:\n"
		"Use a single variable to which you assign a value (1)\n"
		"Use a single char array and strcpy optarg into it (2)\n"
		"Or do something else, including affecting several variables\n"
		"when the option is selected (3).\n", stdout);
		char ans = 0;
		char ans_str[3];
		while (ans == 0) {
			fputs("Enter your choice 1..3: ", stdout);
			fgets(ans_str, 2, stdin);
			ans = ans_str[0];
			if (ans < '1' || ans > '3') ans = 0;
		}

		if (ans < '3') {
			// Option variable name.
			getuserinput("Enter variable name: ", namebuf);
			// Option variable type.
			getuserinput("Enter variable type: ", typebuf);
			sprintf(outbuf, "%s %s;\n", typebuf, namebuf);
			fputs(outbuf, fpdecl);
		} else {
			fprintf(fpdecl, "/* Enter type(s) and name(s) for"
			" variable(s) affected by selecting option %c */\n", c);
		}
		// Option default value.
		if (ans == '1') {
			getuserinput("Enter variable default value: ", defltbuf);
			sprintf(outbuf, "\t%s = %s;\n", namebuf, defltbuf);
		} else if (ans == '2'){
			sprintf(outbuf, "\t%s[0] = 0;\n", namebuf);
		} else {	// ans == 3
			sprintf(outbuf, "/* Enter names and default values "
			"for any/all variables affected by selecting option %c */\n"
			, c);
		}
		fputs(outbuf, fpdeflt);

		// Option helptext
		getmultilines(outbuf, "help text", PATH_MAX, 1);
		fprintf(fphelp, "-%c\n", c);
		fputs(outbuf, fphelp);
		fputs("\n", fphelp);	/* empty line marks end of help text
								   for this option. */
		// Option C code.
		if (ans == '1') {
			char *prompt = "Starting with an assignment operator,"
			" enter the C code to set the option\n variable.";
			getmultilines(codebuf, prompt, NAME_MAX, 0);
			sprintf(outbuf, "\t\tcase %s:\n\t\t%s %s;\n\t\tbreak;\n",
					codeopt, namebuf, codebuf);
		} else if (ans == '2') {
			sprintf(outbuf, "\t\tcase %s:\n\t\tstrcpy(%s, %s);"
			"\n\t\tbreak;"
			"\n", codeopt, namebuf, "optarg");
		} else {	// ans == '3'
			sprintf(outbuf, "\t\tcase %s:\n\t\t%s\n\t\tbreak;\n",
			 codeopt,
			"/* Enter the C code required to run when this option"
			" is selected. */");
		}
		fputs(outbuf, fpcode);
	} // for(idx ...)

	fclose(fpcode);
	fclose(fpdeflt);
	fclose(fpdecl);

	// Usage strings.
	fputs("/* usage */\n", fphelp);
	fputs("progname ", fphelp);
	char usagebuf[NAME_MAX];
	/* the literal "progname" will be replaced by the actual program
	 * name at program generation time. This name is not known now. */
	char *prompt = "Enter usage text";
	getmultilines(usagebuf, prompt, NAME_MAX, 1);
	fputs(usagebuf, fphelp);
	fputs("\n", fphelp);	// empty line marks end of usage lines.
	fclose(fphelp);

	free(optstringout);

	// non-option arguments.
	fputs(eols, stdout);
	char noabuf[NAME_MAX];
	prompt = "how many non-option arguments are to be input, 0..n? ";
	getuserinput(prompt, noabuf);
	int noargs = strtol(noabuf, NULL, 10);
	if (noargs == 0) return;	// done
	FILE *fpnoarg = fopen("noargsTXT.c", "w");
	fputs("\t//Non-option arguments\n", fpnoarg);
	int i;
	for (i=0; i < noargs; i++) {
		// Does the argv[optind] exist?
		char ans = 0;
		fputs("\tif(!argv[optind]) {\n", fpnoarg);
		while (ans == 0) {
			char * prompt = "What kind of object is required?\n"
			"Dir(1), File(2), String(3) or something else(4)\n";
			getuserinput(prompt, noabuf);
			ans = noabuf[0];
			if (ans > '4' || ans < '0') ans = 0;
		}
		char *rqd[] = {
			"dir",
			"file",
			"string",
			"/* Insert name of required object here. */"
		};
		char *fmt = "\t\tfputs(\"No %s provided.\", stderr);\n";
		fprintf(fpnoarg, fmt, rqd[ans - 49]);
		/* 48 difference between '1' and 1, and then the list (rqd) is
		 * zero based so deduct another 1. */
		fputs("\t\tdohelp(1);\n", fpnoarg);
		fputs("\t};\n", fpnoarg);
		fputs("\toptind++;\n", fpnoarg);
	}
	fclose(fpnoarg);
} // getoptdata()

void getmultilines(char *multi, char *display, unsigned maxlen,
					int wanteol)
{	/* Inform user using text at display and return many lines '\n'
	separated in multi. */

	/* Using the C library functions (man 3 ...) gives me unwanted
	 * optimisations that end up with output prompts and inputs all
	 * over the place. Using system calls (man 2 ...) allows me to
	 * control the order of prompts and reads as I need them to be.
	 * */
	char result[PATH_MAX], work[NAME_MAX+1], longprompt[NAME_MAX];
	char *fmt = "Input %s.\nUse as many lines as required. An empty"
	" line ends input.\nInput will be truncated at %d characters.\n";
	sprintf(longprompt, fmt, display, maxlen);
	write(1, longprompt, strlen(longprompt));
	result[0] = 0;
	while(1) {
		read(0, work, NAME_MAX);
		char *cp = strchr(work, '\n');
		if (cp) *cp = '\0';
		if (strlen(work) == 0) break;
		if (wanteol) strcat(work, "\n");	// terminate input with EOL
		strcat(result, work);
		if (strlen(result) >= maxlen) {
			result[maxlen - 2] = '\n';
			result[maxlen - 1] = '\0';
			break;
		}
	}
	strcpy(multi, result);
} // getmultilines()

void getuserinput(const char *prompt, char *reply)
{	/* This routine does avoid the utterly unwanted optimisations that
	 * get the requests and replies in all the wrong order.
	 * That is when using fgets/fputs.
	*/
	char buf[NAME_MAX];
	sprintf(buf, "%s", prompt);
	write(1, buf, strlen(buf));
	read(0, buf, NAME_MAX);
	char *cp = strchr(buf, '\n');
	if (cp) *cp = '\0';
	strcpy(reply, buf);
} // getuserinput()

void generatecode(const char *progname, int cols)
{	/* Writes the files getoptions.h, getoptions.c and main.c
	 * Source files are boilerplate, getoptionsBP.h, getoptionsBP.c
	 * and mainBP.c located in $HOME/.config/genco/boilerplate/
	 * and the the purpose written noargsTXT.c. helpTXT.h, codeTXT.c
	 * declTXT.h and defltTXT.c
	*/
	// 1. generate main.c
	fdata bpdat = readfile(mainBP_C, 0, 1);
	// a) write the preamble.
	char *sft = NULL;
	char *sfem = NULL;
	char *stt = "\n/* non-option arguments */";
	char *stem = "Corrupt mainBP.c, '/*non-option arguments*/' not found.";

	fdata bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt,
									stem);
	writefile("main.c", bppart.from, bppart.to, "w");
	// b) append non-option argument processing
	fdata wfdat = readfile("noargsTXT.c", 0, 1);
	writefile("main.c", wfdat.from, wfdat.to, "a");
	free(wfdat.from);
	// c) append the rest of main.c
	sft = "\n/* non-option arguments */";
	sfem = "no emesg needed.";
	stt = NULL;
	stem = NULL;
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("main.c", bppart.from, bppart.to, "a");
	free(bpdat.from);

	// 2. generate getoptions.h
	bpdat = readfile(getoptionsBP_H, 0, 1);
	// a) write the preamble.
	sft = NULL;
	sfem = NULL;
	stt = "\n/* declarations */";
	stem = "Corrupt getoptionsBP.c, '/* declarations */' not found.";
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.h", bppart.from, bppart.to, "w");
	sync();
	// b) append the user's variable declarations.
	wfdat = readfile("declTXT.h", 0, 1);
	writefile("getoptions.h", wfdat.from, wfdat.to, "a");
	free(wfdat.from);
	sync();
	// c) append part BP file
	sft = "\n/* helpmsg */";
	sfem = "Corrupt getoptionsBP.c, '* helpmsg */' not found.";
	stt = "\n/* usage */";
	stem = "Corrupt getoptionsBP.c, '/* usage */' not found.";
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.h", bppart.from, bppart.to, "a");
	sync();
	// d) append the help text.
	wfdat = readfile("helpTXT.h", 0, 1);
	// d.1) append the usage lines
	// TODO: change fmtusagelines to deal with only lines that come
	// after /* usage */ and use bracketsearch() to describe that scope.
	sft = "\n/* usage */";
	sfem = "not needed.";
	stt = NULL;
	stem = NULL;
	bppart = bracketsearch(wfdat.from, wfdat.to, sft, sfem, stt, stem);
	bppart = fmtusagelines(progname, bppart.from, bppart.to);
	writefile("getoptions.h", bppart.from, bppart.to, "a");
	free(bppart.from);
	sync();
	// d.2) append the common options help lines.
	sft = "\n/* usage */";
	sfem = "emsg not needed";
	stt = "\n/* options */";
	stem = "Corrupt getoptionsBP.c, '/* options */' not found.";
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.h", bppart.from, bppart.to, "a");
	sync();
	// d.3) append the user's options help lines.
	sft = "\n/* helptext */";
	sfem = "Corrupt helpTXT.h, '/* helptext */' not found.";
	stt = "\n/* usage */";
	stem = "Corrupt helpTXT.h, '/* usage */' not found.";
	bppart = bracketsearch(wfdat.from, wfdat.to, sft, sfem, stt, stem);
	fdata hldat = fmthelplines(bppart.from, bppart.to, cols);
	writefile("getoptions.h", hldat.from, hldat.to, "a");
	free(hldat.from);
	free(wfdat.from);
	// e) append the tail end of the BP file.
	sft = "\n/* options */";
	sfem = "emsg not needed";
	stt = NULL;
	stem = NULL;
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.h", bppart.from, bppart.to, "a");
	free(bpdat.from);

	// 3. write getoptions.c
	bpdat = readfile(getoptionsBP_C, 0, 1);
	// a) write the preamble.
	sft = NULL;
	sfem = NULL;
	stt = "\n/* defaults */";
	stem = "Corrupt getoptionsBP.c, '/* defaults */' not found.";
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.c", bppart.from, bppart.to, "w");
	// b) append defaults initialisation.
	bppart = readfile("defltTXT.c", 0, 1);
	writefile("getoptions.c", bppart.from, bppart.to, "a");
	free(bppart.from);
	// c) append the top of the options while loop.
	sft = "\n/* defaults */";
	sfem = "no emesg";	// search string exists
	stt = "\n/* options */";
	stem = "Corrupt getoptionsBP.c, '/* options */' not found.";
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.c", bppart.from, bppart.to, "a");
	// d) append the user's option processing.
	bppart = readfile("codeTXT.c", 0, 1);
	writefile("getoptions.c", bppart.from, bppart.to, "a");
	free(bppart.from);
	// e) append the remainder of the BP file
	sft = "\n/* options */";
	sfem = "no emesg";	// search string exists
	stt = NULL;
	stem = NULL;
	bppart = bracketsearch(bpdat.from, bpdat.to, sft, sfem, stt, stem);
	writefile("getoptions.c", bppart.from, bppart.to, "a");
	free(bpdat.from);
} // generatecode()

void fatal(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

fdata fmtusagelines(const char *progname, char *from, char *to)
{	/*
	* This takes the user provided usage lines and formats them thus:
	* Usage: progname -i [option] arg1 arg2 ...
	*        progname -g [option] some_other_arg ...
	*/
	char retbuf[PATH_MAX];
	char work[NAME_MAX];
	fdata ret;
	char *bol = from;
	char *fmt = "  \"\\tUsage: %s %s\\n\"\n";
	retbuf[0] = '\0';
	char *eol = memchr(bol, '\n', to - bol);
	if (!eol) fatal("Corrupt helpTXT.h, no '\n' found.");
	*eol = '\0';
	size_t sl;
	do {
		char *cp = strstr(bol, "progname");
		if (cp) bol += strlen("progname") + 1;
		sprintf(work, fmt, progname, bol);
		strcat(retbuf, work);
		fmt = "  \"\\t       %s %s\\n\"\n";
		sl = strlen(bol);
		bol += sl + 1;
		eol = memchr(bol, '\n', to - bol);
		if (!eol) break;
		*eol = '\0';
		sl = strlen(bol);
	} while (sl);
	strcat(retbuf, "\n");	// empty line after usage lines.
	size_t len = strlen(retbuf);
	ret.from = malloc(len);
	memcpy(ret.from, retbuf, len);
	ret.to = ret.from + len;
	return ret;	// caller is required to free ret.from
} // fmtusagelines()

fdata fmthelplines(char *from, char *to, int cols)
{	/*
	 * formats each option and lines following like this:
	 *  "\t-x[, --longx] Lorem ipsum dolor sit amet, consetetur \n"
	 *  "\t              elitr, sed diam nonumyeirmod tempor invidunt\n"
	 *  "\t              ut labore et dolore kimata sanctus est Lorem\n"
	 *  "\t              ipsum dolor sit amet.Lorem ipsum dolor sit\n"
	 *  "\t              amet, consete\n"
	*/
	char resbuf[PATH_MAX];
	resbuf[0] = '\0';
	size_t tabsize = 8;

	// the scope of the search

	resbuf[0] = '\0';	// will concatenate the options text here
	fdata opthelp;
	opthelp.from = from; 	// the starting point.
	while (1) {
		// the scope of each option and associated help lines.
		opthelp.from = memmem(opthelp.from, to - opthelp.from,
								"\n-", 2);	/* look for eol followed by
											 -something */
		if (!opthelp.from) break;	// done all options and help text.
		opthelp.from++;	// now pointing at the actual option
		opthelp.to = memmem(opthelp.from, to - opthelp.from,
							"\n-", 2);
		if (!opthelp.to) opthelp.to = to;	// now at last option.
		// make a copy of the user provided options mess.
		char *optmess = malloc(opthelp.to - opthelp.from);
		char *omend = optmess + (opthelp.to - opthelp.from);
		memcpy(optmess, opthelp.from, opthelp.to - opthelp.from);
		// now make C strings of this stuff.
		char *cp = optmess;
		while (cp < omend) {
			if (*cp == '\n') *cp = '\0';
			cp++;
		}
		/*now process the the actual option name(s):
		 * -<ch> || -<ch>, --<string> || --<string> */
		cp = optmess;
		char *ofmt = "  \"\\t%s\\n\"\n";
		char line[NAME_MAX];
		sprintf(line, ofmt, cp);
		strcat(resbuf, line);
		// format the actual help lines to suit available width.
		// First gather all the help lines into 1 long line.
		cp += strlen(cp) + 1;	// get past option name(s).
		char longline[NAME_MAX + 8];
		size_t lineidx = 0;
		while (cp < omend) {
			if (*cp == '\0') {
				longline[lineidx] = ' ';
			}  else {
				longline[lineidx] = *cp;
			}
			cp++;
			lineidx++;
			if (lineidx >= NAME_MAX + 8) break;	// no buffer overflow
		}
		longline[lineidx] = '\0';
		// set an arbitrary limit on the length of this stuff.
		if (strlen(longline) > NAME_MAX) longline[NAME_MAX] = '\0';
		// now break up this long line into pieces to suit the width.
		size_t wid = cols - tabsize;
		cp = longline;
		char buf[NAME_MAX];
		while (strlen(cp) > wid) {
			char *limit = cp + strlen(cp);
			char *target = cp + wid;
			while(*target != ' ') target--;
			*target = '\0';
			// Excessive string lengths will silently dissapear.
			if (strlen(resbuf) + strlen(cp) < PATH_MAX - 5) {
				sprintf(buf, ofmt, cp);
				strcat(resbuf, buf);
			}
			cp += strlen(cp) + 1;
			if (cp > limit) break;
		}
		if (strlen(cp)) {
			if (strlen(resbuf) + strlen(cp) < PATH_MAX - 5) {
				sprintf(buf, ofmt, cp);
				strcat(resbuf, buf);
			}
		}
		free(optmess);
		// next option if any
		opthelp.from = memmem(opthelp.from, to - opthelp.from,
								"\n-", 2);	/* look for eol followed by
											 -something */
		if (!opthelp.from) break;	// done
	}
	fdata retdat;
	size_t len = strlen(resbuf);
	retdat.from = malloc(len);
	memcpy(retdat.from, resbuf, len);
	retdat.to = retdat.from + len;
	return retdat;
} // fmthelplines()

fdata bracketsearch(char *from, char *to,
					const char *srchfrom, const char *fromemsg,
					const char *srchto, const char *toemsg)
{
	/* Search in memory between from and to for the strings srchfrom
	 * and/or srchto. The error messages fromemsg and/or toemsg
	 * will be produced if the searched for strings are not found.
	 * Such not found condition is fatal.
	 * Either srchfrom or srchto may be NULL and in such case the
	 * corresponding emsg must be NULL also. If a search string exists
	 * then there must be a non-NULL emsg. Any mismatch between a
	 * search string NULL state and its emsg is fatal.
	 *
	 * Search strings may be prefixed with '\n' to ensure that the
	 * rest of the search target begins on a new line. A from target
	 * points to data on the line after the search target but a to
	 * target points to such target unless it's prefixed with '\n'. In
	 * such case the result points to the next line.
	*/
	if (!srchfrom && fromemsg) fatal ("From error message exists"
										" but search from does not.");
	if (srchfrom && !fromemsg) fatal ("Search from exists but from"
										" error message does not.");
	if (!srchto && toemsg) fatal ("To error message exists"
										" but search to does not.");
	if (srchto && !toemsg) fatal ("Search to exists but to"
										" error message does not.");
	fdata result;
	if (srchfrom) {
		result.from = memmem(from, to - from, srchfrom,
								strlen(srchfrom));
		if (!result.from) fatal(fromemsg);
		result.from += strlen(srchfrom) + 1;
	} else {
		result.from = from;
	}
	if (srchto) {
		result.to = memmem(from, to - from, srchto, strlen(srchto));
		if (!result.to) fatal(toemsg);
		if (srchto[0] == '\n') {
			result.to++;
		}
	} else {
		result.to = to;
	}
	return result;
} // bracketsearch()
