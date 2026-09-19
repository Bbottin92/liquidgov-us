// Unit cases for src/reqjson.hpp. Compiled and run by test_reqjson.sh (header-only, no daemon needed).
// Prints "reqjson_unit: N checks, F failed"; exit status 1 if any failed.
#include "reqjson.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace commons::reqjson;

static int g_checks = 0, g_failed = 0;

static void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) { ++g_failed; std::cout << "  FAIL " << what << "\n"; }
}
static std::string show(const std::optional<std::string>& v) { return v ? "\"" + *v + "\"" : "nullopt"; }
static void str_is(const std::string& body, const std::string& key, const std::optional<std::string>& want) {
    auto got = field_str(body, key);
    check(got == want, "field_str(" + key + ") of " + body + " : expected " + show(want) + ", got " + show(got));
}
static void num_is(const std::string& body, const std::string& key, std::optional<double> want) {
    auto got = field_num(body, key);
    bool ok = (got.has_value() == want.has_value()) && (!got || *got == *want);
    check(ok, "field_num(" + key + ") of " + body + " : expected " +
                  (want ? std::to_string(*want) : "nullopt") + ", got " + (got ? std::to_string(*got) : "nullopt"));
}

int main() {
    const std::optional<std::string> none;

    // ---- the plain shape the client sends
    const std::string plain = R"({"id":"g1","secret":"s","region":"1,1","x":10,"y":20.5,"facing":3,"display_name":"Bo"})";
    str_is(plain, "id", "g1"); str_is(plain, "secret", "s"); str_is(plain, "region", "1,1"); str_is(plain, "display_name", "Bo");
    num_is(plain, "x", 10); num_is(plain, "y", 20.5); num_is(plain, "facing", 3);

    // ---- THE BUG: a value equal to a field name must never be read as the key.
    // Name first, for every field name a guest could type.
    for (const std::string nm : {"x", "y", "id", "secret", "region", "facing", "from_region", "to_region", "item", "policy",
                                 "username", "display_name"}) {
        std::string b = R"({"display_name":")" + nm + R"(","id":"g1","secret":"s","region":"1,1","x":5,"y":6,"facing":2,"item":"i","policy":"p","from_region":"f","to_region":"t"})";
        str_is(b, "display_name", nm);
        str_is(b, "id", "g1"); str_is(b, "secret", "s"); str_is(b, "region", "1,1");
        str_is(b, "item", "i"); str_is(b, "policy", "p"); str_is(b, "from_region", "f"); str_is(b, "to_region", "t");
        num_is(b, "x", 5); num_is(b, "y", 6); num_is(b, "facing", 2);
    }
    // Values that are themselves field names, in a chain: id="secret", secret="region", region="x"
    const std::string chain = R"({"id":"secret","secret":"region","region":"x","x":1,"y":2})";
    str_is(chain, "id", "secret"); str_is(chain, "secret", "region"); str_is(chain, "region", "x");
    num_is(chain, "x", 1); num_is(chain, "y", 2);
    // the exact case Grok named
    num_is(R"({"display_name":"x","x":1.5})", "x", 1.5);
    // a value that is only a key of a LATER field must not satisfy a lookup of a key that is absent
    num_is(R"({"id":"x"})", "x", std::nullopt);
    str_is(R"({"id":"secret"})", "secret", none);
    str_is(R"({"a":"id","b":1})", "id", none);

    // ---- escapes: a name that carries quote/colon/comma text stays inside its own string
    const std::string forged = R"({"display_name":"a\",\"x\":999,\"y\":\"b","x":5,"y":6})";
    str_is(forged, "display_name", std::string("a\",\"x\":999,\"y\":\"b"));
    num_is(forged, "x", 5); num_is(forged, "y", 6);
    str_is(R"({"display_name":"back\\slash","id":"g"})", "display_name", std::string("back\\slash"));
    str_is(R"({"display_name":"back\\slash","id":"g"})", "id", "g");
    str_is(R"({"n":"ends with backslash\\","id":"g"})", "id", "g");   // \\ then closing quote

    // ---- whitespace anywhere JSON allows it
    const std::string ws = "  {\n \"id\" : \"a\" ,\t\"x\"\r\n:  7 , \"y\":-2.5 }\n";
    str_is(ws, "id", "a"); num_is(ws, "x", 7); num_is(ws, "y", -2.5);

    // ---- nested values are skipped whole; their inner keys are not fields of this body
    num_is(R"({"meta":{"x":99,"id":"no"},"x":1})", "x", 1);
    str_is(R"({"meta":{"x":99,"id":"no"},"x":1})", "id", none);
    num_is(R"({"a":[1,{"x":2}],"x":3})", "x", 3);
    num_is(R"({"a":"}]","x":4})", "x", 4);          // brackets inside a string do not close the array/object
    num_is(R"({"a":{"b":"}","c":[ "]" ]},"x":5})", "x", 5);
    num_is(R"({"x":{"n":1}})", "x", std::nullopt);   // a nested value is not a number
    str_is(R"({"id":["g"]})", "id", none);           // ...nor a string

    // ---- absent / empty / not an object
    str_is("{}", "id", none); num_is("{}", "x", std::nullopt);
    str_is("", "id", none);   num_is("", "x", std::nullopt);
    str_is("[1,2]", "id", none); str_is(R"("id")", "id", none); str_is("id:1", "id", none);
    str_is(R"({"id":""})", "id", std::string(""));   // an empty string is a value, not "missing"

    // ---- type mismatches are "missing", never coerced
    num_is(R"({"x":"5"})", "x", std::nullopt);
    str_is(R"({"id":5})", "id", none);
    num_is(R"({"x":null})", "x", std::nullopt); num_is(R"({"x":true})", "x", std::nullopt);
    str_is(R"({"id":null})", "id", none);

    // ---- numbers
    num_is(R"({"x":-3})", "x", -3); num_is(R"({"x":2.5})", "x", 2.5); num_is(R"({"x":1e2})", "x", 100);
    num_is(R"({"x":0})", "x", 0); num_is(R"({"x":12}   )", "x", 12); num_is(R"({"x":12})", "x", 12);
    num_is(R"({"x":0x10})", "x", std::nullopt);      // strtod would read hex
    num_is(R"({"x":5abc})", "x", std::nullopt);
    num_is(R"({"x":+5})", "x", std::nullopt);
    num_is(R"({"x":.5})", "x", std::nullopt);
    num_is(R"({"x":--1})", "x", std::nullopt);
    num_is(R"({"x":1e999})", "x", std::nullopt);     // out of range
    num_is(R"({"x":inf})", "x", std::nullopt); num_is(R"({"x":nan})", "x", std::nullopt);
    num_is(R"({"x":-})", "x", std::nullopt);
    num_is(R"({"x":})", "x", std::nullopt);

    // ---- duplicates: the first top-level one wins
    str_is(R"({"id":"a","id":"b"})", "id", "a");

    // ---- malformed input before the key -> nothing (a lookup never reads past garbage)
    str_is(R"({"a":"unterminated)", "id", none);
    str_is(R"({"a" "id":"g"})", "id", none);         // missing ':'
    str_is(R"({"a":1 "id":"g"})", "id", none);       // missing ','
    str_is(R"({a:1,"id":"g"})", "id", none);         // unquoted key
    str_is(R"({"a":1,,"id":"g"})", "id", none);
    // ...but a key that is reached before the damage is still found
    str_is(R"({"id":"g","a":)", "id", "g");
    num_is(R"({"x":5)", "x", 5);

    std::cout << "reqjson_unit: " << g_checks << " checks, " << g_failed << " failed\n";
    return g_failed ? 1 : 0;
}
