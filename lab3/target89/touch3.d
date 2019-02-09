
touch3.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <.text>:
   0:	48 c7 04 24 35 65 35 	movq   $0x33356535,(%rsp)
   7:	33 
   8:	48 c7 44 24 04 37 36 	movq   $0x62303637,0x4(%rsp)
   f:	30 62 
  11:	48 8d 3c 24          	lea    (%rsp),%rdi
  15:	68 d6 18 40 00       	pushq  $0x4018d6
  1a:	c3                   	retq   
