#ifndef JSONOBJECT_H
#define JSONOBJECT_H
/*
jsonobject.h

Class declaration for JSONobject, a simple multi-platform JSON parser
that wraps the use and creation of JSON into a single object.
The internal parser is a single pass JSON parser.  The class follows
the shape of XMLTag: one tree of values, operator overloads for access,
and an export of the other markup format.  XMLTag exports JSON.
JSONobject exports XML.


Copyright (C) 2026  Stephen D. Mathews Jr.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <stdio.h>
#include <string>
#include <vector>
#include <stddef.h>


/**
* If a JSON rule is broken during parsing, or a member is requested
* that does not exist, a JSONobject instance throws a JSONobjectException.
*/
struct JSONobjectException
{
    /**
    * Text description of the failure.
    */
    std::string msg;
    /**
    * A short sample of the cause of failure when possible.
    */
    std::string sample;
};


/**
* One JSON value.  A value is null, a bool, a number, a string, an array,
* or an object.  Object members and array elements are child values.
* The name is the object key when this value is a member of an object.
* Array elements have an empty name.  The root name is not part of the
* JSON text; it is the root element name used by the XML export.
*/
class JSONobject
{

public:
    enum Type
    {
        TypeNull = 0,
        TypeBool,
        TypeNumber,
        TypeString,
        TypeArray,
        TypeObject
    };

private:
    std::string m_name;
    std::string m_value;
    Type m_type;
    std::vector<JSONobject*> m_children;
    bool m_ignoreCase;
    bool m_strictMode;
    bool m_incomplete;
    JSONobject* m_parent;

    FILE* m_fp;
    const char* m_user;
    int m_userSize;
    int m_pos;
    std::string m_raw;

    mutable char* m_tempbuf;

    void init();
    void wipeChildren();
    void become(Type type, const std::string& value);
    std::string fold(const std::string& text) const;
    bool nameEquals(const std::string& stored, const char* key) const;

    JSONobject& link(JSONobject* child);
    JSONobject& insertAt(int index, JSONobject* child, bool ownOnFailure);
    void destroyAt(int index);
    bool equalValue(const JSONobject& other) const;
    void ensureObject();
    void ensureArray();

    const char* publish(const std::string& text) const;
    void generateJSON(std::string& out, int tabs) const;
    void generateXML(std::string& out, int tabs) const;
    std::string xmlElementName(bool* needsNameAttribute) const;

    bool nextByte(char* out);
    void unreadByte(char byte);
    char requireByte();
    void skipWs();
    void fail(const std::string& msg, const std::string& sample);
    void expectRest(const char* rest);
    unsigned parseHex4();
    std::string parseString();
    void parseNumber(JSONobject* node, char first);
    void parseObject(JSONobject* node, int depth);
    void parseArray(JSONobject* node, int depth);
    void parseValue(JSONobject* node, int depth);
    bool parseDocument();

public:

    /**
    * Default constructor.  The value is null.
    */
    JSONobject();

    /**
    * Value of a given type.  TypeArray builds an empty array.
    * TypeObject builds an empty object.  Scalars start empty:
    * false, 0, "", or null.
    */
    explicit JSONobject(Type type);

    /**
    * Copy constructor.  Deep-copies the tree.  The copy has no parent.
    */
    JSONobject(const JSONobject& other);

    /**
    * Constructor used while building a tree.  parent is not adopted.
    */
    JSONobject(JSONobject* parent);

    /**
    * Empty object with a name.  The name is the XML root (or the key,
    * once this value is placed under a parent).
    */
    JSONobject(const char* name);

    /**
    * String value with a name.
    */
    JSONobject(const char* name, const char* value);

    JSONobject(const char* name, bool value);
    JSONobject(const char* name, int value);
    JSONobject(const char* name, unsigned int value);
    JSONobject(const char* name, float value);
    JSONobject(const char* name, double value);

    /**
    * Empty object named by strName.
    */
    JSONobject(std::string strName);

    /**
    * String value named by strName.
    */
    JSONobject(std::string strName, std::string strValue);

    ~JSONobject();

    /**
    * Clears the name, value, and children.  Does not change the parent,
    * strict mode, or ignore-case flag.
    */
    void clear();

    /**
    * Missing members always throw.  When bMode is true, a second JSON
    * value after the first one also throws.  When bMode is false the
    * parser stops after one value so a stream can carry the next value.
    */
    void strictMode(bool bMode);

    /**
    * When set, member names are compared without regard to ASCII case.
    * Names assigned while this is on are stored in lowercase.
    */
    void ignoreCase(bool bIgnore = false);

    /**
    * Parse one JSON value from a file.  Returns false if the file cannot
    * be opened or the value is incomplete.  Throws JSONobjectException
    * when the text breaks JSON grammar.
    */
    bool load(const char* szFileName);

    /**
    * Write this value as JSON.  If the file exists it is overwritten.
    * bFormatted selects the tabbed layout.
    */
    bool store(const char* szFileName, bool bFormatted = false) const;

    /**
    * Parse one JSON value from a buffer.  A zero size uses strlen.
    * Returns the number of bytes consumed, including trailing whitespace
    * of that value.  Incomplete input returns after setting incomplete().
    */
    int read(const char* pBuffer, int iBufferSize = 0);

    /**
    * Parse one JSON value from the current position of an open file.
    * Leaves the file positioned after that value.
    */
    bool read(FILE* fp);

    /**
    * Write compact JSON to an open file.  Returns the number of bytes
    * written, or 0 on failure.
    */
    int write(FILE* fp) const;

    /**
    * Write compact JSON into a new buffer.  The caller deletes it with
    * delete[].  Returns the number of bytes written, not counting the
    * terminating NUL.
    */
    int write(char** pBuffer) const;

    /**
    * Compact JSON for this value.  The pointer is owned by this object
    * and is invalidated by the next getJSON, getXML, write, or store.
    */
    const char* getJSON() const;

    /**
    * Tab-indented JSON for this value.  Same lifetime as getJSON().
    */
    const char* getJSONFormatted() const;

    /**
    * Compact XML export.  Same lifetime as getJSON().
    *
    * Objects become elements.  Member names become child element names.
    * Strings are text.  Numbers, bools, null, arrays, and empty objects
    * carry a type attribute.  Array elements are item elements.  A name
    * that is not a legal XML name is written as an item with a name
    * attribute.  An empty root name is written as json.
    */
    const char* getXML() const;

    /**
    * Tab-indented XML export.  Same lifetime as getJSON().
    */
    const char* getXMLFormatted() const;

    /**
    * Assigns a string value.  Children of this node are discarded.
    */
    JSONobject& operator=(const char* szValue);

    /**
    * Deep-copies the other value.  This node's parent is kept.
    */
    JSONobject& operator=(const JSONobject& other);

    /**
    * Compares a scalar to szComp.  Objects and arrays compare their name.
    */
    bool operator==(const char* szComp) const;

    bool operator!=(const char* szComp) const;

    /**
    * Compares name, type, scalar text, and children, in order.
    */
    bool operator==(const JSONobject& other) const;

    bool operator!=(const JSONobject& other) const;

    /**
    * Path from the root, joined with '/'.  An unnamed node contributes
    * its zero-based index.
    */
    std::string path() const;

    /**
    * Walk a path.  '/' separates names.  '#' takes the next int argument
    * as a zero-based index.  Throws if a step is missing.
    */
    JSONobject& path(const std::string strPath, std::vector<int> indexes);
    JSONobject& path(const std::string strPath, ...);

    /**
    * True when every step of the path exists.
    */
    bool hasPath(const std::string strPath, ...) const;

    /**
    * Create any missing object members in the path and return the last.
    * The first segment renames this node when the path contains a '/'.
    * '#' selects an existing index and does not create it.
    */
    JSONobject& buildPath(const std::string strPath, ...);

    /**
    * Like buildPath, but the last segment is always inserted, even when
    * a member with that name already exists.
    */
    JSONobject& addPath(const std::string strPath, ...);

    /**
    * First child member with this name.  Throws if it does not exist.
    */
    JSONobject& operator[](const std::string& strName) const;
    JSONobject& operator[](const char* szName) const;

    JSONobject& get(const std::string& strName) const;
    JSONobject& get(const char* szName) const;

    /**
    * Child by insertion order.  Works for objects and arrays.
    * Throws if the index is out of range.  The int overload is what
    * makes a literal 0 select the first child instead of a null name.
    */
    JSONobject& operator[](int index) const;
    JSONobject& operator[](size_t index) const;
    JSONobject& get(int index) const;
    JSONobject& get(size_t index) const;

    JSONobject* parent();
    const JSONobject* parent() const;

    /**
    * Bytes consumed by the last parse.  Empty when this value was built
    * by hand.
    */
    const char* buffer() const;
    int buffer_size() const;

    /**
    * Number of child members or array elements.
    */
    int count() const;

    Type type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    /**
    * Parse one JSON value and store it as a new member.
    */
    JSONobject& addJSON(const char* szName, std::string strJSON);
    JSONobject& addJSON(std::string strName, std::string strJSON);

    /**
    * Take ownership of pChild and store it as a member.  pChild must be
    * a heap object that no other parent owns.
    */
    JSONobject& add(JSONobject* pChild);

    /**
    * Deep-copy other and store the copy as a member.
    */
    JSONobject& add(const JSONobject& other);

    /**
    * Add an empty object member.
    */
    JSONobject& add(const char* szName);
    JSONobject& add(const char* szName, const char* szValue);
    JSONobject& add(const char* szName, bool value);
    JSONobject& add(const char* szName, int value);
    JSONobject& add(const char* szName, unsigned int value);
    JSONobject& add(const char* szName, float value);
    JSONobject& add(const char* szName, double value);
    JSONobject& add(std::string strName);
    JSONobject& add(std::string strName, std::string strValue);

    /**
    * Parse successive JSON values and append each one to this array.
    * A null value becomes an array.  Stops at the end of the buffer.
    * Incomplete trailing text is left unparsed.
    */
    void insertJSON(std::string strJSON);

    /**
    * Append to this array.  A null value becomes an array.
    * push() with no value appends an empty object.
    */
    JSONobject& push();
    JSONobject& pushNull();
    JSONobject& push(const char* szValue);
    JSONobject& push(bool value);
    JSONobject& push(int value);
    JSONobject& push(unsigned int value);
    JSONobject& push(float value);
    JSONobject& push(double value);
    JSONobject& push(std::string strValue);
    JSONobject& push(const JSONobject& other);
    JSONobject& push(JSONobject* pChild);

    /**
    * Parse one JSON value and append it.  Objects and arrays are one
    * element, not a list of members.
    */
    JSONobject& pushJSON(const char* szJSON);
    JSONobject& pushJSON(std::string strJSON);

    /**
    * Insert at a zero-based index.  An index equal to count() appends.
    * A null value becomes an array.  insert() with only an index inserts
    * an empty object.
    */
    JSONobject& insert(int index);
    JSONobject& insertNull(int index);
    JSONobject& insert(int index, const char* szValue);
    JSONobject& insert(int index, bool value);
    JSONobject& insert(int index, int value);
    JSONobject& insert(int index, unsigned int value);
    JSONobject& insert(int index, float value);
    JSONobject& insert(int index, double value);
    JSONobject& insert(int index, std::string strValue);
    JSONobject& insert(int index, const JSONobject& other);
    JSONobject& insert(int index, JSONobject* pChild);
    JSONobject& insertJSON(int index, const char* szJSON);
    JSONobject& insertJSON(int index, std::string strJSON);

    /**
    * First or last child.  Throws if this value has no children.
    */
    JSONobject& front() const;
    JSONobject& back() const;

    /**
    * Remove the last array element and return a copy.  Throws if this
    * is not a non-empty array.
    */
    JSONobject pop();

    /**
    * Delete the child at index.  Returns false if the index is out of
    * range.  Works for array elements and for object members.
    */
    bool deleteAt(int index);
    bool deleteAt(size_t index);

    /**
    * Grow or shrink this array.  New elements are null.  A null value
    * becomes an array.  resize(0) leaves an empty array.
    */
    void resize(int size);
    void resize(size_t size);

    /**
    * Index of the first matching array element, or -1.
    * A number matches numerically.  A string matches element text.
    * An object or array matches by value, ignoring the element's own name.
    */
    int indexOf(const char* szValue) const;
    int indexOf(const std::string& strValue) const;
    int indexOf(bool value) const;
    int indexOf(int value) const;
    int indexOf(unsigned int value) const;
    int indexOf(double value) const;
    int indexOf(const JSONobject& other) const;

    bool contains(const char* szValue) const;
    bool contains(const std::string& strValue) const;
    bool contains(bool value) const;
    bool contains(int value) const;
    bool contains(unsigned int value) const;
    bool contains(double value) const;
    bool contains(const JSONobject& other) const;

    /**
    * Append a value.  Appending an array copies each of its elements.
    * A null value becomes an array.
    */
    JSONobject& operator+=(const JSONobject& other);
    JSONobject& operator+=(const char* szValue);
    JSONobject& operator+=(const std::string& strValue);
    JSONobject& operator+=(bool value);
    JSONobject& operator+=(int value);
    JSONobject& operator+=(unsigned int value);
    JSONobject& operator+=(double value);

    bool has(const char* szName) const;
    bool has(const std::string& strName) const;

    /**
    * Delete the first member with this name.  Returns false if it was
    * not present.
    */
    bool deleteMember(const char* szName);
    bool deleteMember(const std::string& strName);

    /**
    * Scalar text.  Bools are "true" or "false".  Null is "null".
    * Objects and arrays return an empty string.
    */
    const std::string& value() const;

    int intValue() const;
    float floatValue() const;
    double doubleValue() const;

    /**
    * True for true, yes, hi, high, on, and any number other than zero.
    * The comparison is case insensitive.  Anything else is false.
    */
    bool boolValue() const;

    void setValue(const char* szValue);
    void setValue(std::string strValue);
    void setValue(int value);
    void setValue(unsigned int value);
    void setValue(float value);
    void setValue(double value);

    void setNull();
    void setObject();
    void setArray();

    const std::string& name() const;
    void setName(const char* szName);
    void setName(std::string strName);

    /**
    * True when the last parse hit the end of the input before the value
    * was finished.
    */
    bool incomplete() const
    {
        return m_incomplete;
    }
};

#endif
