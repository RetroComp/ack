.sect .text; .sect .rom; .sect .data; .sect .bss
.define .csb2

.sect .text
.csb2:
				!bx, descriptor address
				!ax,  index
	mov	dx,(bx)
	mov	cx,2(bx)
1:
	add	bx,4
	dec     cx
	jl      2f
	cmp     ax,(bx)
	jne     1b
	mov	bx,2(bx)
2:
	test    bx,bx
	jnz     3f
.extern ECASE
.extern .fat
	mov     ax,ECASE
	push    ax
	jmp     .fat
3:
	jmp     bx
