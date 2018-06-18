/* $Id$ */
/*
 * (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
 *
 *          This product is part of the Amsterdam Compiler Kit.
 *
 * Permission to use, sell, duplicate or disclose this software must be
 * obtained in writing. Requests for such permissions may be sent to
 *
 *      Dr. Andrew S. Tanenbaum
 *      Wiskundig Seminarium
 *      Vrije Universiteit
 *      Postbox 7161
 *      1007 MC Amsterdam
 *      The Netherlands
 *
 */

#include "pc.h"

void _wrc(int c, struct file* f)
{
	*f->ptr = c;
	_wf(f);
	_outcpt(f);
}

void _wln(struct file* f)
{
#ifdef CPM
	_wrc('\r', f);
#endif
	_wrc('\n', f);
	f->flags |= ELNBIT;
}

void _pag(struct file* f)
{
	_wrc('\014', f);
	f->flags |= ELNBIT;
}
