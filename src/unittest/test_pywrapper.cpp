#include <util.h>
#include <pywrapper.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <map>
#include <string>

struct TestPyWrapper {
    static void init()
    {
        PyWrapper::init();
    }

    static void returnFromEval()
    {
        int32_t i;
        double d;
        std::string s;
        char c;
        std::vector<long> vi;
        std::vector<double> vf;

        testOk1(PyWrapper::exec("17", false, &i) == true && i == 17);
        testOk1(PyWrapper::exec("17", false, &c) == true && c == 17);
        testOk1(PyWrapper::exec("17", false, &d) == true && d == 17.0);
        testOk1(PyWrapper::exec("17", false, s)  == true && s == "17");
        testOk1(PyWrapper::exec("17", false, vi) == false);
        testOk1(PyWrapper::exec("17", false, vf) == false);

        testOk1(PyWrapper::exec("13.0", false, &i) == true && i == 13);
        testOk1(PyWrapper::exec("13.0", false, &c) == true && c == 13);
        testOk1(PyWrapper::exec("13.0", false, &d) == true && d == 13.0);
        testOk1(PyWrapper::exec("13.0", false,  s) == true && s.substr(0,4) == "13.0");
        testOk1(PyWrapper::exec("13.0", false, vi) == false);
        testOk1(PyWrapper::exec("13.0", false, vf) == false);

        testOk1(PyWrapper::exec("True", false, &i) == true && i == 1);
        testOk1(PyWrapper::exec("True", false, &c) == true && c == 1);
        testOk1(PyWrapper::exec("True", false, &d) == true && d == 1.0);
        testOk1(PyWrapper::exec("True", false,  s) == true && s == "1");
        testOk1(PyWrapper::exec("True", false, vi) == false);
        testOk1(PyWrapper::exec("True", false, vf) == false);

        testOk1(PyWrapper::exec("'5.2'", false, &i) == false);
        testOk1(PyWrapper::exec("'5.2'", false, &c) == false);
        testOk1(PyWrapper::exec("'5.2'", false, &d) == false);
        testOk1(PyWrapper::exec("'5.2'", false, s)  == true && s == "5.2");
        testOk1(PyWrapper::exec("'5.2'", false, vi) == false);
        testOk1(PyWrapper::exec("'5.2'", false, vf) == false);

        testOk1(PyWrapper::exec("a=12", false, &i) == false);
        testOk1(PyWrapper::exec("a=12", false, &c) == false);
        testOk1(PyWrapper::exec("a=12", false, &d) == false);
        testOk1(PyWrapper::exec("a=12", false, s)  == false);
        testOk1(PyWrapper::exec("a=12", false, vi) == false);
        testOk1(PyWrapper::exec("a=12", false, vf) == false);

        testOk1(PyWrapper::exec("[1,2,3]", false, &i) == false);
        testOk1(PyWrapper::exec("[1,2,3]", false, &c) == false);
        testOk1(PyWrapper::exec("[1,2,3]", false, &d) == false);
        testOk1(PyWrapper::exec("[1,2,3]", false, s)  == false);
        testOk1(PyWrapper::exec("[1,2,3]", false, vi) == true && vi.size() == 3 && vi[0] == 1   && vi[1] == 2   && vi[2] ==3);
        testOk1(PyWrapper::exec("[1,2,3]", false, vf) == true && vf.size() == 3 && vf[0] == 1.0 && vf[1] == 2.0 && vf[2] ==3.0);

        testOk1(PyWrapper::exec("[4.0,5.0]", false, &i) == false);
        testOk1(PyWrapper::exec("[4.0,5.0]", false, &c) == false);
        testOk1(PyWrapper::exec("[4.0,5.0]", false, &d) == false);
        testOk1(PyWrapper::exec("[4.0,5.0]", false, s)  == false);
        testOk1(PyWrapper::exec("[4.0,5.0]", false, vi) == true && vi.size() == 2 && vi[0] == 4   && vi[1] == 5);
        testOk1(PyWrapper::exec("[4.0,5.0]", false, vf) == true && vf.size() == 2 && vf[0] == 4.0 && vf[1] == 5.0);
    }
};

MAIN(testpywrapper)
{
    testPlan(42);

    TestPyWrapper::init();
    TestPyWrapper::returnFromEval();

    return testDone();
}
