// This file may be redistributed and modified only under the terms of
// the GNU Lesser General Public License (See COPYING for details).
// Copyright (C) 2000 Stefanus Du Toit

// $Id$

#include <Atlas/Message/DecoderBase.h>

#include <Atlas/Message/Element.h>

#include <Atlas/Debug.h>
#include <Atlas/Exception.h>

#include <iostream>

#include <cassert>


namespace Atlas::Message {

DecoderBase::DecoderBase() {
}

void DecoderBase::streamBegin() {
	ATLAS_DEBUG(std::cout << "DecoderBase::streamBegin" << std::endl)
	m_state.push(STATE_STREAM);
}

void DecoderBase::streamMessage() {
	ATLAS_DEBUG(std::cout << "DecoderBase::streamMessage" << std::endl)
	m_maps.emplace();
	m_state.push(STATE_MAP);
}

void DecoderBase::streamEnd() {
	ATLAS_DEBUG(std::cout << "DecoderBase::streamEnd" << std::endl)
	assert(!m_state.empty());
	m_state.pop();
}

void DecoderBase::mapMapItem(std::string name) {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapMapItem Map" << std::endl)
	m_names.push(std::move(name));
	m_maps.emplace();
	m_state.push(STATE_MAP);
}

void DecoderBase::mapListItem(std::string name) {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapListItem List" << std::endl)
	m_names.push(std::move(name));
	m_lists.emplace();
	m_state.push(STATE_LIST);
}

void DecoderBase::mapIntItem(std::string name, std::int64_t i) {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapIntItem" << std::endl)
	assert(!m_maps.empty());
	m_maps.top().emplace(std::move(name), i);
}

void DecoderBase::mapFloatItem(std::string name, double d) {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapFloatItem" << std::endl)
	assert(!m_maps.empty());
	m_maps.top().emplace(std::move(name), d);
}

void DecoderBase::mapStringItem(std::string name, std::string s) {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapStringItem" << std::endl)
	assert(!m_maps.empty());
	m_maps.top().emplace(std::move(name), std::move(s));
}

void DecoderBase::mapNoneItem(std::string name) {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapNoneItem" << std::endl)
	assert(!m_maps.empty());
	m_maps.top().emplace(std::move(name), Atlas::Message::Element());
}

void DecoderBase::mapEnd() {
	ATLAS_DEBUG(std::cout << "DecoderBase::mapEnd" << std::endl)
	assert(!m_maps.empty());
	assert(!m_state.empty());
	m_state.pop();
	switch (m_state.top()) {
		case STATE_MAP: {
			MapType map = std::move(m_maps.top());
			m_maps.pop();
			assert(!m_maps.empty());
			assert(!m_names.empty());
			m_maps.top().emplace(std::move(m_names.top()), std::move(map));
			m_names.pop();
		}
			break;
		case STATE_LIST: {
			assert(!m_lists.empty());
			m_lists.top().emplace_back(std::move(m_maps.top()));
			m_maps.pop();
		}
			break;
		case STATE_STREAM: {
			messageArrived(std::move(m_maps.top()));
			m_maps.pop();
		}
			break;
		default: {
			// MapType map = m_maps.top();
			m_maps.pop();
		}
			break;
	}
}

void DecoderBase::listMapItem() {
	ATLAS_DEBUG(std::cout << "DecoderBase::listMapItem" << std::endl)
	m_maps.emplace();
	m_state.push(STATE_MAP);
}

void DecoderBase::listListItem() {
	ATLAS_DEBUG(std::cout << "DecoderBase::listListItem" << std::endl)
	m_lists.emplace();
	m_state.push(STATE_LIST);
}

void DecoderBase::listIntItem(std::int64_t i) {
	ATLAS_DEBUG(std::cout << "DecoderBase::listIntItem" << std::endl)
	assert(!m_lists.empty());
	m_lists.top().emplace_back(i);
}

void DecoderBase::listFloatItem(double d) {
	ATLAS_DEBUG(std::cout << "DecoderBase::listFloatItem" << std::endl)
	m_lists.top().emplace_back(d);
}

void DecoderBase::listStringItem(std::string s) {
	ATLAS_DEBUG(std::cout << "DecoderBase::listStringItem" << std::endl)
	assert(!m_lists.empty());
	m_lists.top().emplace_back(std::move(s));
}

void DecoderBase::listNoneItem() {
	ATLAS_DEBUG(std::cout << "DecoderBase::listNoneItem" << std::endl)
	assert(!m_lists.empty());
	m_lists.top().emplace_back();
}

void DecoderBase::listEnd() {
	ATLAS_DEBUG(std::cout << "DecoderBase::listEnd" << std::endl)
	assert(!m_lists.empty());
	assert(!m_state.empty());
	m_state.pop();
	switch (m_state.top()) {
		case STATE_MAP:
			assert(!m_maps.empty());
			assert(!m_names.empty());
			m_maps.top().emplace(m_names.top(), std::move(m_lists.top()));
			m_names.pop();
			m_lists.pop();
			break;
		case STATE_LIST: {
			ListType list = std::move(m_lists.top());
			m_lists.pop();
			assert(!m_lists.empty());
			m_lists.top().emplace_back(std::move(list));
		}
			break;
                case STATE_STREAM:
                        m_lists.pop();
                        throw Atlas::Exception("DecoderBase::listEnd encountered unexpected STATE_STREAM");
	}
}

}
// namespace Atlas::Message
