#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;

#define err_print(_s, _a ...) do { \
  if (lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", lineno, ## _a); \
} while (0);


int64_t vmaddr = 0;    /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8",  REG_R8,  3},
    {"%r9",  REG_R9,  3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}
};
const reg_t* find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}


/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovq", 6,HPACK(I_RRMOVQ, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVQ, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVQ, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVQ, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2 },
    {"irmovq", 6,HPACK(I_IRMOVQ, F_NONE), 10 },
    {"rmmovq", 6,HPACK(I_RMMOVQ, F_NONE), 10 },
    {"mrmovq", 6,HPACK(I_MRMOVQ, F_NONE), 10 },
    {"addq", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subq", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andq", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorq", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 9 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 9 },
    {"jl", 2,    HPACK(I_JMP, C_L), 9 },
    {"je", 2,    HPACK(I_JMP, C_E), 9 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 9 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 9 },
    {"jg", 2,    HPACK(I_JMP, C_G), 9 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 9 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2 },
    {"popq", 4,  HPACK(I_POPQ, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
	if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
	    return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;  //symtab is a linked list with empty head for symbol

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{	
	symbol_t *this = symtab->next;//last added symbol is the next of head
	while (this)
	{
		if (!strcmp(this->name, name)) // if equal strcmp return 0
			return this;
		this = this->next;
	}
	return NULL;//not exist
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
    /* check duplicate (maek sure the symbol not exist)*/
	if (find_symbol(name))
	{
		err_print("Dup symbol:%s",name);
		return -1;
	}
    /* create new symbol_t (don't forget to free it)*/
	symbol_t *this = (symbol_t*)malloc(sizeof(symbol_t));
	memset(this, 0, sizeof(symbol_t));
	this->name = name;
	this->addr = vmaddr;
	//this->next = symtab->next;
    /* add the new symbol_t to symbol table */
	//symtab->next = this;//add to the first
    symbol_t *last = symtab;
	while (last->next)
		last = last->next;
	last->next = this;
	return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist  							??????
 */
void add_reloc(char *name, bin_t *bin)
{
    /* create new reloc_t (don't forget to free it)*/
    reloc_t *this = (reloc_t*)malloc( sizeof(reloc_t) );
	if (this == NULL)
	{
		exit(0);
	}
	this->y64bin = bin;
	this->name = name;
	/* add the new reloc_t to relocation table */
	reloc_t *last = reltab;
	while (last->next)
	{
		last = last->next;
	}
	last->next = this;
}


/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    /* skip the blank */
	char *P = *ptr;
	SKIP_BLANK(P); 									//why so much blank
	//if (IS_END(P))
	//	return PARSE_ERR;
    /* find_instr and check end */
	instr_t *tmpins = find_instr(P); // p is a pointer to string
	if (tmpins == NULL)
		return PARSE_ERR;

	P += tmpins->len;
	if (!IS_END(P) && !IS_BLANK(P))	//ret ins with end, rrmovq with blank
		return PARSE_ERR;
    /* set 'ptr' and 'inst' */
	*ptr = P;
	*inst = tmpins;
    return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)// delim is certain
{
    /* skip the blank and check */
	char *P = *ptr;
	SKIP_BLANK(P);
	
	if (IS_END(P))
		return PARSE_ERR;
	if (*P != delim)
	{
		err_print("Invalid '%c'",delim );
		return PARSE_ERR;
	}
    /* set 'ptr' */
	*ptr = P + 1;
    return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    /* skip the blank and check */	
	char *P = *ptr;
	SKIP_BLANK(P);  
	if (IS_END(P))
		return PARSE_ERR;
	if (!IS_REG(P))
		return PARSE_ERR;  
    /* find register */
	reg_t* tmpreg = find_register(P);
	if (tmpreg == NULL)
	{
		err_print("Invalid REG");
		return PARSE_ERR;
	}
    /* set 'ptr' and 'regid' */
	*ptr = P + tmpreg->namelen;//+3 or 4
	*regid = tmpreg->id;
    return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    /* skip the blank and check */
	char *P = *ptr;
	SKIP_BLANK(P);
	if (!IS_LETTER(P))
		return PARSE_ERR;
    /* allocate name and copy to it */
	*name = (char*)malloc(MAX_INSLEN); 
	int len = 0;
	while (IS_LETTER(P+len) || IS_DIGIT(P+len))
	{	
		*(*name + len) = *(P + len);		
		len++;
	}
	*(*name + len) = '\0';
    /* set 'ptr' and 'name' */
	*ptr = P + len;
    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    /* skip the blank and check */
	char *P = *ptr; 
	SKIP_BLANK(P);
	if (!IS_DIGIT(P))
		return PARSE_ERR;
    /* calculate the digit, (NOTE: see strtoull()) 
	   strtoull(const char *nptr, char **endptr, int base)
	   endptr will save the place of non_digit after return*/
	char *endptr;
	long digit = strtoull(P, &endptr, 0);
	
    /* set 'ptr' and 'value' */
	*ptr = endptr;
	*value = digit;
    return PARSE_DIGIT;

}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
	char *P = *ptr;
	SKIP_BLANK(P);
	if (!IS_IMM(P) && !IS_LETTER(P))
	{
		err_print("Invalid Immediate"); 
		return PARSE_ERR;
	}
    /* if IS_IMM, then parse the digit */
	if (IS_IMM(P))
	{	
		P++;
		if (!IS_DIGIT(P))
		{
			err_print("Invalid Immediate");
			return PARSE_ERR;
		}
		char *endptr;
		long digit = strtoull(P, &endptr, 0);
	/* set 'ptr' and 'name' or 'value' */
		*ptr = endptr;
		*value = digit;
		return PARSE_DIGIT;
	}
    /* if IS_LETTER, then parse the symbol */
    if (IS_LETTER(P))
	{
		int len = 0;
		while (IS_LETTER(P+len) || IS_DIGIT(P+len))
			len++;
		char *tmpname = (char*)malloc(len + 1);
		strncpy(tmpname, P, len);
		tmpname[len] = '\0';
	/* set 'ptr' and 'name' or 'value' */
		*ptr = P + len;
		*name = tmpname;
		return PARSE_SYMBOL;

	}
    /* set 'ptr' and 'name' or 'value' */

    //return PARSE_ERR;
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%rbp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
    /* skip the blank and check */
	char *P = *ptr;
	SKIP_BLANK(P);
	if (!IS_DIGIT(P) && *P != '(')
	{	
		err_print("Invalid MEM");
		return PARSE_ERR;
	}
    /* calculate the digit and register, (ex: (%rbp) or 8(%rbp)) */
	long digit = 0;
	if (IS_DIGIT(P))
	{
		char *endptr;
		digit = strtoull(P, &endptr, 0);
		P = endptr;
	}
	if (*P != '(')
	{
		err_print("Invalid MEM");
		return PARSE_ERR;
	}
	
	P++;
	if (!IS_REG(P))
	{
		err_print("Invalid MEM");
		return PARSE_ERR;
	}
	reg_t* tmpreg = find_register(P);
	if (tmpreg == NULL)
		return PARSE_ERR;
	P += tmpreg->namelen;
	if (*P != ')')
	{
		err_print("Invalid MEM");
		return PARSE_ERR;
	}
    /* set 'ptr', 'value' and 'regid' */
	*ptr = P + 1;
	*value = digit;
	*regid = tmpreg->id;
    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
	char *P = *ptr;
	SKIP_BLANK(P);
	if (!IS_DIGIT(P) && !IS_LETTER(P))
		return PARSE_ERR;
    /* if IS_DIGIT, then parse the digit */
	if (IS_DIGIT(P))
	{
		char *endptr;
		long digit = strtoull(P, &endptr, 0);
		*ptr = endptr;
		*value = digit;
		return PARSE_DIGIT;
	}
    /* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(P))
	{
		//int len = 0;
		//while (IS_LETTER(P+len) || IS_DIGIT(P+len))
		//	len++;
		//char *tmpname = (char*)malloc(len + 1);
		//strncpy(tmpname, P, len);
		//tmpname[len] = '\0';

		//*ptr = P + len;
		//*name = tmpname;
		return parse_symbol(ptr, name);
	}
    /* set 'ptr', 'name' and 'value' */

    return PARSE_ERR;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    /* skip the blank and check */
	char *P = *ptr;
	SKIP_BLANK(P);
	if (!IS_LETTER(P))
		return PARSE_ERR;
    /* allocate name and copy to it */
	/*int len = 0;
	while (IS_LETTER(P+len) || IS_DIGIT(P+len))
		len++;
	if (*(P+len) != ':')
		return PARSE_ERR;
	char **tmpname = (char*)malloc(len+1);
	strncpy(tmpname, P, len);
	tmpname[len] = '\0';*/
	if (parse_symbol(&P, name) == PARSE_ERR)
		return PARSE_ERR;	
	if (*P != ':')
		return PARSE_ERR;
    /* set 'ptr' and 'name' */
	*ptr = P + 1; // +1 for :
    return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y64 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{

/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%rbp), %rcx
*           call SUM  #invoke SUM function */	

	char *y64asm = line->y64asm;
	char *P = y64asm;
    /* skip blank and check IS_END */
   	SKIP_BLANK(P);
	if (IS_END(P))
		return TYPE_COMM;
    /* is a comment ? */
	if (IS_COMMENT(P))
		return TYPE_COMM;
    /* is a label ? */
	char *name;
	if (parse_label(&P, &name) == PARSE_LABEL)
	{
		if (add_symbol(name) == -1)
		{
			line->type = TYPE_ERR;
			return line->type;
		}
		symbol_t *symbol = find_symbol(name);
		//symbol->addr = vmaddr;
		SKIP_BLANK(P);
		if (IS_END(P) || IS_COMMENT(P))
		{
			line->y64bin.addr = vmaddr;
			line->type = TYPE_INS;
			return line->type;
		}	
	}
	else
	{
		P = y64asm;
	}
    /* is an instruction ? */
	instr_t *ins;
	if (parse_instr(&P, &ins) == PARSE_ERR)
	{
		line->type = TYPE_ERR;
		return line->type;
	}
    /* set type and y64bin */
	line->type = TYPE_INS;
	line->y64bin.bytes = ins->bytes; 	//size of ins
	line->y64bin.codes[0] = ins->code; 	//byte for ins+op
	line->y64bin.addr = vmaddr;
    /* update vmaddr */    
	vmaddr += ins->bytes;
    /* parse the rest of instruction according to the itype */
	regid_t rA, rB;
	long imm;
	long *tmp;
	parse_t sign;

	switch (HIGH(line->y64bin.codes[0]))
	{
		case I_HALT:
		case I_NOP:
		case I_RET:
			return line->type;
		case I_RRMOVQ:
		case I_ALU://add rA, rB
			if (parse_reg(&P, &rA) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_delim(&P, ',') == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_reg(&P, &rB) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			line->y64bin.codes[1] = HPACK(rA, rB);
			break;
		case I_PUSHQ:
		case I_POPQ:
			if (parse_reg(&P, &rA) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			line->y64bin.codes[1] = HPACK(rA, REG_NONE); // REG_NONE is 0xF
			break;
		case I_IRMOVQ://irmovq V, rB
			sign = parse_imm(&P, &name, &imm);
			if (sign == PARSE_ERR)
			{	
				line->type = TYPE_ERR;
				return line->type;
			}
			if (sign == PARSE_SYMBOL)
			{
				add_reloc(name, &line->y64bin);
			}
			else if (sign == PARSE_DIGIT)
			{
				//(long *) (line->y64bin.codes+2) = &imm;
				tmp = (long *) (line->y64bin.codes + 2);
				*tmp = imm;
			}
			if (parse_delim(&P, ',') == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_reg(&P, &rB) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			line->y64bin.codes[1] = HPACK(REG_NONE, rB);
			break;
		case I_RMMOVQ://rmmovq rA, D(rB)
			if (parse_reg(&P, &rA) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_delim(&P, ',') == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_mem(&P, &imm, &rB) == PARSE_ERR)
			{
				line->type = TYPE_ERR;
				return line->type;
			}

			line->y64bin.codes[1] = HPACK(rA, rB);
			//(long *)&line->y64bin.codes[2] = &imm; 
			tmp = (long *) (line->y64bin.codes + 2);
			*tmp = imm;
			break;
		case I_MRMOVQ://mrmovq D(rB), rA
			if (parse_mem(&P, &imm, &rB) == PARSE_ERR) {
	            line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_delim(&P, ',') == PARSE_ERR) {
				line->type = TYPE_ERR;
				return line->type;
			}
			if (parse_reg(&P, &rA) == PARSE_ERR) {
				line->type = TYPE_ERR;
				return line->type;
			}
			line->y64bin.codes[1] = HPACK(rA, rB);
			//(long*)&line->y64bin.codes[2] = &imm;
			tmp = (long *) (line->y64bin.codes + 2);
			*tmp = imm;

			break;
		case I_JMP:	//jxx D
		case I_CALL://call D
			if (parse_symbol(&P, &name) == PARSE_ERR)
			{
				err_print("Invalid DEST");
				line->type = TYPE_ERR;
				return line->type;
			}
			add_reloc(name, &line->y64bin);
			break;
		case I_DIRECTIVE:
			switch (LOW(line->y64bin.codes[0]))  
			{
				case D_DATA:
					sign = parse_data(&P, &name, &imm);
					if (sign == PARSE_ERR)
					{
						line->type = TYPE_ERR;
						return line->type;
					}
					if (sign == PARSE_SYMBOL)
						add_reloc(name, &line->y64bin);
					if (sign == PARSE_DIGIT)
					{
						//(long*)line->y64bin.codes = &imm;
						tmp = (long *) (line->y64bin.codes);
						*tmp = imm;
					}	
					break;
				case D_POS: //.pos 0x100 , make here address to 0x100
					if (parse_digit(&P, &imm) == PARSE_ERR)
					{
						line->type = TYPE_ERR;
						return line->type;
					}
					vmaddr = imm;
					line->y64bin.addr = vmaddr;
					break;
				case D_ALIGN://.align 8
					if (parse_digit(&P, &imm) == PARSE_ERR)
					{
						line->type = TYPE_ERR;
						return line->type;
					}
					if (vmaddr % imm != 0)
						vmaddr += imm - vmaddr % imm;
					line->y64bin.addr = vmaddr;
					break;
			}
			break;
		default:
			line->type = TYPE_ERR;
			return line->type;


	}
    
    return line->type;
}

/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        while ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno ++;

        if (parse_line(line) == TYPE_ERR) {
            return -1;
        }
    }

	lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;
	long *tmp;    
    rtmp = reltab->next;
	while (rtmp) {
		//fprintf(stderr, "relocate name %s code%x 888 \n", rtmp->name, rtmp->y64bin->codes);
        /* find symbol */

		symbol_t *symbol = find_symbol(rtmp->name);
		if (symbol == NULL)
		{
			err_print("Unknown symbol:'%s'", rtmp->name);
			return -1;
		}
		//fprintf(stderr, "relocate find error888 \n");
        /* relocate y64bin according itype */
		
		switch (HIGH(rtmp->y64bin->codes[0]))
		{
			case I_IRMOVQ:
				//(long*)&rtmp->y64bin->codes[2] = symbol->addr;
				tmp = (long *) (rtmp->y64bin->codes + 2);
				*tmp = symbol->addr;
				break;
			case I_JMP:
			case I_CALL:
				//(long*)&rtmp->y64bin->codes[1] = symbol->addr;
				tmp = (long *) (rtmp->y64bin->codes + 1);
				*tmp = symbol->addr;
				break;
			case I_DIRECTIVE:
				//(long*)&rtmp->y64bin->codes[0] = symbol->addr;
				tmp = (long *) (rtmp->y64bin->codes + 0);
				*tmp = symbol->addr;
				break;
			default:
				//impossible
				return -1;
		}
		//int pos = rtmp->y64bin->bytes - 8;
		//memcpy(rtmp->y64bin->codes + pos, (void*)&symbol->addr, sizeof(int));
        /* next */
        rtmp = rtmp->next;

    }
    return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y64 binary code */
	line_t *line_buf = line_head->next;
    /* binary write y64 code to output file (NOTE: see fwrite()) */
    /*while (line_buf)
	{
		if (fwrite(line_buf->y64bin.codes, sizeof(byte_t), line_buf->y64bin.bytes, out) != line_buf->y64bin.bytes)
			return -1;
		line_buf = line_buf->next;
	}*/
	while (line_buf) {
		if (line_buf->type == TYPE_INS) {
	    	if (fseek(out, line_buf->y64bin.addr, SEEK_SET) != 0)
				return -1;
			if (fwrite(line_buf->y64bin.codes, 1, line_buf->y64bin.bytes, out) 
				!= line_buf->y64bin.bytes)
				return -1;
		}
		line_buf = line_buf->next;
	}
    return 0;
	
}


/* whether print the readable output to screen or not ? */
bool_t screen = TRUE; 

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (value >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[64];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y64bin = &line->y64bin;
        int i;
        
        strcpy(buf, "  0x000:                      | ");
        
        hexstuff(buf+4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf+9+2*i, y64bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = line_head->next;
        if (line_head->y64asm) 
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print("File name too long");
        exit(1);
    }
 

    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);


    /* relocate binary code */
    if (relocate() < 0) {
        err_print("Relocate binary code error");
        exit(1);
    }


    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
    
    /* print to screen (.yo file) */
    if (screen)
       print_screen(); 

    /* finit */
    finit();
    return 0;
}


