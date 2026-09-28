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
void register_m_11530_tests(void);
void register_m_11df8_tests(void);
void register_m_13324_tests(void);
void register_m_13712_tests(void);
void register_m_12f30_tests(void);
void register_m_12f9c_tests(void);
void register_m_12a9c_tests(void);
void register_vhw_irq_tests(void);
void register_vga_tests(void);
void register_lockstep_tests(void);
void register_g2_tests(void);
void register_joystick_tests(void);

static struct { const char *name; oracle_test_fn fn; } tests[128];
static int test_count;

void oracle_register(const char *name, oracle_test_fn fn)
{
    tests[test_count].name = name;
    tests[test_count].fn = fn;
    test_count++;
}

/* Match the game launcher: DOS memory must be reserved in a suspended child before the
 * Windows loader creates its heaps, TLS and initial thread stack in that address range. */
static int relaunch_with_low_memory_reserved(int *exit_code)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR path[MAX_PATH];
    void *reserved;
    DWORD code = 1;
    char child[8];
    if (GetEnvironmentVariableA("KE_ORACLE_CHILD", child, sizeof child) != 0)
        return 0;
    SetEnvironmentVariableA("KE_ORACLE_CHILD", "1");
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) {
        SetEnvironmentVariableA("KE_ORACLE_CHILD", NULL);
        fprintf(stderr, "ke_oracle: cannot resolve executable path (%lu)\n",
                (unsigned long)GetLastError());
        return -1;
    }
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(path, GetCommandLineW(), NULL, NULL, TRUE, CREATE_SUSPENDED,
                        NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        SetEnvironmentVariableA("KE_ORACLE_CHILD", NULL);
        fprintf(stderr, "ke_oracle: cannot start reserved-memory child (%lu)\n",
                (unsigned long)error);
        return -1;
    }
    reserved = VirtualAllocEx(pi.hProcess, (void *)LOWMEM_BASE,
                              LOWMEM_END - LOWMEM_BASE, MEM_RESERVE, PAGE_READWRITE);
    if (reserved != (void *)LOWMEM_BASE) {
        DWORD error = GetLastError();
        fprintf(stderr, "ke_oracle: cannot reserve low memory in child (%lu)\n",
                (unsigned long)error);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        SetEnvironmentVariableA("KE_ORACLE_CHILD", NULL);
        return -1;
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    SetEnvironmentVariableA("KE_ORACLE_CHILD", NULL);
    if (code != 0)
        fprintf(stderr, "ke_oracle: reserved-memory child exited with code %08lXh\n",
                (unsigned long)code);
    *exit_code = (int)code;
    return 1;
}

/* The oracle links the whole port; the historical main() is never run here. */
int main(int argc, char **argv)
{
    char image[512], symbols[512];
    const char *dir = argc > 1 ? argv[1] : "build/port/oracle";
    const char *filter = argc > 2 ? argv[2] : NULL;
    int i, failed = 0, run = 0;
    int child_exit, relaunch = relaunch_with_low_memory_reserved(&child_exit);
    if (relaunch != 0)
        return relaunch > 0 ? child_exit : 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    snprintf(image, sizeof image, "%s/ke_image.bin", dir);
    snprintf(symbols, sizeof symbols, "%s/ke_symbols.txt", dir);
    if (oracle_load(image, symbols) != 0)
        return 2;
    /* oracle_load reserves its relocated VGA alias before image/config allocations; keep
     * config and logging startup after it so they cannot claim the alias. */
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
    register_lockstep_tests();
    register_g2_tests();
    /* A9 trace fixtures replace PIC port callbacks, so run the live PIC test first. */
    register_m_11258_tests();
    register_m_11494_tests();
    register_m_11530_tests();
    register_joystick_tests();
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
