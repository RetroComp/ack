/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#define _COST_

typedef struct cost {
	int ct_space;
	int ct_time;
} cost_t,*cost_p;
