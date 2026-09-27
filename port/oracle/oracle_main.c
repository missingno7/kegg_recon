/* oracle_main.c - ke_oracle.exe: load KE.EXE natively and run every differential test.
 *
 *   python port/tools/le_export.py            (once: writes build/port/oracle/ke_image.bin)
 *   build/port/oracle/ke_oracle.exe [IMAGE_DIR] [TEST_SUBSTRING]
 * Exit code 0 when every test passes.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <timeapi.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

/* every test module adds its register function here */
void register_m_137a8_tests(void);
void register_m_0982c_tests(void);
void register_vhw_irq_tests(void);

static struct { const char *name; oracle_test_fn fn; } tests[128];
static int test_count;

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
    setvbuf(stdout, NULL, _IONBF, 0);
    ke_config_load(1, argv);
    ke_config.log_level = KE_LOG_WARN;
    ke_log_init(NULL);
    snprintf(image, sizeof image, "%s/ke_image.bin", dir);
    snprintf(symbols, sizeof symbols, "%s/ke_symbols.txt", dir);
    if (oracle_load(image, symbols) != 0)
        return 2;
    timeBeginPeriod(1);
    vpic_init();
    vga_init();
    register_m_0982c_tests();
    register_m_137a8_tests();
    register_vhw_irq_tests();
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
