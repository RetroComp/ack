.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define	.xor

	! #bytes in cx
.xor:
	pop	bx		! return address
	mov	dx,di
	mov	di,sp
	add	di,cx
	sar	cx,1
1:
	pop	ax
	xor	ax,(di)
	stos
	loop	1b
	mov	di,dx
	jmp	bx
