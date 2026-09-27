/* oracle_test.h - minimal registry for differential tests (one register_* per module). */
#ifndef KE_ORACLE_TEST_H
#define KE_ORACLE_TEST_H
typedef int (*oracle_test_fn)(void);       /* returns the number of failures */
void oracle_register(const char *name, oracle_test_fn fn);
#endif
