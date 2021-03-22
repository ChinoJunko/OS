/* Real Mode Hello World */
#.code16
#
#.global start
#start:
#	movw %cs, %ax
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %ss
#	movw $0x7d00, %ax
#	movw %ax, %sp # setting stack pointer to 0x7d00

#loop:
#	jmp loop


/* Protected Mode Hello World */
#.code16
#
#.global start
#start:
#	movw %cs, %ax
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %ss
#
#.code32
#start32:
#	movw $0x10, %ax # setting data segment selector
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %fs
#	movw %ax, %ss
#loop32:
#	jmp loop32
#
#
#.p2align 2


/* Protected Mode Loading Hello World APP */
.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	#TODO: Protected Mode Here
    #关中断
    cli 
    #打开A20
    pushw %ax
    movw $0x2401, %ax
    int $0x15
    popw %ax
    #加载GDTR   
    lgdt gdtDesc
    #设置cr0的PE位为1
    movl %cr0, %eax                                 
    orl $0x1, %eax
    movl %eax, %cr0
    #长跳转切换至保护模式
    ljmp $0x08, $start32

    

.code32
start32:
	movw $0x10, %ax # setting data segment selector
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %ss
	movw $0x18, %ax # setting graphics data segment selector
	movw %ax, %gs
	
	movl $0x8000, %eax # setting esp
	movl %eax, %esp
	jmp bootMain # jump to bootMain in boot.c

.p2align 2
gdt: 
        #GDT第一个表项为空
        	.word 0,0                       
        .byte 0,0,0,0

        #代码段描述符
        .word 0xffff,0                  
        .byte 0,0x9a,0xcf,0

        #数据段描述符
        .word 0xffff,0                  
        .byte 0,0x92,0xcf,0
        
        #视频段描述符
        .word 0xffff,0x8000             
        .byte 0x0b,0x92,0xcf,0

gdtDesc:
        .word (gdtDesc - gdt -1)
        .long gdt
