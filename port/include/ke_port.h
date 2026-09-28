/* ke_port.h - host services shared by port/ modules (never included by historical src/). */
#ifndef KE_PORT_H
#define KE_PORT_H

#include <stdint.h>
#include <stdarg.h>

/* ---- logging (port/host/log.c) ------------------------------------------------------ */
enum { KE_LOG_ERROR, KE_LOG_WARN, KE_LOG_INFO, KE_LOG_DEBUG, KE_LOG_TRACE };
void ke_log_init(const char *path);
void ke_log(int level, const char *subsystem, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void ke_vlog(int level, const char *subsystem, const char *fmt, va_list ap);
int ke_log_enabled(int level);
/* Log `fmt` once per distinct call site key (e.g. an unknown port number). */
void ke_log_once(const char *key, int level, const char *subsystem, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* ---- configuration (environment, read once at startup) -------------------------------- */
typedef struct KeConfig {
    int irq_async;        /* KE_IRQ=async (default) | sync                                 */
    int windows_host;     /* KE_WINDOWS=1: answer INT 2Fh/1600h as Windows 3.1 enhanced    */
    int sound_blaster;    /* KE_SB=1: attach the virtual Sound Blaster at 220h/IRQ7/DMA1   */
    int joystick;         /* KE_JOY=1: attach the virtual gameport                          */
    int mouse_native;     /* KE_MOUSE_MODE=native: absolute SDL mouse; default faithful     */
    int scale;            /* KE_SCALE=n: initial window scale (default 3)                   */
    int aspect;           /* KE_ASPECT=1 (default): 4:3 display aspect; 0: square pixels    */
    int log_level;        /* KE_LOG_LEVEL=0..4                                               */
    const char *data_dir; /* KE_DATA or argv[1]: directory holding KE.EXE's data files      */
} KeConfig;
extern KeConfig ke_config;
void ke_config_load(int argc, char **argv);
int ke_config_fullscreen(void);
int ke_config_integer_scale(void);
int ke_config_volume(void);
int ke_config_validate_assets(char *missing, size_t missing_size);

/* ---- game thread lifecycle (port/host/gamethread.c) ------------------------------------ */
/* The historical main() is compiled as ke_game_main (u_13a95.c gets -Dmain=ke_game_main). */
void ke_game_main(void);
int ke_game_thread_start(void);          /* returns 0 on success                           */
int ke_game_thread_finished(void);       /* nonzero once the game thread has left main     */
int ke_game_exit_code(void);
void ke_request_quit(void);              /* window closed: unwind the game at its next     */
int ke_quit_requested(void);             /*   service boundary                             */
void ke_check_quit(void);                /* called by vhw on the game thread               */
int ke_on_game_thread(void);
void ke_game_thread_adopt(void);        /* tests: calling thread becomes the game thread  */
int ke_game_run_here(void (*entry)(void)); /* lockstep: main() on this thread, exit code  */
void *ke_game_thread_handle(void);       /* HANDLE                                          */
/* Watcom exit()/atexit() replacements (watcom_compat.h maps the names). */
void ke_exit(int code) __attribute__((noreturn));
int ke_atexit(void (*fn)(void));
void ke_stop_game_from_fault(const char *why) __attribute__((noreturn));
void ke_log_game_backtrace(const char *why); /* EIP + return addresses of the game thread */

/* ---- stubs (port/host/stub.c) ---------------------------------------------------------- */
void ke_stub_hit(const char *name, const char *owner);
void ke_stub_report(void);               /* list every stub that was reached              */

/* ---- presentation (port/host/present.c) ------------------------------------------------ */
struct SDL_Window;
struct SDL_Renderer;
int ke_present_init(struct SDL_Window *window, struct SDL_Renderer *renderer);
void ke_present_frame(void);
int ke_present_map_mouse(float window_x, float window_y, int *game_x, int *game_y);
void ke_present_shutdown(void);
void ke_native_mouse_position(float window_x, float window_y);
void ke_native_mouse_set_game_position(int game_x, int game_y);

/* ---- host time ------------------------------------------------------------------------- */
uint64_t ke_now_ns(void);                /* monotonic                                       */
void ke_sleep_ns(uint64_t ns);           /* high-resolution sleep                           */

#endif
