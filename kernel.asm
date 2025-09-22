bits 32

; Copyright (C) 2014  Arjun Sreedharan
; License: GPL version 2 or higher http://www.gnu.org/licenses/gpl.html

; ===================
;  HEADER DE RECONHECIMENTO MODO MULTIBOOT
; ===================

section .text
		; Multiboot header para ser carregado por bootloaders como GRUB
		align 4
		dd 0x1BADB002              ; magic (identificador multiboot)
		dd 0x00                    ; flags
		dd - (0x1BADB002 + 0x00)   ; checksum 

; ===================
;  EXPORTAÇÃO DE SÍMBOLOS PARA O C POR MEIO DO LINKER
; ===================
global start
global keyboard_handler
global read_port
global write_port
global load_idt

extern kmain         ; Função principal do kernel (em C)
extern keyboard_handler_main

; ===================
;  FUNÇÕES DE PORTA DE I/O (usadas pelo C)
; ===================
read_port:
	mov edx, [esp + 4]
	; al é o byte menos significativo de eax
	in al, dx    ; dx é o menos significativo de edx
	ret

write_port:
	mov   edx, [esp + 4]    
	mov   al, [esp + 4 + 4]  
	out   dx, al  
	ret

; ===================
;  FUNÇÃO PARA CARREGAR A IDT (Interrupt Descriptor Table)
; ===================
load_idt:
	mov edx, [esp + 4]
	lidt [edx]
	sti                 ; habilita interrupções
	ret

; ===================
;  HANDLER DE INTERRUPÇÃO DO TECLADO
; ===================
keyboard_handler:                 
	call    keyboard_handler_main
	iretd

; ===================
;  PONTO DE ENTRADA DO KERNEL
; ===================
start:
	cli                 ; desabilita interrupções
	mov esp, stack_space ; inicializa pilha
	call kmain          ; chama função principal em C
	hlt                 ; para a CPU

section .bss
resb 8192 ; 8KB para pilha
stack_space:
