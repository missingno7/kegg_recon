/* oracle_main.c - ke_oracle.exe: load KE.EXE natively and run every differential test.
 *
 *   python port/tools/le_export.py            (once: writes build/port/oracle/ke_image.bin)
 *   build/port/oracle/ke_oracle.exe [IMAGE_DIR] [TEST_SUBSTRING]
 * Exit code 0 when every test passes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <timeapi.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

/* every test module adds its register function here */
void register_m_137a8_tests(void);
void register_m_09f64_tests(void);
void register_m_0982c_tests(void);
void register_m_0a284_tests(void);
void register_m_13944_tests(void);
void register_m_13a48_tests(void);
void register_m_11258_tests(void);
void register_m_11494_tests(void);
void register_m_11df8_tests(void);
void register_m_13324_tests(void);
void register_m_13712_tests(void);
void register_m_12f30_tests(void);
void register_m_12f9c_tests(void);
void register_m_12a9c_tests(void);
void register_vhw_irq_tests(void);
void register_vga_tests(void);

static struct { const char *name; oracle_test_fn fn; } tests[128];
static int test_count;

/* Reserve the oracle's VGA aperture in the suspended child before its CRT startup. */
static int relaunch_with_vga_reserved(int *exit_code)
{
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    WCHAR path[MAX_PATH];
    void *wanted = (void *)(uintptr_t)0xA0000u;
    void *reserved = NULL;
    DWORD result = 2, last_error = 0;
    int attempt, started = 0;

    if (getenv("KE_ORACLE_CHILD")) {
        if (oracle_vga_adopt_reserved_window() != 0) {
            fprintf(stderr, "oracle: child did not inherit the reserved VGA window\n");
            *exit_code = 2;
            return 1;
        }
        return 0;
    }

    SetEnvironmentVariableA("KE_ORACLE_CHILD", "1");
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) {
        fprintf(stderr, "oracle: cannot get executable path (%lu)\n", GetLastError());
        *exit_code = 2;
        return 1;
    }
    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    for (attempt = 0; attempt < 8; attempt++) {
        if (!CreateProcessW(path, GetCommandLineW(), NULL, NULL, FALSE, CREATE_SUSPENDED,
                            NULL, NULL, &startup, &process)) {
            last_error = GetLastError();
            continue;
        }
        reserved = VirtualAllocEx(process.hProcess, wanted, 0x20000u, MEM_RESERVE, PAGE_NOACCESS);
        if (reserved == wanted) {
            started = 1;
            break;
        }
        last_error = GetLastError();
        TerminateProcess(process.hProcess, 2);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    if (!started) {
        fprintf(stderr, "oracle: cannot start child with reserved VGA window after 8 attempts "
                "(%lu)\n", last_error);
        *exit_code = 2;
        return 1;
    }
    ResumeThread(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &result);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    *exit_code = (int)result;
    return 1;
}

void oracle_register(const char *name, oracle_test_fn fn)
{
    tests[test_count].name = name;
    tests[test_count].fn = fn;
    test_count++;
}

/* The oracle links the whole port; the historical main() is never run here. */
int main(int argc, char **argv)
{
    char image[512], symbols[512];
    const char *dir = argc > 1 ? argv[1] : "build/port/oracle";
    const char *filter = argc > 2 ? argv[2] : NULL;
    int i, failed = 0, run = 0;
    int child_exit;
    if (relaunch_with_vga_reserved(&child_exit))
        return child_exit;
    setvbuf(stdout, NULL, _IONBF, 0);
    snprintf(image, sizeof image, "%s/ke_image.bin", dir);
    snprintf(symbols, sizeof symbols, "%s/ke_symbols.txt", dir);
    if (oracle_load(image, symbols) != 0)
        return 2;
    /* oracle_load reserves A0000h before its first allocations; keep config and logging
     * startup after that guard so their CRT bookkeeping cannot claim the VGA aperture. */
    ke_config_load(1, argv);
    ke_config.log_level = KE_LOG_WARN;
    ke_log_init(NULL);
    timeBeginPeriod(1);
    vpic_init();
    vga_init();
    register_m_0982c_tests();
    register_m_12a9c_tests();
    register_m_137a8_tests();
    register_m_09f64_tests();
    register_m_0a284_tests();
    register_m_13944_tests();
    register_m_13a48_tests();
    register_m_11df8_tests();
    register_m_13324_tests();
    register_m_13712_tests();
    register_m_12f30_tests();
    register_m_12f9c_tests();
    register_vhw_irq_tests();
    register_vga_tests();
    /* A9 trace fixtures replace PIC port callbacks, so run the live PIC test first. */
    register_m_11258_tests();
    register_m_11494_tests();
    for (i = 0; i < test_count; i++) {
        int f;
        if (filter && !strstr(tests[i].name, filter))
            continue;
        f = tests[i].fn();
        run++;
        printf("%s  %s%s\n", f ? "FAIL" : "PASS", tests[i].name, f ? "" : "");
        if (f)
            failed++;
    }
    printf("%d/%d tests passed\n", run - failed, run);
    return failed ? 1 : 0;
}
