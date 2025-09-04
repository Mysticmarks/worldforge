# Metaserver

## Introduction
Welcome to the WorldForge Next Generation MetaServer!

The new MetaServer aims to have the following goals:

1. Maintain backwards compatibility with the existing WorldForge MetaServer protocol/libraries
2. Provide additional features beyond the current MetaServer capabilities
3. Provide a robust framework that would facilitate future development
4. Serve as an evaluation of multiple boost libraries for inclusion into the WorldForge codebase (most specifically targeted was boost::asio).

Code: https://github.com/worldforge/worldforge  
Bugs / Features: https://launchpad.net/metaserver-ng

This is as always, a work-in-progress. If you have questions / comments / suggestions, please feel free to contact any of the WorldForge folks as detailed on <http://worldforge.org>.

## Installation
The MetaServer is built with the same CMake and Conan workflow used by the rest of WorldForge.

### Dependency setup

```bash
conan profile detect --force
conan remote add worldforge https://artifactory.ogenvik.org/artifactory/api/conan/conan-local
conan install . --build missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
```

### Build and installation

```bash
cmake --preset conan-release -DBUILD_METASERVER_SERVER=ON -DCMAKE_INSTALL_PREFIX=./build/install/release
cmake --build --preset conan-release -j --target metaserver-ng
cmake --build --preset conan-release -j --target install
```

The MetaServer server is disabled by default. Enable it by passing `-DBUILD_METASERVER_SERVER=ON` to the CMake configuration step. To build only the MetaServer server, disable the main client and server components during dependency setup:

```bash
conan install . -o Worldforge/*:with_client=False -o Worldforge/*:with_server=False --build missing \
    -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
```

Tests can be run with:

```bash
cmake --build --preset conan-release --target check
```

## MetaServer Overview
The MetaServer Next Generation is a generic lobby type server. The main services that it provides are:

1. Maintain a list of currently available servers, and their corresponding attributes
2. Maintain a list of currently connected clients, and their corresponding attributes
3. Provide mechanism for servers to update, and clients to query this information
4. Provide delegated authentication, if requested by the connecting server

## Feature Roadmap
Features / Fixes can be brought up as either blueprints (new features), or bugs (abhorrent behaviour) at https://launchpad.net/metaserver-ng.

The current list of planned features (in no specific order):

- binary packet logging (partially implemented)
- replay binary packet log
- delegate authentication
- encryption of comms
- context sensitive listreq (in layman terms, the client can set a filter once on any server attribute, and all list requests are constrained by those filters)
- client to client game selection
- automatic client:server matching
- webui/utility to modify runtime configuration
- scoreboard session list
- admin interface to query/effect data:
  - kill a session
  - create a session
  - query session list
- serve up internal data structure as a DNS request

## MetaServer Configuration
[sections] - high level organisation of configuration options  
key=value  - specific option

Unknown options in the configuration file are ignored, and incorrectly formatted values raise a fatal exception on startup.

In the case of a mismatch, the `metaserver-ng.conf` file contains the authoritative list of default values, and running the metaserver with the `--help` option will display the authoritative list of accepted options.

The majority of options are self explanatory. Some options will be given a description as the purpose may be unclear.

Example:

```ini
[server]
port=8453
ip=0.0.0.0
daemon=true
logfile=~/.metaserver-ng/metaserver-ng.log
client_stats=true
server_stats=true
```

## MetaServer Protocol
Refer to `PROTOCOL` document.

## Contributors
All contributors will be listed in the `AUTHORS` file.

-- Sean Ryan <sryan@evercrack.com>
