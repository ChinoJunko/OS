#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	//putS("in irqHandle");
	/*
	int irq = tf->irq;
	do {
		putChar((char)((irq % 10) + '0'));
		irq /= 10;
	} while (irq);
	putChar('\n');
	*/
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
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
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	asm volatile("cli");
	uint32_t code = getKeyCode();
	if (code == 0)
		return;
	char character = getChar(code);
	if (code == 0xe) { // 退格符
		// TODO: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if (displayCol > 0 && bufferHead != bufferTail) {
			uint16_t data = 0 | (0x0c << 8);
			displayCol--;
			int pos = (displayRow * 80 + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			bufferTail = (bufferTail + MAX_KEYBUFFER_SIZE - 1) % MAX_KEYBUFFER_SIZE;
		}
	} else if (code == 0x1c) { // 回车符
		// TODO: 处理回车情况
		keyBuffer[bufferTail] = '\n';
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		displayCol = 0;
		displayRow++;
		if (displayRow >= 25) {
			displayRow = 24;
			scrollScreen();
		}
		putChar(character);
	} else if (code < 0x81 && character > 31 && character < 127) { // 正常字符
		// TODO: 注意输入的大小写的实现、不可打印字符的处理
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
	asm volatile("sti");
}

void syscallHandle(struct TrapFrame *tf) {
	switch (tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default: break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch (tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	//putS("in syscallPrint");
	int sel = USEL(SEG_UDATA); //TODO: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==25){
				displayRow=24;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==80){
				displayRow++;
				displayCol=0;
				if(displayRow==25){
					displayRow=24;
					displayCol=0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	if (bufferHead == bufferTail) {
		while (bufferHead == bufferTail)
			asm volatile("hlt");
		while (keyBuffer[(bufferTail + MAX_KEYBUFFER_SIZE - 1) % MAX_KEYBUFFER_SIZE] != '\n')
			asm volatile("hlt");
		tf->eax = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;	
		if ((bufferTail + MAX_KEYBUFFER_SIZE - 1) % MAX_KEYBUFFER_SIZE == bufferHead)
			bufferHead = bufferTail;
	}
	else {
		tf->eax = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
	}
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	int sel = USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	char character;
	int i = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	if (bufferHead == bufferTail) {
		while (bufferHead == bufferTail)
			asm volatile("hlt");
		while (keyBuffer[(bufferTail + MAX_KEYBUFFER_SIZE - 1) % MAX_KEYBUFFER_SIZE] != '\n')
			asm volatile("hlt");
	}
	while (i < size - 1) {
		if (keyBuffer[bufferHead] == '\n') {
			bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
			break;
		}
		character = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
		asm volatile("movb %0, %%es:(%1)"::"r"(character), "r"(str+i));
		i++;	
	}
	asm volatile("movb $0, %%es:(%0)"::"r"(str+i));
}
