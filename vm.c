#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t memory[UINT16_MAX];

enum {
	R0 = 0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	PC,
	COND,
	COUNT
};

uint16_t registers[COUNT];

enum {
	BR = 0, //branch
	ADD, 	//add
	LD, 	//load
	ST, 	//store
	JSR, 	//jump reg
	AND, 	//and
	LDR, 	//load reg
	STR, 	//store register
	RTI, 	//unused
	NOT, 	//bitwise not
	LDI, 	//load indirect
	STI, 	//store indirect
	JMP, 	//jump
	RES, 	//reserved
	LEA, 	//load effective address
	TRAP, 	//execute trap
};

enum {
	FL_POS = 1 << 0,
	FL_ZRO = 1 << 1,
	FL_NEG = 1 << 2,
};


uint16_t sign_extend(uint16_t x, int bitcount)
{
	/**
	 * x = 1010b, bit count = 8
	 * 1010 >> 7 & 1
	 * 1. 0000 1010 >> 7 = 0000 0000
	 * 2. 0000 0000 & 1 = 0
	 * return 1010 = 10;
	 *
	 * case 2,
	 * x = 10001000, bitcount = 8
	 * 1. 1000 1000 >> 7 = 0000 0001
	 * 2. 0000 0001 & 1 = 1
	 * 3. 0XFFFF << 7 = 1000 0000
	 * 4. (1111 1111) 1000 1000 | 1000 0000 = 1000 1000
	 * return (1111 1111)1000 1000
	 *
	 * in short, will turn: 1000 1000 to 1111 1111 1000 1000
	 */
	if ( (x >> (bitcount - 1)) & 1 )
	{
		x |= (0xFFFF) << bitcount;
	}
	return x;
}

void update_flags(uint16_t r)
{
	if (registers[r] == 0)
	{
		registers[COND] = FL_ZRO;
	}
	else if (registers[r] << 0xF)
	{
		registers[COND] = FL_NEG;
	}
	else
	{
		registers[COND] = FL_POS;
	}
}

/*********************************
 * INSTR IMPLEMENTATIONS
 *********************************/

/**
 * @param uint16_t instr
 * 15...............0
 * op - 4 bits (12 - 15)
 * dest - 2 bits (9 - 11)
 * src1 - 2 bits (6 - 8)
 * 1 bit (bit 5), (whether imm or reg mode)
 * if (imm)
 *  bit 0 - 4 -> contains 5 bit value
 * else
 *  bit 3 - 4 -> unused
 *  bit 0 - 3 -> reg containing value
 *
 *  FORM1: ADD DEST SRC1 SRC2
 *  FORM2: ADD DEST SRC1 10
 */
void add(uint16_t instr)
{	
	//destination register
	uint16_t r0 = (instr >> 9) & 0x7;
	//first operand
	uint16_t r1 = (instr >> 6) & 0x7;
	//whether we are in imm mode
	uint16_t imm_flag = (instr >> 5) & 0x1;
	if (imm_flag)
	{
		uint16_t imm5 = sign_extend(instr & 0x1F, 5);
		registers[r0] = registers[r1] + imm5;
	}
	else
	{
		uint16_t r2 = instr & 0x7;
		registers[r0] = registers[r1] + registers[r2];
	}
	update_flags(r0);
}

/**
 * AND 
 */
//15 bit instr
//12 - 15, op code
//9 - 11, dest reg
//6 - 8, src reg 1
//5 - imm or reg?
//if (imm)
//  0 - 4, imm value
//else
// 3 - 4, unused
// 0 - 2, src reg 2
void and(uint16_t instr)	
{
	uint16_t dr = (instr >> 9) & 0x7;
	uint16_t sr1 = (instr >> 6) & 0x7;
	uint16_t imm = (instr >> 5) & 0x1;
	if (imm)
	{
		uint16_t imm_value = (instr & 0x1F);
		registers[dr] = registers[sr1] & imm_value;
	}
	else
	{
		uint16_t sr2 = instr & 0x7;
		registers[dr] = registers[sr1] & registers[sr2];
	}
	update_flags(dr);
}

void not(uint16_t instr)
{
	uint16_t dr = (instr >> 9) & 0x7;
	uint16_t sr = (instr >> 6) & 0x7;
	registers[dr] = ~registers[sr];
	update_flags(dr);
}

void br(uint16_t instr)
{
	//bit 9  = P
	//bit 10 = Z 
	//bit 11 = N 
	uint16_t cond_flag = (instr >> 9) & 0x7;
	uint16_t PCoffset9 = instr & 0x1FF;

	if (cond_flag)
	{
		registers[PC] = sign_extend(PCoffset9, 9);
	}
}

void jmp(uint16_t instr)
{
	//jump to location at bits 6 - 8
    uint16_t r1 = (instr >> 6) & 0x7;
    registers[PC] = registers[r1];
}

void lea(uint16_t instr)
{
	uint16_t dr = (instr >> 9) & 0x7;
	uint16_t pcoffset9 = (instr & 0x1ff);
	pcoffset9 = sign_extend(pcoffset9, 9);

	registers[dr] = registers[PC] + pcoffset9;
	update_flags(dr);
}

/*
 * SAMPLE ASSEMBLY PROGRAM FOR THIS VM
.ORIG 0x3000
LEA R0, HELLO_STR
PUTs
HALT
HELLO_STR .STRINGZ "Hello, world"
*/

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: ....\n");
		exit(1);
	}

	for (int j = 0; j < argc; ++j) {
		if (!read_image(argv[j]))
		{
			printf("Failed to load image: %s\n", argv[j]);
			exit(1);
		}
	}
	//1. load args
	//2. setup
	
	// set the PC to starting pos
	// 0x3000 is the default
	enum { PC_START = 0x3000 };
	registers[PC] = PC_START;

	int running = 1;
	while (running) {
		//FETCH
		uint16_t instr = mem_read(registers[PC]++);
		uint16_t op = instr >> 12;

		switch(op) {
			case ADD:
				add(instr);
				break;
			case AND:
				and(instr);
				break;
			case NOT:
				not(instr);
				break;
			case BR:
				br(instr);
				break;
			case JMP:
				jmp(instr);
				break;
			case JSR:
				break;
			case LD:
				break;
			case LDI:
				break;
			case LDR:
				break;
			case LEA:
				lea(instr);
				break;
			case ST:
				break;
			case STI:
				break;
			case STR:
				break;
			case TRAP:
				break;
			case RES:
			case RTI:
			default:
				break;

		}
	}
	//shutdown
	return 0;
}















