#include <util.h>

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

MAIN(testutil)
{
    testPlan(18);
    TestReplace::basic();
    TestReplace::multipleInstances();
    TestReplace::multipleFields();
    TestReplace::singleChar();
    TestEscape::newLine();
    TestEscape::singleQuote();
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
