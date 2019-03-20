/** @file
 * Sources of the "BRANCH" group instructions
 */

/* $Id$ */

#include	"em_abs.h"
#include	"global.h"
#include	"log.h"
#include	"mem.h"
#include	"trap.h"
#include	"text.h"
#include	"fra.h"
#include	"warn.h"

/*	Note that in the EM assembly language brach instructions have
	lables as their arguments, where in the EM machine language they
	have (relative) offsets as parameters.  This is not described in the
	EM manual but follows from the Pascal interpreter.
*/

#define	do_jump(j)	{ newPC(PC + (j)); }

void DoBRA(register long jump)
{
	/* BRA b: Branch unconditionally to label b */

	LOG(("@B6 DoBRA(%ld)", jump));
	do_jump(arg_c(jump));
}

void DoBLT(register long jump)
{
	/* BLT b: Branch less (pop 2 words, branch if top > second) */
	register long t = wpop();

	LOG(("@B6 DoBLT(%ld)", jump));
	spoilFRA();
	if (wpop() < t)
		do_jump(arg_c(jump));
}

void DoBLE(register long jump)
{
	/* BLE b: Branch less or equal */
	register long t = wpop();

	LOG(("@B6 DoBLE(%ld)", jump));
	spoilFRA();
	if (wpop() <= t)
		do_jump(arg_c(jump));
}

void DoBEQ(register long jump)
{
	/* BEQ b: Branch equal */
	register long t = wpop();

	LOG(("@B6 DoBEQ(%ld)", jump));
	spoilFRA();
	if (t == wpop())
		do_jump(arg_c(jump));
}

void DoBNE(register long jump)
{
	/* BNE b: Branch not equal */
	register long t = wpop();

	LOG(("@B6 DoBNE(%ld)", jump));
	spoilFRA();
	if (t != wpop())
		do_jump(arg_c(jump));
}

void DoBGE(register long jump)
{
	/* BGE b: Branch greater or equal */
	register long t = wpop();

	LOG(("@B6 DoBGE(%ld)", jump));
	spoilFRA();
	if (wpop() >= t)
		do_jump(arg_c(jump));
}

void DoBGT(register long jump)
{
	/* BGT b: Branch greater */
	register long t = wpop();

	LOG(("@B6 DoBGT(%ld)", jump));
	spoilFRA();
	if (wpop() > t)
		do_jump(arg_c(jump));
}

void DoZLT(register long jump)
{
	/* ZLT b: Branch less than zero (pop 1 word, branch negative) */

	LOG(("@B6 DoZLT(%ld)", jump));
	spoilFRA();
	if (wpop() < 0)
		do_jump(arg_c(jump));
}

void DoZLE(register long jump)
{
	/* ZLE b: Branch less or equal to zero */

	LOG(("@B6 DoZLE(%ld)", jump));
	spoilFRA();
	if (wpop() <= 0)
		do_jump(arg_c(jump));
}

void DoZEQ(register long jump)
{
	/* ZEQ b: Branch equal zero */

	LOG(("@B6 DoZEQ(%ld)", jump));
	spoilFRA();
	if (wpop() == 0)
		do_jump(arg_c(jump));
}

void DoZNE(register long jump)
{
	/* ZNE b: Branch not zero */

	LOG(("@B6 DoZNE(%ld)", jump));
	spoilFRA();
	if (wpop() != 0)
		do_jump(arg_c(jump));
}

void DoZGE(register long jump)
{
	/* ZGE b: Branch greater or equal zero */

	LOG(("@B6 DoZGE(%ld)", jump));
	spoilFRA();
	if (wpop() >= 0)
		do_jump(arg_c(jump));
}

void DoZGT(register long jump)
{
	/* ZGT b: Branch greater than zero */

	LOG(("@B6 DoZGT(%ld)", jump));
	spoilFRA();
	if (wpop() > 0)
		do_jump(arg_c(jump));
}
