/* sopt -- simple option parsing
 *
 * Version 1.0
 *
 * Copyright 2021 Ryan Farley <ryan.farley@gmx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SOPTH_INCLUDE
#define SOPTH_INCLUDE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

/* By setting SOPT_INVAL to '?', and terminating with it, we ensure that --
 * should a simple search through the option array yield no match, the final
 * element will have the value '?'*/
#define SOPT_INVAL '?'
#define SOPT_AFTER -1

struct sopt {
	/* Option ID. Should be unique. Indicates the following, based on
	 * value:
	 * 	alphanumeric:
	 * 		a short option, i.e. triggered with -o for 'o'
	 * 	SOPT_INVAL:
	 * 		signals end of option array
	 * 	SOPT_AFTER:
	 * 		specifies a non-option parameter documented in
	 * 		usage text
	 * 	other:
	 * 		identifies long option. Ideally, should be > UCHAR_MAX
	 * 		for this purpose, to ensure no collisions with
	 * 		potential short options.
	 */
	int val;
	/* Long option name, if not null. i.e. --long-option would be "long-option" */
	char *name;
	/* Parameter, if not null. Or name of unparsed argument, if SOPT_AFTER */
	char *arg;
	/* Description for usage text */
	char *desc;
};


/* Initializer macros, for static option array definitions. Use like:
 * 	struct sopt optspec[] = {
 * 		SOPT...(...),
 * 		SOPT...(...),
 * 		...
 * 		SOPT_END
 * 	};
*/
/* Define a simple option like -o (takes no parameter) */
#define SOPT_INIT(opt, desc) { (opt), NULL, NULL, (desc) }
/* Same as above, but with a long option name given */
#define SOPT_INITL(opt, name, desc) { (opt), (name), NULL, (desc) }
/* Define an option with an argument, i.e. -o foo */
#define SOPT_INIT_ARG(opt, param, desc) { (opt), NULL, (param), (desc)}
/* Same as above, but with long option name given */
#define SOPT_INIT_ARGL(opt, name, param, desc) {(opt), (name), (param), (desc)}
/* Define an unparsed argument, i.e. after -- */
#define SOPT_INIT_AFTER(str, desc) {SOPT_AFTER, NULL, (str), (desc)}
/* Terminate the option array. Must be last element. */
#define SOPT_INIT_END {SOPT_INVAL, NULL, NULL, NULL}

#define SOPT_VALID(opt) ((opt)->val != SOPT_INVAL)

/*simple helper -- print out usage example*/
static inline void sopt_usage_printopt(struct sopt *opt)
{
	bool shortopt, longopt;

	shortopt = (((unsigned char)opt->val) == opt->val) && isalnum(opt->val);
	longopt = opt->name;
	if (!(shortopt || longopt)) {
		return; /*borked, yo*/
	}
	if (shortopt) {
		fprintf(stderr, "-%c", opt->val);
	}
	if (shortopt && longopt) {
		fprintf(stderr, "|");
	}
	if (longopt) {
		fprintf(stderr, "--%s", opt->name);
	}
	if (opt->arg) {
		fprintf(stderr, " %s", opt->arg);
	}
}
/*print out usage message
 * Formatted as such:
 * $name: $desc
 *
 * USAGE: $name [-o|--opt] -- afteropt
 * 	-o|--opt:
 * 		option description here
*/
static void sopt_usage(struct sopt *optspec, char *name, char *desc)
{
	struct sopt *opt;
	bool afteropt = false;

	if (!(name && desc && optspec))
		return;

	fprintf(stderr, "%s: %s\n\nUSAGE: %s", name, desc, name);
	for (opt = optspec; SOPT_VALID(opt); ++opt) {
		if (opt->val == SOPT_AFTER) {
			afteropt = true;
			continue;
		}
		fprintf(stderr, " [");
		sopt_usage_printopt(opt);
		fprintf(stderr, "]");
	}
	if (afteropt) {
		fprintf(stderr, " --");
		for (opt = optspec; SOPT_VALID(opt); ++opt) {
			if (opt->val == SOPT_AFTER)
				fprintf(stderr, " %s", opt->arg);
		}
	}
	fprintf(stderr, "\n\t");

	/* now we get to the descriptions */
	for (opt = optspec; SOPT_VALID(opt); ++opt) {
		if (opt->val == SOPT_AFTER)
			continue;
		sopt_usage_printopt(opt);
		fprintf(stderr, ":\n\t\t%s\n\t", opt->desc);
	}
	if (afteropt) {
		for (opt = optspec; SOPT_VALID(opt); ++opt) {
			if (opt->val == SOPT_AFTER)
				fprintf(stderr, "%s:\n\t\t%s\n\t", opt->arg, opt->desc);
		}
	}
	/*make it prettier*/
	fprintf(stderr, "\n");
}
/*print out usage message, but with static storage of paramters.
 * If 'set' is true, other parameters are stored, and the function returns.
 * If 'set' is false, other parameters are ignored, and sopt_usage() is called
 * with stored values used */
static void sopt_usage_static(struct sopt *opt, char *name, char *desc, bool set)
{
	static char *name_s, *desc_s;
	static struct sopt *opt_s;
	if (set) {
		name_s = name;
		desc_s = desc;
		opt_s = opt;
	} else {
		sopt_usage(opt_s, name_s, desc_s);
	}
}
/* for convenience, set the static usage values for future use */
static inline void sopt_usage_set(struct sopt *opt, char *name, char *desc)
{
	sopt_usage_static(opt, name, desc, true);
}
/* for convenience, call sopt_usage_static with stored parameters */
static inline void sopt_usage_s(void)
{
	sopt_usage_static(NULL, NULL, NULL, false);
}

/* replacement for getopt()
 * argc:
 * 	argc, obviously
 * argv:
 * 	argv, obviously
 * opt:
 * 	array of possible option structures
 * cpos:
 * 	Stores the current position in a combined option, i.e. -abcd.
 * 	*cpos MUST BE ZERO ON FIRST CALL
 * optind:
 * 	Current position in the argv array. At end of processing, will point
 * 	to first non-parsed argument.
 * 	*optind MUST BE ZERO ON FIRST CALL
 * optarg:
 * 	Pointer to any argument given after the option, or NULL.
 *
 * RETURNS:
 * 	'?' if unknown or invalid input given,
 * 	opt->val for the found option otherwise.
 */
static int sopt_getopt(int argc, char **argv, struct sopt *opt, int *cpos, int *optind, char **optarg)
{
	if (!(opt && cpos &&argv && optind && optarg && argc))
		return -1;
	/* handle the case of combined options */
	if (*cpos)
		goto shortopt;
	/*otherwise proceed normally*/
	if (++*optind >= argc)
		return -1;
	if (argv[*optind][0] != '-')
		return -1;

	if (argv[*optind][1] == '-') {
		/*end of options*/
		if (!argv[*optind][2]) {
			++*optind; //optind points at next non-option
			return -1;
		}
		/*or a long option*/
		for (; SOPT_VALID(opt); ++opt) {
			/*don't want to be passing NULL to strcmp, now do we?*/
			if (opt->name) {
				if (!strcmp(opt->name, argv[*optind] + 2))
					break;
			}
		}
	} else {
shortopt:
		/* if we're not in a combined option, start at first option
		 * character */
		if (!*cpos)
			*cpos = 1;
		/* find our shortopt */
		for (; SOPT_VALID(opt); ++opt) {
			if (opt->val == argv[*optind][*cpos])
				break;
		}
		/* check if we're ina combined option */
		if (argv[*optind][++*cpos]) {
			/* make sure that we're not expecting a param for
			 * this argument -- if we are, someone fucked up */
			if (opt->arg)
				return '?';
		} else {
			*cpos = 0;
		}
	}
	if (opt->arg) {
		if (!(*optarg = argv[++*optind])) {
			/* we were expecting an argument, got none */
			return '?';
		}
	} else {
		*optarg = NULL;
	}
	return opt->val;
}

#endif
