JSONobject: A recursive JSON wrapper class for reading and writing JSON in C++ (that also outputs XML)
======================================================================

A small multi-platform C++ JSON parser that wraps reading and writing
JSON into a single object. The parser is a single pass over the input
and stops at the end of one value, so a stream can carry the next value
after it. The class follows [XMLTag](https://github.com/busysteve/XMLTag):
a tree you index with operators, build with `add`, and print with one call.
XMLTag exports JSON. JSONobject exports XML.

- [Intro](#intro)
- [User Guide](#user-guide)
    - [Example](#example)
    - [Values](#values)
    - [Building a tree](#building-a-tree)
    - [Parsing](#parsing)
    - [XML export](#xml-export)
- [License](#license)

Intro
-----
JSONobject is a C++ class that represents one JSON value:

- Name (the object key, or the XML root name)
- Value
- Type: null, bool, number, string, array, or object
- Children, in the order they were inserted

User Guide
----------

Example
-------

```c++
#include "jsonobject.h"
...
    JSONobject json("toptag");
    JSONobject &ref = json.add("innertag");
    ref.add("child1", "sammy");
    ref.add("child3", "frank"); // Retains inserted order
    ref.add("child2", "tootie");

    // Print third child of inner object
    std::cout << json["innertag"][2].value() << std::endl;

    // Print tab formatted JSON output
    std::cout << std::endl << json.getJSONFormatted();

    // Print tab formatted XML output
    std::cout << std::endl << json.getXMLFormatted();
```
```text
g++ -o jsonobject_example1 jsonobject_example1.cpp jsonobject.cpp
> ./jsonobject_example1
tootie

{
	"innertag": {
		"child1": "sammy",
		"child3": "frank",
		"child2": "tootie"
	}
}

<toptag>
	<innertag>
		<child1>sammy</child1>
		<child3>frank</child3>
		<child2>tootie</child2>
	</innertag>
</toptag>
```

`make test` builds `jsonobject_test` and runs the checks.

Values
------

| Type | `getJSON()` | Notes |
| --- | --- | --- |
| null | `null` | `isNull()`, `setNull()` |
| bool | `true` / `false` | `boolValue()` also accepts yes, hi, high, on, and any number other than zero |
| number | the literal | `1`, `-12.50e+1`. `intValue()`, `floatValue()`, `doubleValue()` |
| string | quoted | `setValue("text")` or `obj = "text"` |
| object | `{...}` | members keep insertion order. Duplicate names are kept. `operator[]` returns the first |
| array | `[...]` | `push`, `insertJSON` |

`operator[]` takes a name or an index. A missing name or an index that is out of range throws `JSONobjectException`. `has` and `hasPath` return false instead of throwing.

`operator==` on two objects compares name, type, scalar text, and children. `operator==` on a string compares a scalar's text, or an object or array's name.

Paths use `/` between names and `#` for an index:

```c++
json.path("innertag/#", 2).value();     // tootie
json.hasPath("innertag/child1");
json["innertag"]["child1"].path();       // toptag/innertag/child1
built.buildPath("root/child/item").setValue("hi");
```

`buildPath` creates missing object members. When the path contains `/`, the first segment renames this node. `addPath` always inserts the last segment.

`strictMode(true)` rejects a second value after the first. Left off, `read` stops after one value so the caller can parse the next. `ignoreCase(true)` compares member names without regard to ASCII case.

`getJSON()` and `getXML()` return a buffer owned by the object. The next call to either, or to `write` or `store`, replaces it.

Building a tree
---------------

```c++
JSONobject doc("doc");
doc.add("n", 12);
doc.add("ok", true);
doc.add("name", "sammy");

JSONobject &arr = doc.add("nums");
arr.setArray();
arr.push(1);
arr.push("two");
arr.pushNull();

doc["name"] = "frank";          // string assignment
doc.deleteMember("ok");
```

`add` stores a named member and turns a null value into an object. `push` appends to an array and turns a null value into an array. `addJSON(name, text)` parses one JSON value and stores it under that name. `insertJSON` parses successive values and appends each one.

Parsing
-------

```c++
JSONobject doc("doc");
doc.read("{\"n\":12,\"ok\":true}");
doc.load("data.json");
doc.store("out.json", true);    // true writes the tabbed layout
```

`read` returns how many bytes it consumed. `incomplete()` is true when the buffer ended before the value did. Broken JSON throws `JSONobjectException` with `msg` and `sample`. A name set before `read` or `load` is kept, so the XML root stays put. Numbers are kept as they were written, so `-12.50e+1` round-trips.

`load` and `read(FILE*)` read one value and leave a file stream positioned after it.

XML export
----------

`getXML()` and `getXMLFormatted()` write the tree as XML, the same job `getJSON()` does on XMLTag.

- The root name is the root element. An empty root name is `json`.
- Object members become child elements, in order.
- Strings are element text.
- Numbers, bools, and null carry `type="number"`, `type="bool"`, or `type="null"`.
- Arrays carry `type="array"`. Elements are `item` children.
- An empty object is `<name type="object"/>`.
- A name that is not a legal XML name is written as `<item name="the key">`.
- `<`, `>`, `&`, quotes, and apostrophes in text are escaped.

```xml
<doc>
	<n type="number">12</n>
	<ok type="bool">true</ok>
	<nums type="array">
		<item type="number">1</item>
		<item>two</item>
		<item type="null"/>
	</nums>
</doc>
```

License
-------
This software is licensed under the [GNU LGPL v2.1][lgpl].
© 2026 Stephen D. Mathews Jr.

[lgpl]: https://www.gnu.org/licenses/old-licenses/lgpl-2.1-standalone.html
