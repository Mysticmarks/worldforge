// This file may be redistributed and modified under the terms of the
// GNU Lesser General Public License (See COPYING for details).
// Copyright (C) 2000-2001 Michael Day, Stefanus Du Toit

// $Id$

#include "Atlas/Codecs/XML.h"
#include "Atlas/Message/Element.h"

#include <iostream>
#include <stdexcept>

namespace Atlas::Codecs {

XML::XML(std::istream& in, std::ostream& out, Atlas::Bridge& b)
		: m_istream(in), m_ostream(out), m_bridge(b) {
	m_token = TOKEN_DATA;
	m_state.push(PARSE_NOTHING);
	m_data.emplace("");
}

void XML::tokenTag(char next) {
	m_tag.erase();

	switch (next) {
		case '/':
			m_token = TOKEN_END_TAG;
			break;

                case '>':
                        throw std::runtime_error("XML parse error: unexpected '>' in tag token");

		default:
			m_token = TOKEN_START_TAG;
			m_tag += next;
			break;
	}
}

void XML::tokenStartTag(char next) {
        switch (next) {
                case '<':
                        throw std::runtime_error("XML parse error: unexpected '<' in start tag");

		case '>':
			parseStartTag();
			m_token = TOKEN_DATA;
			m_data.emplace("");
			break;

		case '/':
			parseStartTag();
			m_token = TOKEN_END_TAG;
			m_data.emplace("");
			break;

		default:
			m_tag += next;
			break;
	}
}

void XML::tokenEndTag(char next) {
        switch (next) {
                case '<':
                        throw std::runtime_error("XML parse error: unexpected '<' in end tag");

		case '>':
			parseEndTag();
			m_token = TOKEN_DATA;
			m_data.pop();
			break;

		default:
			m_tag += next;
			break;
	}
}

void XML::tokenData(char next) {
        switch (next) {
                case '<':
                        m_token = TOKEN_TAG;
                        break;

                case '>':
                        throw std::runtime_error("XML parse error: unexpected '>' in data");

		default:
			m_data.top() += next;
			break;
	}
}

void XML::parseStartTag() {
	int tag_end = (int) m_tag.find(' ');
	int name_start = (int) m_tag.find("name=\"") + 6;
	int name_end = (int) m_tag.rfind('\"');

	if (name_start < name_end) {
		m_name = unescape(
				std::string(m_tag, (unsigned long) name_start, (unsigned long) (name_end - name_start)));
	} else {
		m_name.erase();
	}

	m_tag = std::string(m_tag, 0, (unsigned long) tag_end);

	switch (m_state.top()) {
                case PARSE_NOTHING:
                        if (m_tag == "atlas") {
                                m_bridge.streamBegin();
                                m_state.push(PARSE_STREAM);
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' while not parsing");
                        }
                        break;

                case PARSE_STREAM:
                        if (m_tag == "map") {
                                m_bridge.streamMessage();
                                m_state.push(PARSE_MAP);
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in stream");
                        }
                        break;

                case PARSE_MAP:
			if (m_tag == "map") {
				m_bridge.mapMapItem(m_name);
				m_state.push(PARSE_MAP);
			} else if (m_tag == "list") {
				m_bridge.mapListItem(m_name);
				m_state.push(PARSE_LIST);
			} else if (m_tag == "int") {
				m_state.push(PARSE_INT);
			} else if (m_tag == "float") {
				m_state.push(PARSE_FLOAT);
			} else if (m_tag == "string") {
				m_state.push(PARSE_STRING);
			} else if (m_tag == "none") {
				m_state.push(PARSE_NONE);
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in map");
                        }
                        break;

                case PARSE_LIST:
			if (m_tag == "map") {
				m_bridge.listMapItem();
				m_state.push(PARSE_MAP);
			} else if (m_tag == "list") {
				m_bridge.listListItem();
				m_state.push(PARSE_LIST);
			} else if (m_tag == "int") {
				m_state.push(PARSE_INT);
			} else if (m_tag == "float") {
				m_state.push(PARSE_FLOAT);
			} else if (m_tag == "string") {
				m_state.push(PARSE_STRING);
			} else if (m_tag == "none") {
				m_state.push(PARSE_NONE);
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in list");
                        }
                        break;

                case PARSE_INT:
                case PARSE_FLOAT:
                case PARSE_STRING:
                case PARSE_NONE:
                        throw std::runtime_error("XML parse error: unexpected tag inside value");
                        break;
        }
}

void XML::parseEndTag() {
        switch (m_state.top()) {
                case PARSE_NOTHING:
                        throw std::runtime_error("XML parse error: unexpected end tag '" + m_tag + "'");

                case PARSE_STREAM:
                        if (m_tag == "atlas") {
                                m_bridge.streamEnd();
                                m_state.pop();
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in stream end");
                        }
                        break;

                case PARSE_MAP:
			if (m_tag == "map") {
				m_bridge.mapEnd();
				m_state.pop();
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in map end");
                        }
                        break;

                case PARSE_LIST:
			if (m_tag == "list") {
				m_bridge.listEnd();
				m_state.pop();
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in list end");
                        }
                        break;

                case PARSE_INT:
                        if (m_tag == "int") {
                                m_state.pop();
				try {
					Atlas::Message::IntType value = 0;
					auto data = m_data.top();
					if (!data.empty()) {
						value = std::stol(data);
					}
					if (m_state.top() == PARSE_MAP) {
						m_bridge.mapIntItem(m_name, value);
					} else {
						m_bridge.listIntItem(value);
					}
                                } catch (...) {
                                        //Could not parse long; just ignore
                                }
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in int end");
                        }
                        break;

                case PARSE_FLOAT:
                        if (m_tag == "float") {
				m_state.pop();
				try {
					Atlas::Message::FloatType value = 0;
					auto data = m_data.top();
					if (!data.empty()) {
						value = std::stod(data);
					}
					if (m_state.top() == PARSE_MAP) {
						m_bridge.mapFloatItem(m_name, value);
					} else {
						m_bridge.listFloatItem(value);
					}
                                } catch (...) {
                                        //Could not parse double; just ignore.
                                }
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in float end");
                        }
                        break;

                case PARSE_STRING:
                        if (m_tag == "string") {
				m_state.pop();
				if (m_state.top() == PARSE_MAP) {
					m_bridge.mapStringItem(m_name, unescape(m_data.top()));
				} else {
					m_bridge.listStringItem(unescape(m_data.top()));
				}
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in string end");
                        }
                        break;
                case PARSE_NONE:
                        if (m_tag == "none") {
				m_state.pop();
				if (m_state.top() == PARSE_MAP) {
					m_bridge.mapNoneItem(m_name);
				} else {
					m_bridge.listNoneItem();
				}
                        } else {
                                throw std::runtime_error("XML parse error: unexpected tag '" + m_tag + "' in none end");
                        }
                        break;
        }
}

void XML::poll() {
	m_istream.peek();

	std::streamsize count;

	while ((count = m_istream.rdbuf()->in_avail()) > 0) {

		for (std::streamsize i = 0; i < count; ++i) {

			char next = (char)m_istream.rdbuf()->sbumpc();

			switch (m_token) {
				case TOKEN_TAG:
					tokenTag(next);
					break;
				case TOKEN_START_TAG:
					tokenStartTag(next);
					break;
				case TOKEN_END_TAG:
					tokenEndTag(next);
					break;
				case TOKEN_DATA:
					tokenData(next);
					break;
			}
		}
	}
}

void XML::streamBegin() {
	m_ostream << "<atlas>";
}

void XML::streamEnd() {
	m_ostream << "</atlas>";
}

void XML::streamMessage() {
	m_ostream << "<map>";
}

void XML::mapMapItem(std::string name) {
	m_ostream << "<map name=\"" << escape(name) << "\">";
}

void XML::mapListItem(std::string name) {
	m_ostream << "<list name=\"" << escape(name) << "\">";
}

void XML::mapIntItem(std::string name, std::int64_t data) {
	m_ostream << "<int name=\"" << escape(name) << "\">" << data << "</int>";
}

void XML::mapFloatItem(std::string name, double data) {
	m_ostream << "<float name=\"" << escape(name) << "\">" << data << "</float>";
}

void XML::mapStringItem(std::string name, std::string data) {
	m_ostream << "<string name=\"" << escape(name) << "\">" << escape(data) << "</string>";
}

void XML::mapNoneItem(std::string name) {
	m_ostream << "<none name=\"" << escape(name) << "\"></none>";
}

void XML::mapEnd() {
	m_ostream << "</map>";
}

void XML::listMapItem() {
	m_ostream << "<map>";
}

void XML::listListItem() {
	m_ostream << "<list>";
}

void XML::listIntItem(std::int64_t data) {
	m_ostream << "<int>" << data << "</int>";
}

void XML::listFloatItem(double data) {
	m_ostream << "<float>" << data << "</float>";
}

void XML::listStringItem(std::string data) {
	m_ostream << "<string>" << escape(data) << "</string>";
}

void XML::listNoneItem() {
	m_ostream << "<none></none>";
}

void XML::listEnd() {
	m_ostream << "</list>";
}

std::string XML::escape(const std::string& original) {
	std::string buffer;
	buffer.reserve(original.size() + (original.size() / 2));
	for (char pos : original) {
		switch (pos) {
			case '&':
				buffer.append("&amp;");
				break;
			case '\"':
				buffer.append("&quot;");
				break;
			case '\'':
				buffer.append("&apos;");
				break;
			case '<':
				buffer.append("&lt;");
				break;
			case '>':
				buffer.append("&gt;");
				break;
			default:
				buffer.append(1, pos);
				break;
		}
	}
	return buffer;
}

std::string XML::unescape(const std::string& original) {
	std::string buffer;
	buffer.reserve(original.size());
	for (size_t pos = 0; pos != original.size(); ++pos) {
		if (original[pos] == '&') {
			if (original.size() - pos >= 3) {
				if (original[pos + 1] == 'l' && original[pos + 2] == 't' && original[pos + 3] == ';') {
					buffer.append(1, '<');
					pos += 3;
					continue;
				} else if (original[pos + 1] == 'g' && original[pos + 2] == 't' && original[pos + 3] == ';') {
					buffer.append(1, '>');
					pos += 3;
					continue;
				}
			}
			if (original.size() - pos >= 4) {
				if (original[pos + 1] == 'a' && original[pos + 2] == 'm' && original[pos + 3] == 'p' &&
					original[pos + 4] == ';') {
					buffer.append(1, '&');
					pos += 4;
					continue;
				}
			}
			if (original.size() - pos >= 5) {
				if (original[pos + 1] == 'q' && original[pos + 2] == 'u' && original[pos + 3] == 'o' &&
					original[pos + 4] == 't' && original[pos + 5] == ';') {
					buffer.append(1, '"');
					pos += 5;
					continue;
				} else if (original[pos + 1] == 'a' && original[pos + 2] == 'p' && original[pos + 3] == 'o' &&
						   original[pos + 4] == 's' && original[pos + 5] == ';') {
					buffer.append(1, '\'');
					pos += 5;
					continue;
				}
			}
		}
		buffer.append(1, original[pos]);
	}
	return buffer;
}


}
//namespace Atlas::Codecs
