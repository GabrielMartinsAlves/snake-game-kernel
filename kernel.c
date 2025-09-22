
// ===================
//  INCLUDES E DEFINIÇÕES
// ===================
#include "keyboard_map.h"


// ===================
//  DEFINIÇÕES DE VÍDEO
// ===================
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES


// ===================
//  DEFINIÇÕES DE TECLADO E INTERRUPÇÃO
// ===================
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

// ===================
//  DEFINIÇÕES DO JOGO SNAKE
// ===================
#define GAME_WIDTH 78
#define GAME_HEIGHT 22
#define MAX_SNAKE_LENGTH 500
#define INITIAL_SNAKE_LENGTH 3
#define GAME_SPEED 10

#define DIRECTION_UP 1
#define DIRECTION_DOWN 2
#define DIRECTION_LEFT 3
#define DIRECTION_RIGHT 4

#define STATE_MENU 0
#define STATE_PLAYING 1
#define STATE_GAME_OVER 2


// ===================
//  VARIÁVEIS E FUNÇÕES EXTERNAS (Assembly)
// ===================
extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);


// ===================
//  VARIÁVEIS DE VÍDEO
// ===================
unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;


// ===================
//  ESTRUTURAS DE DADOS DO JOGO
// ===================
typedef struct {
	int x;
	int y;
} Position;

typedef struct {
	Position body[MAX_SNAKE_LENGTH];
	int length;
	int direction;
} Snake;

typedef struct {
	Snake snake;
	Position food;
	int score;
	int state;
	char input_buffer[10];
	int buffer_index;
	unsigned long tick_counter;
} Game;

Game game;


// ===================
//  FUNÇÕES DE VÍDEO
// ===================
void kprint(const char *str);
void kprint_newline(void);
void clear_screen(void);


// ===================
//  ESTRUTURA E VARIÁVEL DA IDT (TECLADO/INTERRUPÇÃO)
// ===================
struct IDT_entry {
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];


// ===================
//  FUNÇÃO DE RANDOMIZAÇÃO (JOGO)
// ===================
unsigned int rand_seed = 1;

unsigned int simple_rand(void) {
	rand_seed = (rand_seed * 1103515245 + 12345) & 0x7fffffff;
	return rand_seed;
}


// ===================
//  FUNÇÕES DE VÍDEO (DESENHO NA TELA)
// ===================
void set_cursor(int x, int y) {
	current_loc = (y * COLUMNS_IN_LINE + x) * BYTES_FOR_EACH_ELEMENT;
}

void draw_char(int x, int y, char c, char color) {
	int pos = (y * COLUMNS_IN_LINE + x) * BYTES_FOR_EACH_ELEMENT;
	vidptr[pos] = c;
	vidptr[pos + 1] = color;
}

void draw_border(void) {
	int x, y;
	// Desenha as bordas do campo do jogo
	for (x = 0; x < GAME_WIDTH + 2; x++) {
		draw_char(x, 0, '#', 0x0F);
		draw_char(x, GAME_HEIGHT + 1, '#', 0x0F);
	}
	for (y = 0; y < GAME_HEIGHT + 2; y++) {
		draw_char(0, y, '#', 0x0F);
		draw_char(GAME_WIDTH + 1, y, '#', 0x0F);
	}
}


// ===================
//  FUNÇÕES DO JOGO SNAKE
// ===================
void init_game(void) {
	int i;
	// Inicializa a cobrinha e a comida
	game.snake.length = INITIAL_SNAKE_LENGTH;
	game.snake.direction = DIRECTION_RIGHT;
	game.score = 0;
	for (i = 0; i < INITIAL_SNAKE_LENGTH; i++) {
		game.snake.body[i].x = GAME_WIDTH / 2 - i;
		game.snake.body[i].y = GAME_HEIGHT / 2;
	}
	game.food.x = (simple_rand() % (GAME_WIDTH - 2)) + 1;
	game.food.y = (simple_rand() % (GAME_HEIGHT - 2)) + 1;
}


// Verifica colisão da cobrinha com parede ou corpo
int check_collision(int x, int y) {
	int i;
	if (x <= 0 || x >= GAME_WIDTH + 1 || y <= 0 || y >= GAME_HEIGHT + 1) {
		return 1;
	}
	for (i = 0; i < game.snake.length; i++) {
		if (game.snake.body[i].x == x && game.snake.body[i].y == y) {
			return 1;
		}
	}
	return 0;
}


// Gera nova posição para a comida
void generate_food(void) {
	do {
		game.food.x = (simple_rand() % GAME_WIDTH) + 1;
		game.food.y = (simple_rand() % GAME_HEIGHT) + 1;
	} while (check_collision(game.food.x, game.food.y));
}


// Atualiza a posição da cobrinha e verifica colisão/comida
void update_snake(void) {
	int i;
	Position new_head;
	new_head = game.snake.body[0];
	switch (game.snake.direction) {
		case DIRECTION_UP:
			new_head.y--;
			break;
		case DIRECTION_DOWN:
			new_head.y++;
			break;
		case DIRECTION_LEFT:
			new_head.x--;
			break;
		case DIRECTION_RIGHT:
			new_head.x++;
			break;
	}
	if (check_collision(new_head.x, new_head.y)) {
		game.state = STATE_GAME_OVER;
		return;
	}
	for (i = game.snake.length - 1; i > 0; i--) {
		game.snake.body[i] = game.snake.body[i - 1];
	}
	game.snake.body[0] = new_head;
	if (new_head.x == game.food.x && new_head.y == game.food.y) {
		if (game.snake.length < MAX_SNAKE_LENGTH) {
			game.snake.length++;
			game.score += 10;
		}
		generate_food();
	}
}


// Desenha o estado atual do jogo na tela
void draw_game(void) {
	int i, x, y;
	for (y = 1; y <= GAME_HEIGHT; y++) {
		for (x = 1; x <= GAME_WIDTH; x++) {
			draw_char(x, y, ' ', 0x00);
		}
	}
	draw_char(game.food.x, game.food.y, '*', 0x0E);
	for (i = 0; i < game.snake.length; i++) {
		if (i == 0) {
			draw_char(game.snake.body[i].x, game.snake.body[i].y, 'O', 0x0A);
		} else {
			draw_char(game.snake.body[i].x, game.snake.body[i].y, 'o', 0x02);
		}
	}
	set_cursor(0, GAME_HEIGHT + 3);
	kprint("Score: ");
	char score_str[10];
	int score = game.score;
	int j = 0;
	if (score == 0) {
		score_str[j++] = '0';
	} else {
		char temp[10];
		int k = 0;
		while (score > 0) {
			temp[k++] = '0' + (score % 10);
			score /= 10;
		}
		while (k > 0) {
			score_str[j++] = temp[--k];
		}
	}
	score_str[j] = '\0';
	kprint(score_str);
}


// ===================
//  FUNÇÕES DE INTERRUPÇÃO/TECLADO
// ===================
void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	// Preenche a entrada da IDT para a interrupção do teclado
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	// Inicialização dos controladores PIC
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);

	// Mascara todas as interrupções
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	// Preenche o descritor da IDT
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}


// Habilita apenas a interrupção do teclado (IRQ1)
void kb_init(void)
{
	write_port(0x21 , 0xFD);
}


// Impressão de strings e manipulação de tela
void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
	}
	current_loc = 0;
}


// Handler principal da interrupção do teclado
void keyboard_handler_main(void)
{
	unsigned char status;
	char keycode;
	char ch;

	write_port(0x20, 0x20); // Envia EOI para o PIC

	status = read_port(KEYBOARD_STATUS_PORT);
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0 || keycode >= 128)
			return;

		ch = keyboard_map[(unsigned char) keycode];
		// ===================
		//  LÓGICA DE ENTRADA DO JOGO
		// ===================
		if (game.state == STATE_MENU) {
			if (ch != 0) {
				if (keycode == ENTER_KEY_CODE) {
					if (game.buffer_index >= 5 &&
					    game.input_buffer[0] == 's' &&
					    game.input_buffer[1] == 't' &&
					    game.input_buffer[2] == 'a' &&
					    game.input_buffer[3] == 'r' &&
					    game.input_buffer[4] == 't') {
						game.state = STATE_PLAYING;
						clear_screen();
						init_game();
						draw_border();
						draw_game();
					}
					game.buffer_index = 0;
					for (int i = 0; i < 10; i++) game.input_buffer[i] = 0;
					kprint_newline();
				} else if (game.buffer_index < 9) {
					game.input_buffer[game.buffer_index++] = ch;
					vidptr[current_loc++] = ch;
					vidptr[current_loc++] = 0x07;
				}
			}
		} else if (game.state == STATE_PLAYING) {
			// Controle da cobrinha via teclado
			if (ch == 'w' && game.snake.direction != DIRECTION_DOWN) {
				game.snake.direction = DIRECTION_UP;
				update_snake();
				draw_game();
				if (game.state == STATE_GAME_OVER) {
					clear_screen();
					kprint("GAME OVER!");
					kprint_newline();
					kprint("Press ENTER to continue");
				}
			} else if (ch == 's' && game.snake.direction != DIRECTION_UP) {
				game.snake.direction = DIRECTION_DOWN;
				update_snake();
				draw_game();
				if (game.state == STATE_GAME_OVER) {
					clear_screen();
					kprint("GAME OVER!");
					kprint_newline();
					kprint("Press ENTER to continue");
				}
			} else if (ch == 'a' && game.snake.direction != DIRECTION_RIGHT) {
				game.snake.direction = DIRECTION_LEFT;
				update_snake();
				draw_game();
				if (game.state == STATE_GAME_OVER) {
					clear_screen();
					kprint("GAME OVER!");
					kprint_newline();
					kprint("Press ENTER to continue");
				}
			} else if (ch == 'd' && game.snake.direction != DIRECTION_LEFT) {
				game.snake.direction = DIRECTION_RIGHT;
				update_snake();
				draw_game();
				if (game.state == STATE_GAME_OVER) {
					clear_screen();
					kprint("GAME OVER!");
					kprint_newline();
					kprint("Press ENTER to continue");
				}
			}
		} else if (game.state == STATE_GAME_OVER) {
			if (keycode == ENTER_KEY_CODE) {
				game.state = STATE_MENU;
				game.buffer_index = 0;
				for (int i = 0; i < 10; i++) game.input_buffer[i] = 0;
				clear_screen();
				kprint("Type 'start' to play Snake Game!");
				kprint_newline();
			}
		}
	}
}


// ===================
//  FUNÇÃO PRINCIPAL DO KERNEL
// ===================
void kmain(void)
{
	clear_screen();
	kprint("Type 'start' to play Snake Game!");
	kprint_newline();

	game.state = STATE_MENU;
	game.buffer_index = 0;
	int i;
	for (i = 0; i < 10; i++) game.input_buffer[i] = 0;

	idt_init(); // Inicializa IDT e interrupção do teclado
	kb_init();  // Habilita IRQ do teclado

	while(1); // Loop infinito do kernel
}
