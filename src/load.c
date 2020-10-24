/**************************************************************************
*** Project: SGF Syntax Checker & Converter
***	File:	 load.c
***
*** Copyright (C) 1996-2018 by Arno Hollosi
*** (see 'main.c' for more copyright information)
***
*** Notes:	Almost all routines in this file return either
***			- FALSE (NULL)	for reaching the end of file (UNEXPECTED_EOF)
***			- TRUE (value)	for success (or for: 'continue with parsing')
***			- exit program on a fatal error (e.g. if malloc() fails)
*** 		Almost all routines get passed a current SGFInfo structure
***			and read/modify sgfc->current
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "all.h"
#include "protos.h"

/* defines for SkipText */

#define INSIDE	0u
#define OUTSIDE 1u
#define P_ERROR	2u


/**************************************************************************
*** Function:	SkipSGFText
***				Wrapper for SkipText, using sgfc->current as buffer
*** Parameters: brk		... break char
***				mode	... see SkipText
*** Returns:	TRUE or FALSE
**************************************************************************/

static int SkipSGFText(struct SGFInfo *sgfc, char brk, unsigned int mode)
{
	char *pos = SkipText(sgfc, sgfc->current, sgfc->b_end, brk, mode);
	if (!pos)
	{
		sgfc->current = sgfc->b_end;	/* Apparently we reached the buffer end */
		return(FALSE);
	}
	sgfc->current = pos;
	return TRUE;
}	


/**************************************************************************
*** Function:	SkipText
***				Skips all chars until break char is detected or
***				end of buffer is reached
*** Parameters: s		... pointer to buffer start
***				e		... pointer to buffer end
***							(may be NULL -> buffer terminated with '\0')
***				end		... break char
***				mode	... INSIDE  : do escaping ('\')
***							OUTSIDE : detect faulty chars
***							P_ERROR : print UNEXPECTED_EOF error message
*** Returns:	pointer to break char or NULL
**************************************************************************/

char *SkipText(struct SGFInfo *sgfc, char *s, const char *e, char end, unsigned int mode)
{
	while(!e || s < e)
	{
		if(*s == end)			/* found break char? */
			return(s);

		if(!e && !*s)			/* end of buffer? */
			return(NULL);

		if(mode & OUTSIDE)		/* '.. [] ..' */
		{
			if(!isspace(*s))
				PrintError(E_ILLEGAL_OUTSIDE_CHAR, sgfc, s, TRUE);
		}
		else					/* '[ .... ]' */
		{
			if(*s == '\\')		/* escaping */
			{
				if(!e && !*(s+1))	/* can't escape '\0' char if !e */
					return(NULL);
				s += 2;
				continue;
			}
		}

		s++;
	}

	if(mode & P_ERROR)
		PrintError(E_UNEXPECTED_EOF, sgfc, s);

	return(NULL);
}


/**************************************************************************
*** Function:	GetNextChar
***				Sets sgfc->current to next meaningful SGF char
***				Detects bad chars and prints an error message if desired
***				Chars: ( ) ; [ uppercase
***					In last case sgfc->current points to beginning of text
***				 	(leading lowercase possible)
*** Parameters: print_error ... print error message
***				errc ... error code for printing on failure (or E_NO_ERROR)
*** Returns:	TRUE or FALSE
**************************************************************************/

static int GetNextChar(struct SGFInfo *sgfc, int print_error, U_LONG errc)
{
	int lc = 0;

	while(!SGF_EOF)
	{
		switch(*sgfc->current)
		{
			case ';':
			case '(':
			case ')':
			case '[':	if(print_error && lc)
							PrintError(E_ILLEGAL_OUTSIDE_CHARS, sgfc, sgfc->current-lc, TRUE, lc);
						return(TRUE);

			default:	if(isupper(*sgfc->current))
						{
							sgfc->current -= lc;	/* set back to start */
							return(TRUE);			/* of text */
						}

						if(islower(*sgfc->current))
							lc++;
						else		/* !islower && !isupper */
						{
							if(print_error)
							{
								if(lc)
									PrintError(E_ILLEGAL_OUTSIDE_CHARS, sgfc, sgfc->current-lc, TRUE, lc);
								if(!isspace(*sgfc->current))
									PrintError(E_ILLEGAL_OUTSIDE_CHAR, sgfc, sgfc->current, TRUE);
							}
							lc = 0;
						}
						sgfc->current++;

						break;
		}
	}

	if(errc != E_NO_ERROR)
		PrintError(errc, sgfc, sgfc->current);
	return(FALSE);
}


/**************************************************************************
*** Function:	SkipValues
***				Skips all property values of current value list
*** Parameters: print_error ... print error message
***								(passed on to GetNextChar)
*** Returns:	TRUE or FALSE
**************************************************************************/

static int SkipValues(struct SGFInfo *sgfc, int print_error)
{
	if(!SkipSGFText(sgfc, '[', OUTSIDE|P_ERROR))	/* search start of first value */
		return(FALSE);

	while(*sgfc->current == '[')
	{
		if(!SkipSGFText(sgfc, ']', INSIDE|P_ERROR))	/* skip value */
			return(FALSE);

		sgfc->current++;

		/* search next value start */
		if(!GetNextChar(sgfc, print_error, E_UNEXPECTED_EOF))
			return(FALSE);
	}

	return(TRUE);
}


/**************************************************************************
*** Function:	CopyValue
***				Copies property value into new buffer and
***				deletes all CTRL chars and adds a '\0' at the end ->
***				value becomes real C string
*** Parameters: d		... destination buffer
***				s		... source buffer
***				size	... number of bytes to copy
***				printerror ... to print or not to print
*** Returns:	-
**************************************************************************/

void CopyValue(struct SGFInfo *sgfc, char *d, const char *s, size_t size, int printerror)
{
	while(size--)
	{
		if(*s)
			*d++ = *s;
		else if(printerror)
			PrintError(W_CTRL_BYTE_DELETED, sgfc, s);
		s++;
	}
	*d = 0;
}


/**************************************************************************
*** Function:	Add_PropValue
***				Adds a value to the property (inits structure etc.)
*** Parameters: p		... pointer to property
***				buffer	... pointer to position in source buffer (or NULL)
***				value	... pointer to first value
***				size	... length of first value (excluding any 0 bytes)
***				value2	... pointer to second value (or NULL)
***				size2	... length of second value (excluding any 0 bytes)
*** Returns:	pointer to PropValue structure
***				(exits on fatal error)
**************************************************************************/

struct PropValue *Add_PropValue(struct SGFInfo *sgfc,
								struct Property *p, char *buffer,
								const char *value, size_t size,
								const char *value2, size_t size2)
{
	struct PropValue *newv;

	SaveMalloc(struct PropValue *, newv, sizeof(struct PropValue), "property value structure")

	if(value)
	{
		/* +2 because Parse_Float may add 1 char and for trailing '\0' byte */
		SaveMalloc(char *, newv->value, size+2, "property value buffer")
		CopyValue(sgfc, newv->value, value, size, TRUE);
	}
	else
		newv->value = NULL;

	if(value2)
	{
		SaveMalloc(char *, newv->value2, size2+2, "property value2 buffer")
		CopyValue(sgfc, newv->value2, value2, size2, TRUE);
	}
	else
		newv->value2 = NULL;

	newv->buffer = buffer;

	AddTail(&p->value, newv);				/* add value to property */
	return(newv);
}


/**************************************************************************
*** Function:	NewValue
***				Adds one property value to the given property
*** Parameters: p		... pointer to property
***				flags	... property flags (as in sgf_token[])
*** Returns:	TRUE or FALSE
**************************************************************************/

static int NewValue(struct SGFInfo *sgfc, struct Property *p, U_SHORT flags)
{
	char *s, *t, *buffer;

	buffer = sgfc->current++;
	s = sgfc->current;					/* points to char after '[' */

	if(!SkipSGFText(sgfc, ']', INSIDE|P_ERROR))
		return(FALSE);					/* value isn't added */

	sgfc->current++;					/* points now to char after ']' */

	if(flags & (PVT_COMPOSE|PVT_WEAKCOMPOSE))	/* compose datatype? */
	{
		t = SkipText(sgfc, s, sgfc->current, ':', INSIDE);
		if(!t)
		{
			if(flags & PVT_WEAKCOMPOSE)	/* no compose -> parse as normal */
				Add_PropValue(sgfc, p, buffer, s, sgfc->current - s - 1, NULL, 0);
			else						/* not weak -> error */
				PrintError(E_COMPOSE_EXPECTED, sgfc, s-1, p->idstr);
		}
		else	/* composed value */
			Add_PropValue(sgfc, p, buffer, s, t - s, t+1, sgfc->current - t - 2);
	}
	else
		Add_PropValue(sgfc, p, buffer, s, sgfc->current - s - 1, NULL, 0);

	return(TRUE);
}


/**************************************************************************
*** Function:	Add_Property
***				Creates new property structure and adds it to the node
*** Parameters: n		... node to which property belongs to
***				id		... tokenized ID of property
***				id_buf	... pointer to property ID string
***				idstr	... ID string
*** Returns:	pointer to new Property structure
***				(exits on fatal error)
**************************************************************************/

struct Property *Add_Property(struct Node *n, token id, char *id_buf, char *idstr)
{
	struct Property *newp;
	char *str;

	SaveMalloc(struct Property *, newp, sizeof(struct Property), "property structure")

	newp->id = id;							/* init property structure */

	if(id == TKN_UNKNOWN)
	{
		SaveMalloc(char *, str, strlen(idstr)+2, "ID string")
		strcpy(str, idstr);
		newp->idstr = str;
	}
	else
		newp->idstr = sgf_token[id].id;

	newp->priority = sgf_token[id].priority;
	newp->flags = sgf_token[id].flags;		/* local copy */
	newp->buffer = id_buf;

	newp->value = NULL;
	newp->valend = NULL;

	if(n)
		Enqueue(&n->prop, newp);				/* add to node (sorted!) */
	return(newp);
}


/**************************************************************************
*** Function:	NewProperty
***				Adds one property (id given) to a node
*** Parameters: n		... node to which property belongs to
***				id		... tokenized ID of property
***				id_buf	... pointer to property ID
***				idstr	... ID string
*** Returns:	TRUE or FALSE
**************************************************************************/

static int NewProperty(struct SGFInfo *sgfc, struct Node *n, token id, char *id_buf, char *idstr)
{
	struct Property *newp;
	int ret = TRUE;
	char *too_many = NULL;

	if(!n)	return(TRUE);

	newp = Add_Property(n, id, id_buf, idstr);

	while(TRUE)
	{
		if(!NewValue(sgfc, newp, newp->flags))	/* add value */
		{
			ret = FALSE;	break;
		}

		if(!GetNextChar(sgfc, TRUE, E_VARIATION_NESTING))
		{
			ret = FALSE;	break;
		}

		if(*sgfc->current == '[')	/* more than one value? */
		{
			if(newp->flags & PVT_LIST)
				continue;
			else					/* error, as only one value allowed */
			{
				if (!too_many)
					too_many = sgfc->current;
				if (!newp->value || !strlen(newp->value->value))	/* if previous value is empty, */
				{									/* then use the later value */
					Del_PropValue(newp, newp->value);
					continue;
				}
				SkipValues(sgfc, FALSE);
				break;
			}
		}
		else						/* reached end of value list */
			break;
	}

	if (too_many)
		PrintError(E_TOO_MANY_VALUES, sgfc, too_many, idstr);

	if(!newp->value)				/* property has values? */
		Del_Property(n, newp);		/* no -> delete it */

	return(ret);
}


/**************************************************************************
*** Function:	MakeProperties
***				builds property-list from a given SGF string
*** Parameters: n	... node to which properties should belong
*** Returns:	TRUE or FALSE
**************************************************************************/

static int MakeProperties(struct SGFInfo *sgfc, struct Node *n)
{
	char *id, propid[100];
	int i;

	while(TRUE)
	{
		if(!GetNextChar(sgfc, TRUE, E_VARIATION_NESTING))
			return(FALSE);

		switch(*sgfc->current)
		{
			case '(':	/* ( ) ; indicate node end */
			case ')':
			case ';':	return(TRUE);
			case ']':	PrintError(E_ILLEGAL_OUTSIDE_CHAR, sgfc, sgfc->current, TRUE);
						sgfc->current++;
						break;
			case '[':	PrintError(E_VALUES_WITHOUT_ID, sgfc, sgfc->current);
						if(!SkipValues(sgfc, TRUE))
							return(FALSE);
						break;

			default:	/* isalpha */

				id = sgfc->current;
				i = 0;

				while(!SGF_EOF)
				{
					if(isupper(*sgfc->current))
					{
						if(i < 100)						/* max. 100 uc chars */
							propid[i++] = *sgfc->current;
					}
					else
						if(!islower(*sgfc->current))	/* end of PropID? */
						{
							if(i >= 100)
								break;

							if(!GetNextChar(sgfc, TRUE, E_UNEXPECTED_EOF))
								return(FALSE);

							/* propID in propid[], sgfc->current points to '[' */
							propid[i] = 0;

							if(*sgfc->current != '[')
							{
								PrintError(E_NO_PROP_VALUES, sgfc, id, propid);
								break;
							}

							if(i > 2)
								PrintError(WS_LONG_PROPID, sgfc, sgfc->current, propid);

							for(i = 1; sgf_token[i].id; i++)
								if(!strcmp(propid, sgf_token[i].id))
									break;

							if(!sgf_token[i].id)	/* EOF sgf_token */
							{
								if(!sgfc->options->keep_unknown_props)
								{
									PrintError(WS_UNKNOWN_PROPERTY, sgfc, id, propid, "deleted");
									if(!SkipValues(sgfc, TRUE))
										return(FALSE);
									break;
								}
								else
								{
									PrintError(WS_UNKNOWN_PROPERTY, sgfc, id, propid, "found");
									i = TKN_UNKNOWN;
								}
							}

							if(sgf_token[i].flags & DELETE_PROP)
							{
								PrintError(W_PROPERTY_DELETED, sgfc, id, "", propid);
								if(!SkipValues(sgfc, TRUE))
									return(FALSE);
								break;
							}

							if(!NewProperty(sgfc, n, (token)i, id, propid))
								return(FALSE);
							break;
						}
					sgfc->current++;
				}

				if(SGF_EOF)
				{
					PrintError(E_UNEXPECTED_EOF, sgfc, sgfc->current);
					return(FALSE);
				}

				if(i >= 100)
				{
					PrintError(E_PROPID_TOO_LONG, sgfc, id, sgfc->current);
					if(!SkipValues(sgfc, TRUE))
						return(FALSE);
				}
				break;
		}
	}
}


/**************************************************************************
*** Function:	NewNode
***				Inserts a new node into the current SGF tree
*** Parameters: parent	 ... parent node
***				newchild ... create a new child for parent node
***							 (insert an empty node into the tree)
*** Returns:	pointer to node or NULL (success / error)
***				(exits on fatal error)
**************************************************************************/

struct Node *NewNode(struct SGFInfo *sgfc, struct Node *parent, int newchild)
{
	struct Node *newn, *hlp;

	SaveMalloc(struct Node *, newn, sizeof(struct Node), "node structure")

	newn->parent	= parent;		/* init node structure */
	newn->child		= NULL;
	newn->sibling	= NULL;
	newn->prop		= NULL;
	newn->last		= NULL;
	newn->buffer	= sgfc->current;

	AddTail(sgfc, newn);

	if(parent)						/* no parent -> root node */
	{
		if(newchild)				/* insert node as new child of parent */
		{
			newn->child = parent->child;
			parent->child = newn;

			hlp = newn->child;		/* set new parent of children */
			while(hlp)
			{
				hlp->parent = newn;
				hlp = hlp->sibling;
			}
		}
		else
		{
			if(!parent->child)			/* parent has no child? */
				parent->child = newn;
			else						/* parent has a child already */
			{							/* -> insert as sibling */
				hlp = parent->child;
				while(hlp->sibling)
					hlp = hlp->sibling;
				hlp->sibling = newn;
			}
		}
	}
	else							/* new root node */
	{
		if(!sgfc->root)				/* first root? */
			sgfc->root = newn;
		else
		{
			hlp = sgfc->root;		/* root sibling */
			while(hlp->sibling)
				hlp = hlp->sibling;
			hlp->sibling = newn;
		}
	}

	if(!newchild)
		if(!MakeProperties(sgfc, newn))
			return(NULL);

	return(newn);
}


/**************************************************************************
*** Function:	BuildSGFTree
***				Recursive function to build up the sgf tree structure
*** Parameters: r ... tree root
*** Returns:	TRUE or FALSE on success/error
**************************************************************************/

static int BuildSGFTree(struct SGFInfo *sgfc, struct Node *r)
{
	int end_tree = 0, empty = 1;

	while(GetNextChar(sgfc, TRUE, E_VARIATION_NESTING))
	{
		sgfc->current++;
		switch(*(sgfc->current-1))
		{
			case ';':	if(end_tree)
						{
							PrintError(E_NODE_OUSIDE_VAR, sgfc, sgfc->current-1);
							sgfc->current--;
							if(!BuildSGFTree(sgfc, r))
								return(FALSE);
							end_tree = 1;
						}
						else
						{
							empty = 0;
							r = NewNode(sgfc, r, FALSE);
							if(!r)
								return(FALSE);
						}
						break;
			case '(':	if(empty)
							PrintError(E_VARIATION_START, sgfc, sgfc->current-1);
						else
						{
							if(!BuildSGFTree(sgfc, r))
								return(FALSE);
							end_tree = 1;
						}
						break;
			case ')':	if(empty)
							PrintError(E_EMPTY_VARIATION, sgfc, sgfc->current-1);
						return(TRUE);

			default:	if(empty)		/* assume there's a missing ';' */
						{
							PrintError(E_MISSING_NODE_START, sgfc, sgfc->current-1);
							empty = 0;
							sgfc->current--;
							r = NewNode(sgfc, r, FALSE);
							if(!r)
								return(FALSE);
						}
						else
							PrintError(E_ILLEGAL_OUTSIDE_CHAR, sgfc, sgfc->current-1, TRUE);
						break;
		}
	}

	return(FALSE);
}


/**************************************************************************
*** Function:	FindStart
***				sets sgfc->current to '(' of start mark '(;'
*** Parameters: firsttime ... search for the first time?
***							  (TRUE -> if search fails -> fatal error)
*** Returns:	FALSE ... ok/ TRUE ... missing ';'  (exits on fatal error)
**************************************************************************/

static int FindStart(struct SGFInfo *sgfc, int firsttime)
{
	int warn = 0, o, c;
	char *tmp;

	while(!SGF_EOF)
	{
		/* search for '[' (lc) (lc) ']' */

		if((sgfc->current + 4 <= sgfc->b_end) &&
		  (*sgfc->current == '['))
			if(islower(*(sgfc->current+1)) && islower(*(sgfc->current+2)) &&
			  (*(sgfc->current+3) == ']'))
			{
				if(!warn)		/* print warning only once */
				{
					PrintError(W_SGF_IN_HEADER, sgfc, sgfc->current);
					warn = 1;
				}

				if(!firsttime)
					PrintError(E_ILLEGAL_OUTSIDE_CHARS, sgfc, sgfc->current, TRUE, 4);

				sgfc->current += 4;	/* skip '[aa]' */
				continue;
			}

		if(*sgfc->current == '(')	/* test for start mark '(;' */
		{
			tmp = sgfc->current + 1;
			while((tmp < sgfc->b_end) && isspace(*tmp))
				tmp++;

			if(tmp == sgfc->b_end)
				break;

			if(*tmp == ';')
				return(FALSE);
			else
			{
				o = c = 0;

				if(sgfc->options->findstart == 1)
				{		/* found a '(' but no ';' -> might be a missing ';' */
					tmp = sgfc->current + 1;
					while((tmp != sgfc->b_end) && *tmp != ')' && *tmp != '(')
					{
						if(*tmp == '[')		o++;
						if(*tmp == ']')		c++;
						tmp++;
					}
				}

				if((sgfc->options->findstart == 3) ||
				  ((o >= 2) && (o >= c) && (o-c <= 1)))
				{
					PrintError(E_MISSING_SEMICOLON, sgfc, sgfc->current);
					*sgfc->current = ';';
					return(TRUE);
				}
			}
		}
		else
			if(!firsttime && !isspace(*sgfc->current))
				PrintError(E_ILLEGAL_OUTSIDE_CHAR, sgfc, sgfc->current, TRUE);

		sgfc->current++;
	}

	if(firsttime)
		PrintFatalError(FE_NO_SGFDATA, sgfc);

	return(FALSE);
}


/**************************************************************************
*** Function:	LoadSGF
***				Loads a SGF file into the memory and inits all
***				necessary information in sgfinfo-structure
*** Parameters: sgf ... pointer to SGFInfo structure
*** Returns:	- (exits on fatal error)
**************************************************************************/

void LoadSGF(struct SGFInfo *sgf, char *name)
{
	long size;
	FILE *file;

	file = fopen(name, "rb");
	if(!file)
		PrintFatalError(FE_SOURCE_OPEN, sgf, name);

	fseek(file, 0, SEEK_END);
	size = ftell(file);

	if(size == -1L)
		goto load_error;

	sgf->buffer = (char *) malloc((size_t) size);
	if(!sgf->buffer)
	{
		fclose(file);
		PrintFatalError(FE_OUT_OF_MEMORY, sgf, "source file buffer");
	}

	if(fseek(file, 0, SEEK_SET) == -1L)
		goto load_error;
	if(size != (long)fread(sgf->buffer, 1, (size_t)size, file))
		goto load_error;

	sgf->b_end   = sgf->buffer + size;
	sgf->current = sgf->buffer;
	fclose(file);
	LoadSGFFromFileBuffer(sgf);
	return;

load_error:
	fclose(file);
	PrintFatalError(FE_SOURCE_READ, sgf, name);
}


/**************************************************************************
*** Function:	LoadSGFFromFileBuffer
***				Seeks start of SGF data and builds basic tree structure
***             Assumes sgf->buffer and sgf->b_end is already set
*** Parameters: sgf ... pointer to SGFInfo structure
*** Returns:	- (exits on fatal error)
**************************************************************************/

void LoadSGFFromFileBuffer(struct SGFInfo *sgfc)
{
	int miss;

	miss = FindStart(sgfc, TRUE);		/* skip junk in front of '(;' */
	sgfc->start = sgfc->current;

	while(!SGF_EOF)
	{
		if(!miss)
			sgfc->current++;			/* skip '(' */
		if(!BuildSGFTree(sgfc, NULL))
			break;
		miss = FindStart(sgfc, FALSE);	/* skip junk in front of '(;' */
	}

	PrintError(E_NO_ERROR, sgfc);		/* flush accumulated messages */
}
