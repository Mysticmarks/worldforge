/*
 Copyright (C) 2010 Erik Ogenvik <erik@ogenvik.org>

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

#include "TransferInfoStringSerializer.h"
#include "AvatarTransferInfo.h"

#include "framework/Log.h"

#include <Atlas/Message/Element.h>
#include <Atlas/Message/MEncoder.h>
#include <Atlas/Message/QueuedDecoder.h>
#include <Atlas/Codecs/Bach.h>
#include <Atlas/Formatter.h>
#include <cstdint>

namespace Ember {

bool TransferInfoStringSerializer::serialize(const TransferInfoStore& infoObjects, std::iostream& ostream) {
	Atlas::Message::ListType infos;
	for (const auto& transferInfo: infoObjects) {
		Atlas::Message::MapType info;
		info["host"] = transferInfo.getTransferInfo().getHost();
		info["port"] = transferInfo.getTransferInfo().getPort();
		info["key"] = transferInfo.getTransferInfo().getPossessKey();
		info["entityid"] = transferInfo.getTransferInfo().getPossessEntityId();
		info["avatarname"] = transferInfo.getAvatarName();
		info["timestamp"] = (transferInfo.getTimestamp() - WFMath::TimeStamp::epochStart()).milliseconds();
		infos.emplace_back(info);
	}

	Atlas::Message::QueuedDecoder decoder;
	Atlas::Codecs::Bach codec(ostream, ostream, decoder);

	Atlas::Message::Encoder encoder(codec);

	Atlas::Message::MapType map;
	map["teleports"] = infos;
	Atlas::Formatter formatter(ostream, codec);
	formatter.streamBegin();
	encoder.streamMessageElement(map);
	formatter.streamEnd();

	return true;
}

bool TransferInfoStringSerializer::deserialize(TransferInfoStore& infoObjects, std::iostream& istream) {
	try {
		Atlas::Message::QueuedDecoder decoder;

		istream.seekg(0);
		Atlas::Codecs::Bach codec(istream, istream, decoder);
		// Read whole stream into decoder queue
		while (!istream.eof()) {
			codec.poll();
		}

		// Read decoder queue
		if (decoder.queueSize() > 0) {

			Atlas::Message::MapType map = decoder.popMessage();
			auto I = map.find("teleports");
			if (I != map.end()) {
				if (I->second.isList()) {
					Atlas::Message::ListType infos = I->second.asList();
					for (auto& infoElement: infos) {
						if (infoElement.isMap()) {
							Atlas::Message::MapType info = infoElement.asMap();
							const std::string& host = info["host"].asString();
							auto port = info["port"].asInt();
                                                        const std::string& key = info["key"].asString();
                                                        const std::string& entityId = info["entityid"].asString();
                                                        const std::string& avatarName = info["avatarname"].asString();
                                                        std::int64_t timestamp = info["timestamp"].asInt();
                                                        infoObjects.emplace_back(avatarName, WFMath::TimeStamp::epochStart() + WFMath::TimeDiff(timestamp), Eris::TransferInfo(host, (int) port, key, entityId));
						}
					}
				}
			}
		}
	} catch (const std::exception& ex) {
		logger->error("Couldn't deserialize transfer info objects:", ex.what());
		return false;
	}
	logger->debug("Read {} transfer info objects from storage.", infoObjects.size());
	return true;
}

}
