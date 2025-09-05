/*
 Copyright (C) 2011 Erik Ogenvik

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

#include "Screen.h"
#include "camera/Recorder.h"

#include "services/config/ConfigService.h"

#include "framework/Exception.h"
#include "framework/Log.h"
#include "framework/ConsoleBackend.h"

#include <OgreCamera.h>
#include <OgreRenderWindow.h>
#include <OgreViewport.h>
#include <OgreRoot.h>
#include <OgreRenderSystem.h>
#include <filesystem>


namespace Ember::OgreView {

Screen::Screen(Ogre::RenderWindow& window) :
		ToggleRendermode("toggle_rendermode", this, "Toggle between wireframe and solid render modes."),
		Screenshot("screenshot", this, "Take a screenshot and write to disk."),
		Record("+record", this, "Record to disk."),
		mWindow(window),
		mRecorder(std::make_unique<Camera::Recorder>()),
		mPolygonMode(Ogre::PM_SOLID),
		mFrameStats{} {
}

Screen::~Screen() = default;

void Screen::runCommand(const std::string& command, const std::string& args) {
	if (Screenshot == command) {
		//just take a screen shot
		takeScreenshot();
	} else if (ToggleRendermode == command) {
		toggleRenderMode();
	} else if (Record == command) {
		mRecorder->startRecording();
	} else if (Record.getInverseCommand() == command) {
		mRecorder->stopRecording();
	}
}

void Screen::toggleRenderMode() {

	if (mPolygonMode == Ogre::PM_SOLID) {
		mPolygonMode = Ogre::PM_WIREFRAME;
	} else {
		mPolygonMode = Ogre::PM_SOLID;
	}

	for (unsigned short i = 0; i < mWindow.getNumViewports(); ++i) {
		Ogre::Camera* camera = mWindow.getViewport(i)->getCamera();
		if (camera) {
			camera->setPolygonMode(mPolygonMode);
		}
	}

}

std::string Screen::_takeScreenshot() {
	// retrieve current time
	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	// construct filename string
	// padding with 0 for single-digit values
	std::stringstream filename;
	filename << "screenshot_" << ((*timeinfo).tm_year + 1900); // 1900 is year "0"
	int month = ((*timeinfo).tm_mon + 1); // January is month "0"
	if (month <= 9) {
		filename << "0";
	}
	filename << month;
	int day = (*timeinfo).tm_mday;
	if (day <= 9) {
		filename << "0";
	}
	filename << day << "_";
	int hour = (*timeinfo).tm_hour;
	if (hour <= 9) {
		filename << "0";
	}
	filename << hour;
	int min = (*timeinfo).tm_min;
	if (min <= 9) {
		filename << "0";
	}
	filename << min;
	int sec = (*timeinfo).tm_sec;
	if (sec <= 9) {
		filename << "0";
	}
	filename << sec << ".jpg";

	auto dir = ConfigService::getSingleton().getHomeDirectory(BaseDirType_DATA) / "screenshots";
	try {
		//make sure the directory exists

		if (!std::filesystem::exists(dir)) {
			std::filesystem::create_directories(dir);
		}
	} catch (const std::exception& ex) {
		logger->error("Error when creating directory for screenshots: {}", ex.what());
		throw Exception("Error when saving screenshot.");
	}

	try {
		// take screenshot
		mWindow.writeContentsToFile((dir / filename.str()).string());
	} catch (const std::exception& ex) {
		logger->error("Could not write screenshot to disc: {}", ex.what());
		throw Exception("Error when saving screenshot.");
	}
	return (dir / filename.str()).string();
}

void Screen::takeScreenshot() {
	try {
		const std::string& result = _takeScreenshot();
		logger->info("Screenshot saved at: {}", result);
		ConsoleBackend::getSingletonPtr()->pushMessage("Wrote image: " + result, "info");
	} catch (const std::exception& ex) {
		ConsoleBackend::getSingletonPtr()->pushMessage(std::string("Error when saving screenshot: ") + ex.what(), "error");
	} catch (...) {
		ConsoleBackend::getSingletonPtr()->pushMessage("Unknown error when saving screenshot.", "error");
	}
}

const Ogre::RenderTarget::FrameStats& Screen::getFrameStats() {
        mFrameStats = Ogre::RenderTarget::FrameStats();
        // Retrieve stats for the primary window via custom attribute to get FPS values
        Ogre::RenderTarget::FrameStats windowStats{};
        mWindow.getCustomAttribute("FrameStats", &windowStats);
        mFrameStats.avgFPS = windowStats.avgFPS;
        mFrameStats.bestFPS = windowStats.bestFPS;
        mFrameStats.worstFPS = windowStats.worstFPS;

        // Sum triangle and batch counts across all active render targets
        auto renderSystem = Ogre::Root::getSingleton().getRenderSystem();
        auto rtIterator = renderSystem->getRenderTargetIterator();
        while (rtIterator.hasMoreElements()) {
                Ogre::RenderTarget* target = rtIterator.getNext();
                if (!target->isActive()) {
                        continue;
                }
                Ogre::RenderTarget::FrameStats targetStats{};
                target->getCustomAttribute("FrameStats", &targetStats);
                mFrameStats.triangleCount += targetStats.triangleCount;
                mFrameStats.batchCount += targetStats.batchCount;
        }

        logger->debug("Aggregated frame stats: triangles={}, batches={}", mFrameStats.triangleCount, mFrameStats.batchCount);
        return mFrameStats;
}

}

