/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#ifndef lint
static char rcsid[] = "$Header$";
#endif

/*
 * Memory manager. Memory is divided into NMEMS pieces. There is a struct
 * for each piece telling where it is, how many bytes are used, and how may
 * are left. If a request for core doesn't fit in the left bytes, an sbrk()
 * is done and pieces after the one that requested the growth are moved up.
 *
 * Unfortunately, we cannot use sbrk to request more memory, because its
 * result cannot be trusted. More specifically, it does not work properly
 * on 2.9 BSD, and probably does not work properly on 2.8 BSD and V7 either.
 * The problem is that "sbrk" adds the increment to the current "break"
 * WITHOUT testing the carry bit. So, if your break is at 40000, and
 * you "sbrk(30000)", it will succeed, but your break will be at 4464
 * (70000 - 65536).
 */

#include <out.h>
#include "const.h"
#include "assert.h"
#include "debug.h"
#include "memory.h"

static		copy_down();
static		copy_up();
static		free_saved_moduls();
static		writelong();
static		namecpy();

struct memory	mems[NMEMS];

bool	incore = TRUE;	/* TRUE while everything can be kept in core. */
ind_t	core_position = (ind_t)0;	/* Index of current module. */

#define AT_LEAST	(ind_t)2	/* See comment about string areas. */
#define GRANULE		64	/* power of 2 */

static char *BASE;
static ind_t refused;

sbreak(incr)
	ind_t incr;
{
	extern char	*sbrk();
	extern char	*brk();
	unsigned int	inc;

	incr = (incr + (GRANULE - 1)) & ~(GRANULE - 1);

	inc = incr;
	if ((refused && refused < incr) ||
	    inc != incr ||
	    BASE + inc < BASE ||
	    (int) brk(BASE + inc) == -1) {
		refused = refused && refused > incr ? incr : refused;
		return -1;
	}
	BASE = sbrk(0);
	return 0;
}

/*
 * Initialize some pieces of core. We hope that this will be our last
 * real allocation, meaning we've made the right choices.
 */
init_core()
{
	register char		*base;
	register ind_t		total_size;
	register struct memory	*mem;
	extern char		*brk();
	extern char		*sbrk();

#include "mach.c"

	total_size = (ind_t)0;	/* Will accumulate the sizes. */
	BASE = base = sbrk(0);		/* First free. */
	for (mem = mems; mem < &mems[NMEMS]; mem++) {
		mem->mem_base = base;
		mem->mem_full = (ind_t)0;
		base += mem->mem_left;	/* Each piece will start after prev. */
		total_size += mem->mem_left;
	}
	/*
	 * String areas are special-cased. The first byte is unused as a way to
	 * distinguish a name without string from a name which has the first
	 * string in the string area.
	 */
	if (mems[ALLOLCHR].mem_left == 0)
		total_size += 1;
	else
		mems[ALLOLCHR].mem_left -= 1;
	if (mems[ALLOGCHR].mem_left ==  0)
		total_size += 1;
	else
		mems[ALLOGCHR].mem_left -= 1;

	if (sbreak(total_size) == -1) {
		incore = FALSE;	/* In core strategy failed. */
		if (sbreak(AT_LEAST) == -1)
			fatal("no core at all");
		
		base = BASE;
		for (mem = mems; mem < &mems[NMEMS]; mem++) {
			mem->mem_base = base;
			mem->mem_full = (ind_t)0;
			mem->mem_left = 0;
		}
	}

	mems[ALLOLCHR].mem_full = 1;
	mems[ALLOGCHR].mem_full = 1;
}

/*
 * Allocate an extra block of `incr' bytes and move all pieces with index
 * higher than `piece' up with the size of the block. Return whether the
 * allocate succeeded.
 */
static bool
move_up(piece, incr)
	register int		piece;
	register ind_t		incr;
{
	register struct memory	*mem;

	debug("move_up(%d, %d)\n", piece, (int)incr, 0, 0);
	if (sbreak(incr) == -1)
		return FALSE;

	for (mem = &mems[NMEMS - 1]; mem > &mems[piece]; mem--)
		copy_up(mem, incr);

	mems[piece].mem_left += incr;
	return TRUE;
}

extern int	passnumber;

/*
 * This routine is called if `piece' needs `incr' bytes and the system won't
 * give them. We first steal the free bytes of all lower pieces and move them
 * and `piece' down. If that doesn't give us enough bytes, we steal the free
 * bytes of all higher pieces and move them up. We return whether we have
 * enough bytes, the first or the second time.
 */
static bool
compact(piece, incr, freeze)
	register int		piece;
	register ind_t		incr;
{
	register ind_t		gain, size;
	register struct memory	*mem;
#define ALIGN 8			/* minimum alignment for pieces */
#define SHIFT_COUNT 2		/* let pieces only contribute if their free
				   memory is more than 1/2**SHIFT_COUNT * 100 %
				   of its occupied memory
				*/

	debug("compact(%d, %d)\n", piece, (int)incr, 0, 0);
	for (mem = &mems[0]; mem < &mems[NMEMS - 1]; mem++) {
		assert(mem->mem_base + mem->mem_full + mem->mem_left == (mem+1)->mem_base);
	}
	/*
	 * First, check that moving will result in enough space
	 */
	if (! freeze) {
		gain = mems[piece].mem_left & ~(ALIGN - 1);
		for (mem = &mems[0]; mem <= &mems[NMEMS-1]; mem++) {
			/* 
			 * Don't give it all away! 
			 * If this does not give us enough, bad luck
			 */
			if (mem == &mems[piece]) continue;
			size = mem->mem_full >> SHIFT_COUNT;
			if (mem->mem_left > size)
				gain += (mem->mem_left - size) & ~(ALIGN - 1);
		}
		if (gain < incr) return 0;
	}

	gain = 0;
	for (mem = &mems[0]; mem != &mems[piece]; mem++) {
		/* Here memory is inserted before a piece. */
		assert(passnumber == FIRST || gain == (ind_t)0);
		copy_down(mem, gain);
		if (freeze || gain < incr) {
			if (freeze) size = 0;
			else size = mem->mem_full >> SHIFT_COUNT;
			if (mem->mem_left > size) {
				size = (mem->mem_left - size) & ~(ALIGN - 1);
				gain += size;
				mem->mem_left -= size;
			}
		}
	}
	/*
	 * Now mems[piece]:
	 */
	copy_down(mem, gain);
	gain += mem->mem_left & ~(ALIGN - 1);
	mem->mem_left &= (ALIGN - 1);

	if (gain < incr) {
		register ind_t	up = (ind_t)0;

		for (mem = &mems[NMEMS - 1]; mem > &mems[piece]; mem--) {
			/* Here memory is appended after a piece. */
			if (freeze || gain + up < incr) {
				if (freeze) size = 0;
				else size = mem->mem_full >> SHIFT_COUNT;
				if (mem->mem_left > size) {
					size = (mem->mem_left - size) & ~(ALIGN - 1);
					up += size;
					mem->mem_left -= size;
				}
			}
			copy_up(mem, up);
		}
		gain += up;
	}
	mems[piece].mem_left += gain;
	assert(freeze || gain >= incr);
	for (mem = &mems[0]; mem < &mems[NMEMS - 1]; mem++) {
		assert(mem->mem_base + mem->mem_full + mem->mem_left == (mem+1)->mem_base);
	}
	return gain >= incr;
}

/*
 * The bytes of `mem' must be moved `dist' down in the address space.
 * We copy the bytes from low to high, because the tail of the new area may
 * overlap with the old area, but we do not want to overwrite them before they
 * are copied.
 */
static
copy_down(mem, dist)
	register struct memory	*mem;
	ind_t			dist;
{
	register char		*old;
	register char		*new;
	register ind_t		size;

	if (!dist) return;
	size = mem->mem_full;
	old = mem->mem_base;
	new = old - dist;
	mem->mem_base = new;
	while (size--)
		*new++ = *old++;
}

/*
 * The bytes of `mem' must be moved `dist' up in the address space.
 * We copy the bytes from high to low, because the tail of the new area may
 * overlap with the old area, but we do not want to overwrite them before they
 * are copied.
 */
static
copy_up(mem, dist)
	register struct memory	*mem;
	ind_t			dist;
{
	register char		*old;
	register char		*new;
	register ind_t		size;

	if (!dist) return;
	size = mem->mem_full;
	old = mem->mem_base + size;
	new = old + dist;
	while (size--)
		*--new = *--old;
	mem->mem_base = new;
}

/*
 * Add `size' bytes to the bytes already allocated for `piece'. If it has no
 * free bytes left, ask them from memory or, if that fails, from the free
 * bytes of other pieces. The offset of the new area is returned. No matter
 * how many times the area is moved, because of another allocate, this offset
 * remains valid.
 */
ind_t
alloc(piece, size)
	int			piece;
	register long		size;
{
	register ind_t		incr = 0;
	ind_t			left = mems[piece].mem_left;
	register ind_t		full = mems[piece].mem_full;

	assert(passnumber == FIRST || (!incore && piece == ALLOMODL));
	if (size == (long)0)
		return full;
	if (size != (ind_t)size)
		return BADOFF;

	while (left + incr < size)
		incr += INCRSIZE;

	if (incr == 0 || move_up(piece, incr) || compact(piece, size, 0)) {
		mems[piece].mem_full += size;
		mems[piece].mem_left -= size;
		return full;
	} else {
		incore = FALSE;
		return BADOFF;
	}
}

/*
 * Same as alloc() but for a piece which really needs it. If the first
 * attempt fails, release the space occupied by other pieces and try again.
 */
ind_t
hard_alloc(piece, size)
	register int	piece;
	register long	size;
{
	register ind_t	ret;
	register int	i;

	if (size != (ind_t)size)
		return BADOFF;
	if ((ret = alloc(piece, size)) != BADOFF)
		return ret;

	/*
	 * Deallocate what we don't need.
	 */
	for (i = 0; i < NMEMS; i++) {
		switch (i) {
		case ALLOGLOB:
		case ALLOGCHR:
		case ALLOSYMB:
		case ALLOARCH:
		case ALLOMODL:
			break;	/* Do not try to deallocate this. */
		default:
			dealloc(i);
			break;
		}
	}
	free_saved_moduls();

	return alloc(piece, size);
}

/*
 * We don't need the previous modules, so we put the current module
 * at the start of the piece allocated for module contents, thereby
 * overwriting the saved modules, and release its space.
 */
static
free_saved_moduls()
{
	register ind_t		size;
	register char		*old, *new;
	register struct memory	*mem = &mems[ALLOMODL];

	size = mem->mem_full - core_position;
	new = mem->mem_base;
	old = new + core_position;
	while (size--)
		*new++ = *old++;
	mem->mem_full -= core_position;
	mem->mem_left += core_position;
	core_position = (ind_t)0;
}

/*
 * The piece of memory with index `piece' is no longer needed.
 * We take care that it can be used by compact() later, if needed.
 */
dealloc(piece)
	register int		piece;
{
	/*
	 * Some pieces need their memory throughout the program.
	 */
	assert(piece != ALLOGLOB);
	assert(piece != ALLOGCHR);
	assert(piece != ALLOSYMB);
	assert(piece != ALLOARCH);
	mems[piece].mem_left += mems[piece].mem_full;
	mems[piece].mem_full = (ind_t)0;
}

char *
core_alloc(piece, size)
	register int	piece;
	register long	size;
{
	register ind_t	off;

	if ((off = alloc(piece, size)) == BADOFF)
		return (char *)0;
	return address(piece, off);
}

/*
 * Reset index into piece of memory for modules and
 * take care that the allocated pieces will not be moved.
 */
freeze_core()
{
	register int	i;

	core_position = (ind_t)0;

	if (incore)
		return;

	for (i = 0; i < NMEMS; i++) {
		switch (i) {
		case ALLOGLOB:
		case ALLOGCHR:
		case ALLOSYMB:
		case ALLOARCH:
			break;	/* Do not try to deallocate this. */
		default:
			dealloc(i);
			break;
		}
	}
	compact(NMEMS - 1, (ind_t)0, 1);
}

/* ------------------------------------------------------------------------- */

/*
 * To transform the various pieces of the output in core to the file format,
 * we must order the bytes in the ushorts and longs as ACK prescribes.
 */
write_bytes()
{
	ushort			nsect;
	long			offchar;
	register struct memory	*mem;
	extern ushort		NLocals, NGlobals;
	extern long		NLChars, NGChars;
	extern int		flagword;
	extern struct outhead	outhead;
	extern struct outsect	outsect[];
	extern char		*outputname;
	int			sectionno = 0;

	nsect = outhead.oh_nsect;
	offchar = OFF_CHAR(outhead);

	/*
	 * We allocated two areas: one for local and one for global names.
	 * Also, we used another kind of on_foff than on file.
	 * At the end of the global area we have put the section names.
	 */
	if (!(flagword & SFLAG)) {
		namecpy((struct outname *)mems[ALLOLOCL].mem_base,
			NLocals,
			offchar
		);
		namecpy((struct outname *)mems[ALLOGLOB].mem_base,
			NGlobals + nsect,
			offchar + NLChars
		);
	}
	if (! wr_open(outputname)) {
		fatal("can't create %s", outputname);
	}
	/*
	 * These pieces must always be written.
	 */
	wr_ohead(&outhead);
	wr_sect(outsect, nsect);
	for (mem = &mems[ALLOEMIT]; mem < &mems[ALLORELO]; mem++)
		wrt_emit(mem->mem_base, sectionno++, mem->mem_full);
	/*
	 * The rest depends on the flags.
	 */
	if (flagword & RFLAG)
		wr_relo((struct outrelo *) mems[ALLORELO].mem_base,
			outhead.oh_nrelo);
	if (!(flagword & SFLAG)) {
		wr_name((struct outname *) mems[ALLOLOCL].mem_base,
			NLocals);
		wr_name((struct outname *) mems[ALLOGLOB].mem_base,
			NGlobals+nsect);
		wr_string(mems[ALLOLCHR].mem_base + 1, (long)NLChars);
		wr_string(mems[ALLOGCHR].mem_base + 1, (long)NGChars);
#ifdef SYMDBUG
		wr_dbug(mems[ALLODBUG].mem_base, mems[ALLODBUG].mem_full);
#endif SYMDBUG
	}
	wr_close();
}

static
namecpy(name, nname, offchar)
	register struct outname	*name;
	register ushort		nname;
	register long		offchar;
{
	while (nname--) {
		if (name->on_foff)
			name->on_foff += offchar - 1;
		name++;
	}
}
