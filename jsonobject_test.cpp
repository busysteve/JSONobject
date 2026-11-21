#include "jsonobject.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

static int g_fails = 0;

static void check(bool cond, const char* expr, int line)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL %d: %s\n", line, expr);
        g_fails++;
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

static bool threwOn(const char* text)
{
    try {
        JSONobject value;
        value.read(text);
    } catch (const JSONobjectException&) {
        return true;
    }
    return false;
}

int main()
{
    JSONobject json("toptag");
    JSONobject& ref = json.add("innertag");
    ref.add("child1", "sammy");
    ref.add("child3", "frank");
    ref.add("child2", "tootie");

    CHECK(json["innertag"][2].value() == "tootie");
    CHECK(std::string(json.getJSON()) ==
            "{\"innertag\":{\"child1\":\"sammy\",\"child3\":\"frank\",\"child2\":\"tootie\"}}");

    const char* pretty =
        "{\n"
        "\t\"innertag\": {\n"
        "\t\t\"child1\": \"sammy\",\n"
        "\t\t\"child3\": \"frank\",\n"
        "\t\t\"child2\": \"tootie\"\n"
        "\t}\n"
        "}\n";
    CHECK(std::string(json.getJSONFormatted()) == pretty);

    const char* xml =
        "<toptag>\n"
        "\t<innertag>\n"
        "\t\t<child1>sammy</child1>\n"
        "\t\t<child3>frank</child3>\n"
        "\t\t<child2>tootie</child2>\n"
        "\t</innertag>\n"
        "</toptag>\n";
    CHECK(std::string(json.getXMLFormatted()) == xml);
    CHECK(std::string(json.getXML()) ==
            "<toptag><innertag><child1>sammy</child1><child3>frank</child3><child2>tootie</child2></innertag></toptag>");

    JSONobject parsed;
    parsed.setName("toptag");
    CHECK(parsed.read(json.getJSON()) > 0);
    CHECK(!parsed.incomplete());
    CHECK(parsed["innertag"]["child1"].value() == "sammy");
    CHECK(parsed["innertag"][2] == "tootie");
    CHECK(parsed["innertag"] == json["innertag"]);
    CHECK(parsed == json);

    JSONobject doc;
    doc.add("n", 12);
    doc.add("z", 0);
    doc.add("f", 1.5);
    doc.add("ok", true);
    doc.add("no", false);
    doc.add("empty", "");
    JSONobject& nil = doc.add("nil");
    nil.setNull();
    JSONobject& arr = doc.add("arr");
    arr.setArray();
    arr.push("a");
    arr.push(2);
    arr.push(false);
    arr.pushNull();
    JSONobject& obj = arr.push();
    obj.add("k", "v");

    CHECK(doc["z"].isNumber());
    CHECK(doc["z"].intValue() == 0);
    CHECK(doc["ok"].isBool());
    CHECK(doc["ok"].boolValue());
    CHECK(!doc["no"].boolValue());
    CHECK(doc["nil"].isNull());
    CHECK(doc["empty"].isString());
    CHECK(doc["empty"].value().empty());
    CHECK(doc["arr"].isArray());
    CHECK(doc["arr"][4].isObject());
    CHECK(doc["arr"][4]["k"].value() == "v");
    CHECK(std::string(doc.getJSON()) ==
            "{\"n\":12,\"z\":0,\"f\":1.5,\"ok\":true,\"no\":false,\"empty\":\"\",\"nil\":null,\"arr\":[\"a\",2,false,null,{\"k\":\"v\"}]}");

    const char* docXml =
        "<json>"
        "<n type=\"number\">12</n>"
        "<z type=\"number\">0</z>"
        "<f type=\"number\">1.5</f>"
        "<ok type=\"bool\">true</ok>"
        "<no type=\"bool\">false</no>"
        "<empty></empty>"
        "<nil type=\"null\"/>"
        "<arr type=\"array\">"
        "<item>a</item>"
        "<item type=\"number\">2</item>"
        "<item type=\"bool\">false</item>"
        "<item type=\"null\"/>"
        "<item><k>v</k></item>"
        "</arr>"
        "</json>";
    CHECK(std::string(doc.getXML()) == docXml);

    JSONobject again;
    CHECK(again.read(doc.getJSON()) > 0);
    CHECK(std::string(again.getJSON()) == std::string(doc.getJSON()));

    JSONobject esc;
    esc.add("s", "a\"b\\c\n\t");
    JSONobject back;
    CHECK(back.read(esc.getJSON()) > 0);
    CHECK(back["s"].value() == "a\"b\\c\n\t");

    JSONobject uni;
    CHECK(uni.read("\"\\u0041\\u00e9\\uD83D\\uDE00\"") > 0);
    CHECK(uni.isString());
    CHECK(uni.value().size() == 7);
    CHECK(uni.value()[0] == 'A');
    CHECK((unsigned char)uni.value()[1] == 0xC3);
    CHECK((unsigned char)uni.value()[2] == 0xA9);

    JSONobject inc;
    CHECK(inc.read("{\"a\":") >= 0);
    CHECK(inc.incomplete());

    CHECK(threwOn("{,}"));
    CHECK(threwOn("{\"a\":1 /*no*/}"));
    CHECK(threwOn("01"));
    CHECK(threwOn("[1,]"));
    CHECK(!threwOn("\"ok\""));

    bool missing = false;
    try {
        (void)json["nope"];
    } catch (const JSONobjectException& error) {
        missing = error.msg.find("no member") != std::string::npos;
    }
    CHECK(missing);

    CHECK(json.hasPath("innertag/child1"));
    CHECK(!json.hasPath("innertag/missing"));
    CHECK(json.path("innertag/child2").value() == "tootie");
    CHECK(json.path("innertag/#", 0).value() == "sammy");
    CHECK(json["innertag"]["child1"].path() == "toptag/innertag/child1");

    JSONobject built;
    built.buildPath("root/child/item").setValue("hi");
    CHECK(built.name() == "root");
    CHECK(built.isObject());
    CHECK(built["child"]["item"].value() == "hi");
    CHECK(built["child"]["item"].isString());

    JSONobject extra;
    extra.addPath("root/child/item").setValue("one");
    extra.addPath("root/child/item").setValue("two");
    CHECK(extra["child"].count() == 2);
    CHECK(extra["child"][1].value() == "two");

    JSONobject copy = json;
    copy["innertag"]["child1"] = "changed";
    CHECK(json["innertag"]["child1"].value() == "sammy");
    CHECK(copy["innertag"]["child1"].value() == "changed");
    CHECK(copy["innertag"]["child1"].isString());

    CHECK(doc.store("/tmp/jsonobject-test.json", true));
    JSONobject loaded("kept");
    CHECK(loaded.load("/tmp/jsonobject-test.json"));
    CHECK(loaded.name() == "kept");
    CHECK(std::string(loaded.getJSON()) == std::string(doc.getJSON()));
    CHECK(!loaded.load("/tmp/jsonobject-does-not-exist-xyz.json"));

    JSONobject many;
    many.insertJSON("1 true \"x\"");
    CHECK(many.isArray());
    CHECK(many.count() == 3);
    CHECK(many[0].intValue() == 1);
    CHECK(many[1].boolValue());
    CHECK(many[2].value() == "x");

    JSONobject emptyObj;
    CHECK(emptyObj.read("{}") == 2);
    CHECK(emptyObj.isObject());
    CHECK(emptyObj.count() == 0);
    CHECK(std::string(emptyObj.getJSON()) == "{}");

    JSONobject emptyArr;
    CHECK(emptyArr.read("[]") == 2);
    CHECK(emptyArr.isArray());
    CHECK(std::string(emptyArr.getJSON()) == "[]");
    CHECK(std::string(emptyArr.getXML()) == "<json type=\"array\"/>");

    JSONobject num;
    CHECK(num.read("{\"n\":-12.50e+1}") > 0);
    CHECK(num["n"].isNumber());
    CHECK(num["n"].value() == "-12.50e+1");
    CHECK(num["n"].doubleValue() > -126.0 && num["n"].doubleValue() < -124.0);

    JSONobject words;
    words.setValue("yes");
    CHECK(words.isString());
    CHECK(words.boolValue());

    JSONobject marked("r");
    marked.add("a", "1<2&3");
    std::string escaped = marked.getXML();
    CHECK(escaped.find("&lt;") != std::string::npos);
    CHECK(escaped.find("&amp;") != std::string::npos);
    CHECK(escaped.find("<r>") != std::string::npos);

    JSONobject odd;
    odd.add("a b", 3);
    CHECK(std::string(odd.getXML()) == "<json><item name=\"a b\" type=\"number\">3</item></json>");

    JSONobject ic("Root");
    ic.ignoreCase(true);
    ic.add("Child", "Zed");
    CHECK(ic.has("child"));
    CHECK(ic["CHILD"].value() == "Zed");
    ic["child"] = "Yep";
    CHECK(ic["Child"].value() == "Yep");

    JSONobject nf;
    CHECK(nf.read("[null,false,true,0,\"\"]") > 0);
    CHECK(nf[0].isNull());
    CHECK(nf[0].value() == "null");
    CHECK(!nf[1].boolValue());
    CHECK(nf[2].boolValue());
    CHECK(nf[3].isNumber());
    CHECK(nf[4].isString());
    CHECK(nf[4].value().empty());
    CHECK(std::string(nf.getJSON()) == "[null,false,true,0,\"\"]");

    JSONobject dup;
    dup.add("a", "1");
    dup.add("a", "2");
    CHECK(dup["a"].value() == "1");
    CHECK(dup[1].value() == "2");
    CHECK(dup.deleteMember("a"));
    CHECK(dup.count() == 1);
    CHECK(dup["a"].value() == "2");

    JSONobject tr;
    tr.strictMode(true);
    bool trailing = false;
    try {
        tr.read("1 2");
    } catch (const JSONobjectException&) {
        trailing = true;
    }
    CHECK(trailing);

    JSONobject tr2;
    int consumed = tr2.read("1 2");
    CHECK(!tr2.incomplete());
    CHECK(tr2.intValue() == 1);
    CHECK(consumed == 2);

    const char bom[] = "\xEF\xBB\xBF{\"q\":1}";
    JSONobject bomDoc;
    CHECK(bomDoc.read(bom, (int)std::strlen(bom)) > 0);
    CHECK(bomDoc["q"].intValue() == 1);

    JSONobject wrap("doc");
    wrap.addJSON("inner", "{\"k\":[1,2,3]}");
    CHECK(wrap["inner"]["k"][2].intValue() == 3);
    CHECK(std::string(wrap.getXMLFormatted()).find("<doc>") != std::string::npos);

    char* buf = NULL;
    int bytes = json.write(&buf);
    CHECK(bytes > 0);
    CHECK(buf != NULL);
    CHECK(std::string(buf) == json.getJSON());
    delete[] buf;

    CHECK(json == json);
    CHECK(json != doc);

    JSONobject deep = json;
    deep["innertag"].add("child1", "second");
    CHECK(deep["innertag"]["child1"].value() == "sammy");
    CHECK(deep["innertag"][3].value() == "second");

    if (g_fails) {
        std::fprintf(stderr, "%d failed\n", g_fails);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
