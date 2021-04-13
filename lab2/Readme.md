# Lab 2
## Information
#### Name: CaiHongbin
#### Num: 191220002
#### EMail: imaizumikagerouzi@foxmail.com
#### Development Environment: Linux (Deepin -> Ubuntu)
#### IDE: Vscode
---
## ProGress:  All finished
---
## Here is the code I modified
#### 1.Start protection mode from real mode and jump to bootloader
    movl $0x1fffff, %eax # tss.esp0 = 0x1fffff in kvm.c
#### 2.Loading kernel by bootloader
    kMainEntry = (void (*)(void))((struct ELFHeader*)elf)->entry;
	phoff = ((struct ELFHeader*)elf)->phoff;
	struct ProgramHeader* p = (struct ProgramHeader*)(elf + phoff);
	if (p->vaddr < 0x100000)
		p++;
	offset = p->off;

    for (i = 0; i < 200 * 512; i++) {
		*(unsigned char *)(elf + i) = *(unsigned char *)(elf + i + offset);
	}
	
	kMainEntry();
#### 3.Improve kernel related initialization settings
    initIdt();	// initialize idt
	initIntr();	// iniialize 8259a
	initSeg();	// initialize gdt, tss
	initVga();  // initialize vga device
	initKeyTable();	// initialize keyboard device
	loadUMain(); // load user program, enter user space
#### 4.Loading user program by kernel(Like bootloader)
    uint32_t uMainEntry = ((struct ELFHeader*)elf)->entry;
	int phoff = ((struct ELFHeader*)elf)->phoff;
	int offset = ((struct ProgramHeader*)(elf + phoff))->off;
	
	for (i = 0; i < 200 * 512; i++) {
		*(unsigned char *)(elf + i) = *(unsigned char *)(elf + i + offset);
	}
	
	enterUserSpace(uMainEntry);
#### 5.Implement library functions needed by users
##### The hardest part(irqHandle made me mad:D)
##### a.Initial IDT
    setTrap(idt + 0x1e, KSEL(SEG_KCODE), (uint32_t)irqSecException, DPL_KERN);
    ...
    setTrap(idt + 0x80, KSEL(SEG_KCODE), (uint32_t)irqSyscall, DPL_USER);

    saveIdt(idt, sizeof(idt));
##### b.Interrupt handling
    switch(tf->irq) {
		case -1: 
			break;
		case 0xd:
			GProtectFaultHandle(tf);
			break;
		case 0x21:
			KeyboardHandle(tf);
			break;
		case 0x80:
			syscallHandle(tf);
			break;
		default:
			assert(0);
	}
##### c.KeyboardHandle
    uint32_t code = getKeyCode();
	if (code == 0)
		return;
	char character = getChar(code);
	if (code == 0xe) {
		if (displayCol > 0 && bufferHead != bufferTail) {
			uint16_t data = 0 | (0x0c << 8);
			displayCol--;
			int pos = (displayRow * 80 + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			bufferTail = (bufferTail + MAX_KEYBUFFER_SIZE - 1) % MAX_KEYBUFFER_SIZE;
		}
	} else if (code == 0x1c) {
		keyBuffer[bufferTail] = '\n';
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		displayCol = 0;
		displayRow++;
		if (displayRow >= 25) {
			displayRow = 24;
			scrollScreen();
		}
		putChar(character);
	} else if (code < 0x81 && character > 31 && character < 127) {
		keyBuffer[bufferTail] = character;
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		uint16_t data = character | (0x0c << 8);
		int pos = (displayRow * 80 + displayCol) * 2;
		asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		displayCol++;
		if (displayCol >= 80) {
			displayCol = 0;
			displayRow++;
			if (displayRow >= 25) {
				displayRow = 24;
				scrollScreen();
			}
		}
		putChar(character);
	}
	updateCursor(displayRow, displayCol);
##### d.otherHandle(syscall-getchar,getstr,printf)
    ...
##### e.SysCallFunc-read
    char getChar(){
	    return syscall(SYS_READ, STD_IN, 0, 0, 0, 0);
    }

    void getStr(char *str, int size){
	    syscall(SYS_READ, STD_STR, (uint32_t)str, size, 0, 0);
	    return;
    }
##### f.SysCallFunc-write
    while(format[i]!=0){
        switch(format[i])
        {
            case '%':   
                i++;
                switch(format[i])
                {
                    case 'd':
				        index++;
				        i++;
				        paraList+=_INTSIZEOF(int);
				        decimal=*(int*)paraList;
				        int size=0;
				        int temp=decimal;
				        while(temp!=0)
				        {
					        temp/=10;
					        size++;
				        }
				        count=dec2Str(decimal,buffer,size,count);
                        break;
				    case...
				    default:	
						buffer[count]=format[i];
						count++;
						if(count==MAX_BUFFER_SIZE) {
							syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)MAX_BUFFER_SIZE, 0, 0);
						count=0;
													}
						i++;break;
           	  }
        break;
		default:
						buffer[count]=format[i];
						count++;
						if(count==MAX_BUFFER_SIZE) {
							syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)MAX_BUFFER_SIZE, 0, 0);
						count=0;
													}
						i++;break;
		}
	}
#### 6.The user program calls the library function of user-defined implementation to complete the input and output formatting, and passes the test code(The following is the result of the test)
![](https://i.bmp.ovh/imgs/2021/04/d7d85c029767a276.png)

---
## Promblem
#### -About why I have to change my environment
When I firstly finished the part of SyscallgetChar, the system maybe can echo the character correctly. But it can't end when enter. 
![](https://i.bmp.ovh/imgs/2021/04/2045836d283ae7f8.png)

I found the bug in irqHandle.c:

    while (bufferHead == bufferTail)
		asm volatile("hlt");
	while (keyBuffer[(bufferTail + MAX_KEYBUFFER_SIZE - 1) % MAX_KEYBUFFER_SIZE] != '\n')
		asm volatile("hlt");
"hlt" is used to wait for external interrupt
but there's something wrong with it. It does not return the clock signal to normal.

Hours later...

I gave up to sovle it. I guess it is because of Multi architecture support function. But Deepin 32-bit Lib is hard to find on the Internet. So I change to Ubuntu and it work well this time.

#### If you have any suggestions that can help me do the lab in Deepin, welcome to write to my mailbox. Thank you!