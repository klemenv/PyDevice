#include <util.h>
#include <pywrapper.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <map>
#include <string>

#define testOkExcept1(a) { \
    try { \
        testOk1(a) \
    } catch (...) { \
        testFail("Exception by " #a) \
    } \
}

#define testExcept(a) try { a; testFail("Test failed"); } catch (...) { testPass(#a " raised exception"); }

struct TestPyWrapper {
    static void init()
    {
        PyWrapper::init();
    }

    static void returnFromEval()
    {
        testOk1(PyWrapper::exec("17").get_long() == 17);
        testOk1(PyWrapper::exec("17").get_unsigned() == 17UL);
        testOk1(PyWrapper::exec("17").get_double() == 17.0);
        testOk1(PyWrapper::exec("17").get_string() == "17");
        testExcept(PyWrapper::exec("17").get_long_array());
        testExcept(PyWrapper::exec("17").get_double_array());
        testExcept(PyWrapper::exec("17").get_unsigned_array());
        testExcept(PyWrapper::exec("17").get_string_array());

        testOk1(PyWrapper::exec("13.1").get_long() == 13);
        testOk1(PyWrapper::exec("13.1").get_unsigned() == 13UL);
        testOk1(PyWrapper::exec("13.1").get_double() == 13.1);
        testOk1(PyWrapper::exec("13.1").get_string().substr(0,4) == "13.1");
        testExcept(PyWrapper::exec("13.1").get_long_array());
        testExcept(PyWrapper::exec("13.1").get_double_array());
        testExcept(PyWrapper::exec("13.1").get_unsigned_array());
        testExcept(PyWrapper::exec("13.1").get_string_array());

        testOk1(PyWrapper::exec("True").get_long() == 1);
        testOk1(PyWrapper::exec("True").get_unsigned() == 1UL);
        testOk1(PyWrapper::exec("True").get_double() == 1.0);
        testOk1(PyWrapper::exec("True").get_string() == "True");
        testExcept(PyWrapper::exec("True").get_long_array());
        testExcept(PyWrapper::exec("True").get_double_array());
        testExcept(PyWrapper::exec("True").get_unsigned_array());
        testExcept(PyWrapper::exec("True").get_string_array());

        testOk1(PyWrapper::exec("'5.2'").get_long() == 5);
        testOk1(PyWrapper::exec("'5.2'").get_unsigned() == 5UL);
        testOk1(PyWrapper::exec("'5.2'").get_double() == 5.2);
        testOk1(PyWrapper::exec("'5.2'").get_string().substr(0,3) == "5.2");
        testExcept(PyWrapper::exec("'5.2'").get_long_array());
        testExcept(PyWrapper::exec("'5.2'").get_unsigned_array());
        testExcept(PyWrapper::exec("'5.2'").get_double_array());
        testExcept(PyWrapper::exec("'5.2'").get_string_array());

        testExcept(PyWrapper::exec("a=12").get_long());
        testExcept(PyWrapper::exec("a=12").get_unsigned());
        testExcept(PyWrapper::exec("a=12").get_double());
        testExcept(PyWrapper::exec("a=12").get_string());
        testExcept(PyWrapper::exec("a=12").get_long_array());
        testExcept(PyWrapper::exec("a=12").get_unsigned_array());
        testExcept(PyWrapper::exec("a=12").get_double_array());
        testExcept(PyWrapper::exec("a=12").get_string_array());

        std::vector<long long int> cmplli = {1, 2, 3};
        std::vector<unsigned long long int> cmpulli = {1UL, 2UL, 3UL};
        std::vector<double> cmpd = {1.0, 2.0, 3.0};
        std::vector<std::string> cmps = {"1", "2", "3"};
        testExcept(PyWrapper::exec("[1,2,3]").get_long());
        testExcept(PyWrapper::exec("[1,2,3]").get_unsigned());
        testExcept(PyWrapper::exec("[1,2,3]").get_double());
        testExcept(PyWrapper::exec("[1,2,3]").get_string());
        testOk1(PyWrapper::exec("[1,2,3]").get_long_array() == cmplli);
        testOk1(PyWrapper::exec("[1,2,3]").get_unsigned_array() == cmpulli);
        testOk1(PyWrapper::exec("[1,2,3]").get_double_array() == cmpd);
        testOk1(PyWrapper::exec("[1,2,3]").get_string_array() == cmps);
    }
};

MAIN(testpywrapper)
{
    testPlan(48);

    TestPyWrapper::init();
    TestPyWrapper::returnFromEval();

    return testDone();
}
