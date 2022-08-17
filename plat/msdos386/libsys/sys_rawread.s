#
! $Source$
! $State$
! $Revision$

! Declare segments (the order is important).

.sect .text
.sect .rom
.sect .data
.sect .bss

.sect .text

.extern pmode_ds
.extern pmode_cs
.extern rmode
.extern transfer_buffer_ptr
.extern interrupt_ptr

! Read bytes from a file descriptor.  These routines do not do any
! translation between CRLF and LF line endings.
!
! Note that, for MS-DOS, a raw "write" request of zero bytes will truncate
! (or extend) the file to the current file position.

.define __sys_rawread
__sys_rawread:
	enter 4, 0
amount_transferred = -1*4
file_handle = 2*4
write_buffer = 3*4
amount_to_read = 4*4

	mov amount_transferred(ebp), 0

mainloop:
	mov eax, amount_to_read(ebp)
	test eax, eax
	jz exit

	mov ecx, 32*1024
	cmp eax, ecx
	jge 2f
	mov ecx, eax
2:

	! Read from DOS into the transfer buffer.

	movb ah, 0x3f
	o16 mov dx, (transfer_buffer_ptr)
	o16 mov bx, file_handle(ebp)
	mov ecx, 0x80
	or ebx, 0x210000
	callf (interrupt_ptr)
	jc exit
	test eax, eax
	jz exit

	! Copy eax bytes out of the transfer buffer.

	mov ecx, eax
	push eax
	movzx esi, (transfer_buffer_ptr)
	mov edi, write_buffer(ebp)
	mov es, (pmode_ds)
	cld
1:
	eseg lods
	movb (edi), al
	add edi, 4
	loop 1b
	pop eax

	add write_buffer(ebp), eax
	add amount_transferred(ebp), eax
	sub amount_to_read(ebp), eax
	jmp mainloop

exit:
	mov eax, amount_transferred(ebp)
	leave
	ret


