//
// C++ Implementation: OgreSetup
//
// Description:
//
//
// Author: Erik Ogenvik <erik@ogenvik.org>, (C) 2006
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.//
//

#include "OgreSetup.h"


#include "OgreInfo.h"
#include "MeshSerializerListener.h"
#include "ClusterLodStrategy.h"
#include <MeshLodGenerator/OgreMeshLodGenerator.h>

#include "services/config/ConfigService.h"
#include "services/config/ConfigListenerContainer.h"
#include "services/input/Input.h"

#include "framework/ConsoleBackend.h"
#include "framework/MainLoopController.h"
#include "Version.h"
#include <OgreBuildSettings.h>

#if OGRE_THREAD_SUPPORT == 0
#error OGRE must be built with thread support.
#endif

#include <RenderSystems/GL3Plus/OgreGLContext.h>

#ifdef _WIN32
#include "platform/platform_windows.h"
#endif

#include <OgreMeshManager.h>
#include <OgreStringConverter.h>
#include <Overlay/OgreOverlaySystem.h>
#include <OgreLogManager.h>
#include <OgreRoot.h>
#include <OgreLodStrategyManager.h>

#ifdef OGRE_STATIC_LIB

#include <Plugins/STBICodec/OgreSTBICodec.h>
#include <Plugins/ParticleFX/OgreParticleFXPlugin.h>
#include <RenderSystems/GL3Plus/OgreGL3PlusPlugin.h>

#endif

#include <SDL3/SDL.h>
#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <filesystem>
#include <memory>
#include <framework/Log.h>

namespace Ember::OgreView {

OgreSetup::OgreSetup() :
		DiagnoseOgre("diagnoseOgre", this, "Diagnoses the current Ogre state and writes the output to the log."),
		mRoot(std::make_unique<Ogre::Root>("", "", "")),
		mRenderWindow(nullptr),
		mMeshSerializerListener(std::make_unique<MeshSerializerListener>(true)),
		mOverlaySystem(std::make_unique<Ogre::OverlaySystem>()),
		mMeshLodGenerator(std::make_unique<Ogre::MeshLodGenerator>()),
		mConfigListenerContainer(std::make_unique<ConfigListenerContainer>()),
		mSaveShadersToCache(false) {


	logger->info("Compiled against Ogre version {}", OGRE_VERSION);

#if OGRE_DEBUG_MODE
	logger->info("Compiled against Ogre in debug mode.");
#else
	logger->info("Compiled against Ogre in release mode.");
#endif

#if OGRE_THREAD_SUPPORT == 0
	logger->info("Compiled against Ogre without threading support.");
#elif OGRE_THREAD_SUPPORT == 1
	logger->info("Compiled against Ogre with multi threading support.");
#elif OGRE_THREAD_SUPPORT == 2
	logger->info("Compiled against Ogre with semi threading support.");
#elif OGRE_THREAD_SUPPORT == 3
	logger->info("Compiled against Ogre with threading support without synchronization.");
#else
	logger->info("Compiled against Ogre with unknown threading support.");
#endif

#if OGRE_THREAD_PROVIDER == 0
	logger->info("Using no thread provider.");
#elif OGRE_THREAD_PROVIDER == 1
	logger->info("Using Boost thread provider.");
#elif OGRE_THREAD_PROVIDER == 2
	logger->info("Using Poco thread provider.");
#elif OGRE_THREAD_PROVIDER == 3
	logger->info("Using TBB thread provider.");
#elif OGRE_THREAD_PROVIDER == 4
	logger->info("Using STD thread provider.");
#else
	logger->info("Using unknown thread provider.");
#endif


	//Ownership of the queue instance is passed to Root.
//mRoot->setWorkQueue(OGRE_NEW EmberWorkQueue(MainLoopController::getSingleton().getEventService()));

#ifdef OGRE_STATIC_LIB
	Ogre::STBIImageCodec::startup();
	mPlugins.emplace_back(std::make_unique<Ogre::GL3PlusPlugin>());
	mPlugins.emplace_back(std::make_unique<Ogre::ParticleFXPlugin>());
	for (auto& plugin: mPlugins) {
		mRoot->installPlugin(plugin.get());
	}
#else
	mPluginLoader.loadPlugin("Codec_STBI");
	mPluginLoader.loadPlugin("Plugin_ParticleFX");
	mPluginLoader.loadPlugin("RenderSystem_GL3Plus"); //We'll use OpenGL on Windows too, to make it easier to develop
#endif

	auto renderSystem = mRoot->getAvailableRenderers().front();
	try {
		//Set the default resolution to 1280 x 720 unless overridden by the user.
		renderSystem->setConfigOption("Video Mode", "1280 x  720"); //OGRE stores the value with two spaces after "x".
	} catch (const std::exception& ex) {
		logger->warn("Could not set default resolution: {}", ex.what());
	}
	mRoot->setRenderSystem(renderSystem);

}

OgreSetup::~OgreSetup() {
	if (mMaterialsListener) {
		Ogre::MaterialManager::getSingleton().removeListener(mMaterialsListener.get());
	}

	logger->info("Shutting down Ogre.");
	if (mRoot) {


		if (Ogre::GpuProgramManager::getSingletonPtr()) {
			try {
				auto cachePath = ConfigService::getSingleton().getHomeDirectory(BaseDirType_CACHE) / ("gpu-" EMBER_VERSION ".cache");
				auto cacheStream = Ogre::Root::createFileStream(cachePath.string(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);
				if (cacheStream) {
					Ogre::GpuProgramManager::getSingleton().saveMicrocodeCache(cacheStream);
				}
			} catch (...) {
				logger->warn("Error when trying to save GPU cache file.");
			}
		}

		//This should normally not be needed, but there seems to be a bug in Ogre for Windows where it will hang if the render window isn't first detached.
		//The bug appears in Ogre 1.7.2.
		if (mRenderWindow) {
			mRoot->detachRenderTarget(mRenderWindow);
			mRoot->destroyRenderTarget(mRenderWindow);
			mRenderWindow = nullptr;
		}

		//Clean up, else it seems we can get error referencing gpu programs that are destroyed.
		Ogre::RTShader::ShaderGenerator::destroy();
#ifdef OGRE_STATIC_LIB
		Ogre::STBIImageCodec::shutdown();
#endif

	}
}

void OgreSetup::runCommand(const std::string& command, const std::string& args) {
	if (DiagnoseOgre == command) {
		std::stringstream ss;
		OgreInfo::diagnose(ss);
		logger->info(ss.str());
		ConsoleBackend::getSingleton().pushMessage("Ogre diagnosis information has been written to the log.", "info");
	}
}

void OgreSetup::saveConfig() {
	if (mRoot) {

		//Save renderer settings
		if (mRoot->getRenderSystem()) {
			auto configOptions = mRoot->getRenderSystem()->getConfigOptions();
			for (const auto& configOption: configOptions) {
				//Keys in varconf are mangled, so we store the entry with a ":" delimiter.
				ConfigService::getSingleton().setValue("renderer", configOption.second.name, configOption.second.name + ":" + configOption.second.currentValue);
			}
		}
	}
}

void OgreSetup::createOgreSystem() {
	auto& configSrv = ConfigService::getSingleton();

//	if (!configSrv.getPrefix().empty()) {
//		//We need to set the current directory to the prefix before trying to load Ogre.
//		//The reason for this is that Ogre loads a lot of dynamic modules, and in some build configuration
//		//(like AppImage) the lookup path for some of these are based on the installation directory of Ember.
//		if (chdir(configSrv.getPrefix().c_str())) {
//			logger->warn("Failed to change to the prefix directory '" << configSrv.getPrefix() << "'. Ogre loading might fail.");
//		}
//	}



	if (chdir(configSrv.getEmberDataDirectory().generic_string().c_str())) {
		logger->warn("Failed to change to the data directory '{}'.", configSrv.getEmberDataDirectory().string());
	}

}

void OgreSetup::configure() {
	mConfigListenerContainer->registerConfigListener("ogre", "loglevel", [](const std::string& section, const std::string& key, varconf::Variable& variable) {
		if (variable.is_string()) {
			auto string = variable.as_string();
			if (string == "trivial") {
				Ogre::LogManager::getSingleton().getDefaultLog()->setMinLogLevel(Ogre::LML_TRIVIAL);
			} else if (string == "normal") {
				Ogre::LogManager::getSingleton().getDefaultLog()->setMinLogLevel(Ogre::LML_NORMAL);
			} else if (string == "warning") {
				Ogre::LogManager::getSingleton().getDefaultLog()->setMinLogLevel(Ogre::LML_WARNING);
			} else if (string == "critical") {
				Ogre::LogManager::getSingleton().getDefaultLog()->setMinLogLevel(Ogre::LML_CRITICAL);
			}
		}
	}, true);

	auto& configService = ConfigService::getSingleton();

	// we start by trying to figure out what kind of resolution the user has selected, and whether full screen should be used or not.
	unsigned int height = 720, width = 1280; //default resolution unless user selects other
	bool fullscreen = false;

	try {

		auto rendererConfig = configService.getSection("renderer");
		for (const auto& entry: rendererConfig) {
			if (entry.second.is_string()) {
				try {
					//Keys in varconf are mangled, so we've stored the entry with a ":" delimiter.
					auto splits = Ogre::StringUtil::split(entry.second.as_string(), ":");
					if (splits.size() > 1) {
						mRoot->getRenderSystem()->setConfigOption(splits[0], splits[1]);
					}
				} catch (const std::exception& ex) {
					logger->warn("Got exception when trying to set setting: {}", ex.what());
				}
			}
		}

		auto validation = mRoot->getRenderSystem()->validateConfigOptions();
		if (!validation.empty()) {
			logger->warn("Possible issue when setting render system options: {}", validation);
		}

		parseWindowGeometry(mRoot->getRenderSystem()->getConfigOptions(), width, height, fullscreen);


        } catch (const std::exception& ex) {
                logger->error("Got exception when setting up OGRE: {}", ex.what());
        }

        if (configService.hasItem("graphics", "window_width")) {
                width = static_cast<unsigned int>(configService.getValue("graphics", "window_width"));
        }
        if (configService.hasItem("graphics", "window_height")) {
                height = static_cast<unsigned int>(configService.getValue("graphics", "window_height"));
        }

        bool handleOpenGL = false;
#ifdef __APPLE__
	handleOpenGL = true;
#endif

	std::string windowId = Input::getSingleton().createWindow(width, height, fullscreen, true, handleOpenGL);

	mRoot->initialise(false, "Ember");
	Ogre::NameValuePairList misc;
#ifdef __APPLE__
	misc["currentGLContext"] = Ogre::String("true");
	misc["macAPI"] = Ogre::String("cocoa");
#else
//We should use "externalWindowHandle" on Windows, and "parentWindowHandle" on Linux.
#ifdef _WIN32
	misc["externalWindowHandle"] = windowId;
#else
	misc["parentWindowHandle"] = windowId;
#endif
#endif

	mRenderWindow = mRoot->createRenderWindow("MainWindow", width, height, fullscreen, &misc);

	Input::getSingleton().EventSizeChanged.connect(sigc::mem_fun(*this, &OgreSetup::input_SizeChanged));

	registerOpenGLContextFix();

	if (mSaveShadersToCache) {
		Ogre::GpuProgramManager::getSingleton().setSaveMicrocodesToCache(true);

		auto cacheFilePath = configService.getHomeDirectory(BaseDirType_CACHE) / ("gpu-" EMBER_VERSION ".cache");
		if (std::ifstream(cacheFilePath.string()).good()) {
			try {
				if (auto cacheStream = Ogre::Root::openFileStream(cacheFilePath.string())) {
					Ogre::GpuProgramManager::getSingleton().loadMicrocodeCache(cacheStream);
				}
			} catch (...) {
				logger->warn("Error when trying to open GPU cache file.");
			}
		}
	}


	mRenderWindow->setActive(true);
	//We'll control the rendering ourselves and need to turn off the autoupdating.
	mRenderWindow->setAutoUpdated(false);
	mRenderWindow->setVisible(true);

	setStandardValues();
}

void OgreSetup::input_SizeChanged(unsigned int width, unsigned int height) {

//On Windows we can't tell the window to resize, since that will lead to an infinite loop of resize events (probably stemming from how Windows lacks a proper window manager).
#ifndef _WIN32
	mRenderWindow->resize(width, height);
#endif
	mRenderWindow->windowMovedOrResized();
}

void OgreSetup::setStandardValues() {

	// Set default animation mode
	Ogre::Animation::setDefaultInterpolationMode(Ogre::Animation::IM_SPLINE);

	//remove padding for bounding boxes
	//Ogre::MeshManager::getSingletonPtr()->setBoundsPaddingFactor(0);

	//all new movable objects shall by default be unpickable; it's up to the objects themselves to make themselves pickable
	Ogre::MovableObject::setDefaultQueryFlags(0);

	//Default to require tangents for all meshes. This could perhaps be turned off on platforms which has no use, like Android?
	Ogre::MeshManager::getSingleton().setListener(mMeshSerializerListener.get());

        // Register custom cluster-based LOD strategy
        Ogre::LodStrategy* lodStrategy = OGRE_NEW Lod::ClusterLodStrategy();
        Ogre::LodStrategyManager::getSingleton().addStrategy(lodStrategy);

	Ogre::RTShader::ShaderGenerator::initialize();

	struct GenerateShadersListener : Ogre::MaterialManager::Listener {
		Ogre::RTShader::ShaderGenerator* shaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();

		Ogre::Technique* handleSchemeNotFound(unsigned short schemeIndex,
											  const Ogre::String& schemeName,
											  Ogre::Material* originalMaterial,
											  unsigned short lodIndex,
											  const Ogre::Renderable* rend) override {

			Ogre::Technique* firstTech = originalMaterial->getTechnique(0);
			//If first pass already has fragment and vertex shaders, don't generate anything.
			if (firstTech->getPass(0)->hasVertexProgram() && firstTech->getPass(0)->hasFragmentProgram()) {
				return nullptr;
			}

			// Create shader generated technique for this material.
			bool techniqueCreated = shaderGenerator->createShaderBasedTechnique(
					*originalMaterial,
					Ogre::MaterialManager::DEFAULT_SCHEME_NAME,
					schemeName);

			if (!techniqueCreated) {
				logger->warn("Could not create RTShader tech for material {}.", originalMaterial->getName());
				return nullptr;
			}
			// Case technique registration succeeded.

			logger->debug("Created auto generated shaders for material {}", originalMaterial->getName());

			// Force creating the shaders for the generated technique.
			shaderGenerator->validateMaterial(schemeName, originalMaterial->getName(), originalMaterial->getGroup());

			// Grab the generated technique.
			Ogre::Material::Techniques::const_iterator it;
			for (it = originalMaterial->getTechniques().begin(); it != originalMaterial->getTechniques().end(); ++it) {
				Ogre::Technique* curTech = *it;

				if (curTech->getSchemeName() == schemeName) {
					return curTech;
				}
			}

			return nullptr;
		}

		bool afterIlluminationPassesCreated(Ogre::Technique* tech) override {
			if (tech->getSchemeName() == Ogre::MSN_SHADERGEN) {
				const auto mat = tech->getParent();
				shaderGenerator->validateMaterialIlluminationPasses(tech->getSchemeName(),
																	mat->getName(), mat->getGroup());
				return true;
			}
			return false;
		}

		bool beforeIlluminationPassesCleared(Ogre::Technique* tech) override {
			if (tech->getSchemeName() == Ogre::MSN_SHADERGEN) {
				const auto mat = tech->getParent();
				shaderGenerator->invalidateMaterialIlluminationPasses(tech->getSchemeName(),
																	  mat->getName(), mat->getGroup());
				return true;
			}
			return false;
		}
	};

	mMaterialsListener = std::make_unique<GenerateShadersListener>();

	Ogre::MaterialManager::getSingleton().addListener(mMaterialsListener.get());

}

void OgreSetup::parseWindowGeometry(const Ogre::ConfigOptionMap& config, unsigned int& width, unsigned int& height, bool& fullscreen) {
	auto opt = config.find("Video Mode");
	if (opt != config.end()) {
		Ogre::String val = opt->second.currentValue;
		auto pos = val.find('x');
		if (pos != Ogre::String::npos) {

			width = Ogre::StringConverter::parseUnsignedInt(val.substr(0, pos));
			height = Ogre::StringConverter::parseUnsignedInt(val.substr(pos + 1));
		}
	}

	//now on to whether we should use fullscreen
	opt = config.find("Full Screen");
	if (opt != config.end()) {
		fullscreen = (opt->second.currentValue == "Yes");
	}

}

void OgreSetup::registerOpenGLContextFix() {
	/**
	 * This is needed to combat a bug found at least on KDE 4.14.4 when using OpenGL in the window manager.
	 * For some reason the OpenGL context of the application sometimes is altered when the window is minimized and restored.
	 * This results in segfaults when Ogre then tries to issue OpenGL commands.
	 * The exact cause and reasons for this bug are unknown, but by making sure that the OpenGL context is set each
	 * time the window is resized, minimized or restored we seem to avoid the bug.
	 *
	 */
	Ogre::GLContext* ogreGLcontext = nullptr;
	mRenderWindow->getCustomAttribute("GLCONTEXT", &ogreGLcontext);
	if (ogreGLcontext) {
		logger->info("Registering OpenGL context loss fix.");
		Input::getSingleton().EventSDLEventReceived.connect([=](const SDL_Event& event) {
			switch (event.type) {
				case SDL_EVENT_WINDOW_SHOWN:
				case SDL_EVENT_WINDOW_HIDDEN:
				case SDL_EVENT_WINDOW_RESIZED:
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				case SDL_EVENT_WINDOW_MINIMIZED:
				case SDL_EVENT_WINDOW_MAXIMIZED:
				case SDL_EVENT_WINDOW_RESTORED:
					ogreGLcontext->setCurrent();
					break;
				default:
					break;
			}
		});
	}
}

}

