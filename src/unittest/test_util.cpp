#include <util.h>
#include <pywrapper.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <map>
#include <string>

struct TestEscape {
    static void newLine()
    {
        testOk1(Util::escape("new\nline") == "new\\nline");
        testOk1(Util::escape("one\ntwo\nthree") == "one\\ntwo\\nthree");
    }

    static void singleQuote()
    {
        testOk1(Util::escape("single ' quote") == "single \\' quote");
        testOk1(Util::escape("embedded 'string'") == "embedded \\'string\\'");
        testOk1(Util::escape("h'ello") == "h\\'ello");
    }
};

struct TestReplace {
    static void basic()
    {
        const static std::map<std::string, std::string> fields = {
            { "%VAL%", "hello" }
        };
        testOk1(Util::replace("test string %VAL%", fields) == "test string hello");
        testOk1(Util::replace("%VAL%", fields) == "hello");
        testOk1(Util::replace("'%VAL%'", fields) == "'hello'");
    }

    static void multipleInstances()
    {
        const static std::map<std::string, std::string> fields = {
            { "%VAL%", "hello" }
        };
        testOk1(Util::replace("%VAL% %VAL%", fields) == "hello hello");
        testOk1(Util::replace("%VAL%,%VAL%", fields) == "hello,hello");
        testOk1(Util::replace("%VAL%/%VAL%/%VAL%", fields) == "hello/hello/hello");
    }

    static void multipleFields()
    {
        const static std::map<std::string, std::string> fields = {
            { "%VAL%", "hello" },
            { "%NAME%", "Test:PV" },
            { "%HIGH%", "5" }
        };
        testOk1(Util::replace("test string %VAL%", fields) == "test string hello");
        testOk1(Util::replace("%NAME%=%VAL%", fields) == "Test:PV=hello");
        testOk1(Util::replace("%NAME%: VAL=%VAL% HIGH=%HIGH%", fields) == "Test:PV: VAL=hello HIGH=5");
    }

    static void singleChar()
    {
        const static std::map<std::string, std::string> fields = {
            { "\n", "\\n" },
            { "a", "b" },
            { "b", "c" },
        };
        testOk1(Util::replace("\n", fields) == "\\n");
        testOk1(Util::replace("a", fields) == "b");
        testOk1(Util::replace("aaa", fields) == "bbb");
        testOk1(Util::replace("aba", fields) == "bcb");
    }
};

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

MAIN(testutil)
{
    testPlan(60);
    TestReplace::basic();
    TestReplace::multipleInstances();
    TestReplace::multipleFields();
    TestReplace::singleChar();
    TestEscape::newLine();
    TestEscape::singleQuote();

    TestPyWrapper::init();
    TestPyWrapper::returnFromEval();
    /*
    testSetup();
    logger_config_env();
    Tester().loopback(false);
    Tester().loopback(true);
    Tester().lazy();
    Tester().timeout();
    Tester().cancel();
    Tester().orphan();
    TestPutBuilder().testSet();
    testRO();
    testError();
    cleanup_for_valgrind();
    */
    return testDone();
}
