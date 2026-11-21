/*
jsonobject.cpp

Class definition for JSONobject, a simple multi-platform JSON parser
that wraps the use and creation of JSON into a single object.
The internal parser is a single pass JSON parser.  The class follows
the shape of XMLTag and exports XML the way XMLTag exports JSON.


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

#include "jsonobject.h"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace {

struct EndOfInput {
};

const int kMaxDepth = 512;

bool isWs(char byte)
{
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
}

void appendUtf8(std::string& out, unsigned code)
{
    if (code <= 0x7Fu) {
        out += (char)code;
    } else if (code <= 0x7FFu) {
        out += (char)(0xC0u | (code >> 6));
        out += (char)(0x80u | (code & 0x3Fu));
    } else if (code <= 0xFFFFu) {
        out += (char)(0xE0u | (code >> 12));
        out += (char)(0x80u | ((code >> 6) & 0x3Fu));
        out += (char)(0x80u | (code & 0x3Fu));
    } else {
        out += (char)(0xF0u | (code >> 18));
        out += (char)(0x80u | ((code >> 12) & 0x3Fu));
        out += (char)(0x80u | ((code >> 6) & 0x3Fu));
        out += (char)(0x80u | (code & 0x3Fu));
    }
}

void appendJsonString(std::string& out, const std::string& text)
{
    out += '"';
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char byte = (unsigned char)text[i];
        switch (byte) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (byte < 0x20u) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", byte);
                out += buf;
            } else {
                out += (char)byte;
            }
            break;
        }
    }
    out += '"';
}

void appendIndent(std::string& out, int tabs)
{
    for (int i = 0; i < tabs; i++)
        out += '\t';
}

void appendXmlText(std::string& out, const std::string& text)
{
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char byte = (unsigned char)text[i];
        if (byte == '<')
            out += "&lt;";
        else if (byte == '>')
            out += "&gt;";
        else if (byte == '&')
            out += "&amp;";
        else if (byte == '\'')
            out += "&apos;";
        else if (byte == '"')
            out += "&quot;";
        else if (byte < 0x20u && byte != '\t' && byte != '\n' && byte != '\r') {
            char buf[12];
            std::snprintf(buf, sizeof(buf), "&#x%02x;", byte);
            out += buf;
        } else {
            out += (char)byte;
        }
    }
}

bool isXmlNameStart(unsigned char byte)
{
    return (byte >= 'A' && byte <= 'Z') ||
           (byte >= 'a' && byte <= 'z') ||
           byte == '_' || byte == ':';
}

bool isXmlNameChar(unsigned char byte)
{
    return isXmlNameStart(byte) ||
           (byte >= '0' && byte <= '9') ||
           byte == '-' || byte == '.' ;
}

bool isValidXmlName(const std::string& name)
{
    if (name.empty() || !isXmlNameStart((unsigned char)name[0]))
        return false;
    for (size_t i = 1; i < name.size(); i++) {
        if (!isXmlNameChar((unsigned char)name[i]))
            return false;
    }
    return true;
}

std::string formatSigned(long value)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%ld", value);
    return buf;
}

std::string formatUnsigned(unsigned long value)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%lu", value);
    return buf;
}

std::string formatDouble(double value)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.15g", value);
    return buf;
}

} // namespace


void JSONobject::init()
{
    m_type = TypeNull;
    m_parent = NULL;
    m_fp = NULL;
    m_user = NULL;
    m_userSize = 0;
    m_pos = 0;
    m_tempbuf = NULL;
    m_ignoreCase = false;
    m_strictMode = false;
    m_incomplete = false;
    m_value = "null";
}

void JSONobject::wipeChildren()
{
    for (size_t i = 0; i < m_children.size(); i++)
        delete m_children[i];
    m_children.clear();
}

void JSONobject::become(Type type, const std::string& value)
{
    wipeChildren();
    m_type = type;
    m_value = value;
}

std::string JSONobject::fold(const std::string& text) const
{
    if (!m_ignoreCase)
        return text;
    std::string out = text;
    for (size_t i = 0; i < out.size(); i++) {
        if (out[i] >= 'A' && out[i] <= 'Z')
            out[i] = (char)(out[i] - 'A' + 'a');
    }
    return out;
}

bool JSONobject::nameEquals(const std::string& stored, const char* key) const
{
    std::string wanted = key ? key : "";
    return fold(stored) == fold(wanted);
}

JSONobject::JSONobject()
{
    init();
}

JSONobject::JSONobject(const JSONobject& other)
{
    init();
    m_name = other.m_name;
    m_value = other.m_value;
    m_type = other.m_type;
    m_ignoreCase = other.m_ignoreCase;
    m_strictMode = other.m_strictMode;
    m_incomplete = other.m_incomplete;
    for (size_t i = 0; i < other.m_children.size(); i++) {
        JSONobject* child = new JSONobject(*other.m_children[i]);
        child->m_parent = this;
        m_children.push_back(child);
    }
}

JSONobject::JSONobject(JSONobject* parent)
{
    init();
    m_parent = parent;
    if (parent) {
        m_ignoreCase = parent->m_ignoreCase;
        m_strictMode = parent->m_strictMode;
    }
}

JSONobject::JSONobject(const char* name)
{
    init();
    m_type = TypeObject;
    m_value.clear();
    setName(name);
}

JSONobject::JSONobject(const char* name, const char* value)
{
    init();
    setName(name);
    setValue(value);
}

JSONobject::JSONobject(const char* name, bool value)
{
    init();
    setName(name);
    setValue(value ? "true" : "false");
    m_type = TypeBool;
}

JSONobject::JSONobject(const char* name, int value)
{
    init();
    setName(name);
    setValue(value);
}

JSONobject::JSONobject(const char* name, unsigned int value)
{
    init();
    setName(name);
    setValue(value);
}

JSONobject::JSONobject(const char* name, float value)
{
    init();
    setName(name);
    setValue(value);
}

JSONobject::JSONobject(const char* name, double value)
{
    init();
    setName(name);
    setValue(value);
}

JSONobject::JSONobject(std::string strName)
{
    init();
    m_type = TypeObject;
    m_value.clear();
    setName(strName);
}

JSONobject::JSONobject(std::string strName, std::string strValue)
{
    init();
    setName(strName);
    setValue(strValue);
}

JSONobject::~JSONobject()
{
    wipeChildren();
    delete[] m_tempbuf;
}

void JSONobject::clear()
{
    wipeChildren();
    m_name.clear();
    m_value = "null";
    m_type = TypeNull;
    m_raw.clear();
    m_incomplete = false;
    delete[] m_tempbuf;
    m_tempbuf = NULL;
}

void JSONobject::strictMode(bool bMode)
{
    m_strictMode = bMode;
}

void JSONobject::ignoreCase(bool bIgnore)
{
    m_ignoreCase = bIgnore;
}

JSONobject& JSONobject::operator=(const char* szValue)
{
    setValue(szValue);
    return *this;
}

JSONobject& JSONobject::operator=(const JSONobject& other)
{
    if (this == &other)
        return *this;

    JSONobject copy(other);
    std::swap(m_name, copy.m_name);
    std::swap(m_value, copy.m_value);
    std::swap(m_type, copy.m_type);
    std::swap(m_children, copy.m_children);
    std::swap(m_ignoreCase, copy.m_ignoreCase);
    std::swap(m_strictMode, copy.m_strictMode);
    std::swap(m_incomplete, copy.m_incomplete);

    for (size_t i = 0; i < m_children.size(); i++)
        m_children[i]->m_parent = this;
    for (size_t i = 0; i < copy.m_children.size(); i++)
        copy.m_children[i]->m_parent = &copy;

    return *this;
}

bool JSONobject::operator==(const char* szComp) const
{
    std::string wanted = szComp ? szComp : "";
    if (m_type == TypeObject || m_type == TypeArray)
        return fold(m_name) == fold(wanted);
    return fold(m_value) == fold(wanted);
}

bool JSONobject::operator!=(const char* szComp) const
{
    return !(*this == szComp);
}

bool JSONobject::operator==(const JSONobject& other) const
{
    if (m_type != other.m_type || m_name != other.m_name)
        return false;
    if (m_type != TypeObject && m_type != TypeArray && m_value != other.m_value)
        return false;
    if (m_children.size() != other.m_children.size())
        return false;
    for (size_t i = 0; i < m_children.size(); i++) {
        if (!(*m_children[i] == *other.m_children[i]))
            return false;
    }
    return true;
}

bool JSONobject::operator!=(const JSONobject& other) const
{
    return !(*this == other);
}

std::string JSONobject::path() const
{
    std::string here = m_name;
    if (here.empty() && m_parent) {
        int index = 0;
        for (size_t i = 0; i < m_parent->m_children.size(); i++) {
            if (m_parent->m_children[i] == this) {
                index = (int)i;
                break;
            }
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", index);
        here = buf;
    }

    if (m_parent == NULL)
        return here;

    std::string above = m_parent->path();
    if (above.empty())
        return here;
    if (here.empty())
        return above;
    return above + "/" + here;
}

JSONobject& JSONobject::path(const std::string strPath, std::vector<int> indexes)
{
    JSONobject* ref = this;
    std::string elem;
    size_t used = 0;
    for (size_t i = 0; i < strPath.size(); i++) {
        if (strPath[i] == '/') {
            if (!elem.empty()) {
                ref = &((*ref)[elem]);
                elem.clear();
            }
        } else if (strPath[i] == '#') {
            if (used >= indexes.size())
                fail("Not enough indexes for '#' in path", strPath);
            ref = &((*ref)[(size_t)indexes[used]]);
            used++;
        } else {
            elem += strPath[i];
            if (i + 1 == strPath.size())
                ref = &((*ref)[elem]);
        }
    }
    return *ref;
}

JSONobject& JSONobject::path(const std::string strPath, ...)
{
    va_list ap;
    va_start(ap, strPath);

    JSONobject* ref = this;
    std::string elem;
    for (size_t i = 0; i < strPath.size(); i++) {
        if (strPath[i] == '/') {
            if (!elem.empty()) {
                ref = &((*ref)[elem]);
                elem.clear();
            }
        } else if (strPath[i] == '#') {
            int index = va_arg(ap, int);
            ref = &((*ref)[(size_t)index]);
        } else {
            elem += strPath[i];
            if (i + 1 == strPath.size())
                ref = &((*ref)[elem]);
        }
    }

    va_end(ap);
    return *ref;
}

bool JSONobject::hasPath(const std::string strPath, ...) const
{
    va_list ap;
    va_start(ap, strPath);

    const JSONobject* ref = this;
    std::string elem;
    bool ok = true;

    for (size_t i = 0; i < strPath.size() && ok; i++) {
        if (strPath[i] == '/') {
            if (!elem.empty()) {
                if (!ref->has(elem.c_str()))
                    ok = false;
                else
                    ref = &((*ref)[elem.c_str()]);
                elem.clear();
            }
        } else if (strPath[i] == '#') {
            int index = va_arg(ap, int);
            if (index < 0 || (size_t)index >= ref->m_children.size())
                ok = false;
            else
                ref = ref->m_children[(size_t)index];
        } else {
            elem += strPath[i];
            if (i + 1 == strPath.size()) {
                if (!ref->has(elem.c_str()))
                    ok = false;
            }
        }
    }

    va_end(ap);
    return ok;
}

JSONobject& JSONobject::buildPath(const std::string strPath, ...)
{
    va_list ap;
    va_start(ap, strPath);

    JSONobject* ref = this;
    std::string elem;
    int depth = 0;

    for (size_t i = 0; i < strPath.size(); i++) {
        if (strPath[i] == '/') {
            if (!elem.empty()) {
                if (depth == 0) {
                    setName(elem.c_str());
                    ref = this;
                } else if (!ref->has(elem.c_str())) {
                    ref = &ref->add(elem.c_str());
                } else {
                    ref = &((*ref)[elem.c_str()]);
                }
                elem.clear();
            }
            depth++;
        } else if (strPath[i] == '#') {
            int index = va_arg(ap, int);
            ref = &((*ref)[(size_t)index]);
        } else {
            elem += strPath[i];
            if (i + 1 == strPath.size()) {
                if (!ref->has(elem.c_str()))
                    ref = &ref->add(elem.c_str());
                else
                    ref = &((*ref)[elem.c_str()]);
            }
        }
    }

    va_end(ap);
    return *ref;
}

JSONobject& JSONobject::addPath(const std::string strPath, ...)
{
    va_list ap;
    va_start(ap, strPath);

    JSONobject* ref = this;
    std::string elem;
    int depth = 0;

    for (size_t i = 0; i < strPath.size(); i++) {
        if (strPath[i] == '/') {
            if (!elem.empty()) {
                if (depth == 0) {
                    setName(elem.c_str());
                    ref = this;
                } else if (!ref->has(elem.c_str())) {
                    ref = &ref->add(elem.c_str());
                } else {
                    ref = &((*ref)[elem.c_str()]);
                }
                elem.clear();
            }
            depth++;
        } else if (strPath[i] == '#') {
            int index = va_arg(ap, int);
            ref = &((*ref)[(size_t)index]);
        } else {
            elem += strPath[i];
            if (i + 1 == strPath.size())
                ref = &ref->add(elem.c_str());
        }
    }

    va_end(ap);
    return *ref;
}

JSONobject& JSONobject::operator[](const std::string& strName) const
{
    return (*this)[strName.c_str()];
}

JSONobject& JSONobject::operator[](const char* szName) const
{
    for (size_t i = 0; i < m_children.size(); i++) {
        if (nameEquals(m_children[i]->m_name, szName))
            return *m_children[i];
    }

    JSONobjectException error;
    error.msg = "There is no member with that name";
    error.sample = szName ? szName : "";
    error.sample += " does not exist";
    throw error;
}

JSONobject& JSONobject::get(const std::string& strName) const
{
    return (*this)[strName];
}

JSONobject& JSONobject::get(const char* szName) const
{
    return (*this)[szName];
}

JSONobject& JSONobject::operator[](int index) const
{
    if (index < 0) {
        JSONobjectException error;
        error.msg = "Index out of bounds";
        error.sample = "negative index";
        throw error;
    }
    return (*this)[(size_t)index];
}

JSONobject& JSONobject::operator[](size_t index) const
{
    if (index < m_children.size())
        return *m_children[index];

    char sample[160];
    std::snprintf(sample, sizeof(sample),
            "There are %zu members in this zero based index; there is no item %zu",
            m_children.size(), index);

    JSONobjectException error;
    error.msg = "Index out of bounds";
    error.sample = sample;
    throw error;
}

JSONobject& JSONobject::get(int index) const
{
    return (*this)[index];
}

JSONobject& JSONobject::get(size_t index) const
{
    return (*this)[index];
}

JSONobject* JSONobject::parent()
{
    return m_parent;
}

const JSONobject* JSONobject::parent() const
{
    return m_parent;
}

const char* JSONobject::buffer() const
{
    return m_raw.c_str();
}

int JSONobject::buffer_size() const
{
    return (int)m_raw.size();
}

int JSONobject::count() const
{
    return (int)m_children.size();
}

JSONobject::Type JSONobject::type() const
{
    return m_type;
}

bool JSONobject::isNull() const { return m_type == TypeNull; }
bool JSONobject::isBool() const { return m_type == TypeBool; }
bool JSONobject::isNumber() const { return m_type == TypeNumber; }
bool JSONobject::isString() const { return m_type == TypeString; }
bool JSONobject::isArray() const { return m_type == TypeArray; }
bool JSONobject::isObject() const { return m_type == TypeObject; }

void JSONobject::ensureObject()
{
    if (m_type == TypeNull) {
        m_type = TypeObject;
        m_value.clear();
        return;
    }
    if (m_type != TypeObject) {
        fail("Cannot add a named member to this JSON value", m_name);
    }
}

void JSONobject::ensureArray()
{
    if (m_type == TypeNull) {
        m_type = TypeArray;
        m_value.clear();
        return;
    }
    if (m_type != TypeArray) {
        fail("Cannot append an element to this JSON value", m_name);
    }
}

JSONobject& JSONobject::link(JSONobject* child)
{
    child->m_parent = this;
    m_children.push_back(child);
    return *child;
}

JSONobject& JSONobject::addJSON(const char* szName, std::string strJSON)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    try {
        child->m_ignoreCase = m_ignoreCase;
        child->m_strictMode = m_strictMode;
        child->read(strJSON.c_str(), (int)strJSON.size());
        if (child->incomplete())
            fail("Incomplete JSON value", strJSON);
        child->m_ignoreCase = m_ignoreCase;
        child->setName(szName);
    } catch (...) {
        delete child;
        throw;
    }
    return link(child);
}

JSONobject& JSONobject::addJSON(std::string strName, std::string strJSON)
{
    return addJSON(strName.c_str(), strJSON);
}

JSONobject& JSONobject::add(JSONobject* pChild)
{
    if (pChild == NULL)
        fail("Cannot add a null child pointer", "");
    ensureObject();
    pChild->m_ignoreCase = m_ignoreCase;
    pChild->m_strictMode = m_strictMode;
    return link(pChild);
}

JSONobject& JSONobject::add(const JSONobject& other)
{
    ensureObject();
    JSONobject* child = new JSONobject(other);
    return link(child);
}

JSONobject& JSONobject::add(const char* szName)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->m_type = TypeObject;
    child->m_value.clear();
    child->setName(szName);
    return link(child);
}

JSONobject& JSONobject::add(const char* szName, const char* szValue)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->setName(szName);
    child->setValue(szValue);
    return link(child);
}

JSONobject& JSONobject::add(const char* szName, bool value)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->setName(szName);
    child->setValue(value ? "true" : "false");
    child->m_type = TypeBool;
    return link(child);
}

JSONobject& JSONobject::add(const char* szName, int value)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->setName(szName);
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::add(const char* szName, unsigned int value)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->setName(szName);
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::add(const char* szName, float value)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->setName(szName);
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::add(const char* szName, double value)
{
    ensureObject();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->setName(szName);
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::add(std::string strName)
{
    return add(strName.c_str());
}

JSONobject& JSONobject::add(std::string strName, std::string strValue)
{
    return add(strName.c_str(), strValue.c_str());
}

void JSONobject::insertJSON(std::string strJSON)
{
    ensureArray();
    int pos = 0;
    int len = (int)strJSON.size();
    while (pos < len) {
        while (pos < len && isWs(strJSON[pos]))
            pos++;
        if (pos >= len)
            break;

        JSONobject temp;
        temp.m_ignoreCase = m_ignoreCase;
        temp.m_strictMode = m_strictMode;
        int consumed = temp.read(strJSON.c_str() + pos, len - pos);
        if (temp.incomplete() || consumed <= 0)
            break;

        JSONobject* child = new JSONobject(temp);
        child->m_name.clear();
        link(child);
        pos += consumed;
    }
}

JSONobject& JSONobject::push()
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->m_ignoreCase = m_ignoreCase;
    child->m_strictMode = m_strictMode;
    child->m_type = TypeObject;
    child->m_value.clear();
    return link(child);
}

JSONobject& JSONobject::pushNull()
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setNull();
    return link(child);
}

JSONobject& JSONobject::push(const char* szValue)
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setValue(szValue);
    return link(child);
}

JSONobject& JSONobject::push(bool value)
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setValue(value ? "true" : "false");
    child->m_type = TypeBool;
    return link(child);
}

JSONobject& JSONobject::push(int value)
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::push(unsigned int value)
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::push(float value)
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::push(double value)
{
    ensureArray();
    JSONobject* child = new JSONobject();
    child->setValue(value);
    return link(child);
}

JSONobject& JSONobject::push(std::string strValue)
{
    return push(strValue.c_str());
}

JSONobject& JSONobject::push(const JSONobject& other)
{
    ensureArray();
    JSONobject* child = new JSONobject(other);
    child->m_name.clear();
    return link(child);
}

JSONobject& JSONobject::push(JSONobject* pChild)
{
    if (pChild == NULL)
        fail("Cannot push a null child pointer", "");
    ensureArray();
    pChild->m_name.clear();
    return link(pChild);
}

bool JSONobject::has(const std::string& strName) const
{
    return has(strName.c_str());
}

bool JSONobject::has(const char* szName) const
{
    for (size_t i = 0; i < m_children.size(); i++) {
        if (nameEquals(m_children[i]->m_name, szName))
            return true;
    }
    return false;
}

bool JSONobject::deleteMember(const std::string& strName)
{
    return deleteMember(strName.c_str());
}

bool JSONobject::deleteMember(const char* szName)
{
    for (size_t i = 0; i < m_children.size(); i++) {
        if (nameEquals(m_children[i]->m_name, szName)) {
            delete m_children[i];
            m_children.erase(m_children.begin() + (std::ptrdiff_t)i);
            return true;
        }
    }
    return false;
}

const std::string& JSONobject::value() const
{
    return m_value;
}

int JSONobject::intValue() const
{
    return std::atoi(m_value.c_str());
}

float JSONobject::floatValue() const
{
    return (float)std::atof(m_value.c_str());
}

double JSONobject::doubleValue() const
{
    return std::atof(m_value.c_str());
}

bool JSONobject::boolValue() const
{
    if (m_type == TypeBool)
        return m_value == "true";

    std::string text = m_value;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] >= 'A' && text[i] <= 'Z')
            text[i] = (char)(text[i] - 'A' + 'a');
    }

    if (text == "true" || text == "yes" || text == "hi" ||
            text == "on" || text == "high")
        return true;

    if (text.empty())
        return false;

    return std::atoi(text.c_str()) != 0;
}

void JSONobject::setValue(const char* szValue)
{
    become(TypeString, szValue ? szValue : "");
}

void JSONobject::setValue(std::string strValue)
{
    setValue(strValue.c_str());
}

void JSONobject::setValue(int value)
{
    become(TypeNumber, formatSigned(value));
}

void JSONobject::setValue(unsigned int value)
{
    become(TypeNumber, formatUnsigned(value));
}

void JSONobject::setValue(float value)
{
    become(TypeNumber, formatDouble(value));
}

void JSONobject::setValue(double value)
{
    become(TypeNumber, formatDouble(value));
}

void JSONobject::setNull()
{
    become(TypeNull, "null");
}

void JSONobject::setObject()
{
    if (m_type == TypeObject)
        return;
    become(TypeObject, "");
}

void JSONobject::setArray()
{
    if (m_type == TypeArray)
        return;
    become(TypeArray, "");
}

const std::string& JSONobject::name() const
{
    return m_name;
}

void JSONobject::setName(const char* szName)
{
    m_name = szName ? szName : "";
    if (m_ignoreCase)
        m_name = fold(m_name);
}

void JSONobject::setName(std::string strName)
{
    setName(strName.c_str());
}

const char* JSONobject::publish(const std::string& text) const
{
    delete[] m_tempbuf;
    m_tempbuf = new char[text.size() + 1];
    std::memcpy(m_tempbuf, text.c_str(), text.size() + 1);
    return m_tempbuf;
}

void JSONobject::generateJSON(std::string& out, int tabs) const
{
    const bool pretty = tabs >= 0;

    if (m_type == TypeObject) {
        if (m_children.empty()) {
            out += "{}";
            return;
        }
        out += '{';
        if (pretty)
            out += '\n';
        for (size_t i = 0; i < m_children.size(); i++) {
            if (pretty)
                appendIndent(out, tabs + 1);
            appendJsonString(out, m_children[i]->m_name);
            out += pretty ? ": " : ":";
            m_children[i]->generateJSON(out, pretty ? tabs + 1 : -1);
            if (i + 1 < m_children.size())
                out += ',';
            if (pretty)
                out += '\n';
        }
        if (pretty)
            appendIndent(out, tabs);
        out += '}';
        return;
    }

    if (m_type == TypeArray) {
        if (m_children.empty()) {
            out += "[]";
            return;
        }
        out += '[';
        if (pretty)
            out += '\n';
        for (size_t i = 0; i < m_children.size(); i++) {
            if (pretty)
                appendIndent(out, tabs + 1);
            m_children[i]->generateJSON(out, pretty ? tabs + 1 : -1);
            if (i + 1 < m_children.size())
                out += ',';
            if (pretty)
                out += '\n';
        }
        if (pretty)
            appendIndent(out, tabs);
        out += ']';
        return;
    }

    if (m_type == TypeString) {
        appendJsonString(out, m_value);
        return;
    }

    if (m_type == TypeBool || m_type == TypeNumber || m_type == TypeNull) {
        out += m_value;
        return;
    }

    out += "null";
}

std::string JSONobject::xmlElementName(bool* needsNameAttribute) const
{
    *needsNameAttribute = false;
    if (isValidXmlName(m_name))
        return m_name;

    if (!m_name.empty())
        *needsNameAttribute = true;

    return "item";
}

void JSONobject::generateXML(std::string& out, int tabs) const
{
    const bool pretty = tabs >= 0;
    bool needsNameAttribute = false;
    std::string element = xmlElementName(&needsNameAttribute);

    if (m_parent == NULL && m_name.empty())
        element = "json";

    if (pretty)
        appendIndent(out, tabs < 0 ? 0 : tabs);

    out += '<';
    out += element;

    if (needsNameAttribute) {
        out += " name=\"";
        appendXmlText(out, m_name);
        out += '"';
    }

    if (m_type == TypeNumber)
        out += " type=\"number\"";
    else if (m_type == TypeBool)
        out += " type=\"bool\"";
    else if (m_type == TypeNull)
        out += " type=\"null\"";
    else if (m_type == TypeArray)
        out += " type=\"array\"";
    else if (m_type == TypeObject && m_children.empty())
        out += " type=\"object\"";

    bool emptyElement = (m_type == TypeNull) ||
                        (m_type == TypeArray && m_children.empty()) ||
                        (m_type == TypeObject && m_children.empty());

    if (emptyElement) {
        out += "/>";
        if (pretty)
            out += '\n';
        return;
    }

    if (m_type == TypeString || m_type == TypeNumber || m_type == TypeBool) {
        out += '>';
        appendXmlText(out, m_value);
        out += "</";
        out += element;
        out += '>';
        if (pretty)
            out += '\n';
        return;
    }

    out += '>';
    if (pretty)
        out += '\n';
    for (size_t i = 0; i < m_children.size(); i++)
        m_children[i]->generateXML(out, pretty ? tabs + 1 : -1);
    if (pretty)
        appendIndent(out, tabs < 0 ? 0 : tabs);
    out += "</";
    out += element;
    out += '>';
    if (pretty)
        out += '\n';
}

const char* JSONobject::getJSON() const
{
    std::string text;
    generateJSON(text, -1);
    return publish(text);
}

const char* JSONobject::getJSONFormatted() const
{
    std::string text;
    generateJSON(text, 0);
    text += '\n';
    return publish(text);
}

const char* JSONobject::getXML() const
{
    std::string text;
    generateXML(text, -1);
    return publish(text);
}

const char* JSONobject::getXMLFormatted() const
{
    std::string text;
    generateXML(text, 0);
    return publish(text);
}

bool JSONobject::load(const char* szFileName)
{
    if (szFileName == NULL || szFileName[0] == '\0')
        return false;

    FILE* fp = std::fopen(szFileName, "rb");
    if (fp == NULL)
        return false;

    m_fp = fp;
    m_user = NULL;
    m_userSize = 0;

    try {
        bool ok = parseDocument();
        m_fp = NULL;
        std::fclose(fp);
        return ok;
    } catch (...) {
        m_fp = NULL;
        std::fclose(fp);
        throw;
    }
}

bool JSONobject::store(const char* szFileName, bool bFormatted) const
{
    if (szFileName == NULL || szFileName[0] == '\0')
        return false;

    FILE* fp = std::fopen(szFileName, "wb");
    if (fp == NULL)
        return false;

    const char* text = bFormatted ? getJSONFormatted() : getJSON();
    size_t size = std::strlen(text);
    size_t wrote = std::fwrite(text, 1, size, fp);
    std::fclose(fp);
    return wrote == size;
}

int JSONobject::read(const char* pBuffer, int iBufferSize)
{
    if (pBuffer == NULL)
        return 0;

    m_fp = NULL;
    m_user = pBuffer;
    if (iBufferSize == 0)
        m_userSize = (int)std::strlen(pBuffer);
    else
        m_userSize = iBufferSize;

    if (m_userSize < 0)
        m_userSize = 0;

    parseDocument();
    return m_pos;
}

bool JSONobject::read(FILE* fp)
{
    if (fp == NULL)
        return false;

    m_fp = fp;
    m_user = NULL;
    m_userSize = 0;

    try {
        bool ok = parseDocument();
        m_fp = NULL;
        return ok;
    } catch (...) {
        m_fp = NULL;
        throw;
    }
}

int JSONobject::write(FILE* fp) const
{
    if (fp == NULL)
        return 0;

    const char* text = getJSON();
    size_t size = std::strlen(text);
    if (std::fwrite(text, 1, size, fp) != size)
        return 0;
    return (int)size;
}

int JSONobject::write(char** pBuffer) const
{
    if (pBuffer == NULL)
        return 0;

    const char* text = getJSON();
    size_t size = std::strlen(text);
    *pBuffer = new char[size + 1];
    std::memcpy(*pBuffer, text, size + 1);
    return (int)size;
}

bool JSONobject::nextByte(char* out)
{
    if (m_fp != NULL) {
        int ch = std::fgetc(m_fp);
        if (ch == EOF)
            return false;
        *out = (char)ch;
        m_pos++;
        m_raw.push_back(*out);
        return true;
    }

    if (m_user == NULL || m_pos >= m_userSize)
        return false;

    *out = m_user[m_pos];
    m_pos++;
    m_raw.push_back(*out);
    return true;
}

void JSONobject::unreadByte(char byte)
{
    (void)byte;
    if (!m_raw.empty())
        m_raw.erase(m_raw.size() - 1);
    if (m_pos > 0)
        m_pos--;
    if (m_fp != NULL)
        std::ungetc((unsigned char)byte, m_fp);
}

char JSONobject::requireByte()
{
    char byte = 0;
    if (!nextByte(&byte))
        throw EndOfInput();
    return byte;
}

void JSONobject::skipWs()
{
    for (;;) {
        char byte = 0;
        if (!nextByte(&byte))
            return;
        if (!isWs(byte)) {
            unreadByte(byte);
            return;
        }
    }
}

void JSONobject::fail(const std::string& msg, const std::string& sample)
{
    JSONobjectException error;
    error.msg = msg;
    if (!sample.empty()) {
        error.sample = sample;
    } else if (!m_raw.empty()) {
        size_t start = m_raw.size() > 24 ? m_raw.size() - 24 : 0;
        error.sample = m_raw.substr(start);
    }
    throw error;
}

void JSONobject::expectRest(const char* rest)
{
    for (size_t i = 0; rest[i] != '\0'; i++) {
        char byte = requireByte();
        if (byte != rest[i])
            fail("Invalid literal", std::string(rest, rest + std::strlen(rest)));
    }
}

unsigned JSONobject::parseHex4()
{
    unsigned code = 0;
    for (int i = 0; i < 4; i++) {
        char hex = requireByte();
        code <<= 4;
        if (hex >= '0' && hex <= '9')
            code += (unsigned)(hex - '0');
        else if (hex >= 'a' && hex <= 'f')
            code += (unsigned)(hex - 'a' + 10);
        else if (hex >= 'A' && hex <= 'F')
            code += (unsigned)(hex - 'A' + 10);
        else
            fail("Invalid hex digit in \\u escape", std::string(1, hex));
    }
    return code;
}

std::string JSONobject::parseString()
{
    char quote = requireByte();
    if (quote != '"')
        fail("Expected a string", std::string(1, quote));

    std::string text;
    for (;;) {
        char byte = requireByte();
        if (byte == '"')
            break;
        if ((unsigned char)byte < 0x20u)
            fail("Unescaped control character in string", "");
        if (byte != '\\') {
            text += byte;
            continue;
        }

        char escaped = requireByte();
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            text += escaped;
            break;
        case 'b': text += '\b'; break;
        case 'f': text += '\f'; break;
        case 'n': text += '\n'; break;
        case 'r': text += '\r'; break;
        case 't': text += '\t'; break;
        case 'u': {
            unsigned code = parseHex4();
            if (code >= 0xD800u && code <= 0xDBFFu) {
                char slash = requireByte();
                char marker = requireByte();
                if (slash != '\\' || marker != 'u')
                    fail("Expected a low surrogate", "");
                unsigned low = parseHex4();
                if (low < 0xDC00u || low > 0xDFFFu)
                    fail("Invalid low surrogate", "");
                code = 0x10000u + (((code - 0xD800u) << 10) | (low - 0xDC00u));
            } else if (code >= 0xDC00u && code <= 0xDFFFu) {
                fail("Lone low surrogate", "");
            }
            if (code > 0x10FFFFu)
                fail("Invalid Unicode code point", "");
            appendUtf8(text, code);
            break;
        }
        default:
            fail("Invalid escape", std::string("\\") + escaped);
            break;
        }
    }
    return text;
}

void JSONobject::parseNumber(JSONobject* node, char first)
{
    std::string text;
    char byte = first;

    if (byte == '-') {
        text += byte;
        byte = requireByte();
    }

    if (byte < '0' || byte > '9')
        fail("Invalid number", text);

    if (byte == '0') {
        text += byte;
        if (nextByte(&byte)) {
            if (byte >= '0' && byte <= '9')
                fail("Invalid number", text + byte);
            unreadByte(byte);
        }
    } else {
        text += byte;
        for (;;) {
            if (!nextByte(&byte))
                break;
            if (byte >= '0' && byte <= '9')
                text += byte;
            else {
                unreadByte(byte);
                break;
            }
        }
    }

    if (nextByte(&byte)) {
        if (byte == '.') {
            text += byte;
            byte = requireByte();
            if (byte < '0' || byte > '9')
                fail("Invalid number", text);
            text += byte;
            for (;;) {
                if (!nextByte(&byte))
                    break;
                if (byte >= '0' && byte <= '9')
                    text += byte;
                else {
                    unreadByte(byte);
                    break;
                }
            }
        } else {
            unreadByte(byte);
        }
    }

    if (nextByte(&byte)) {
        if (byte == 'e' || byte == 'E') {
            text += byte;
            if (!nextByte(&byte))
                throw EndOfInput();
            if (byte == '+' || byte == '-') {
                text += byte;
                byte = requireByte();
            }
            if (byte < '0' || byte > '9')
                fail("Invalid number", text);
            text += byte;
            for (;;) {
                if (!nextByte(&byte))
                    break;
                if (byte >= '0' && byte <= '9')
                    text += byte;
                else {
                    unreadByte(byte);
                    break;
                }
            }
        } else {
            unreadByte(byte);
        }
    }

    node->m_type = TypeNumber;
    node->m_value = text;
}

void JSONobject::parseObject(JSONobject* node, int depth)
{
    node->m_type = TypeObject;
    node->m_value.clear();

    skipWs();
    char byte = requireByte();
    if (byte == '}')
        return;
    unreadByte(byte);

    for (;;) {
        skipWs();
        std::string key = parseString();
        skipWs();
        byte = requireByte();
        if (byte != ':')
            fail("Expected ':' after member name", std::string(1, byte));

        JSONobject* child = new JSONobject();
        child->m_ignoreCase = node->m_ignoreCase;
        child->m_strictMode = node->m_strictMode;
        child->setName(key.c_str());
        node->link(child);
        parseValue(child, depth + 1);

        skipWs();
        byte = requireByte();
        if (byte == '}')
            return;
        if (byte != ',')
            fail("Expected ',' or '}'", std::string(1, byte));
    }
}

void JSONobject::parseArray(JSONobject* node, int depth)
{
    node->m_type = TypeArray;
    node->m_value.clear();

    skipWs();
    char byte = requireByte();
    if (byte == ']')
        return;
    unreadByte(byte);

    for (;;) {
        JSONobject* child = new JSONobject();
        child->m_ignoreCase = node->m_ignoreCase;
        child->m_strictMode = node->m_strictMode;
        node->link(child);
        parseValue(child, depth + 1);

        skipWs();
        byte = requireByte();
        if (byte == ']')
            return;
        if (byte != ',')
            fail("Expected ',' or ']'", std::string(1, byte));
    }
}

void JSONobject::parseValue(JSONobject* node, int depth)
{
    if (depth > kMaxDepth)
        fail("JSON nesting is too deep", "");

    node->wipeChildren();
    skipWs();
    char byte = requireByte();

    if (byte == '{') {
        parseObject(node, depth);
    } else if (byte == '[') {
        parseArray(node, depth);
    } else if (byte == '"') {
        unreadByte(byte);
        node->m_type = TypeString;
        node->m_value = parseString();
    } else if (byte == 't') {
        expectRest("rue");
        node->m_type = TypeBool;
        node->m_value = "true";
    } else if (byte == 'f') {
        expectRest("alse");
        node->m_type = TypeBool;
        node->m_value = "false";
    } else if (byte == 'n') {
        expectRest("ull");
        node->m_type = TypeNull;
        node->m_value = "null";
    } else if (byte == '-' || (byte >= '0' && byte <= '9')) {
        parseNumber(node, byte);
    } else {
        fail("Expected a JSON value", std::string(1, byte));
    }
}

bool JSONobject::parseDocument()
{
    std::string savedName = m_name;
    bool savedIgnore = m_ignoreCase;
    bool savedStrict = m_strictMode;
    JSONobject* savedParent = m_parent;

    clear();

    m_name = savedName;
    m_ignoreCase = savedIgnore;
    m_strictMode = savedStrict;
    m_parent = savedParent;
    m_raw.clear();
    m_pos = 0;
    m_incomplete = false;

    if (m_fp == NULL && (m_user == NULL || m_userSize == 0)) {
        m_incomplete = true;
        return false;
    }

    if (m_fp == NULL && m_user != NULL && m_userSize >= 3 &&
            (unsigned char)m_user[0] == 0xEFu &&
            (unsigned char)m_user[1] == 0xBBu &&
            (unsigned char)m_user[2] == 0xBFu) {
        m_pos = 3;
        m_raw.append(m_user, 3);
    }

    try {
        parseValue(this, 0);
        skipWs();
        if (m_strictMode) {
            char extra = 0;
            if (nextByte(&extra)) {
                unreadByte(extra);
                fail("Trailing data after JSON value", std::string(1, extra));
            }
        }
        m_incomplete = false;
        return true;
    } catch (const EndOfInput&) {
        m_incomplete = true;
        return false;
    }
}
