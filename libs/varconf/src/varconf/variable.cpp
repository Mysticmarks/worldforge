/*
 *  variable.cpp - implementation of the typeless value container.
 *  Copyright (C) 2001, Stefanus Du Toit, Joseph Zupko
 *            (C) 2003-2006 Alistair Riddoch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *  Contact:  Joseph Zupko
 *            jaz147@psu.edu
 *
 *            189 Reese St.
 *            Old Forge, PA 18518
 */

#include "variable.h"

#include <string>
#include <string_view>
#include <cstdlib>
#include <utility>
#include <cctype>

namespace {

std::string_view trim_ascii_whitespace(std::string_view value) {
        constexpr std::string_view whitespace = " \t\n\r\f\v";

        const auto first = value.find_first_not_of(whitespace);
        if (first == std::string_view::npos) {
                return {};
        }

        const auto last = value.find_last_not_of(whitespace);
        return value.substr(first, last - first + 1);
}

std::string ascii_lower_copy(std::string_view value) {
        std::string result;
        result.reserve(value.size());

        for (unsigned char ch: value) {
                result.push_back(static_cast<char>(std::tolower(ch)));
        }

        return result;
}

bool try_parse_bool(std::string_view value, bool& result) {
        const std::string_view trimmed = trim_ascii_whitespace(value);
        if (trimmed.empty()) {
                return false;
        }

        const std::string normalized = ascii_lower_copy(trimmed);

        if (normalized == "on" || normalized == "1" || normalized == "true"
            || normalized == "yes" || normalized == "y") {
                result = true;
                return true;
        }

        if (normalized == "off" || normalized == "0" || normalized == "false"
            || normalized == "no" || normalized == "n") {
                result = false;
                return true;
        }

        return false;
}

} // namespace

namespace varconf {

VarBase::VarBase()
		: m_have_bool(false), m_have_int(false), m_have_double(false),
		  m_have_string(false), m_val_bool(false), m_val_int(0), m_val_double(0.0),
		  m_scope(GLOBAL) {
}

VarBase::VarBase(const VarBase& c)
		: sigc::trackable(c), m_have_bool(c.m_have_bool), m_have_int(c.m_have_int),
		  m_have_double(c.m_have_double), m_have_string(c.m_have_string),
		  m_val_bool(c.m_val_bool), m_val_int(c.m_val_int),
		  m_val_double(c.m_val_double), m_val(c.m_val), m_scope(GLOBAL) {
}

VarBase::VarBase(bool b)
		: m_have_bool(true), m_have_int(false), m_have_double(false),
		  m_have_string(true), m_val_bool(b), m_val_int(0), m_val_double(0.0),
		  m_scope(GLOBAL) {
	m_val = (b ? "true" : "false");
}

VarBase::VarBase(int i)
		: m_have_bool(false), m_have_int(true), m_have_double(false),
		  m_have_string(true), m_val_bool(false), m_val_int(i), m_val_double(0.0),
		  m_val(std::to_string(i)), m_scope(GLOBAL) {
}


VarBase::VarBase(double d)
		: m_have_bool(false), m_have_int(false), m_have_double(true),
		  m_have_string(true), m_val_bool(false), m_val_int(0), m_val_double(d),
		  m_val(std::to_string(d)), m_scope(GLOBAL) {
}

VarBase::VarBase(std::string s)
		: m_have_bool(false), m_have_int(false), m_have_double(false),
		  m_have_string(true), m_val_bool(false), m_val_int(0), m_val_double(0.0),
		  m_val(std::move(s)), m_scope(GLOBAL) {
}

VarBase::VarBase(const char* s)
		: m_have_bool(false), m_have_int(false), m_have_double(false),
		  m_have_string(true), m_val_bool(false), m_val_int(0), m_val_double(0.0),
		  m_val(s), m_scope(GLOBAL) {
}

VarBase::~VarBase() = default;

std::ostream& operator<<(std::ostream& out, const VarBase& v) {
	for (char i: v.m_val) {
		if (i == '"' || i == '\\') {
			out << '\\';
		}
		out << i;
	}
	return out;
}

bool operator==(const VarBase& one, const VarBase& two) {
	return one.m_val == two.m_val;
	// scope is explicitly excluded as its nothing to do with value comparisons

}

bool operator!=(const VarBase& one, const VarBase& two) {
	return !(one == two);
}

VarBase& VarBase::operator=(const VarBase& c) {
	if (&c == this) return (*this);
	m_have_bool = c.m_have_bool;
	m_have_int = c.m_have_int;
	m_have_double = c.m_have_double;
	m_have_string = c.m_have_string;
	m_val_bool = c.m_val_bool;
	m_val_int = c.m_val_int;
	m_val_double = c.m_val_double;
	m_val = c.m_val;
	m_scope = c.m_scope;
	return (*this);
}

VarBase& VarBase::operator=(bool b) {
	m_have_bool = true;
	m_have_int = false;
	m_have_double = false;
	m_have_string = true;
	m_val_bool = b;
	m_val_int = 0;
	m_val_double = 0.0;
	m_val = (b ? "true" : "false");
	m_scope = INSTANCE;
	return (*this);
}

VarBase& VarBase::operator=(int i) {
	m_have_bool = false;
	m_have_int = true;
	m_have_double = false;
	m_have_string = true;
	m_val_bool = false;
	m_val_int = i;
	m_val_double = 0.0;
	m_val = std::to_string(i);
	m_scope = INSTANCE;
	return (*this);
}

VarBase& VarBase::operator=(double d) {
	m_have_bool = false;
	m_have_int = false;
	m_have_double = true;
	m_have_string = true;
	m_val_bool = false;
	m_val_int = 0;
	m_val_double = d;
	m_val = std::to_string(d);
	m_scope = INSTANCE;
	return (*this);
}

VarBase& VarBase::operator=(const std::string& s) {
	m_have_bool = false;
	m_have_int = false;
	m_have_double = false;
	m_have_string = true;
	m_val_bool = false;
	m_val_int = 0;
	m_val_double = 0.0;
	m_val = s;
	m_scope = INSTANCE;
	return (*this);
}

VarBase& VarBase::operator=(const char* s) {
        m_have_bool = false;
        m_have_int = false;
        m_have_double = false;
        m_have_string = true;
        m_val_bool = false;
        m_val_int = 0;
        m_val_double = 0.0;
        m_val = s;
        m_scope = INSTANCE;
        return (*this);
}

VarBase::operator bool() const {
        if (!m_have_bool) {
                bool parsed_value = false;
                if (try_parse_bool(m_val, parsed_value)) {
                        m_val_bool = parsed_value;
                } else {
                        m_val_bool = false;
                }
                m_have_bool = true;
        }
        return m_val_bool;
}

VarBase::operator int() const {
	if (!m_have_int) {
		m_val_int = std::stoi(m_val);
		m_have_int = true;
	}
	return m_val_int;
}

VarBase::operator double() const {
	if (!m_have_double) {
		m_val_double = std::stod(m_val);
		m_have_double = true;
	}
	return m_val_double;
}

VarBase::operator std::string() const {
	return m_val;
}

bool VarBase::is_bool() {
        if (!is_string()) return false;
        bool parsed_value = false;
        return try_parse_bool(m_val, parsed_value);
}

bool VarBase::is_int() {
        if (!is_string()) return false;
        if (m_val.empty()) return false;

        std::size_t start = 0;
        if (m_val[0] == '+' || m_val[0] == '-') {
                if (m_val.size() == 1) {
                        return false;
                }
                start = 1;
        }

        for (std::size_t idx = start; idx < m_val.size(); ++idx) {
                if (!std::isdigit(static_cast<unsigned char>(m_val[idx]))) {
                        return false;
                }
        }

        return true;
}

bool VarBase::is_double() {
	if (!is_string()) return false;

	char* p;

	// strtod() points p to the first character
	// in the string that doesn't look like
	// part of a double
	strtod(m_val.c_str(), &p); //-V530

	return p == m_val.c_str() + m_val.size();
}

bool VarBase::is_string() {
	return m_have_string;
}

Variable::Variable(const Variable& c) : VarPtr(static_cast<const VarPtr&>(c)) {

}


Variable::~Variable() = default;

Variable& Variable::operator=(const Variable& c) = default;

Variable& Variable::operator=(VarBase* vb) {
	VarPtr::operator=(vb);
	return *this;
}

Variable& Variable::operator=(const bool b) {
	VarPtr::operator=(new VarBase(b));
	return *this;
}

Variable& Variable::operator=(const int i) {
	VarPtr::operator=(new VarBase(i));
	return *this;
}

Variable& Variable::operator=(const double d) {
	VarPtr::operator=(new VarBase(d));
	return *this;
}

Variable& Variable::operator=(const std::string& s) {
	VarPtr::operator=(new VarBase(s));
	return *this;
}

Variable& Variable::operator=(const char* s) {
	VarPtr::operator=(new VarBase(s));
	return *this;
}

} // namespace varconf
