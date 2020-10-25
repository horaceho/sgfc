/**************************************************************************
*** Project: SGF Syntax Checker & Converter
***	File:	 execute.c
***
*** Copyright (C) 1996-2018 by Arno Hollosi
*** (see 'main.c' for more copyright information)
***
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "protos.h"


/**************************************************************************
*** Function:	Make_Capture
***				Captures stones on marked by Recursive_Capture
*** Parameters: x ... x position
***				y ... y position
*** Returns:	-
**************************************************************************/

static void Make_Capture(int x, int y, struct BoardStatus *st)
{
	if(st->path_board[MXY(x,y)] != st->path_num)
		return;

	st->board[MXY(x,y)] = EMPTY;
	st->path_board[MXY(x,y)] = 0;

	/* recursive calls */
	if(x > 0)	Make_Capture(x-1, y, st);
	if(y > 0)	Make_Capture(x, y-1, st);
	if(x < st->bwidth-1)	Make_Capture(x+1, y, st);
	if(y < st->bheight-1)	Make_Capture(x, y+1, st);
}


/**************************************************************************
*** Function:	Recursive_Capture
***				Checks for capturing stones
***				marks its path on path_board with path_num
*** Parameters: color ... enemy color
***				x	  ... x position
***				y	  ... y position
*** Returns:	TRUE if capture / FALSE if liberty found
**************************************************************************/

static int Recursive_Capture(int color, int x, int y, struct BoardStatus *st)
{
	if(!st->board[MXY(x,y)])
		return(FALSE);		/* liberty found */

	if(st->board[MXY(x,y)] == color)
		return(TRUE);		/* enemy found */

	if(st->path_board[MXY(x,y)] == st->path_num)
		return(TRUE);

	st->path_board[MXY(x,y)] = st->path_num;

	/* recursive calls */
	if(x > 0 			 && !Recursive_Capture(color, x-1, y, st))	return(FALSE);
	if(y > 0 			 && !Recursive_Capture(color, x, y-1, st))	return(FALSE);
	if(x < st->bwidth-1  && !Recursive_Capture(color, x+1, y, st))	return(FALSE);
	if(y < st->bheight-1 && !Recursive_Capture(color, x, y+1, st))	return(FALSE);

	return(TRUE);
}


/**************************************************************************
*** Function:	Capture_Stones
***				Calls Recursive_Capture and does capture if necessary
*** Parameters: st	  ... board status
***				color ... color of enemy stones
***				x	  ... column
***				y	  ... row
*** Returns:	-
**************************************************************************/

static void Capture_Stones(struct BoardStatus *st, int color, int x, int y)
{
	if(x < 0 || y < 0 || x >= st->bwidth || y >= st->bheight)
		return;		/* not on board */

	if(!st->board[MXY(x,y)] || st->board[MXY(x,y)] == color)
		return;		/* liberty or friend found */

	st->path_num++;

	if(Recursive_Capture(color, x, y, st))	/* made prisoners? */
		Make_Capture(x, y, st);				/* ->update board position */
}



/**************************************************************************
*** Function:	Do_Move
***				Executes a move and check for B/W in one node
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Move(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	int x, y;
	unsigned char color;

	if(sgfc->info->GM != 1)		/* game != Go? */
		return(TRUE);

	if(st->annotate & ST_MOVE)	/* there's a move already? */
	{
		PrintError(E_TWO_MOVES_IN_NODE, sgfc, p->buffer);
		Split_Node(sgfc, n, 0, p->id, TRUE);
		return(TRUE);
	}

	st->annotate |= ST_MOVE;

	if(!strlen(p->value->value))	/* pass move */
		return(TRUE);

	x = DecodePosChar(p->value->value[0]) - 1;
	y = DecodePosChar(p->value->value[1]) - 1;
	color = (char)sgf_token[p->id].data;

	if(st->board[MXY(x,y)])
		PrintError(WS_ILLEGAL_MOVE, sgfc, p->buffer);

	st->board[MXY(x,y)] = color;
	Capture_Stones(st, color, x-1, y);		/* check for prisoners */
	Capture_Stones(st, color, x+1, y);
	Capture_Stones(st, color, x, y-1);
	Capture_Stones(st, color, x, y+1);
	Capture_Stones(st, ~color, x, y);		/* check for suicide */

	if(sgfc->options->del_move_markup)		/* if del move markup, then */
	{										/* mark move position as markup */
		st->markup[MXY(x,y)] |= ST_MARKUP;	/* -> other markup at this */
		st->mrkp_chngd = TRUE;				/* position will be deleted */
	}

	return(TRUE);
}


/**************************************************************************
*** Function:	Do_AddStones
***				Executes property checks for unique positions
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Addstones(struct SGFInfo *sgf, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	int x, y;
	char color;
	struct PropValue *v, *w;
	struct Property h;

	if(sgf->info->GM != 1)		/* game != Go? */
		return(TRUE);

	h.value = NULL;
	h.valend = NULL;

	color = (char)sgf_token[p->id].data;

	v = p->value;
	while(v)
	{
		x = DecodePosChar(v->value[0]) - 1;
		y = DecodePosChar(v->value[1]) - 1;
	
		if(st->markup[MXY(x,y)] & ST_ADDSTONE)
		{
			PrintError(E_POSITION_NOT_UNIQUE, sgf, v->buffer, "AddStone", p->idstr);
			v = Del_PropValue(p, v);
			continue;
		}

		st->markup[MXY(x,y)] |= ST_ADDSTONE;
		st->mrkp_chngd = TRUE;

		if(st->board[MXY(x,y)] == color)		/* Add property is redundant */
		{
			w = v->next;
			Delete(&p->value, v);
			AddTail(&h.value, v);
			v = w;
			continue;
		}

		st->board[MXY(x,y)] = color;
		v = v->next;
	}

	if(h.value)
	{
		x = PrintError(WS_ADDSTONE_REDUNDANT, sgf, p->buffer, p->idstr);

		v = h.value;
		while(v)
		{
			if(x)	fprintf(E_OUTPUT, "[%s]", v->value);
			v = Del_PropValue(&h, v);
		}

		if(x)
			fprintf(E_OUTPUT, "\n");
	}

	return(TRUE);
}


/**************************************************************************
*** Function:	Do_Letter
***				Converts L to LB values / checks unique position
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Letter(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	int x, y;
	struct PropValue *v;
	char letter[2] = "a";

	if(sgfc->info->GM != 1)		/* game != Go? */
		return(TRUE);

	v = p->value;
	while(v)
	{
		x = DecodePosChar(v->value[0]) - 1;
		y = DecodePosChar(v->value[1]) - 1;
	
		if(st->markup[MXY(x,y)] & ST_LABEL)
		{
			PrintError(E_POSITION_NOT_UNIQUE, sgfc, v->buffer, "Label", p->idstr);
		}
		else
		{
			st->markup[MXY(x,y)] |= ST_LABEL;
			st->mrkp_chngd = TRUE;
			New_PropValue(sgfc, n, TKN_LB, v->value, letter, FALSE);
			letter[0]++;
		}

		v = v->next;
	}

	return(FALSE);
}


/**************************************************************************
*** Function:	Do_Mark
***				Converts M to MA/TR depending on board / checks uniqueness
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Mark(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	int x, y;
	struct PropValue *v;

	if(sgfc->info->GM != 1)		/* game != Go? */
		return(TRUE);

	v = p->value;
	while(v)
	{
		x = DecodePosChar(v->value[0]) - 1;
		y = DecodePosChar(v->value[1]) - 1;
	
		if(st->markup[MXY(x,y)] & ST_MARKUP)
		{
			PrintError(E_POSITION_NOT_UNIQUE, sgfc, v->buffer, "Markup", p->idstr);
		}
		else
		{
			st->markup[MXY(x,y)] |= ST_MARKUP;
			st->mrkp_chngd = TRUE;

			if(st->board[MXY(x,y)])
				New_PropValue(sgfc, n, TKN_TR, v->value, NULL, FALSE);
			else
				New_PropValue(sgfc, n, TKN_MA, v->value, NULL, FALSE);
		}
		v = v->next;
	}

	return(FALSE);
}


/**************************************************************************
*** Function:	Do_Markup
***				Checks unique positions for markup properties
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Markup(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	int x, y;
	struct PropValue *v;
	U_SHORT flag;
	int empty, not_empty;

	if(sgfc->info->GM != 1)		/* game != Go? */
		return(TRUE);

	v = p->value;
	flag = sgf_token[p->id].data;
	empty = FALSE;
	not_empty = FALSE;

	while(v)
	{
		if(!strlen(v->value))
		{
			if(empty)	/* if we already have an empty value */
			{
				PrintError(E_EMPTY_VALUE_DELETED, sgfc, v->buffer, "Markup", p->idstr);
				v = Del_PropValue(p, v);
				continue;
			}
			empty = TRUE;
			v = v->next;
			continue;
		}
		else
			not_empty = TRUE;

		x = DecodePosChar(v->value[0]) - 1;
		y = DecodePosChar(v->value[1]) - 1;
	
		if(st->markup[MXY(x,y)] & flag)
		{
			PrintError(E_POSITION_NOT_UNIQUE, sgfc, v->buffer, "Markup", p->idstr);
			v = Del_PropValue(p, v);
			continue;
		}

		st->markup[MXY(x,y)] |= flag;
		st->mrkp_chngd = TRUE;
		v = v->next;
	}

	if(empty && not_empty)	/* if we have both empty and non-empty values: delete empty values */
	{
		v = p->value;
		while(v) {
			if(!strlen(v->value))
			{
				PrintError(E_EMPTY_VALUE_DELETED, sgfc, v->buffer, "Markup", p->idstr);
				v = Del_PropValue(p, v);
				continue;
			}
			else
				v = v->next;
		}
	}

	return(TRUE);
}


/**************************************************************************
*** Function:	Do_Annotate
***				Checks annotation properties / converts BM_TE / TE_BM
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Annotate(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	struct Property *hlp;
	U_SHORT flag;

	flag = sgf_token[p->id].data;

	if((st->annotate & ST_ANN_BM) && p->id == TKN_TE) /* DO (doubtful) */
	{
		PrintError(E4_BM_TE_IN_NODE, sgfc, p->buffer, "BM-TE", "DO");
		hlp = Find_Property(n, TKN_BM);
		hlp->id = TKN_DO;
		hlp->value->value[0] = 0;
		return(FALSE);
	}

	if(st->annotate & ST_ANN_TE && p->id == TKN_BM)	/* IT (interesting) */
	{
		PrintError(E4_BM_TE_IN_NODE, sgfc, p->buffer, "TE-BM", "IT");
		hlp = Find_Property(n, TKN_TE);
		hlp->id = TKN_IT;
		hlp->value->value[0] = 0;
		return(FALSE);
	}

	if(st->annotate & flag)
	{
		PrintError(E_ANNOTATE_NOT_UNIQUE, sgfc, p->buffer, p->idstr);
		return(FALSE);
	}

	if((flag & (ST_ANN_MOVE|ST_KO)) && !(st->annotate & ST_MOVE))
	{
		PrintError(E_ANNOTATE_WITHOUT_MOVE, sgfc, p->buffer, p->idstr);
		return(FALSE);
	}

	st->annotate |= flag;
	return(TRUE);
}


/**************************************************************************
*** Function:	Do_Root
***				Checks if root properties are stored in root
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_Root(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	if(n->parent)
	{
		PrintError(E_ROOTP_NOT_IN_ROOTN, sgfc, p->buffer, p->idstr);
		return(FALSE);
	}
	else
		return(TRUE);
}


/**************************************************************************
*** Function:	Do_GInfo
***				checks for uniqueness of properties within the tree
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_GInfo(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	int x, y;
	long ki;
	char *new_km;

	if(st->ginfo && (st->ginfo != n->buffer))
	{
		SearchPos(st->ginfo, sgfc, &x, &y);
		PrintError(E4_GINFO_ALREADY_SET, sgfc, p->buffer, p->idstr, y, x);
		return(FALSE);
	}

	st->ginfo = n->buffer;
	if(p->id != TKN_KI)
		return(TRUE);

	if(Find_Property(n, TKN_KM))
		PrintError(W_INT_KOMI_FOUND, sgfc, p->buffer, "deleted (<KM> property found)");
	else
	{
		PrintError(W_INT_KOMI_FOUND, sgfc, p->buffer, "converted to <KM>");

		ki = strtol(p->value->value, NULL, 10);		/* we can ignore errors here */
		SaveMalloc(char *, new_km, strlen(p->value->value)+3, "new KM number value")
		if(ki % 2)	sprintf(new_km, "%ld.5", ki/2);
		else		sprintf(new_km, "%ld", ki/2);
		New_PropValue(sgfc, n, TKN_KM, new_km, NULL, FALSE);
		free(new_km);
	}
	return(FALSE);
}


/**************************************************************************
*** Function:	Do_View
***				checks and converts VW property
*** Parameters: sgfc ... pointer to SGFInfo structure
***				n	 ... Node that contains the property
***				p	 ... property
***				st	 ... current board status
*** Returns:	TRUE: ok / FALSE: delete property
**************************************************************************/

int Do_View(struct SGFInfo *sgfc, struct Node *n, struct Property *p, struct BoardStatus *st)
{
	struct PropValue *v;
	int i = 0;

	v = p->value;

	if(!strlen(v->value))	/* VW[] */
	{
		if(v->next)
		{
			PrintError(E_BAD_VW_VALUES, sgfc, p->buffer, "values after '[]' value found", "deleted");
			v = v->next;
			while(v)
				v = Del_PropValue(p, v);
		}

		return(TRUE);
	}

	while(v)
	{
		if(!strlen(v->value))	/* '[]' within other values */
		{
			PrintError(E_BAD_VW_VALUES, sgfc, v->buffer, "empty value found in list", "deleted");
			v = Del_PropValue(p, v);
		}
		else
		{
			i++;
			v = v->next;
		}
	}

	if(sgfc->info->GM != 1)		/* game not Go */
		return(TRUE);

	if(sgfc->info->FF < 4)		/* old definition of VW */
	{
		if(i == 2)				/* transform FF3 values */
		{
			v = p->value;
			v->value2 = v->next->value;
			v->next->value = NULL;
			Del_PropValue(p, v->next);
		
			if(!ExpandPointList(sgfc, p, v, FALSE))
			{
				PrintError(E_BAD_VW_VALUES, sgfc, v->buffer, "illegal FF[3] definition", "deleted");
				return(FALSE);
			}

			Del_PropValue(p, v);
		}
		else		/* looks like FF4 definition (wrong FF set?) */
			PrintError(E_BAD_VW_VALUES, sgfc, p->buffer, "FF[4] definition in older FF found", "parsing done anyway");
	}

	return(TRUE);
}
