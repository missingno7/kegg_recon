/* ke_clang_u_00708.h - force-included into src/u_00708.c by the clang (ILP32 world) build.
 * The unit calls six of its own functions before defining them `void f(void)`; gcc -std=gnu89
 * accepts the implicit `int f()` declaration followed by the void definition with a
 * warning, clang rejects it. Declaring the definitions' own prototypes first changes no
 * call: the implicit int result was never used. */
void show_high_score_screen(void);
void update_menu_screen(void);
void show_game_over_screen(void);
void write_high_score_table(void);
void show_instructions(void);
void show_ending_pages(void);
