/*------------------------------------------------------------------------
 |  FILE            shhopt.c
 |
 |  DESCRIPTION     Functions for parsing command line arguments. Values
 |                  of miscellaneous types may be stored in variables,
 |                  or passed to functions as specified.
 |
 |  REQUIREMENTS    Some systems lack the ANSI C -function strtoul. If your
 |                  system is one of those, you'll need to write one yourself,
 |                  or get the GNU liberty-library (from prep.ai.mit.edu).
 |
 |  WRITTEN BY      Sverre H. Huseby <sverrehu@online.no>
 +----------------------------------------------------------------------*/

/*************************************************************************
  This is based on work by Sverre H. Huseby <sverrehu@online.no>.
  These functions are backward compatible with the 'shhopt'
  distributed by Huseby.

  See the file README.shhopt for copy licensing information.
*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "mallocvar.h"
#include "nstring.h"
#include "token.h"
#include "shhopt.h"

/*-----------------------------------------------------------------------+
|  PRIVATE DATA                                                          |
+-----------------------------------------------------------------------*/

static void optFatalFunc(const char *, ...);
static void (*optFatal)(const char *format, ...) = optFatalFunc;

/*-----------------------------------------------------------------------+
|  PRIVATE FUNCTIONS                                                     |
+-----------------------------------------------------------------------*/

/*------------------------------------------------------------------------
 |  NAME          optFatalFunc
 |
 |  FUNCTION      Show given message and abort the program.
 |
 |  INPUT         format, ...
 |                        Arguments used as with printf().
 |
 |  RETURNS       Never returns. The program is aborted.
 */
static void
optFatalFunc(const char *format, ...)
{
    va_list ap;

    fflush(stdout);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(99);
}

/*------------------------------------------------------------------------
 |  NAME          optStructCount
 |
 |  FUNCTION      Get number of options in a optStruct.
 |
 |  INPUT         opt     array of possible options.
 |
 |  RETURNS       Number of options in the given array.
 |
 |  DESCRIPTION   Count elements in an optStruct-array. The structure must
 |                be ended using an element of type OPT_END.
 */
static int
optStructCount(const optEntry opt[])
{
    int ret = 0;

    while (opt[ret].type != OPT_END && ret < 500)
        ++ret;
    return ret;
}



enum Shortlong {SL_SHORT, SL_LONG};


static void
optMatch(optEntry       const opt[],
         const char *   const targetOpt,
         enum Shortlong const shortLong,
         bool *         const foundP,
         int *          const optIndexP) {
/*------------------------------------------------------------------------
 |  FUNCTION      Find a matching option.
 |
 |  INPUT         opt        array of valid option names.
 |                targetOpt  option string to match, without `-' or `--'.
 |                           e.g. "verbose" or "height=5"
 |                shortLong  whether to match short option or long
 |
 |  RETURNS       *foundP     there is a matching option class the table
 !                *optIndexP  index in the option class table
 |                            meaningless if *foundP is false
 |
 |  DESCRIPTION   Short options are matched from the first character in
 |                the given string.
 |
 |                Where multiple entries in opt[] match, return the first.
 */

    unsigned int const optCt = optStructCount(opt);

    unsigned int q;
    unsigned int matchlen;
    bool found;
    unsigned int optIndex;

    if (shortLong == SL_LONG) {
        const char * const equalPos = strchr(targetOpt, '=');
        if (equalPos)
            matchlen = equalPos - &targetOpt[0];
        else
            matchlen = strlen(targetOpt);
    } else
        matchlen = 0;

    for (q = 0, found = false; q < optCt && !found; ++q) {
        switch (shortLong) {
        case SL_LONG: {
            if (opt[q].longName) {
                if (strneq(targetOpt, opt[q].longName, matchlen)) {
                    found = true;
                    optIndex = q;
                }
            }
        }
        case SL_SHORT: {
            if (opt[q].shortName) {
                if (targetOpt[0] == opt[q].shortName) {
                    found = true;
                    optIndex = q;
                }
            }
        }
        }
    }
    *foundP    = found;
    *optIndexP = optIndex;
}



/*------------------------------------------------------------------------
 |  NAME          optString
 |
 |  FUNCTION      Return a (static) string with the option name.
 |
 |  INPUT         opt     the option to stringify.
 |                lng     is it a long option?
 |
 |  RETURNS       Pointer to static string.
 */
static char *
optString(const optEntry opte, int lng)
{
    static char ret[31];

    if (lng) {
        strcpy(ret, "--");
        strncpy(ret + 2, opte.longName, 28);
    } else {
        ret[0] = '-';
        ret[1] = opte.shortName;
        ret[2] = '\0';
    }
    return ret;
}



static optEntry
optStructToEntry(const optStruct opt) {
/*----------------------------------------------------------------------------
   Return the information in 'opt' (an optStruct type) as an optEntry type.
   optEntry is newer and has an additional field.
-----------------------------------------------------------------------------*/
    optEntry opte;

    opte.shortName = opt.shortName;
    opte.longName  = opt.longName;
    opte.type      = opt.type;
    opte.arg       = opt.arg;
    opte.specified = NULL;
    opte.flags     = opt.flags;

    return(opte);
}



static optEntry *
optStructTblToEntryTbl(const optStruct optStructTable[]) {
/*----------------------------------------------------------------------------
   Return a table of optEntry types containing the information in the
   input table of optStruct types.

   Return it in newly malloc'ed storage.
-----------------------------------------------------------------------------*/
    int count;
        /* Number of entries in input table, including OPT_END marker */
    int i;

    optEntry *optEntryTable;  /* malloc'ed array */

    /* Count the entries in optStructTable[] */
    for (i = 0; optStructTable[i].type != OPT_END && i < 500; i++);
    count = i+1;

    optEntryTable = (optEntry *) malloc(count * sizeof(optEntry));
    if (optEntryTable) {
        int i;
        for (i = 0; i < count; i++)
            optEntryTable[i] = optStructToEntry(optStructTable[i]);
    }
    return(optEntryTable);
}




/*------------------------------------------------------------------------
 |  NAME          optNeedsArgument
 |
 |  FUNCTION      Check if an option requires an argument.
 |
 |  INPUT         opt     the option to check.
 |
 |  RETURNS       Boolean value.
 */
static int
optNeedsArgument(const optEntry opt)
{
    return opt.type == OPT_STRING
	|| opt.type == OPT_INT
	|| opt.type == OPT_UINT
	|| opt.type == OPT_LONG
	|| opt.type == OPT_ULONG
    || opt.type == OPT_FLOAT
    || opt.type == OPT_NAMELIST
    || opt.type == OPT_STRINGLIST
        ;
}

/*------------------------------------------------------------------------
 |  NAME          argvRemove
 |
 |  FUNCTION      Remove an entry from an argv-array.
 |
 |  INPUT         argc    pointer to number of options.
 |                argv    array of option-/argument-strings.
 |                i       index of option to remove.
 |
 |  OUTPUT        argc    new argument count.
 |                argv    array with given argument removed.
 */
static void
argvRemove(int *argc, char *argv[], int i)
{
    if (i >= *argc)
        return;
    while (i++ < *argc)
        argv[i - 1] = argv[i];
    --*argc;
}



static void
getToken(const char *  const tokenStart,
         char          const delimiter,
         const char ** const tokenP,
         const char ** const nextP) {
/*----------------------------------------------------------------------------
   Find the token starting at 'tokenStart' up to but not including
   the first 'delimiter' character or end of string.  Return it in newly
   malloced memory as *tokenP, NUL-terminated.

   Make *nextP point just past the token, i.e. to the delimiter or
   end of string NUL character.

   Note that if the string is empty, or starts with the delimiter,
   we return an empty string and *nextP == tokenStart, i.e. *nextP
   doesn't necessarily advance.
-----------------------------------------------------------------------------*/
    const char * error;

    pm_gettoken(tokenStart, delimiter, tokenP, nextP, &error);

    if (error)
        optFatal("error parsing a token: %s", error);
}



static void
parseNameList(const char *           const listText,
              struct optNameValue ** const listP) {

    unsigned int const maxOptionCount = 100;

    const char * cursor;
    unsigned int optionCount;
    struct optNameValue * list;

    MALLOCARRAY_NOFAIL(list, maxOptionCount+1);

    cursor = &listText[0];  /* initial value */

    optionCount = 0;  /* initial value */

    while (optionCount < maxOptionCount && *cursor != '\0') {
        const char * next;
        struct optNameValue pair;

        getToken(cursor, '=', &pair.name, &next);

        cursor = next;

        if (*cursor == '\0')
            optFatal("name=value option value ends prematurely.  An equal "
                     "sign was expected following name '%s'", pair.name);

        assert(*cursor == '=');
        ++cursor;

        getToken(cursor, ',', &pair.value, &next);

        cursor = next;

        list[optionCount++] = pair;

        if (*cursor != '\0') {
            assert(*cursor == ',');
            ++cursor;
        }
    }
    list[optionCount].name  = NULL;
    list[optionCount].value = NULL;

    *listP = list;
}



static void
parseStringList(const char *   const listText,
                const char *** const listP) {

    unsigned int const maxStringCount = 100;

    const char * cursor;
    unsigned int stringCount;
    const char ** list;

    MALLOCARRAY_NOFAIL(list, maxStringCount+1);

    cursor = &listText[0];  /* initial value */

    stringCount = 0;  /* initial value */

    while (stringCount < maxStringCount && *cursor != '\0') {
        const char * next;

        getToken(cursor, ',', &list[stringCount++], &next);

        cursor = next;

        if (*cursor != '\0') {
            assert(*cursor == ',');
            ++cursor;
        }
    }
    list[stringCount] = NULL;

    *listP = list;
}



/*------------------------------------------------------------------------
 |  NAME          optExecute
 |
 |  FUNCTION      Perform the action of an option.
 |
 |  INPUT         opt     element in array of defined options that
 |                        applies to this option
 |                arg     argument to option, if it applies.
 |                lng     was the option given as a long option?
 |
 |  RETURNS       Nothing. Aborts in case of error.
 */
static void
optExecute(optEntry  const opt, char *arg, int lng)
{
    if (opt.specified)
        *opt.specified = 1;

    switch (opt.type) {
    case OPT_FLAG:
        if (opt.arg)
            *((int *) opt.arg) = 1;
        break;

    case OPT_STRING:
        if (opt.arg)
            *((char **) opt.arg) = arg;
        break;

    case OPT_INT:
    case OPT_LONG: {
        long tmp;
        char *e;

        if (arg == NULL)
            optFatal("internal error: optExecute() called with NULL argument "
                     "'%s'", optString(opt, lng));
        tmp = strtol(arg, &e, 10);
        if (*e)
            optFatal("invalid number `%s'", arg);
        if (errno == ERANGE
            || (opt.type == OPT_INT && (tmp > INT_MAX || tmp < INT_MIN)))
            optFatal("number `%s' to `%s' out of range",
                     arg, optString(opt, lng));
        if (opt.type == OPT_INT) {
            *((int *) opt.arg) = (int) tmp;
        } else /* OPT_LONG */ {
            if (opt.arg)
                *((long *) opt.arg) = tmp;
        }
    } break;

    case OPT_UINT:
    case OPT_ULONG: {
        unsigned long tmp;
        char * tailPtr;

        if (arg == NULL)
            optFatal("internal error: optExecute() called with NULL argument "
                     "'%s'", optString(opt, lng));

        if (arg[0] == '-' || arg[1] == '+')
            optFatal("unsigned number '%s' has a sign ('%c')",
                     arg, arg[0]);
        tmp = strtoul(arg, &tailPtr, 10);
        if (*tailPtr)
            optFatal("invalid number `%s'", arg);
        if (errno == ERANGE
            || (opt.type == OPT_UINT && tmp > UINT_MAX))
            optFatal("number `%s' to `%s' out of range",
                     arg, optString(opt, lng));
        if (opt.type == OPT_UINT) {
           if (opt.arg)
               *((unsigned *) opt.arg) = (unsigned) tmp;
        } else /* OPT_ULONG */ {
            if (opt.arg)
                *((unsigned long *) opt.arg) = tmp;
        }
    } break;
    case OPT_FLOAT: {
        float tmp;
        char *e;

        if (arg == NULL)
            optFatal("internal error: optExecute() called with NULL argument "
                     "'%s'", optString(opt, lng));
        tmp = strtod(arg, &e);
        if (*e)
            optFatal("invalid floating point number `%s'", arg);
        if (errno == ERANGE)
            optFatal("floating point number `%s' to `%s' out of range",
                     arg, optString(opt, lng));
        if (opt.arg)
            *((float *) opt.arg) = tmp;
    } break;
    case OPT_NAMELIST: {
        if (arg == NULL)
            optFatal("internal error: optExecute() called with NULL argument "
                     "'%s'", optString(opt, lng));

        if (opt.arg)
            parseNameList(arg, (struct optNameValue **)opt.arg);

    } break;
    case OPT_STRINGLIST: {
        if (arg == NULL)
            optFatal("internal error: optExecute() called with NULL argument "
                     "'%s'", optString(opt, lng));

        if (opt.arg)
            parseStringList(arg, (const char ***)opt.arg);

    } break;
    default:
        break;
    }
}



/*-----------------------------------------------------------------------+
|  PUBLIC FUNCTIONS                                                      |
+-----------------------------------------------------------------------*/

/*------------------------------------------------------------------------
 |  NAME          optSetFatalFunc
 |
 |  FUNCTION      Set function used to display error message and exit.
 |
 |  SYNOPSIS      #include "shhopt.h"
 |                void optSetFatalFunc(void (*f)(const char *, ...));
 |
 |  INPUT         f       function accepting printf()'like parameters,
 |                        that _must_ abort the program.
 */
void
pm_optSetFatalFunc(void (*f)(const char *, ...)) {

    optFatal = f;
}



/*------------------------------------------------------------------------
 |  NAME          pm_optParseOptions
 |
 |  FUNCTION      Parse commandline options.
 |
 |  SYNOPSIS      #include "shhopt.h"
 |                void pm_optParseOptions(int *argc, char *argv[],
 |                                     optStruct opt[], int allowNegNum);
 |
 |  INPUT         argc    Pointer to number of options.
 |                argv    Array of option-/argument-strings.
 |                opt     Array of possible options.
 |                allowNegNum
 |                        a negative number is not to be taken as
 |                        an option.
 |
 |  OUTPUT        argc    new argument count.
 |                argv    array with arguments removed.
 |
 |  RETURNS       Nothing. Aborts in case of error.
 |
 |  DESCRIPTION   This function checks each option in the argv-array
 |                against strings in the opt-array, and `executes' any
 |                matching action. Any arguments to the options are
 |                extracted and stored in the variables or passed to
 |                functions pointed to by entries in opt.
 |
 |                Options and arguments used are removed from the argv-
 |                array, and argc is decreased accordingly.
 |
 |                Any error leads to program abortion.
 */
void
pm_optParseOptions(int *argc, char *argv[], optStruct opt[], int allowNegNum)
{
    int ai;        /* argv index. */
    int optarg;    /* argv index of option argument, or -1 if none. */
    int done;
    char * arg;      /* Pointer to argument to an option. */
    char * o;        /* pointer to an option character */
    char * p;

    optEntry *opt_table;  /* malloc'ed array */

    opt_table = optStructTblToEntryTbl(opt);
    if (opt_table == NULL)
        optFatal("Memory allocation failed (trying to allocate space for "
                 "new-format option table)");

    /*
     *  Loop through all arguments.
     */
    for (ai = 0; ai < *argc; ) {
        /*
         *  "--" indicates that the rest of the argv-array does not
         *  contain options.
         */
        if (streq(argv[ai], "--")) {
            argvRemove(argc, argv, ai);
            break;
        }

        if (allowNegNum && argv[ai][0] == '-' && ISDIGIT(argv[ai][1])) {
            ++ai;
            continue;
        } else if (strneq(argv[ai], "--", 2)) {
            bool found;
            int mi;
            /* long option */
            /* find matching option */
            optMatch(opt_table, argv[ai] + 2, SL_LONG, &found, &mi);
            if (!found)
                optFatal("unrecognized option `%s'", argv[ai]);

            /* possibly locate the argument to this option. */
            arg = NULL;
            if ((p = strchr(argv[ai], '=')) != NULL)
                arg = p + 1;

            /* does this option take an argument? */
            optarg = -1;
            if (optNeedsArgument(opt_table[mi])) {
                /* option needs an argument. find it. */
                if (!arg) {
                    if ((optarg = ai + 1) == *argc)
                        optFatal("option `%s' requires an argument",
                                 optString(opt_table[mi], 1));
                    arg = argv[optarg];
                }
            } else {
                if (arg)
                    optFatal("option `%s' doesn't allow an argument",
                             optString(opt_table[mi], 1));
            }
            optExecute(opt_table[mi], arg, 1);
            /* remove option and any argument from the argv-array. */
            if (optarg >= 0)
                argvRemove(argc, argv, ai);
            argvRemove(argc, argv, ai);
        } else if (*argv[ai] == '-') {
            /* A dash by itself is not considered an option. */
            if (argv[ai][1] == '\0') {
                ++ai;
                continue;
            }
            /* Short option(s) following */
            o = argv[ai] + 1;
            done = 0;
            optarg = -1;
            while (*o && !done) {
                bool found;
                int mi;
                /* find matching option */
                optMatch(opt_table, o, SL_SHORT, &found, &mi);
                if (!found)
                    optFatal("unrecognized option `-%c'", *o);

                /* does this option take an argument? */
                optarg = -1;
                arg = NULL;
                if (optNeedsArgument(opt_table[mi])) {
                    /* option needs an argument. find it. */
                    arg = o + 1;
                    if (!*arg) {
                        if ((optarg = ai + 1) == *argc)
                            optFatal("option `%s' requires an argument",
                                     optString(opt_table[mi], 0));
                        arg = argv[optarg];
                    }
                    done = 1;
                }
                /* perform the action of this option. */
                optExecute(opt_table[mi], arg, 0);
                ++o;
            }
            /* remove option and any argument from the argv-array. */
            if (optarg >= 0)
                argvRemove(argc, argv, ai);
            argvRemove(argc, argv, ai);
        } else {
            /* a non-option argument */
            ++ai;
        }
    }
    free(opt_table);
}


static void
parse_short_option_token(char *argv[], const int argc, const int ai,
                         const optEntry opt_table[],
                         int * const tokens_consumed_p) {
/*----------------------------------------------------------------------------
   Parse a cluster of short options, e.g. -walne .

   The last option in the cluster might take an argument, and we parse
   that as well.  e.g. -cf myfile or -cfmyfile .

   argv[] and argc describe the whole program argument set.  'ai' is the
   index of the argument that is the short option cluster.
-----------------------------------------------------------------------------*/
    char *o;  /* A short option character */
    char *arg;
    unsigned char processed_arg;  /* boolean */
        /* We processed an argument to one of the one-character options.
           This necessarily means there are no more options in this token
           to process.
           */

    *tokens_consumed_p = 1;  /* initial assumption */

    o = argv[ai] + 1;
    processed_arg = 0;  /* initial value */
    while (*o && !processed_arg) {
        bool found;
        int mi;   /* index into option table */
		/* find matching option */
		optMatch(opt_table, o, SL_SHORT, &found, &mi);
		if (!found)
		    optFatal("unrecognized option `-%c'", *o);

		/* does this option take an argument? */
		if (optNeedsArgument(opt_table[mi])) {
		    /* option needs an argument. find it. */
		    arg = o + 1;
		    if (!*arg) {
                if (ai + 1 >= argc)
			    optFatal("option `%s' requires an argument",
				     optString(opt_table[mi], 0));
			arg = argv[ai+1];
            (*tokens_consumed_p)++;
		    }
		    processed_arg = 1;
		} else
            arg = NULL;
		/* perform the action of this option. */
		optExecute(opt_table[mi], arg, 0);
		++o;
    }
}



static void
fatalUnrecognizedLongOption(const char * const optionName,
                            optEntry     const optTable[]) {

    unsigned int const optCt = optStructCount(optTable);

    unsigned int q;

    char optList[1024];

    optList[0] = '\0';  /* initial value */

    for (q = 0;
         q < optCt && strlen(optList) + 1 <= sizeof(optList);
         ++q) {

        const optEntry * const optEntryP = &optTable[q];
        const char * entry;

        if (optEntryP->longName)
            pm_asprintf(&entry, "-%s ", optEntryP->longName);
        else
            pm_asprintf(&entry, "-%c ", optEntryP->shortName);

        strncat(optList, entry, sizeof(optList) - strlen(optList) - 1);

        pm_strfree(entry);

        if (strlen(optList) + 1 == sizeof(optList)) {
            /* Buffer is full.  Overwrite end of list with ellipsis */
            strcpy(&optList[sizeof(optList) - 4], "...");
        }
    }
    optFatal("unrecognized option '%s'.  Recognized options are: %s",
             optionName, optList);
}



static void
parse_long_option(char *   const argv[],
                  int      const argc,
                  int      const ai,
                  int      const namepos,
                  optEntry const opt_table[],
                  int *    const tokens_consumed_p) {
/*----------------------------------------------------------------------------
   Parse a long option, e.g. -verbose or --verbose.

   The option might take an argument, and we parse
   that as well.  e.g. -file=myfile or -file myfile .

   argv[] and argc describe the whole program argument set.  'ai' is the
   index of the argument that is the long option.
-----------------------------------------------------------------------------*/
    char *equals_arg;
      /* The argument of an option, included in the same token, after a
         "=".  NULL if no "=" in the token.
         */
    char *arg;     /* The argument of an option; NULL if none */
    bool found;
    int mi;    /* index into option table */

    /* The current token is an option, and its name starts at
       Index 'namepos' in the argument.
    */
    *tokens_consumed_p = 1;  /* initial assumption */
    /* find matching option */
    optMatch(opt_table, &argv[ai][namepos], SL_LONG, &found, &mi);
    if (!found)
        fatalUnrecognizedLongOption(argv[ai], opt_table);

    /* possibly locate the argument to this option. */
    {
        char *p;
        if ((p = strchr(argv[ai], '=')) != NULL)
            equals_arg = p + 1;
        else
            equals_arg = NULL;
    }
    /* does this option take an argument? */
    if (optNeedsArgument(opt_table[mi])) {
        /* option needs an argument. find it. */
        if (equals_arg)
            arg = equals_arg;
        else {
            if (ai + 1 == argc)
                optFatal("option `%s' requires an argument",
                         optString(opt_table[mi], 1));
            arg = argv[ai+1];
            (*tokens_consumed_p)++;
        }
    } else {
        if (equals_arg)
            optFatal("option `%s' doesn't allow an argument, but you "
                     "have specified it in the form name=value",
                     optString(opt_table[mi], 1));
        else
            arg = NULL;
    }
    /* perform the action of this option. */
    optExecute(opt_table[mi], arg, 1);
}



/*------------------------------------------------------------------------
 |  NAME          pm_optParseOptions2
 |
 |  FUNCTION      Parse commandline options.
 |
 |  SYNOPSIS      #include "shhopt.h"
 |                void pm_optParseOptions2(int *argc, char *argv[],
 |                                      optStruct2 opt, unsigned long flags);
 |
 |  INPUT         argc    Pointer to number of options.
 |                argv    Array of option-/argument-strings.
 |                opt     Structure describing option syntax.
 |                flags   Result is undefined if not zero.
 |                        For future expansion.
 |
 |  OUTPUT        argc    new argument count.
 |                argv    array with arguments removed.
 |
 |  RETURNS       Nothing. Aborts in case of error.
 |
 |  DESCRIPTION   This function checks each option in the argv-array
 |                against strings in the opt-array, and `executes' any
 |                matching action. Any arguments to the options are
 |                extracted and stored in the variables or passed to
 |                functions pointed to by entries in opt.
 |
 |                This differs from pm_optParseOptions in that it accepts
 |                long options with just one hyphen and doesn't accept
 |                any short options.  It also has accommodations for
 |                future expansion.
 |
 |                Options and arguments used are removed from the argv-
 |                array, and argc is decreased accordingly.
 |
 |                Any error leads to program abortion.
 */
void
pm_optParseOptions2(int * const argc_p, char *argv[], const optStruct2 opt,
                 const unsigned long flags)
/*----------------------------------------------------------------------------
   This does the same thing as pm_optParseOptions3(), except that there is no
   "specified" return value.

   This function exists for backward compatibility.
-----------------------------------------------------------------------------*/

{
    optStruct3 opt3;

    opt3.short_allowed = opt.short_allowed;
    opt3.allowNegNum   = opt.allowNegNum;
    opt3.opt_table     = optStructTblToEntryTbl(opt.opt_table);

    if (opt3.opt_table == NULL)
        optFatal("Memory allocation failed (trying to allocate space for "
                 "new-format option table)");

    pm_optParseOptions3(argc_p, argv, opt3, sizeof(opt3), flags);

    free(opt3.opt_table);
}




static void
zero_specified(optEntry opt_table[]) {
/*----------------------------------------------------------------------------
   Set all the "number of times specified" return values identified in the
   option table opt_table[] to zero.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; opt_table[i].type != OPT_END; i++) {
        if (opt_table[i].specified)
            *(opt_table[i].specified) = 0;
    }
}



/*------------------------------------------------------------------------
 |  NAME          pm_optParseOptions3
 |
 |  FUNCTION      Parse commandline options.
 |
 |  INPUT         argc    Pointer to number of options.
 |                argv    Array of option-/argument-strings.
 |                opt     Structure describing option syntax.
 |                optStructSize
 |                        Size of "opt" (since the caller may be older
 |                        than this function, it may be using a structure
 |                        with fewer fields than exist today.  We use this
 |                        parameter to handle those older callers).
 |                flags   Result is undefined if not zero.
 |                        For future expansion.
 |
 |  OUTPUT        argc    new argument count.
 |                argv    array with arguments removed.
 |
 |                Areas pointed to by pointers in 'opt' get updated with
 |                option values and counts.
 |
 |  RETURNS       Nothing. Aborts in case of error.
 |
 |  DESCRIPTION   This function checks each option in the argv-array
 |                against strings in the opt-array, and `executes' any
 |                matching action. Any arguments to the options are
 |                extracted and stored in the variables or passed to
 |                functions pointed to by entries in opt.
 |
 |                This differs from pm_optParseOptions in that it accepts
 |                long options with just one hyphen and doesn't accept
 |                any short options.  It also has accommodations for
 |                future expansion.
 |
 |                Options and arguments used are removed from the argv-
 |                array, and argc is decreased accordingly.
 |
 |                Any error leads to program abortion.
 */
void
pm_optParseOptions3(int * const argc_p, char *argv[], const optStruct3 opt,
                 const unsigned int optStructSize, const unsigned long flags)
{
    int  ai;        /* argv index. */
    int tokens_consumed;
    unsigned char no_more_options;  /* boolean */
        /* We've encountered the "no more options" token */

    zero_specified(opt.opt_table);

    /*
     *  Loop through all arguments.
     */
    no_more_options = 0;  /* initial value */
    for (ai = 0; ai < *argc_p; ) {
        if (no_more_options)
            /* Can't be an option -- there aren't any more */
            ai++;
        else if (argv[ai][0] != '-')
            /* Can't be an option -- doesn't start with a dash */
            ai++;
        else {
            /* It starts with a dash -- could be an option */
            if (argv[ai][1] == '\0') {
                /* A dash by itself is not considered an option. */
                ++ai;
                tokens_consumed = 0;
            } else if (opt.allowNegNum && ISDIGIT(argv[ai][1])) {
                /* It's a negative number parameter, not an option */
                ++ai;
                tokens_consumed = 0;
            } else if (argv[ai][1] == '-') {
                /* It starts with -- */
                if (argv[ai][2] == '\0') {
                    /* The entire thing is "--".  That means no more options */
                    tokens_consumed = 1;
                    no_more_options = 1;
                } else
                    /* It's an option that starts with "--" */
                    parse_long_option(argv, *argc_p, ai, 2,
                                      opt.opt_table, &tokens_consumed);
            } else {
                if (opt.short_allowed) {
                    /* It's a cluster of (one or more) short options */
                    parse_short_option_token(argv, *argc_p, ai,
                                             opt.opt_table, &tokens_consumed);
                } else {
                    /* It's a long option that starts with "-" */
                    parse_long_option(argv, *argc_p, ai, 1,
                                      opt.opt_table, &tokens_consumed);
                }

            }
            /* remove option and any argument from the argv-array. */
            {
                int i;
                for (i = 0; i < tokens_consumed; i++)
                    argvRemove(argc_p, argv, ai);
            }
        }
    }
}



void
pm_optDestroyNameValueList(struct optNameValue * const list) {

    unsigned int i;

    for (i = 0; list[i].name; ++i) {
        pm_strfree(list[i].name);
        pm_strfree(list[i].value);
    }

    free(list);
}
