/* $Header$ */

/* Command grammar */
{
#include	<stdio.h>
#include	<alloc.h>
#include	<signal.h>

#include	"ops.h"
#include	"class.h"
#include	"position.h"
#include	"file.h"
#include	"idf.h"
#include	"symbol.h"
#include	"tree.h"
#include	"langdep.h"
#include	"token.h"
#include	"expr.h"

extern char	*Salloc();
extern char	*strindex();
extern FILE	*db_in;
extern int	disable_intr;
extern p_tree	run_command, print_command;

int		errorgiven = 0;
int		child_interrupted = 0;
int		interrupted = 0;
int		eof_seen = 0;
static int	extended_charset = 0;
static int	in_expression = 0;

struct token	tok, aside;

#define binprio(op)	((*(currlang->binop_prio))(op))
#define unprio(op)	((*(currlang->unop_prio))(op))
}
%start Commands, commands;

%lexical LLlex;

commands
  { p_tree com, lastcom = 0;
    int give_prompt;
  }
:
  [ %persistent command_line(&com)
    [	'\n'		{ give_prompt = 1; }
    |	%default ';'	{ give_prompt = 0; }
    ]
			{ if (com) {
				if (errorgiven) {
					freenode(com);
					com = 0;
				}
				if (lastcom) {
					freenode(lastcom);
					lastcom = 0;
				}

				if (com) {
					eval(com);
			  		if (repeatable(com)) {
						lastcom = com;
					}
					else if (! in_status(com) &&
					        com != run_command &&
						com != print_command) {
						freenode(com);
					}
				}
			  } else if (lastcom && ! errorgiven) {
				eval(lastcom);
			  }
			  if (give_prompt) {
			  	errorgiven = 0;
				interrupted = 0;
				prompt();
			  }
			}
  ]*
			{ signal_child(SIGKILL); }
;

command_line(p_tree *p;)
:
			{ *p = 0; }
[
  list_command(p)
| file_command(p)
| run_command(p)
| stop_command(p)
| when_command(p)
| continue_command(p)
| step_command(p)
| next_command(p)
| regs_command(p)
| where_command(p)
| STATUS		{ *p = mknode(OP_STATUS); }
| DUMP			{ *p = mknode(OP_DUMP); }
| RESTORE INTEGER	{ *p = mknode(OP_RESTORE, tok.ival); }
| delete_command(p)
| print_command(p)
| display_command(p)
| trace_command(p)
| set_command(p)
| help_command(p)
| FIND qualified_name(p){ *p = mknode(OP_FIND, *p); }
| WHICH qualified_name(p){ *p = mknode(OP_WHICH, *p); }
| able_command(p)
|
]
;

where_command(p_tree *p;)
  { long l; }
:
  WHERE
  [ INTEGER		{ l = tok.ival; }
  | '-' INTEGER		{ l = - tok.ival; }
  |			{ l = 0x7fffffff; }
  ]			{ *p = mknode(OP_WHERE, l); }
;

list_command(p_tree *p;)
  { p_tree t1 = 0, t2 = 0; }
:
  LIST
  [
  | count(&t1)
  | qualified_name(&t1)
  ]
  [ ',' count(&t2)
  | '-' 
    [	count(&t2)	{ t2->t_ival = - t2->t_ival; }
    |			{ t2 = mknode(OP_INTEGER, -100000000L); }
    ]
  |
  ]
			{ *p = mknode(OP_LIST, t1, t2); }
;

file_command(p_tree *p;)
:
  XFILE			{ extended_charset = 1; }
  [			{ *p = 0; }
  | name(p)		{ (*p)->t_idf = str2idf((*p)->t_str, 0); }
  ]			{ *p = mknode(OP_FILE, *p);
			  extended_charset = 0;
			}
;

help_command(p_tree *p;)
:
  [ HELP | '?' ]
  [			{ *p = mknode(OP_HELP, (struct idf *) 0, (char *) 0); }
  | name(p)		{ (*p)->t_oper = OP_HELP; }
  | '?'			{ *p = mknode(OP_HELP, str2idf("help",0), (char *) 0); }
  ]
;

run_command(p_tree *p;)
:
  RUN			{ extended_charset = 1; }
  args(p)		{ *p = mknode(OP_RUN, *p);
			  extended_charset = 0;
			}
| RERUN			{ if (! run_command) {
				error("no run command given yet");
			  }
			  else *p = run_command;
			}
;

stop_command(p_tree *p;)
  { p_tree whr = 0, cond = 0; }
:
  STOP
  where(&whr)?
  condition(&cond)?	{ if (! whr && ! cond) {
				error("no position or condition");
				*p = 0;
			  }
			  else *p = mknode(OP_STOP, whr, cond);
			}
;

trace_command(p_tree *p;)
  { p_tree whr = 0, cond = 0, exp = 0; }
:
  TRACE
  [ ON expression(&exp, 0) ]?
  where(&whr)?
  condition(&cond)?	{ *p = mknode(OP_TRACE, whr, cond, exp); }
;

continue_command(p_tree *p;)
  { long l; p_tree pos = 0; }
:
  CONT
  [ INTEGER		{ l = tok.ival; }
  |			{ l = 1; }
  ]
  position(&pos)?
  			{ *p = mknode(OP_CONT, mknode(OP_INTEGER, l), pos); }
;

when_command(p_tree *p;)
  { p_tree	whr = 0, cond = 0; }
:
  WHEN
  where(&whr)?
  condition(&cond)?
  '{' 
  command_line(p)
  [ ';'			{ if (*p) {
				*p = mknode(OP_LINK, *p, (p_tree) 0);
			  	p = &((*p)->t_args[1]);
			  }
			}
    command_line(p)
  ]*
  '}'
			{ if (! whr && ! cond) {
				error("no position or condition");
				freenode(*p);
				*p = 0;
			  }
			  else if (! *p) {
				error("no commands given");
			  }
			  else *p = mknode(OP_WHEN, whr, cond, *p);
			}
;

step_command(p_tree *p;)
  { long	l; }
:
  STEP
  [ INTEGER		{ l = tok.ival; }
  |			{ l = 1; }
  ]			{ *p = mknode(OP_STEP, l); }
;

next_command(p_tree *p;)
  { long	l; }
:
  NEXT
  [ INTEGER		{ l = tok.ival; }
  |			{ l = 1; }
  ]			{ *p = mknode(OP_NEXT, l); }
;

regs_command(p_tree *p;)
  { long	l; }
:
  REGS
  [ INTEGER		{ l = tok.ival; }
  |			{ l = 0; }
  ]			{ *p = mknode(OP_REGS, l); }
;

delete_command(p_tree *p;)
:
  DELETE count_list(p)	{ *p = mknode(OP_DELETE, *p); }
;

print_command(p_tree *p;)
:
  PRINT 
  [ format_expression_list(p)
			{ *p = mknode(OP_PRINT, *p); }
  |
			{ *p = mknode(OP_PRINT, (p_tree) 0); }
  ]
;

display_command(p_tree *p;)
:
  DISPLAY format_expression_list(p)
			{ *p = mknode(OP_DISPLAY, *p); }
;

format_expression_list(p_tree *p;)
:
  format_expression(p)
  [ ','			{ *p = mknode(OP_LINK, *p, (p_tree) 0);
			  p = &((*p)->t_args[1]);
			}
    format_expression(p)
  ]*
;

format_expression(p_tree *p;)
  { p_tree	p1; }
:
  expression(p, 0)
  [ '\\' 
	[ name(&p1)	{ register char *c = p1->t_str;
			  while (*c) {
				if (! strindex("doshcax", *c)) {
					error("illegal format: %c", *c);
					break;
				}
				c++;
			  }
			  *p = mknode(OP_FORMAT, *p, p1);
			}
	|
	]
  |
  ]
;

set_command(p_tree *p;)
:
  SET expression(p, 0)	{ *p = mknode(OP_SET, *p, (p_tree) 0); }
  TO expression(&((*p)->t_args[1]), 0)
;

able_command(p_tree *p;)
:
  [ ENABLE 		{ *p = mknode(OP_ENABLE, (p_tree) 0); }
  | DISABLE 		{ *p = mknode(OP_DISABLE, (p_tree) 0); }
  ]
  count_list(&(*p)->t_args[0])
;

count_list(p_tree *p;)
:
  count(p)
  [ ','			{ *p = mknode(OP_LIST, *p, (p_tree) 0); }
    count(&(*p)->t_args[1])
  ]*
;

condition(p_tree *p;)
:
  IF expression(p, 0)
;

where(p_tree *p;)
:
  IN qualified_name(p)	{ *p = mknode(OP_IN, *p); }
|
  position(p)
;

expression(p_tree *p; int level;)
  { int currprio, currop; }
:			{ in_expression++; }
  factor(p)
  [ %while ((currprio = binprio(currop = (int) tok.ival)) > level)
	[ BIN_OP | PREF_OR_BIN_OP ] 
			{ *p = mknode(OP_BINOP, *p, (p_tree) 0);
			  (*p)->t_whichoper = currop;
			}
	expression(&((*p)->t_args[1]), currprio)
  |
	SEL_OP		{ *p = mknode(OP_BINOP, *p, (p_tree) 0);
			  (*p)->t_whichoper = (int) tok.ival;
			}
	name(&(*p)->t_args[1])
  |
	'['		{ *p = mknode(OP_BINOP, *p, (p_tree) 0);
			  (*p)->t_whichoper = E_ARRAY;
			}
	expression(&(*p)->t_args[1], 0)
	[	','	{ *p = mknode(OP_BINOP, *p, (p_tree) 0);
			  (*p)->t_whichoper = E_ARRAY;
			}
		expression(&(*p)->t_args[1], 0)
	]*
	']'
  ]*
			{ in_expression--; }
;

factor(p_tree *p;)
:
  [
  	%default EXPRESSION	/* lexical analyzer will never return this token */
			{ *p = mknode(OP_INTEGER, 0L); }
  |
  	'(' expression(p, 0) ')'
  |
  	INTEGER		{ *p = mknode(OP_INTEGER, tok.ival); }
  |
  	REAL		{ *p = mknode(OP_REAL, tok.fval); }
  |
  	STRING		{ *p = mknode(OP_STRING, tok.str); }
  |
  	qualified_name(p)
  |
  			{ *p = mknode(OP_UNOP, (p_tree) 0);
			  (*p)->t_whichoper = (int) tok.ival;
			}
  	[ PREF_OP 
  	| PREF_OR_BIN_OP
			{ (*currlang->fix_bin_to_pref)(*p); }
  	]
  	expression(&(*p)->t_args[0], unprio((*p)->t_whichoper))
  ]
  [ %while(1)
	POST_OP		{ *p = mknode(OP_UNOP, *p);
			  (*p)->t_whichoper = (int) tok.ival;
			}
  ]*
;

position(p_tree *p;)
  { p_tree lin;
    char *str;
  }
:
  AT
  [ STRING		{ str = tok.str; }
    ':'
  |			{ if (! listfile) str = 0;
			  else str = listfile->sy_idf->id_text;
			}
  ]
  count(&lin)		{ *p = mknode(OP_AT, lin->t_ival, str);
			  freenode(lin);
			}
;

args(p_tree *p;)
  { int first_time = 1; }
:
  [			{ if (! first_time) {
				*p = mknode(OP_LINK, *p, (p_tree) 0);
				p = &((*p)->t_args[1]);
			  }
			  first_time = 0;
			}
	arg(p)
  ]*
;

arg(p_tree *p;)
:
  name(p)
|
  '>' name(p)		{ (*p)->t_oper = OP_OUTPUT; }
|
  '<' name(p)		{ (*p)->t_oper = OP_INPUT; }
;

count(p_tree *p;)
:
  INTEGER		{ *p = mknode(OP_INTEGER, tok.ival); }
;

qualified_name(p_tree *p;)
:
  name(p)
  [	'`'		{ *p = mknode(OP_SELECT, *p, (p_tree) 0); }
	name(&((*p)->t_args[1]))
  ]*
;

name(p_tree *p;)
:
  [ XFILE
  | LIST
  | RUN
  | RERUN
  | STOP
  | WHEN
  | AT
  | IN
  | IF
  | %default NAME
  | CONT
  | STEP
  | NEXT
  | REGS
  | WHERE
  | STATUS
  | PRINT
  | DELETE
  | DUMP
  | RESTORE
  | TRACE
  | ON
  | SET
  | TO
  | FIND
  | DISPLAY
  | WHICH
  | HELP
  | DISABLE
  | ENABLE
  ]			{ *p = mknode(OP_NAME, tok.idf, tok.str); }
;

{
int
LLlex()
{
  register int c;

  if (ASIDE) {
	tok = aside;
	ASIDE = 0;
	return TOK;
  }
  do {
	c = getc(db_in);
  } while (c != EOF && class(c) == STSKIP);
  if (c == EOF) {
	eof_seen = 1;
	return c;
  }
  if (extended_charset && in_ext(c)) {
	TOK = get_name(c);
	return TOK;
  }
  switch(class(c)) {
  case STSTR:
	TOK = (*currlang->get_string)(c);
	break;
  case STIDF:
	if (in_expression) TOK = (*currlang->get_name)(c);
	else TOK = get_name(c);
	break;
  case STNUM:
	TOK = (*currlang->get_number)(c);
	break;
  case STNL:
	TOK = c;
	break;
  case STSIMP:
	if (! in_expression) {
		TOK = c;
		break;
	}
	/* Fall through */
  default:
	TOK = (*currlang->get_token)(c);
	break;
  }
  return TOK;
}

int
get_name(c)
  register int	c;
{
  char	buf[512+1];
  register char	*p = &buf[0];
  register struct idf *id;

  do {
	if (p - buf < 512) *p++ = c;
	c = getc(db_in);
  } while ((extended_charset && in_ext(c)) || in_idf(c));
  ungetc(c, db_in);
  *p++ = 0;
  if (extended_charset) {
	tok.idf = 0;
	tok.str = Salloc(buf, (unsigned) (p - buf));
	return NAME;
  }
  id = str2idf(buf, 1);
  tok.idf = id;
  tok.str = id->id_text;
  return id->id_reserved ? id->id_reserved : NAME;
}

extern char * symbol2str();

LLmessage(t)
{
  if (t > 0) {
  	if (! errorgiven) {
		error("%s missing before %s", symbol2str(t), symbol2str(TOK));
	}
	aside = tok;
  }
  else if (t == 0) {
  	if (! errorgiven) {
		error("%s unexpected", symbol2str(TOK));
	}
  }
  else if (! errorgiven) {
	error("EOF expected");
  }
  errorgiven = 1;
}

static int
catch_del()
{
  signal(SIGINT, catch_del);
  if (! disable_intr) {
  	signal_child(SIGEMT);
  	child_interrupted = 1;
  }
  interrupted = 1;
}

int
init_del()
{
  signal(SIGINT, catch_del);
}
}