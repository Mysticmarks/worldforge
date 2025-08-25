/*
 Copyright (C) 2012 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "TinyXmlCodec.h"

#include <stack>
#include <stdexcept>

namespace Ember {

class AtlasXmlVisitor : public TiXmlVisitor {
protected:
	TiXmlNode& mRootNode;
	Atlas::Bridge& mBridge;

	enum State {
		PARSE_NOTHING, PARSE_STREAM, PARSE_MAP, PARSE_LIST, PARSE_INT, PARSE_FLOAT, PARSE_STRING
	};

	std::stack<State> mState;
	std::string mData;

	std::string mName;

public:

	AtlasXmlVisitor(TiXmlNode& doc, Atlas::Bridge& bridge) :
			mRootNode(doc), mBridge(bridge) {
		mState.push(PARSE_NOTHING);
	}

	~AtlasXmlVisitor() override = default;

	bool VisitEnter(const TiXmlElement& element, const TiXmlAttribute* firstAttribute) override {
		const std::string& tag = element.ValueStr();
		switch (mState.top()) {
			case PARSE_NOTHING:
                                if (tag == "atlas") {
                                        mBridge.streamBegin();
                                        mState.push(PARSE_STREAM);
                                } else {
                                        // Unexpected tag outside root; halt parsing
                                        throw std::runtime_error("Unexpected tag '" + tag + "'");
                                }
                                break;

			case PARSE_STREAM:
                                if (tag == "map") {
                                        mBridge.streamMessage();
                                        mState.push(PARSE_MAP);
                                } else {
                                        // Invalid tag in stream; stop processing
                                        throw std::runtime_error("Unexpected tag '" + tag + "' in stream");
                                }
                                break;

			case PARSE_MAP: {
				const std::string* name = element.Attribute(std::string("name"));
				mName = *name;
                                if (tag == "map") {
                                        mBridge.mapMapItem(*name);
                                        mState.push(PARSE_MAP);
                                } else if (tag == "list") {
                                        mBridge.mapListItem(*name);
                                        mState.push(PARSE_LIST);
                                } else if (tag == "int") {
                                        mState.push(PARSE_INT);
                                } else if (tag == "float") {
                                        mState.push(PARSE_FLOAT);
                                } else if (tag == "string") {
                                        mState.push(PARSE_STRING);
                                } else {
                                        // Unknown child tag within map; abort parsing
                                        throw std::runtime_error("Unexpected tag '" + tag + "' in map");
                                }
                        }
                                break;

			case PARSE_LIST:
                                if (tag == "map") {
                                        mBridge.listMapItem();
                                        mState.push(PARSE_MAP);
                                } else if (tag == "list") {
                                        mBridge.listListItem();
                                        mState.push(PARSE_LIST);
                                } else if (tag == "int") {
                                        mState.push(PARSE_INT);
                                } else if (tag == "float") {
                                        mState.push(PARSE_FLOAT);
                                } else if (tag == "string") {
                                        mState.push(PARSE_STRING);
                                } else {
                                        // Unsupported list item; throw to halt visitor
                                        throw std::runtime_error("Unexpected tag '" + tag + "' in list");
                                }
                                break;

                        case PARSE_INT:
                        case PARSE_FLOAT:
                        case PARSE_STRING:
                                // Nested elements inside value types are invalid; stop parsing
                                throw std::runtime_error("Unexpected nested tag '" + tag + "'");
                                break;
		}
		return true;
	}

	/// Visit an element.
	bool VisitExit(const TiXmlElement& element) override {
		const std::string& tag = element.ValueStr();
		switch (mState.top()) {
                        case PARSE_NOTHING:
                                // Closing tag without matching open; abort
                                throw std::runtime_error("Unexpected closing tag '" + tag + "'");
                                break;

			case PARSE_STREAM:
                                if (tag == "atlas") {
                                        mBridge.streamEnd();
                                        mState.pop();
                                } else {
                                        // Unknown stream closing tag; halt
                                        throw std::runtime_error("Unexpected closing tag '" + tag + "' in stream");
                                }
                                break;

			case PARSE_MAP:
                                if (tag == "map") {
                                        mBridge.mapEnd();
                                        mState.pop();
                                } else {
                                        // Mismatched map closing tag; raise error
                                        throw std::runtime_error("Unexpected closing tag '" + tag + "' in map");
                                }
                                break;

			case PARSE_LIST:
                                if (tag == "list") {
                                        mBridge.listEnd();
                                        mState.pop();
                                } else {
                                        // Wrong list closing tag; stop parsing
                                        throw std::runtime_error("Unexpected closing tag '" + tag + "' in list");
                                }
                                break;

			case PARSE_INT:
                                if (tag == "int") {
                                        mState.pop();
                                        if (mState.top() == PARSE_MAP) {
                                                mBridge.mapIntItem(mName, std::stoll(mData));
                                        } else {
                                                mBridge.listIntItem(std::stoll(mData));
                                        }
                                } else {
                                        // Mismatched int closing tag; abort
                                        throw std::runtime_error("Unexpected closing tag '" + tag + "' for int");
                                }
                                break;

			case PARSE_FLOAT:
                                if (tag == "float") {
                                        mState.pop();
                                        if (mState.top() == PARSE_MAP) {
                                                mBridge.mapFloatItem(mName, std::stod(mData));
                                        } else {
                                                mBridge.listFloatItem(std::stod(mData));
                                        }
                                } else {
                                        // Mismatched float closing tag; abort
                                        throw std::runtime_error("Unexpected closing tag '" + tag + "' for float");
                                }
                                break;

			case PARSE_STRING:
                                if (tag == "string") {
                                        mState.pop();
                                        if (mState.top() == PARSE_MAP) {
                                                mBridge.mapStringItem(mName, mData);
                                        } else {
                                                mBridge.listStringItem(mData);
                                        }
                                } else {
                                        // Mismatched string closing tag; abort
                                        throw std::runtime_error("Unexpected closing tag '" + tag + "' for string");
                                }
				break;
		}

		return true;
	}

	bool Visit(const TiXmlText& text) override {
		mData = text.ValueStr();
		return true;
	}
};

TinyXmlCodec::TinyXmlCodec(TiXmlNode& rootElement, Atlas::Bridge& bridge) :
		mRootNode(rootElement), mBridge(bridge), mCurrentNode(nullptr) {
}

TinyXmlCodec::~TinyXmlCodec() = default;

void TinyXmlCodec::poll() {
	AtlasXmlVisitor visitor(mRootNode, mBridge);
	mRootNode.Accept(&visitor);
}

void TinyXmlCodec::streamBegin() {
	mCurrentNode = mRootNode.InsertEndChild(TiXmlElement("atlas"));
}

void TinyXmlCodec::streamEnd() {
	mCurrentNode = nullptr;
}

void TinyXmlCodec::streamMessage() {
	mCurrentNode = mCurrentNode->InsertEndChild(TiXmlElement("map"));
}

void TinyXmlCodec::mapMapItem(std::string name) {
	TiXmlElement element("map");
	element.SetAttribute("name", name);
	mCurrentNode = mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::mapListItem(std::string name) {
	TiXmlElement element("list");
	element.SetAttribute("name", name);
	mCurrentNode = mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::mapIntItem(std::string name, Atlas::Message::IntType data) {
	std::stringstream ss;
	ss << data;
	TiXmlElement element("int");
	element.InsertEndChild(TiXmlText(ss.str()));
	element.SetAttribute("name", name);
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::mapFloatItem(std::string name, Atlas::Message::FloatType data) {
	std::stringstream ss;
	ss << data;
	TiXmlElement element("float");
	element.InsertEndChild(TiXmlText(ss.str()));
	element.SetAttribute("name", name);
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::mapStringItem(std::string name, std::string data) {
	TiXmlElement element("string");
	element.InsertEndChild(TiXmlText(data));
	element.SetAttribute("name", name);
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::mapNoneItem(std::string name) {
	TiXmlElement element("none");
	element.SetAttribute("name", name);
	mCurrentNode->InsertEndChild(element);
}


void TinyXmlCodec::mapEnd() {
	mCurrentNode = mCurrentNode->Parent();
}

void TinyXmlCodec::listMapItem() {
	mCurrentNode = mCurrentNode->InsertEndChild(TiXmlElement("map"));
}

void TinyXmlCodec::listListItem() {
	mCurrentNode = mCurrentNode->InsertEndChild(TiXmlElement("list"));
}

void TinyXmlCodec::listIntItem(Atlas::Message::IntType data) {
	std::stringstream ss;
	ss << data;
	TiXmlElement element("int");
	element.InsertEndChild(TiXmlText(ss.str()));
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::listFloatItem(Atlas::Message::FloatType data) {
	std::stringstream ss;
	ss << data;
	TiXmlElement element("float");
	element.InsertEndChild(TiXmlText(ss.str()));
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::listStringItem(std::string data) {
	TiXmlElement element("string");
	element.InsertEndChild(TiXmlText(data));
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::listNoneItem() {
	TiXmlElement element("none");
	mCurrentNode->InsertEndChild(element);
}

void TinyXmlCodec::listEnd() {
	mCurrentNode = mCurrentNode->Parent();
}
}
