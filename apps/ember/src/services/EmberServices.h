/*
 Copyright (C) 2002  Hans Häggström
 Copyright (C) 2005	Erik Ogenvik

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

#ifndef EMBER_EMBERSERVICES_H
#define EMBER_EMBERSERVICES_H

#include <memory>

namespace Ember {

// some forward declarations before we start
class ConfigService;

class MetaserverService;

class ServerService;

class SoundService;

class ScriptingService;

class ServerSettings;


/**
 * Aggregates instances of the various Ember services.
 *
 * Service instances are supplied externally, allowing for dependency injection
 * and improved testability.
 *
 * Example usage:
 * @code
 *   auto services = EmberServices(config,
 *                                 std::make_unique<ScriptingService>(),
 *                                 std::make_unique<SoundService>(config),
 *                                 std::make_unique<ServerService>(session),
 *                                 std::make_unique<MetaserverService>(session, config),
 *                                 std::make_unique<ServerSettings>());
 * @endcode
 */
struct EmberServices {

        EmberServices(ConfigService& configService,
                     std::unique_ptr<ScriptingService> scriptingService,
                     std::unique_ptr<SoundService> soundService,
                     std::unique_ptr<ServerService> serverService,
                     std::unique_ptr<MetaserverService> metaserverService,
                     std::unique_ptr<ServerSettings> serverSettingsService);

        ~EmberServices();

        ConfigService& configService;

        std::unique_ptr<ScriptingService> scriptingService;
        std::unique_ptr<SoundService> soundService;
        std::unique_ptr<ServerService> serverService;
        std::unique_ptr<MetaserverService> metaserverService;
        std::unique_ptr<ServerSettings> serverSettingsService;

};
}

#endif
