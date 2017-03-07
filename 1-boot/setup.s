
.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 2				! nr of setup-sectors
BOOTSEG  = 0x07c0			! original address of boot-sector
INITSEG  = 0x9000			! we move boot here - out of the way
SETUPSEG = 0x9020			! setup starts here
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).


entry _start
_start:
	!设置附加数据段寄存器ES的值指向SETUPSEG，以便可以正常显示字符串数据
	mov ax, #SETUPSEG
	mov	es,ax
! Print some inane message

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 页号bh=0
	int	0x10
	
	mov	cx,#25
	mov	bx,#0x0007		! page 0, attribute 7 (normal) 页号BH=0 属性BL=7正常显示
	mov	bp,#msg1		! ES:BP要显示的字符串地址
	mov	ax,#0x1301		! write string, move cursor AH=13显示字符串 AL=01光标跟随移动
	int	0x10

! ok, we've written the message, now
! ok, the read went well so we get current cursor position and save it for
! posterity.

	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax
	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches
	mov	[0],dx		! it from 0x90000.
! Get memory size (extended mem, kB)

	mov	ah,#0x88
	int	0x15
	mov	[2],ax

! Get video-card data:

	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width

! check for EGA/VGA and some config parameters

	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax
	mov	[10],bx
	mov	[12],cx

! Get hd0 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080
	mov	cx,#0x10
	rep
	movsb

! Get hd1 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	rep
	movsb

! Check that there IS a hd1 :-)

	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3
	je	is_disk1
no_disk1:
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb
is_disk1:

!设置堆栈段寄存器SS，指向0x90000，这里保存了从BIOS读取到的硬件参数
mov ax, #INITSEG
!注意在bootsect.s中已经设置好了堆栈栈寄存器ss和堆栈顶寄存器sp ss=0x90000 sp=0x9ff00
!但这里以防万一，可以再设置下
! put stack at 0x9ff00.
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >>512 (栈指针sp指向远大于512个字节偏移(即地址0x90200)处都可以)

!设置附加数据段寄存器ES的值指向SETUPSEG，以便可以正常显示字符串数据
	mov ax, #SETUPSEG
	mov	es,ax

Print_Cursor:

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 页号bh=0
	int	0x10
	
	mov	cx,#11
	mov	bx,#0x0007		! page 0, attribute 7 (normal) 页号BH=0 属性BL=7正常显示
	mov	bp,#Cursor		! ES:BP要显示的字符串地址
	mov	ax,#0x1301		! write string, move cursor AH=13显示字符串 AL=01光标跟随移动
	int	0x10
	mov ax, #0			!set bp = 0x0000
	mov bp, ax			
	call print_hex
	call print_nl

Print_Memory:

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 页号bh=0
	int	0x10
	
	mov	cx,#12
	mov	bx,#0x0007		! page 0, attribute 7 (normal) 页号BH=0 属性BL=7正常显示
	mov	bp,#Memory		! ES:BP要显示的字符串地址
	mov	ax,#0x1301		! write string, move cursor AH=13显示字符串 AL=01光标跟随移动
	int	0x10
	mov ax, #2			!set bp = 0x0000
	mov bp, ax
	call print_hex
	
	!显示扩展内存最后的单位"KB"
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 页号bh=0
	int	0x10

	mov	cx,#2
	mov	bx,#0x0007		! page 0, attribute 7 (normal) 页号BH=0 属性BL=7正常显示
	mov	bp,#KB		! ES:BP要显示的字符串地址
	mov	ax,#0x1301		! write string, move cursor AH=13显示字符串 AL=01光标跟随移动
	int	0x10
	call print_nl



Print_Cyl_hd0:

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 页号bh=0
	int	0x10
	
	mov	cx,#9
	mov	bx,#0x0007		! page 0, attribute 7 (normal) 页号BH=0 属性BL=7正常显示
	mov	bp,#Cyl_hd0		! ES:BP要显示的字符串地址
	mov	ax,#0x1301		! write string, move cursor AH=13显示字符串 AL=01光标跟随移动
	int	0x10
	mov ax, #0x0080			!set bp = 0x0080
	mov bp, ax
	call print_hex
	call print_nl

Print_Head_hd0:

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 页号bh=0
	int	0x10
	
	mov	cx,#10
	mov	bx,#0x0007		! page 0, attribute 7 (normal) 页号BH=0 属性BL=7正常显示
	mov	bp,#Head_hd0		! ES:BP要显示的字符串地址
	mov	ax,#0x1301		! write string, move cursor AH=13显示字符串 AL=01光标跟随移动
	int	0x10
	mov ax, #0x0082			!set bp = 0x0082
	mov bp, ax
	call print_hex
	call print_nl

!死循环
dead_loop:
	jmp dead_loop

!以16进制方式打印栈顶的16位数
print_hex:
	mov cx, #4		!4个十六进制数字
	mov dx, (bp)	!将(bp)所指的值放入dx中，如果bp是指向栈顶的话
print_digital:
	rol dx, #4
	mov ax, #0xe0f
	and al, dl
	add al, #0x30
	cmp al, #0x3a
	jl outp
	add al, #0x07
outp:
	int 0x10
	loop print_digital
	ret

!打印回车换行
print_nl:
	mov ax, #0xe0d
	int 0x10
	mov al, #0xa
	int 0x10
	ret

msg1:
	.byte 13,10
	.ascii "Now we are in SETUP"
	.byte 13,10,13,10
Cursor:
	.ascii "Cursor POS:"	!0x90000 2bytes
Memory:
	.ascii "Memory SIZE:"	!0x90002 2bytes
Cyl_hd0:
	.ascii "Cyls_hd0:"		!0x90080 2bytes
Head_hd0:
	.ascii "Heads_hd0:"		!0x90082 1byte
KB:
	.ascii "KB"

.text
endtext:
.data
enddata:
.bss
endbss:
