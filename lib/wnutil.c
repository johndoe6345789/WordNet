/*

  wnutil.c - utility functions used by WordNet code

*/

#ifdef _WINDOWS
#include <windows.h>
#include <windowsx.h>
#endif

#include <stdio.h>
#include <ctype.h>

#ifdef __unix__
#ifndef __MACH__
#include <malloc.h>
#endif
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "wn.h"

static int do_init();

static char msgbuf[256];	/* buffer for constructing error messages */

/* used by the strstr wrapper functions */
static char *strstr_word;
static char *strstr_stringstart;
static char *strstr_stringcurrent;

static void append_str(char *buf, size_t size, const char *src)
{
    size_t len = strlen(buf);

    if (len + 1 >= size) {
	return;
    }
    strncat(buf, src, size - len - 1);
}

static void append_fmt(char *buf, size_t size, const char *fmt, ...)
{
    size_t len = strlen(buf);
    va_list args;

    if (len + 1 >= size) {
	return;
    }

    va_start(args, fmt);
    vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);
}


/* Initialization functions */

static void closefps();

int wninit(void)
{
    static int done = 0;
    static int openerr = 0;
    char *env;

    if (!done) {
	if ((env = getenv("WNDBVERSION")) != NULL) {
	    wnrelease = strdup(env);	/* set release */
	    assert(wnrelease);
	}
	openerr = do_init();
	if (!openerr) {	
	    done = 1;	
	    OpenDB = 1;
	    openerr = morphinit();
	}
    }

    return(openerr);
}

int re_wninit(void)
{
    int openerr;
    char *env;

    closefps();

    if ((env = getenv("WNDBVERSION")) != NULL) {
	wnrelease = strdup(env);	/* set release */
	assert(wnrelease);
    }
    openerr = do_init();
    if (!openerr) {
	OpenDB = 1;
	openerr = re_morphinit();
    }

    return(openerr);
}

static void closefps(void) 
{
    int i;

    if (OpenDB) {
	for (i = 1; i < NUMPARTS + 1; i++) {
	    if (datafps[i] != NULL) {
		fclose(datafps[i]);
	    }
	    datafps[i] = NULL;
	    if (indexfps[i] != NULL) {
		fclose(indexfps[i]);
	    }
	    indexfps[i] = NULL;
	}
	if (sensefp != NULL) {
	    fclose(sensefp); sensefp = NULL;
	}
	if (cntlistfp != NULL) {
	    fclose(cntlistfp); cntlistfp = NULL;
	}
	if (keyindexfp != NULL) {
	    fclose(keyindexfp); keyindexfp = NULL;
	}
	if (vsentfilefp != NULL) {
	    fclose(vsentfilefp); vsentfilefp = NULL;
	}
	if (vidxfilefp != NULL) {
	    fclose(vidxfilefp); vidxfilefp = NULL;
	}
	OpenDB = 0;
    }
}

static int do_init(void)
{
    int i, openerr;
    char searchdir[256], tmpbuf[256];

#ifdef _WINDOWS
    HKEY hkey;
    DWORD dwType, dwSize;
#else
    char *env;
#endif
 
    openerr = 0;

    /* Find base directory for database.  If set, use WNSEARCHDIR.
       If not set, check for WNHOME/dict, otherwise use DEFAULTPATH. */

#ifdef _WINDOWS
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\WordNet\\3.0"),
		     0, KEY_READ, &hkey) == ERROR_SUCCESS) {
	dwSize = sizeof(searchdir);
	RegQueryValueEx(hkey, TEXT("WNHome"),
			NULL, &dwType, searchdir, &dwSize);
	RegCloseKey(hkey);
	{
	    size_t len = strlen(searchdir);
	    if (len + 1 < sizeof(searchdir)) {
		strncat(searchdir, DICTDIR, sizeof(searchdir) - len - 1);
	    }
	}
    } else if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\WordNet\\3.0"),
		     0, KEY_READ, &hkey) == ERROR_SUCCESS) {
	dwSize = sizeof(searchdir);
	RegQueryValueEx(hkey, TEXT("WNHome"),
			NULL, &dwType, searchdir, &dwSize);
	RegCloseKey(hkey);
	{
	    size_t len = strlen(searchdir);
	    if (len + 1 < sizeof(searchdir)) {
		strncat(searchdir, DICTDIR, sizeof(searchdir) - len - 1);
	    }
	}
    } else
	snprintf(searchdir, sizeof(searchdir), "%s", DEFAULTPATH);
#else
    if ((env = getenv("WNSEARCHDIR")) != NULL)
	snprintf(searchdir, sizeof(searchdir), "%s", env);
    else if ((env = getenv("WNHOME")) != NULL)
	snprintf(searchdir, sizeof(searchdir), "%s%s", env, DICTDIR);
    else
	snprintf(searchdir, sizeof(searchdir), "%s", DEFAULTPATH);
#endif

    for (i = 1; i < NUMPARTS + 1; i++) {
	sprintf(tmpbuf, DATAFILE, searchdir, partnames[i]);
	if((datafps[i] = fopen(tmpbuf, "r")) == NULL) {
	    sprintf(msgbuf,
		    "WordNet library error: Can't open datafile(%s)\n",
		    tmpbuf);
	    display_message(msgbuf);
	    openerr = -1;
	}
	sprintf(tmpbuf, INDEXFILE, searchdir, partnames[i]);
	if((indexfps[i] = fopen(tmpbuf, "r")) == NULL) {
	    sprintf(msgbuf,
		    "WordNet library error: Can't open indexfile(%s)\n",
		    tmpbuf);
	    display_message(msgbuf);
	    openerr = -1;
	}
    }

    /* This file isn't used by the library and doesn't have to
       be present.  No error is reported if the open fails. */

    sprintf(tmpbuf, SENSEIDXFILE, searchdir);
    sensefp = fopen(tmpbuf, "r");

    /* If this file isn't present, the runtime code will skip printint out
       the number of times each sense was tagged. */

    sprintf(tmpbuf, CNTLISTFILE, searchdir);
    cntlistfp = fopen(tmpbuf, "r");

    /* This file doesn't have to be present.  No error is reported if the
       open fails. */

    sprintf(tmpbuf, KEYIDXFILE, searchdir);
    keyindexfp = fopen(tmpbuf, "r");

    sprintf(tmpbuf, REVKEYIDXFILE, searchdir);
    revkeyindexfp = fopen(tmpbuf, "r");

    sprintf(tmpbuf, VRBSENTFILE, searchdir);
    if ((vsentfilefp = fopen(tmpbuf, "r")) == NULL) {
	sprintf(msgbuf,
"WordNet library warning: Can't open verb example sentence file(%s)\n",
		tmpbuf);
	display_message(msgbuf);
    }

    sprintf(tmpbuf, VRBIDXFILE, searchdir);
    if ((vidxfilefp = fopen(tmpbuf, "r")) == NULL) {
	sprintf(msgbuf,
"WordNet library warning: Can't open verb example sentence index file(%s)\n",
		tmpbuf);
	display_message(msgbuf);
    }

    return(openerr);
}

/* Count the number of underscore or space separated words in a string. */

int cntwords(char *s, char separator)
{
    register int wdcnt = 0;

    while (*s) {
	if (*s == separator || *s == ' ' || *s == '_') {
	    wdcnt++;
	    while (*s && (*s == separator || *s == ' ' || *s == '_'))
		s++;
	} else
	    s++;
    }
    return(++wdcnt);
}

/* Convert string to lower case remove trailing adjective marker if found */

char *strtolower(char *str)
{
    register char *s = str;

    while(*s != '\0') {
	if(*s >= 'A' && *s <= 'Z')
	    *s += 32;
	else if(*s == '(') {
	    *s='\0';
	    break;
	}
	s++;
    }
    return(str);
}

/* Convert string passed to lower case */

char *ToLowerCase(char *str)
{
    register char *s = str;

    while(*s != '\0') {
	if(*s >= 'A' && *s <= 'Z')
	    *s += 32;
	s++;
    }
    return(str);
}

/* Replace all occurences of 'from' with 'to' in 'str' */

char *strsubst(char *str, char from, char to)
{
    register char *p;

    for (p = str; *p != 0; ++p)
	if (*p == from)
	    *p = to;
    return str;
}

/* Return pointer code for pointer type characer passed. */

int getptrtype(char *ptrstr)
{
    register int i;
    for(i = 1; i <= MAXPTR; i++) {
	if(!strcmp(ptrstr, ptrtyp[i]))
	    return(i);
    }
    return(0);
}

/* Return part of speech code for string passed */

int getpos(char *s)
{
    switch (*s) {
    case 'n':
	return(NOUN);
    case 'a':
    case 's':
	return(ADJ);
    case 'v':
	return(VERB);
    case 'r':
	return(ADV);
    default:
	sprintf(msgbuf,
		"WordNet library error: unknown part of speech %s\n", s);
	display_message(msgbuf);
	exit(-1);
    }
}

/* Return synset type code for string passed. */

int getsstype(char *s)
{
    switch (*s) {
    case 'n':
	return(NOUN);
    case 'a':
	return(ADJ);
    case 'v':
	return(VERB);
    case 's':
	return(SATELLITE);
    case 'r':
	return(ADV);
    default:
	sprintf(msgbuf, "WordNet library error: Unknown synset type %s\n", s);
	display_message(msgbuf);
	exit(-1);
    }
}

/* Pass in string for POS, return corresponding integer value */

int StrToPos(char *str)
{
    if (!strcmp(str, "noun"))
	return(NOUN);
    else if (!strcmp(str, "verb"))
	return(VERB);
    else if (!strcmp(str, "adj"))
	return(ADJ);
    else if (!strcmp(str, "adv"))
	return(ADV);
    else {
	return(-1);
    }
}

#define MAX_TRIES	5

/* Find string for 'searchstr' as it is in index file */

char *GetWNStr(char *searchstr, int dbase)
{
    register int i, j, k, offset = 0;
    register char c;
    char *underscore = NULL, *hyphen = NULL, *period = NULL;
    static char strings[MAX_TRIES][WORDBUF];
	
    ToLowerCase(searchstr);

    if (!(underscore = strchr(searchstr, '_')) &&
	!(hyphen = strchr(searchstr, '-')) &&
	!(period = strchr(searchstr, '.')))
	snprintf(strings[0], sizeof(strings[0]), "%s", searchstr);
	return(strings[0]);

    for(i = 0; i < 3; i++)
	snprintf(strings[i], sizeof(strings[i]), "%s", searchstr);
    if (underscore != NULL) strsubst(strings[1], '_', '-');
    if (hyphen != NULL) strsubst(strings[2], '-', '_');
    for(i = j = k = 0; (c = searchstr[i]) != '\0'; i++){
	if(c != '_' && c != '-') strings[3][j++] = c;
	if(c != '.') strings[4][k++] = c;
    }
    strings[3][j] = '\0';
    strings[4][k] = '\0';
	
    for(i = 1; i < MAX_TRIES; i++)
	if(strcmp(strings[0], strings[i]) == 0) strings[i][0] = '\0';
	
    for (i = (MAX_TRIES - 1); i >= 0; i--)
	if (strings[i][0] != '\0')
	    if (bin_search(strings[i], indexfps[dbase]) != NULL)
		offset = i;
	
    return(strings[offset]);
}

/* Return synset for sense key passed. */

SynsetPtr GetSynsetForSense(char *sensekey)
{
    long offset;

    /* Pass in sense key and return parsed sysnet structure */

    if ((offset = GetDataOffset(sensekey)))
	return(read_synset(GetPOS(sensekey),
			   offset,
			   GetWORD(sensekey)));
    else
	return(NULL);
}

/* Find offset of sense key in data file */

long GetDataOffset(char *sensekey)
{
    char *line;

    /* Pass in encoded sense string, return byte offset of corresponding
       synset in data file. */

    if (sensefp == NULL) {
	display_message("WordNet library error: Sense index file not open\n");
	return(0L);
    }
    line = bin_search(sensekey, sensefp);
    if (line) {
	while (*line++ != ' ');
	return(atol(line));
    } else
	return(0L);
}

/* Find polysemy count for sense key passed. */

int GetPolyCount(char *sensekey)
{
    IndexPtr idx;
    int sense_cnt = 0;

    /* Pass in encoded sense string and return polysemy count
       for word in corresponding POS */

    idx = index_lookup(GetWORD(sensekey), GetPOS(sensekey));
    if (idx) {
	sense_cnt = idx->sense_cnt;
	free_index(idx);
    }
    return(sense_cnt);
}

/* Return word part of sense key */
char *GetWORD(char *sensekey)
{
    static char word[100];
    int i = 0;

    /* Pass in encoded sense string and return WORD */

    while ((word[i++] = *sensekey++) != '%');
    word[i - 1] = '\0';
    return(word);
}

/* Return POS code for sense key passed. */

int GetPOS(char *sensekey)
{
    int pos;

    /* Pass in encoded sense string and return POS */

    while (*sensekey++ != '%');	/* skip over WORD */
    sscanf(sensekey, "%1d", &pos);
    return(pos == SATELLITE ? ADJ : pos);
}

/* Reconstruct synset from synset pointer and return ptr to buffer */

char *FmtSynset(SynsetPtr synptr, int defn)
{
    int i;
    static char synset[SMLINEBUF];

    synset[0] = '\0';

    if (fileinfoflag)
	append_fmt(synset, sizeof(synset), "<%s> ", lexfiles[synptr->fnum]);

    append_str(synset, sizeof(synset), "{ ");
    for (i = 0; i < (synptr->wcount - 1); i++)
	append_fmt(synset, sizeof(synset), "%s, ", synptr->words[i]);

    append_str(synset, sizeof(synset), synptr->words[i]);

    if (defn && synptr->defn)
	append_fmt(synset, sizeof(synset), " (%s) ", synptr->defn);

    append_str(synset, sizeof(synset), " }");
    return(synset);
}

/* Convert WordNet sense number passed of IndexPtr entry to sense key. */
char *WNSnsToStr(IndexPtr idx, int sense)
{
    SynsetPtr sptr, adjss;
    char sensekey[512], lowerword[256];
    int j, sstype, pos;

    pos = getpos(idx->pos);
    sptr = read_synset(pos, idx->offset[sense - 1], "");

    if ((sstype = getsstype(sptr->pos)) == SATELLITE) {
	for (j = 0; j < sptr->ptrcount; j++) {
	    if (sptr->ptrtyp[j] == SIMPTR) {
		adjss = read_synset(sptr->ppos[j],sptr->ptroff[j],"");
		sptr->headword = malloc (strlen(adjss->words[0]) + 1);
		assert(sptr->headword);
		snprintf(sptr->headword,
			 strlen(adjss->words[0]) + 1,
			 "%s",
			 adjss->words[0]);
		strtolower(sptr->headword);
		sptr->headsense = adjss->lexid[0];
		free_synset(adjss); 
		break;
	    }
	}
    }

    for (j = 0; j < sptr->wcount; j++) {
	snprintf(lowerword, sizeof(lowerword), "%s", sptr->words[j]);
	strtolower(lowerword);
	if(!strcmp(lowerword, idx->wd))
	    break;
    }

    if (j == sptr->wcount) {
	free_synset(sptr);
	return(NULL);
    }

    if (sstype == SATELLITE) 
	sprintf(sensekey,"%s%%%-1.1d:%-2.2d:%-2.2d:%s:%-2.2d",
		idx->wd, SATELLITE, sptr->fnum,
		sptr->lexid[j], sptr->headword,sptr->headsense);
    else 
	sprintf(sensekey,"%s%%%-1.1d:%-2.2d:%-2.2d::",
		idx->wd, pos, sptr->fnum, sptr->lexid[j]);

    free_synset(sptr);
    return(strdup(sensekey));
}

/* Search for string and/or baseform of word in database and return
   index structure for word if found in database. */

IndexPtr GetValidIndexPointer(char *word, int pos)
{
    IndexPtr idx;
    char *morph_word;

    idx = getindex(word, pos);

    if (idx == NULL) {
	if ((morph_word = morphstr(word, pos)) != NULL)
	    while (morph_word) {
		if ((idx = getindex(morph_word, pos)) != NULL) break;
		morph_word = morphstr(NULL, pos);
	    }
    }
    return (idx);
}

/* Return sense number in database for word and lexsn passed. */

int GetWNSense(char *word, char *lexsn)
{
    SnsIndexPtr snsidx;
    char buf[256];

    sprintf(buf, "%s%%%s", word, lexsn); /* create sensekey */
    if ((snsidx = GetSenseIndex(buf)) != NULL)
	return(snsidx->wnsense);
    else
	return(0);
}

/* Return parsed sense index entry for sense key passed. */

SnsIndexPtr GetSenseIndex(char *sensekey)
{
    char *line;
    char buf[256], loc[9];
    SnsIndexPtr snsidx = NULL;

    if ((line = bin_search(sensekey, sensefp)) != NULL) {
	snsidx = (SnsIndexPtr)malloc(sizeof(SnsIndex));
	assert(snsidx);
	sscanf(line, "%255s %8s %d %d\n",
	       buf,
	       loc,
	       &snsidx->wnsense,
	       &snsidx->tag_cnt);
	snsidx->sensekey = malloc(strlen(buf) + 1);
	assert(snsidx->sensekey);
	snprintf(snsidx->sensekey, strlen(buf) + 1, "%s", buf);
	snsidx->loc = atol(loc);
	/* Parse out word from sensekey to make things easier for caller */
	snsidx->word = strdup(GetWORD(snsidx->sensekey));
	assert(snsidx->word);
	snsidx->nextsi = NULL;
    }
    return(snsidx);
}

/* Return number of times sense is tagged */

int GetTagcnt(IndexPtr idx, int sense) 
{
    char *sensekey, *line;
    char buf[256];
    int snum, cnt = 0;

    if (cntlistfp) {
      
	sensekey = WNSnsToStr(idx, sense);
	if ((line = bin_search(sensekey, cntlistfp)) != NULL) {
	    sscanf(line, "%255s %d %d", buf, &snum, &cnt);
	}
	free(sensekey);
    }

    return(cnt);
}

void FreeSenseIndex(SnsIndexPtr snsidx)
{
    if (snsidx) {
	free(snsidx->word);
	free(snsidx);
    }
}

char *GetOffsetForKey(unsigned int key)
{
    unsigned int rkey;
    char ckey[7];
    static char loc[11] = "";
    char *line;
    char searchdir[256], tmpbuf[256];

    /* Try to open file in case wn_init wasn't called */

    if (!keyindexfp) {
	snprintf(searchdir, sizeof(searchdir), "%s", SetSearchdir());
	snprintf(tmpbuf, sizeof(tmpbuf), KEYIDXFILE, searchdir);
	keyindexfp = fopen(tmpbuf, "r");
    }
    if (keyindexfp) {
	snprintf(ckey, sizeof(ckey), "%6.6u", key);
	if ((line = bin_search(ckey, keyindexfp)) != NULL) {
	    sscanf(line, "%u %10s", &rkey, loc);
	    return(loc);
	}
    } 
    return(NULL);
}


unsigned int GetKeyForOffset(char *loc)
{
    unsigned int key;
    char rloc[11] = "";
    char *line;
    char searchdir[256], tmpbuf[256];

    /* Try to open file in case wn_init wasn't called */

    if (!revkeyindexfp) {
	snprintf(searchdir, sizeof(searchdir), "%s", SetSearchdir());
	snprintf(tmpbuf, sizeof(tmpbuf), REVKEYIDXFILE, searchdir);
	revkeyindexfp = fopen(tmpbuf, "r");
    }
    if (revkeyindexfp) {
	if ((line = bin_search(loc, revkeyindexfp)) != NULL) {
	    sscanf(line, "%10s %u", rloc, &key);
	    return(key);
	}
    }
    return(0);
}

char *SetSearchdir()
{
    static char searchdir[256];
    char *env;

    /* Find base directory for database.  If set, use WNSEARCHDIR.
       If not set, check for WNHOME/dict, otherwise use DEFAULTPATH. */

    if ((env = getenv("WNSEARCHDIR")) != NULL)
	snprintf(searchdir, sizeof(searchdir), "%s", env);
    else if ((env = getenv("WNHOME")) != NULL)
	snprintf(searchdir, sizeof(searchdir), "%s%s", env, DICTDIR);
    else
	snprintf(searchdir, sizeof(searchdir), "%s", DEFAULTPATH);

    return(searchdir);
}

int default_display_message(char *msg)
{
    (void)msg;
    return(-1);
}

/* 
** Wrapper functions for strstr that allow you to retrieve each
** occurance of a word within a longer string, not just the first.
**
** strstr_init is called with the same arguments as normal strstr,
** but does not return any value.
**
** strstr_getnext returns the position offset (not a pointer, as does
** normal strstr) of the next occurance, or -1 if none remain.
*/

void strstr_init (char *string, char *word) {
   strstr_word = word;
   strstr_stringstart = string;
   strstr_stringcurrent = string;
}

int strstr_getnext (void) {
   char *loc = strstr (strstr_stringcurrent, strstr_word);
   if (loc == NULL) return -1;
   strstr_stringcurrent = loc + 1;
   return (loc - strstr_stringstart);
}
