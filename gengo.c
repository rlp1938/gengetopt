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

typedef struct tagpair {
	char *opntag;
	char *clstag;
} tagpair;

char *getoptionsBP_C, *getoptionsBP_H, *mainBP_C, *MakefileBP_;

static void dohelp(int forced);
static void getoptdata(char *useroptstring);
static void getmultilines(char *multi, char *display, unsigned maxlen,
			int wanteol);
static void getuserinput(const char *prompt, char *reply);
static void generatecode(const char *progname, int cols);
static void fatal(const char *msg);
static fdata fmtusagelines(const char *progname, char *from, char *to);
static fdata fmthelplines(char *from, char *to, int cols);
static fdata bracketsearch(char *from, char *to, char *opn, char *cls);
static tagpair maketags(char *tagname);
static void boilerplateinit(const char *bpfilename,
							const char *targetfilename,
							char *tagname);
static void boilerplateappend(const char *targetfilename,
								char *tagname);
static void boilerplatedeinit(void);
static void appenduserfile(const char *userfilename,
							const char *targetfilename);

int main(int argc, char **argv)
{
	int opt;

	// declare option flags, vars.
	int inter, gen, cols;

	// defaults.
	inter = gen = 0;
	cols = 80;

	char *pn = strdup(basename(argv[0]));
	if (checkfirstrun(pn) == -1) {
		fputs("firstrun\n", stdout);
		firstrun(pn, "getoptionsBP.c", "getoptionsBP.h", "mainBP.c",
					"MakefileBP", NULL);
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
		sprintf(buf, "%s/.config/%s/%s", home, pn, "MakefileBP");
		MakefileBP_ = strdup(buf);
	}
	free(pn);

	while((opt = getopt(argc, argv, ":higc:d")) != -1) {
		switch(opt){
		case 'h':
			dohelp(0);
		break;
		case 'd':	// delete workfiles
			if (fileexists("helpTXT.c") == 0) unlink("helpTXT.c");
			if (fileexists("usageTXT.c") == 0) unlink("usageTXT.c");
			if (fileexists("declTXT.h") == 0) unlink("declTXT.h");
			if (fileexists("defltTXT.c") == 0) unlink("defltTXT.c");
			if (fileexists("socodeTXT.c") == 0) unlink("socodeTXT.c");
			if (fileexists("locodeTXT.c") == 0) unlink("locodeTXT.c");
			if (fileexists("lostructTXT.c") == 0)
					unlink("lostructTXT.c");
			if (fileexists("noargsTXT.c") == 0) unlink("noargsTXT.c");
			exit(EXIT_SUCCESS);
		break;
		case 'i': // intermediate files to be written.
			inter = 1;
		break;
		case 'g': // generate program files from intermediates.
			gen = 1;
		break;
		case 'c': // display columns.
			cols = strtol(optarg, NULL, 10);
			if (cols < 72) cols = 72;
			if (cols > 132) cols = 132;
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Unknown option: %c\n",optopt);
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

	// open work files, next 4 always wanted
	FILE *fphelp = dofopen("helpTXT.c", "w");
	FILE *fpusage = dofopen("usageTXT.c", "w");
	FILE *fpdeflt = dofopen("defltTXT.c", "w");

	// next 3 work files opended conditionally
	FILE *fpsocode = NULL;
	FILE *fplocode = NULL;
	FILE *fplostruct = NULL;
	FILE *fpdecl = NULL;
	/*
	FILE *fpsocode = dofopen("socodeTXT.c", "w");
	FILE *fplocode = dofopen("locodeTXT.c", "w");
	FILE *fplostruct = dofopen("lostructTXT.c", "w");
	*/
	// Must initialise some stuff independently of user later input.
	fprintf(fpdeflt, "\toptstr = \"%s\";\n", optstringout);
	fputs("\n/* helptext */\n\n", fphelp);

	len = strlen(optstringout);

	int loidx = 1;	/* 0 has already been consumed by
					{"help", 0, 0, 'h'}, */
	// formats
	char *declfmt = "%s %s;\n";
	char *defltfmt = "\t%s = %s;";
	char *codefmt = "\t\tcase\'%c\':\n\t\t\t%s;\n\t\t\tbreak;\n";
	// result buffers
	char namebuf[NAME_MAX];
	char typebuf[NAME_MAX];
	char defltbuf[NAME_MAX];
	char codebuf[NAME_MAX];
	char helpbuf[NAME_MAX];
	char loname[NAME_MAX];
	char displayopt[80];

	for (idx = 2; idx < len; idx++)  {	// ignore ":h"
		unsigned char c = optstringout[idx];
		if (!fpsocode) fpsocode = dofopen("socodeTXT.c", "w");
		if (c == ':') continue;	// user is responsible for any optarg.
		if (!isalnum(c)) {
			fprintf(stderr, "Option char: %c is not in [a-zA-Z0-9]\n"
					, c);
			exit(EXIT_FAILURE);
		}
		fputs(eols, stdout);	// clear terminal window
		char header[80];
		sprintf(header, "Processing option: %c\n", c);
		dowrite(1, header);
		char *prompt = "For this option will you:\n"
		"Use a single variable to which you assign a value (1)\n"
		"Use a char pointer and strdup optarg onto it (2)\n"
		"Or do something else, possibly affecting several variables\n"
		"when the option is selected (3).\n";
		char ans = getans(prompt, "123");
		switch (ans) {
			case '1':
				// Option variable name.
				getuserinput("Enter variable name: ", namebuf);
				// Option variable type.
				getuserinput("Enter variable type: ", typebuf);
				// Option default value.
				getuserinput("Enter variable default value: ",
								defltbuf);
				// Option C code.
				getuserinput("Begining with an assignment operator,\n"
							"enter C code for this option: ",
							codebuf);
				break;
			case '2':
				// Option variable name.
				getuserinput("Enter variable name: ", namebuf);
				// Option variable type.
				strcpy(typebuf, "char *");
				// Option default value.
				strcpy(defltbuf, "NULL");
				// Option C code.
				strcpy(codebuf, "= strdup(optarg)");
				break;
			case '3':
				// Option variable name.
				sprintf(namebuf, "/* Enter your variable(s) types "
				"and name(s) for option %c here.*/", c);
				// Option variable type.
				typebuf[0] = '\0';
				// Option default value.
				sprintf(defltbuf, "/*Assign the variable(s) for option"
				" %c here.*/", c);
				// Option C code.
				strcpy(codebuf, "/*Enter C code for variable(s) "
				"used by option here.*/");
				break;
			}

			// Is there a long option name?
			loname[0] = '\0';
			getuserinput(
						"Enter long option name or <return> for none:",
							loname);
			if (strlen(loname)) {
				if (!fplostruct) dofopen("lostructTXT.c", "w");
				// I can set up and write the long options struct
				char losbuf[NAME_MAX];
				int rqd = (idx+1 < len && optstringout[idx+1] == ':')
							? 1 : 0;
				sprintf(losbuf, "\t\t{\"%s\",\t%d,\t0,\t\'%c\'},\n",
						loname, rqd, c);
				fputs(losbuf, fplostruct);
				// how the options will display
				sprintf(displayopt, "-%c, --%s", c, loname);
				loidx++;
			} else {
				sprintf(displayopt, "-%c", c);
			}

		// get help line(s) for this option.
		getmultilines(helpbuf, "help text", NAME_MAX, 1);
		fprintf(fphelp, "\n%s\n", displayopt);
		fprintf(fphelp, "\n%s\n", helpbuf);

		// write other workfiles
		// declaration
		if (!fpdecl) fpdecl = dofopen("declTXT.h", "w");
		fprintf(fpdecl, declfmt, typebuf, namebuf);
		// set default value
		fprintf(fpdeflt, defltfmt, namebuf, defltbuf);
		// C code when selected
		fprintf(fpsocode, codefmt, c, namebuf, codebuf);
	} // for(idx ...)

	// Check for any long options not paired with short ones.
	while (1) {
		getuserinput(
			"Enter additional long options, empty line to quit.",
					loname);
		if (strlen(loname) == 0 ) break;

		// I have 1 or more unpaired option so ensure rqd files opened.
		if (!fplocode) fplocode = dofopen("locodeTXT.c", "w");
		if (!fplostruct) fplostruct = dofopen("lostructTXT.c", "w");
		char argans = getans("Does this option want an argument", "Yn");
		int wantsarg = ( argans == 'Y')? 1 : 0;
		// I can write the long options struct now.
		fprintf(fplostruct, "\t\t{%s,\t%d,\t0,\t0 },\n", loname,
					wantsarg);

		// options for display in dohelp()
		sprintf(displayopt, "\n%s\n", loname);
		char *lofmt = "Processing option %s\n"
					"For this option will you:\n"
			"Use a single variable to which you assign a value (1)\n"
			"Use a char pointer and strdup optarg onto it (2)\n"
		"Or do something else, possibly affecting several variables\n"
		"when the option is selected (3).\n";
		char lobuf[NAME_MAX];
		sprintf(lobuf, lofmt, loname);
		char ans = getans(lobuf, "123");
		switch (ans)
		{
			case '1':
				// Option variable name.
				getuserinput("Enter variable name: ", namebuf);
				// Option variable type.
				getuserinput("Enter variable type: ", typebuf);
				// Option default value.
				getuserinput("Enter variable default value: ",
								defltbuf);
				// Option C code.
				getuserinput("Begining with an assignment operator,\n"
							"enter C code for this option: ",
							codebuf);
				break;
			case '2':
				// Option variable name.
				getuserinput("Enter variable name: ", namebuf);
				// Option variable type.
				strcpy(typebuf, "char *");
				// Option default value.
				strcpy(defltbuf, "NULL");
				// Option C code.
				strcpy(codebuf, "= strdup(optarg)");
				break;
			case '3':
				// Option variable name.
				sprintf(namebuf, "/* Enter your variable(s) types "
				"and name(s) for option %s here.*/", loname);
				// Option variable type.
				typebuf[0] = '\0';
				// Option default value.
				sprintf(defltbuf, "/*Assign the variable(s) for option"
				" %s here.*/", loname);
				// Option C code.
				strcpy(codebuf, "/*Enter C code for variable(s) "
				"used by option here.*/");
				break;
		} // switch(ans)
		// now write the workfiles
		// declaration
		if (!fpdecl) fpdecl = dofopen("declTXT.h", "w");
		fprintf(fpdecl, declfmt, typebuf, namebuf);
		// set default value
		fprintf(fpdeflt, defltfmt, namebuf, defltbuf);
		// C code when selected, not the same as short options
		char *codelofmt = "\t\tcase %d:\n\t\t\t%s %s;\n\t\t\tbreak;\n";
		fprintf(fplocode, codelofmt, loidx, namebuf, codebuf);

		// increment the long options index
		loidx++;
	} // while(1)

	// Usage strings.
	char usagebuf[NAME_MAX];
	/* the literal "progname" will be replaced by the actual program
	 * name at program generation time. This name is not known now. */
	fputs("progname ", fpusage);
	char *prompt = "usage text";
	getmultilines(usagebuf, prompt, NAME_MAX, 1);
	fputs(usagebuf, fpusage);
	fputs("\n", fpusage);	// empty line marks end of usage lines.

	if (fphelp) fclose(fphelp);
	if (fpusage) fclose(fpusage);
	if (fpdecl) fclose(fpdecl);
	if (fpdeflt) fclose(fpdeflt);
	if (fpsocode) fclose(fpsocode);
	if (fplocode) fclose(fplocode);
	if (fplostruct) fclose(fplostruct);

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
		fputs("\tif(!argv[optind]) {\n", fpnoarg);
		char * prompt = "What kind of object is required?\n"
			"Dir(1), File(2), String(3) or something else(4)\n";
		char ansno = getans(prompt, "1234");
		char *rqd[] = {
			"dir",
			"file",
			"string",
			"/* Insert name of required object here. */"
		};
		char *fmt = "\t\tfputs(\"No %s provided.\", stderr);\n";
		fprintf(fpnoarg, fmt, rqd[ansno - 49]);
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
	char result[PATH_MAX], longprompt[NAME_MAX], work[NAME_MAX];
	char *fmt = "Input %s.\nUse as many lines as required. An empty"
	" line ends input.\nInput will be truncated at %d characters.\n";
	sprintf(longprompt, fmt, display, maxlen);
	dowrite(1, longprompt);
	result[0] = 0;
	while(1) {
		doread(0, NAME_MAX, work);
		char *cp = strchr(work, '\n');
		if (cp) *cp = '\0';
		if (strlen(work) == 0) break;
		if (wanteol) strcat(work, "\n");	// terminate input with EOL
		strcat(result, work);
	}
	// silent failure if we go over length.
	if (strlen(result) >= maxlen ) {
		result[maxlen - 2] = '\n';
		result[maxlen - 1] = '\0';
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
	dowrite(1, buf);
	doread(0, NAME_MAX, buf);
	char *cp = strchr(buf, '\n');
	if (cp) *cp = '\0';
	strcpy(reply, buf);
} // getuserinput()

void generatecode(const char *progname, int cols)
{	/* Writes the files getoptions.h, getoptions.c and main.c
	 * Source files are boilerplate, getoptionsBP.h, getoptionsBP.c,
	 * mainBP.c and MakefileBP located in
	 * $HOME/.config/genco/boilerplate/
	 * and the purpose written:
	 * helpTXT.c usageTXT.c declTXT.h defltTXT.c socodeTXT.c locodeTXT.c
	 * lostructTXT.c noargsTXT.c
	*/
	// 1. generate main.c
	// a) write the preamble.
	boilerplateinit(mainBP_C, "main.c", "preamble");
	// b) append non-option argument processing
	appenduserfile("noargsTXT.c", "main.c");
	// c) append the rest of main.c
	boilerplateappend("main.c", "tail");
	boilerplatedeinit();

	// 2. generate getoptions.h
	// a) write the preamble.
	boilerplateinit(getoptionsBP_H, "getoptions.h", "preamble");
	// b) append the user's variable declarations.
	appenduserfile("declTXT.", "getoptions.h");
	// c) append the tail end of the BP file.
	boilerplateappend("getoptions.h", "tail");
	boilerplatedeinit();

	// 3. write getoptions.c
	// a) write the preamble.
	boilerplateinit(getoptionsBP_C, "getoptions.c", "preamble");
	// b) set up the help text mess. At the top is usage.
	// b.1 usage.
	if (fileexists("usageTXT.c") == 0) {
		fdata part;
		fdata wfdat = readfile("usageTXT.c", 0, 1);
		part = fmtusagelines(progname, wfdat.from, wfdat.to);
		writefile("getoptions.c", part.from, part.to, "a");
		free(wfdat.from);
		free (part.from);
	}
	// b.2 The common help lines, -h, --help, in the BP file
	boilerplateappend("getoptions.c", "fixedoptions");
	// b.3) append user created help lines.
	if (fileexists("helpTXT.c") == 0) {
		fdata part;
		fdata wfdat = readfile("helpTXT.c", 0, 1);
		part = fmthelplines(wfdat.from, wfdat.to, cols);
		writefile("getoptions.c", part.from, part.to, "a");
		free (part.from);
		free(wfdat.from);
	}
	// b.4 // terminator for help lines.
	boilerplateappend("getoptions.c", "endoptions");

	// c) append defaults initialisation.
	appenduserfile("defltTXT.c", "getoptions.c");

	// d) long option processing
	// d.1) write the top of the loop
	boilerplateappend("getoptions.c", "golongwshortpre");
	// c.2) append any option struct(s) user may have made.
	appenduserfile("lostructTXT.c", "getoptions.c");
	// c.3) finish off long options structs etc
	boilerplateappend("getoptions.c", "golongwshortpost");
	// c.4) write top of long options only loop
	boilerplateappend("getoptions.c", "glongonlypre");
	// c.5) write any long options only C code that user may have made.
	appenduserfile("locodeTXT.c", "getoptions.c");
	// c.6) finish the long options only C code loop
	boilerplateappend("getoptions.c", "glongonlypost");
	// c.7) begin the short options
	boilerplateappend("getoptions.c", "glshortspre");
	// c.8) append user made short option code.
	appenduserfile("socodeTXT.c", "getoptions.c");
	// c.9) finish off short options
	boilerplateappend("getoptions.c", "glshortspost");
	// c.10) complete the file
	boilerplateappend("getoptions.c", "tail");
	boilerplatedeinit();

	// 4. generate a minimal makefile.
	// a) main.c must be renamed to <progname>.c or my brain dead
	// makefile will fail to link the 2 object files.
	char namebuf[NAME_MAX];
	sprintf(namebuf, "%s.c", progname);
	if (rename("main.c", namebuf) == -1) {
		perror(namebuf);
		exit(EXIT_FAILURE);
	}
	// b) don't clobber a Makefile that is there by some other means.
	fdata bpdat;
	char *mf = "Makefile";
	if (fileexists(mf) == 0) mf = "Makefile.gdb";
	bpdat = readfile(MakefileBP_, 1, 1);	// extra byte for '\0'.
	*(bpdat.to - 1) = '\0';	// bpdat.from now a C string.
	// c) generate the makefile, the BP file is a format statement,
	sprintf(namebuf, bpdat.from, progname, progname);
	writefile(mf, namebuf, namebuf+ strlen(namebuf), "w");
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
	 *  "\t-x[, --longx]
	 *  "\tLorem ipsum dolor sit amet, consetetur \n"
	 *  "\telitr, sed diam nonumyeirmod tempor invidunt\n"
	 *  "\tut labore et dolore kimata sanctus est Lorem\n"
	 *  "\tipsum dolor sit amet.Lorem ipsum dolor sit\n"
	 *  "\tamet, consete\n"
	*/
	char resbuf[PATH_MAX];
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
		/* First up, I will split off the actual option identifiers,
		 * "-x", "-x, --longx", or "--longx" alone. I will put this on
		 * it's own line. It is already separated by '\n'.
		*/
		char *eol = memchr(optmess, '\n', omend - optmess);
		*eol = '\0';
		char linebuf[NAME_MAX];
		char *fmt = "  \"\\t%s\\n\"\n";
		sprintf(linebuf, fmt, optmess);
		strcat(resbuf, linebuf);
		char *cp = eol + 1;
		// turn cp into a C string.
		*(omend -1) = '\0';
		// make the rest of the mess into 1 long line.
		while (cp < omend) {
			if (*cp == '\n') *cp = ' ';
			cp++;
		}
		// now split this text into pieces that fit.
		size_t wid = cols - tabsize;
		cp = eol + 1;
		while (strlen(cp) > wid) {
			eol = cp + wid;
			while (*eol != ' ') eol--;
			*eol = '\0';
			sprintf(linebuf, fmt, cp);
			strcat(resbuf, linebuf);
			cp = eol + 1;
		}
		if (strlen(cp)) {
			sprintf(linebuf, fmt, cp);
			strcat(resbuf, linebuf);
		}
		free(optmess);
		opthelp.from = opthelp.to;	// ready for next option if any.
	}
	fdata retdat;
	size_t len = strlen(resbuf);
	retdat.from = malloc(len);
	memcpy(retdat.from, resbuf, len);
	retdat.to = retdat.from + len;
	return retdat;
} // fmthelplines()

fdata bracketsearch(char *from, char *to,  char *opn, char *cls)
{
	/* Search in memory between from and to for the tags opn and cls.
	 * Failure is fatal.
	*/
	fdata result;
	size_t len = strlen(opn);
	char *cp = from;
	cp = memmem(cp, to - cp, opn, len);
	if (!cp) {
		fprintf(stderr, "%s\n", opn);
		fatal("Opening tag not present.\n");
	}
	cp += len;
	if (*cp == '\n') {
		cp++;	// data I want starts on next line.
	}
	result.from = cp;
	len++;	// len cls is ALWAYS 1 more than opn
	cp = memmem(cp, to - cp, cls, len);
	if (!cp) {
		fprintf(stderr, "%s\n", cls);
		fatal("Closing tag not present.\n");
	}
	result.to = cp;
	return result;
} // bracketsearch()

tagpair maketags(char *tagname)
{
	/* make special xml tags from tagname that are also C comments */
	tagpair tp;
	static char opntag[80];
	static char clstag[80];
	char *opnfmt = "//<%s>";
	char *clsfmt = "//</%s>";
	if (strlen(tagname) > 70) {
		fprintf(stderr, "tagname %s > 70\n", tagname);
		exit(EXIT_FAILURE);
	}
	sprintf(opntag, opnfmt, tagname);
	sprintf(clstag, clsfmt, tagname);
	tp.opntag = opntag;
	tp.clstag = clstag;
	return tp;
} // maketags()

// global for 5 functions under.
fdata bpfdat;

void boilerplateinit(const char *bpfilename,
							const char *targetfilename,
							char *tagname)
{
	/* Reads the bpfilename, finds the data between tags named by
	 * tagname and writes such data to targetfilename. */
	bpfdat = readfile(bpfilename, 0, 1);
	tagpair tp = maketags(tagname);
	fdata part = bracketsearch(bpfdat.from, bpfdat.to, tp.opntag,
								tp.clstag);
	writefile(targetfilename, part.from, part.to, "w");
} // boilerplateinit()

void boilerplateappend(const char *targetfilename, char *tagname)
{
	/* finds the data between tags named by tagname in the already
	 * opened bp file and appends it to targetfilename
	*/
	tagpair tp = maketags(tagname);
	fdata part = bracketsearch(bpfdat.from, bpfdat.to, tp.opntag,
								tp.clstag);
	writefile(targetfilename, part.from, part.to, "a");
} // boilerplateappend()

void boilerplatedeinit(void)
{
	/* frees storage allocated by boilerplateinit() */
	free(bpfdat.from);
	bpfdat.from = bpfdat.to = NULL;
} // boilerplatedeinit()
void appenduserfile(const char *userfilename,
						const char *targetfilename)
{
	/* reads the entire content of userfile name and appends it to
	 * targetfilename, if userfilename exists that is.
	*/
	if (fileexists(userfilename) == 0) {
		fdata ufdata = readfile(userfilename, 0, 1);
		writefile(targetfilename, ufdata.from, ufdata.to, "a");
		free(ufdata.from);
	}
} // appenduserfile()
