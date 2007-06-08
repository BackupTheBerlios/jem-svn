//==============================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 JavaDevices Software. 
//
// Jem is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License, version 2, as published by the Free
// Software Foundation.
//
// Jem is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// Jem; if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
// Fifth Floor, Boston, MA 02110-1301, USA
//
//==============================================================================
//
// Jem/JVM malloc test aspect.
//   This aspect adds unit test cases to test the malloc.c functions.
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include "libcli.h"
#include "jemtypes.h"
#include "jemConfig.h"
#include "malloc.h"
#include "embUnit.h"



extern struct cli_def     *kcli;
extern struct cli_command *jemtst_command;
static char               *testRef[10];

#define TESTMALLOCSZ  2000
#define MEMTYPE_OTHER  0
#define MEMTYPE_HEAP   1
#define MEMTYPE_STACK 2
#define MEMTYPE_MEMOBJ 3
#define MEMTYPE_CODE   4
#define MEMTYPE_PROFILING   5
#define MEMTYPE_EMULATION   6
#define MEMTYPE_TMP   7
#define MEMTYPE_DCB   8


void *jemStatMalloc(u32 size, int memtype);
void jemStatFree(void * addr, u32 size, int memtype);
void jemStatFreeCode(void *addr, u32 size);


static void setUpMalloc(void)
{
    int i;

    for (i=0; i<10; i++)
      if (testRef[i] != NULL) 
      {
          jemStatFree(testRef[i], TESTMALLOCSZ, i);
          testRef[i] = NULL;
      }
}


static void setUpFree(void)
{
    int i;

    for (i=0; i<10; i++)
      if (testRef[i] == NULL) 
      {
          testRef[i] = jemStatMalloc(TESTMALLOCSZ, i);
      }
}


static void tearDown(void)
{
}


static void setUp(void)
{
}


static void test_jemMalloc(void) 
{
    testRef[MEMTYPE_OTHER] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_OTHER);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_OTHER]);
    testRef[MEMTYPE_HEAP] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_HEAP);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_HEAP]);
    testRef[MEMTYPE_CODE] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_CODE);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_CODE]);
    testRef[MEMTYPE_STACK] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_STACK);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_STACK]);
    testRef[MEMTYPE_MEMOBJ] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_MEMOBJ);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_MEMOBJ]);
    testRef[MEMTYPE_PROFILING] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_PROFILING);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_PROFILING]);
    testRef[MEMTYPE_EMULATION] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_EMULATION);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_EMULATION]);
    testRef[MEMTYPE_TMP] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_TMP);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_TMP]);
    testRef[MEMTYPE_DCB] = jemStatMalloc(TESTMALLOCSZ, MEMTYPE_DCB);
    TEST_ASSERT_NOT_NULL(testRef[MEMTYPE_DCB]);
    testRef[9] = jemStatMalloc(TESTMALLOCSZ, 9);
    TEST_ASSERT_NOT_NULL(testRef[9]);
}


static void test_jemFree(void)
{
    int i;

    for (i=0; i<10; i++)
      if (testRef[i] != NULL) 
      {
          jemStatFree(testRef[i], TESTMALLOCSZ, i);
          testRef[i] = NULL;
      }
}


ThreadDescProxy *ref_1 = NULL;
TempMemory      *ref_2 = NULL;
static void test_3(void)
{
    // test jemMallocThreadDescProxy, jemMallocTmp
    ref_1   = jemMallocThreadDescProxy(NULL);
    TEST_ASSERT_NOT_NULL(ref_1);
    ref_2   = jemMallocTmp(4000);
    TEST_ASSERT_NOT_NULL(ref_2);
}


static void test_4(void)
{
    // test jemFreeThreadDesc, jemFreeTmp
    if (ref_1 != NULL) 
    {
        ThreadDesc *t = &(ref_1->desc);
        jemFreeThreadDesc(t);
        ref_1 = NULL;
    }
    if (ref_2 != NULL) {
        jemFreeTmp(ref_2);
        ref_2 = NULL;
    }
}


static void setUp_test_3(void)
{
    test_4();
}


char        *ref_3 = NULL;
DomainDesc  *domain = NULL;
static void test_5(void)
{
    // test jemMallocCode

    int i;

    for (i=0; i<10; i++) {
        ref_3 = jemMallocCode(domain, 87040);
        TEST_ASSERT_NOT_NULL(ref_3);
    }
}


static void test_6(void)
{
    // test jemFreeCode
    int i;

    for (i=0; i<getJVMConfig(codeFragments); i++) {
        if (domain->code[i] != NULL) {
            jemStatFreeCode(domain->code[i], domain->code_bytes);
            domain->code[i] = NULL;
        }
    }
    domain->cur_code = -1;
}


static void test_7(void)
{
    // test jemMallocCode until it is full

    int i;
    int size = domain->code_bytes - 1;

    for (i=0; i<getJVMConfig(codeFragments); i++) {
        ref_3 = jemMallocCode(domain, size);
        TEST_ASSERT_NOT_NULL(ref_3);
    }
    ref_3 = jemMallocCode(domain, size);
    TEST_ASSERT_NULL(ref_3);
}


static void setUp_test_5(void)
{
    if (domain == NULL) {
        domain              = kzalloc(sizeof(DomainDesc), GFP_KERNEL);
        domain->codeBorder	= kzalloc((sizeof(char *) * getJVMConfig(codeFragments)), GFP_KERNEL); 
        domain->code		= kzalloc((sizeof(char *) * getJVMConfig(codeFragments)), GFP_KERNEL); 
        domain->codeTop		= kzalloc((sizeof(char *) * getJVMConfig(codeFragments)), GFP_KERNEL); 
        domain->cur_code    = -1;
        domain->code_bytes  = getJVMConfig(codeBytes);
    }
    test_6();
}


static void setUpAll(void)
{
    setUpMalloc();
    setUp_test_3();
    setUp_test_5();
}


EMB_UNIT_TESTFIXTURES(fixturesall) {
    new_TestFixture("test_jemMalloc",test_jemMalloc),
    new_TestFixture("test_jemFree",test_jemFree),
    new_TestFixture("test_3", test_3),
    new_TestFixture("test_4", test_4),
    new_TestFixture("test_5", test_5),
    new_TestFixture("test_6", test_6),
    new_TestFixture("test_7", test_7),
};
EMB_UNIT_TESTCALLER(MallocAllTest,"All malloc tests",setUpAll,test_6,fixturesall);
EMB_UNIT_TESTFIXTURES(fixtures1) {
new_TestFixture("test_jemMalloc",test_jemMalloc),
};
EMB_UNIT_TESTCALLER(MallocTest1,"Malloc jemMalloc test",setUpMalloc,tearDown,fixtures1);
EMB_UNIT_TESTFIXTURES(fixtures2) {
new_TestFixture("test_jemFree",test_jemFree),
};
EMB_UNIT_TESTCALLER(MallocTest2,"Malloc jemFree test",setUpFree,tearDown,fixtures2);
EMB_UNIT_TESTFIXTURES(fixtures3) {
new_TestFixture("test_3",test_3),
};
EMB_UNIT_TESTCALLER(MallocTest3,"Malloc test jemMallocThreadDescProxy, jemMallocTmp",setUp_test_3,tearDown,fixtures3);
EMB_UNIT_TESTFIXTURES(fixtures4) {
new_TestFixture("test_4",test_4),
};
EMB_UNIT_TESTCALLER(MallocTest4,"Malloc test jemFreeThreadDesc, jemFreeTmp",setUp,tearDown,fixtures4);
EMB_UNIT_TESTFIXTURES(fixtures5) {
new_TestFixture("test_5",test_5),
};
EMB_UNIT_TESTCALLER(MallocTest5,"Malloc test jemMallocCode",setUp_test_5,tearDown,fixtures5);
EMB_UNIT_TESTFIXTURES(fixtures6) {
new_TestFixture("test_6",test_6),
};
EMB_UNIT_TESTCALLER(MallocTest6,"Malloc test jemFreeCode",setUp,tearDown,fixtures6);
EMB_UNIT_TESTFIXTURES(fixtures7) {
new_TestFixture("test_7",test_7),
};
EMB_UNIT_TESTCALLER(MallocTest7,"Malloc test jemMallocCode boundary",test_6,tearDown,fixtures7);


static int jemMalloc_test_cmd(struct cli_def *cli, char *command, char *argv[], int argc)
{
    unsigned int state=0;

    if (argc > 0)
    {
        state = 1;
        if (!strncmp(argv[0], "list", 4)) state = 2;
        if (!strncmp(argv[0], "all", 3)) state = 0;
        if (!strncmp(argv[0], "t1", 2)) state = 3;
        if (!strncmp(argv[0], "t2", 2)) state = 4;
        if (!strncmp(argv[0], "t3", 2)) state = 5;
        if (!strncmp(argv[0], "t4", 2)) state = 6;
        if (!strncmp(argv[0], "t5", 2)) state = 7;
        if (!strncmp(argv[0], "t6", 2)) state = 8;
        if (!strncmp(argv[0], "t7", 2)) state = 9;
    }

    switch (state) 
    {
        case 0:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocAllTest);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 1:
          cli_print(cli, "Invalid argument, try 'malloc list'\r\n");
          break;
        case 2:
          cli_print(cli, "malloc takes one of the following arguments:\r\n");
          cli_print(cli, "  list - display this text\r\n");
          cli_print(cli, "  all  - run all tests\r\n");
          cli_print(cli, "  t1   - run %s\r\n", MallocTest1.name);
          cli_print(cli, "  t2   - run %s\r\n", MallocTest2.name);
          cli_print(cli, "  t3   - run %s\r\n", MallocTest3.name);
          cli_print(cli, "  t4   - run %s\r\n", MallocTest4.name);
          cli_print(cli, "  t5   - run %s\r\n", MallocTest5.name);
          cli_print(cli, "  t6   - run %s\r\n", MallocTest6.name);
          cli_print(cli, "  t7   - run %s\r\n", MallocTest7.name);
          break;
        case 3:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest1);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 4:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest2);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 5:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest3);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 6:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest4);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 7:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest5);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 8:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest6);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
        case 9:
          TestRunner_start();
          TestRunner_runTest((TestRef)&MallocTest7);
          TestRunner_end();
          cli_print(cli, "\r\n");
          break;
    }

    return CLI_OK;
}


int around(): execution(int jemMallocInit()) 
{
    int i;

    for (i=0; i<10; i++) {
        testRef[i] = NULL;
    }

    cli_register_command(kcli, NULL, "malloc", jemMalloc_test_cmd, 
                         PRIVILEGE_UNPRIVILEGED, CLI_TEST_MODE, "Run malloc unit tests.");

    return proceed();
}

before(): execution(void jem_exit()) 
{
    cli_unregister_command(kcli, "malloc");
}

