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
        testOk1(Util::replaceFields("test string %VAL%", fields) == "test string hello");
        testOk1(Util::replaceFields("%VAL%", fields) == "hello");
        testOk1(Util::replaceFields("'%VAL%'", fields) == "'hello'");
    }

    static void multipleInstances()
    {
        const static std::map<std::string, std::string> fields = {
            { "%VAL%", "hello" }
        };
        testOk1(Util::replaceFields("%VAL% %VAL%", fields) == "hello hello");
        testOk1(Util::replaceFields("%VAL%,%VAL%", fields) == "hello,hello");
        testOk1(Util::replaceFields("%VAL%/%VAL%/%VAL%", fields) == "hello/hello/hello");
    }

    static void multipleFields()
    {
        const static std::map<std::string, std::string> fields = {
            { "%VAL%", "hello" },
            { "%NAME%", "Test:PV" },
            { "%HIGH%", "5" }
        };
        testOk1(Util::replaceFields("test string %VAL%", fields) == "test string hello");
        testOk1(Util::replaceFields("%NAME%=%VAL%", fields) == "Test:PV=hello");
        testOk1(Util::replaceFields("%NAME%: VAL=%VAL% HIGH=%HIGH%", fields) == "Test:PV: VAL=hello HIGH=5");
    }

    static void singleChar()
    {
        const static std::map<std::string, std::string> fields = {
            { "A", "B" },
            { "B", "C" },
        };
        testOk1(Util::replaceFields("A", fields) == "B");
        testOk1(Util::replaceFields("AAA", fields) == "AAA");
        testOk1(Util::replaceFields("ABA", fields) == "ABA");
    }

    static void injectDelimiter()
    {
        const static std::map<std::string, std::string> fields = {
            { "VAL", "hello" }
        };
        testOk1(Util::replaceFields("%VAL% VAL", fields) == "hello hello");
        testOk1(Util::replaceFields("%VAL%,VAL", fields) == "hello,hello");
        testOk1(Util::replaceFields("This is a test: VAL", fields) == "This is a test: hello");
        testOk1(Util::replaceFields("This is a test: %VAL%", fields) == "This is a test: hello");
        testOk1(Util::replaceFields("This is a test: %VAL", fields) == "This is a test: %hello");
    }

    static void inTheMiddleOfText()
    {
        const static std::map<std::string, std::string> fields = {
            { "VAL", "Hello" },
            { "A",   "ValA" }
        };
        testOk1(Util::replaceFields("ThisIs VAL test", fields) == "ThisIs Hello test");
        testOk1(Util::replaceFields("ThisIsVALtest", fields) == "ThisIsVALtest");
        testOk1(Util::replaceFields("ThisIs%VAL%test", fields) == "ThisIsHellotest");
        testOk1(Util::replaceFields("VAL%A%", fields) == "HelloValA");
        testOk1(Util::replaceFields("%VAL%A%", fields) == "HelloValA%");
        testOk1(Util::replaceFields("%VAL%%A%", fields) == "HelloValA");
    }

    static void randomOnes()
    {
        const static std::map<std::string, std::string> fields = {
            { "A", "17" },
            { "B", "3" }
        };
        testOk1(Util::replaceFields("A*B", fields) == "17*3");

        const static std::map<std::string, std::string> fields2 = {
            { "A", "[1,2,3]" },
            { "B", "['one','two','three']" }
        };
        testOk1(Util::replaceFields("zip(A,B)", fields2) == "zip([1,2,3],['one','two','three'])");
    }
};

struct TestGetReplacables {
    static void getReplacables()
    {
        testOk1(Util::getFields("NAME").count("NAME") == 1);
        testOk1(Util::getFields("    NAME").count("NAME") == 1);
        testOk1(Util::getFields("NAME\t").count("NAME") == 1);
        testOk1(Util::getFields("a,NAME").count("NAME") == 1);
        testOk1(Util::getFields("NAME a").count("NAME") == 1);
        testOk1(Util::getFields("Name").size() == 0);
        testOk1(Util::getFields("name").size() == 0);
        testOk1(Util::getFields("NAMEA").size() == 0);
        testOk1(Util::getFields("aVAL").size() == 0);
        testOk1(Util::getFields("VALa").size() == 0);
        testOk1(Util::getFields("...NAMEa").size() == 0);
        testOk1(Util::getFields("...aNAME!").size() == 0);
        testOk1(Util::getFields("function(NAME)").count("NAME") == 1);
        testOk1(Util::getFields("function(NAME,A)").size() == 2);
        testOk1(Util::getFields("function(NAME,A)").count("NAME") == 1);
        testOk1(Util::getFields("function(NAME,A)").count("A") == 1);

        testOk1(Util::getFields("%NAME%").count("NAME") == 1);
        testOk1(Util::getFields("a%NAME%").count("NAME") == 1);
        testOk1(Util::getFields("%NAME%a").count("NAME") == 1);
        testOk1(Util::getFields("%Name%").size() == 0);
        testOk1(Util::getFields("%name%").size() == 0);
        testOk1(Util::getFields("%NAMEA%").size() == 0);
        testOk1(Util::getFields("...%NAME%a").count("NAME") == 1);

        testOk1(Util::getFields("%VAL%%A%").size() == 2);
        testOk1(Util::getFields("%VAL%A%").size() == 2); // %VAL%, A and a leftover %
        testOk1(Util::getFields("VAL%A%").size() == 2);
    }
};

MAIN(testutil)
{
    testPlan(55);
    TestReplace::basic();
    TestReplace::multipleInstances();
    TestReplace::multipleFields();
    TestReplace::singleChar();
    TestReplace::injectDelimiter();
    TestReplace::inTheMiddleOfText();
    TestReplace::randomOnes();

    TestEscape::newLine();
    TestEscape::singleQuote();

    TestGetReplacables::getReplacables();

    return testDone();
}
