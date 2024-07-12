#include <util.h>
#include <pywrapper.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <algorithm>
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

struct TestJoin {
    static void simple()
    {
        testOk1(Util::join({"Hello","World","!"}, " ") == "Hello World !");
        testOk1(Util::join({"a","b","c"}, "") == "abc");
        testOk1(Util::join({"a","b","c"}, "__") == "a__b__c");
    }
};

struct TestReplace {
    static void basic()
    {
        testOk1(Util::replaceMacro("test string %VAL%", "%VAL%", "hello") == "test string hello");
        testOk1(Util::replaceMacro("%VAL%", "%VAL%", "hello") == "hello");
        testOk1(Util::replaceMacro("'%VAL%'", "%VAL%", "hello") == "'hello'");
    }

    static void multipleInstances()
    {
        testOk1(Util::replaceMacro("%VAL% %VAL%", "%VAL%", "hello") == "hello hello");
        testOk1(Util::replaceMacro("%VAL%,%VAL%", "%VAL%", "hello") == "hello,hello");
        testOk1(Util::replaceMacro("%VAL%/%VAL%/%VAL%", "%VAL%", "hello") == "hello/hello/hello");
    }

    static void singleChar()
    {
        testOk1(Util::replaceMacro("A", "A", "B") == "B");
        testOk1(Util::replaceMacro("AAA", "A", "B") == "AAA");
        testOk1(Util::replaceMacro("ABA", "B", "C") == "ABA");
    }

    static void injectDelimiter()
    {
        testOk1(Util::replaceMacro("%VAL% VAL", "VAL", "hello") == "hello hello");
        testOk1(Util::replaceMacro("%VAL%,VAL", "VAL", "hello") == "hello,hello");
        testOk1(Util::replaceMacro("This is a test: VAL", "VAL", "hello") == "This is a test: hello");
        testOk1(Util::replaceMacro("This is a test: %VAL%", "VAL", "hello") == "This is a test: hello");
        testOk1(Util::replaceMacro("This is a test: %VAL", "VAL", "hello") == "This is a test: %hello");
    }

    static int _countInVector(const std::vector<std::string>& v, const std::string& token) {
        return std::count(v.begin(), v.end(), token);
    }

    static void inTheMiddleOfText()
    {
        testOk1(Util::replaceMacro("ThisIs VAL test", "VAL", "Hello") == "ThisIs Hello test");
        testOk1(Util::replaceMacro("ThisIsVALtest", "VAL", "Hello") == "ThisIsVALtest");
        testOk1(Util::replaceMacro("ThisIs%VAL%test", "VAL", "Hello") == "ThisIsHellotest");
        testOk1(Util::replaceMacro("Hello%A%", "A", "ValA") == "HelloValA");
        testOk1(Util::getMacros("NAME").size() == 1);
        testOk1(Util::getMacros("    NAME").size() == 1);
        testOk1(Util::getMacros("NAME\t").size() == 1);
        testOk1(Util::getMacros("a,NAME").size() == 1);
        testOk1(Util::getMacros("NAME a").size() == 1);
        testOk1(Util::getMacros("Name").size() == 0);
        testOk1(Util::getMacros("name").size() == 0);
        testOk1(Util::getMacros("NAMEA").size() == 0);
        testOk1(Util::getMacros("aVAL").size() == 0);
        testOk1(Util::getMacros("VALa").size() == 0);
        testOk1(Util::getMacros("...NAMEa").size() == 0);
        testOk1(Util::getMacros("...aNAME!").size() == 0);
        testOk1(Util::getMacros("function(NAME)").size() == 1);
        testOk1(Util::getMacros("function(NAME,A)").size() == 2);
        testOk1(_countInVector(Util::getMacros("function(NAME,A)"), "NAME") == 1);
        testOk1(_countInVector(Util::getMacros("function(NAME,A)"), "A") == 1);
        testOk1(_countInVector(Util::getMacros("function(NAME,A,A)"), "A") == 2);

        testOk1(Util::getMacros("%NAME%").size() == 1);
        testOk1(Util::getMacros("a%NAME%").size() == 1);
        testOk1(Util::getMacros("%NAME%a").size() == 1);
        testOk1(Util::getMacros("%Name%").size() == 0);
        testOk1(Util::getMacros("%name%").size() == 0);
        testOk1(Util::getMacros("%NAMEA%").size() == 0);
        testOk1(Util::getMacros("...%NAME%a").size() == 1);

        testOk1(Util::getMacros("%VAL%%A%").size() == 2);
        testOk1(Util::getMacros("%VAL%A%").size() == 2); // %VAL%, A and a leftover %
        testOk1(Util::getMacros("VAL%A%").size() == 2);
    }
};

MAIN(testutil)
{
    testPlan(53);
    TestReplace::basic();
    TestReplace::multipleInstances();
    TestReplace::singleChar();
    TestReplace::injectDelimiter();
    TestReplace::inTheMiddleOfText();

    TestEscape::newLine();
    TestEscape::singleQuote();

    TestJoin::simple();

    return testDone();
}
