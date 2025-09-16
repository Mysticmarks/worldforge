/*
 * Copyright (C) 2012 Arjun Kumar <arjun1991@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "components/ogre/AutoGraphicsLevelManager.h"

#include "services/config/ConfigListenerContainer.h"

#include "framework/MainLoopController.h"
#include "framework/Log.h"
#include "framework/TimeFrame.h"

#include <numeric>


namespace Ember::OgreView {

FrameTimeRecorder::FrameTimeRecorder(MainLoopController& mainLoopController) :
		mRequiredTimeSamples(std::chrono::seconds(2)),
		mTimePerFrameStore(20),
		mAccumulatedFrameTimes(std::chrono::seconds(0)),
		mAccumulatedFrames(0) {
	mainLoopController.EventFrameProcessed.connect(sigc::mem_fun(*this, &FrameTimeRecorder::frameCompleted));
}

FrameTimeRecorder::~FrameTimeRecorder() = default;

void FrameTimeRecorder::frameCompleted(const TimeFrame& timeFrame, unsigned int frameActionMask) {
	if (frameActionMask & MainLoopController::FA_GRAPHICS) {

		mAccumulatedFrameTimes += timeFrame.getElapsedTime();
		mAccumulatedFrames++;

		if (mAccumulatedFrameTimes >= mRequiredTimeSamples) {

			mTimePerFrameStore.push_back(mAccumulatedFrameTimes / mAccumulatedFrames);
			mAccumulatedFrameTimes = std::chrono::microseconds::zero();
			mAccumulatedFrames = 0;

			auto averageTimePerFrame = std::accumulate(mTimePerFrameStore.begin(), mTimePerFrameStore.end(), std::chrono::steady_clock::duration::zero()) / mTimePerFrameStore.size();
			EventAverageTimePerFrameUpdated(averageTimePerFrame);

		}
	}
}

AutomaticGraphicsLevelManager::AutomaticGraphicsLevelManager(MainLoopController& mainLoopController) :
                mDefaultFps(60.0f),
                mEnabled(false),
                mAtMaximumLevel(false),
                mAtMinimumLevel(false),
                mFrameTimeRecorder(mainLoopController),
                mConfigListenerContainer(std::make_unique<ConfigListenerContainer>()) {
	mFpsUpdatedConnection = mFrameTimeRecorder.EventAverageTimePerFrameUpdated.connect(sigc::mem_fun(*this, &AutomaticGraphicsLevelManager::averageTimePerFrameUpdated));
	mConfigListenerContainer->registerConfigListener("general", "desiredfps", sigc::mem_fun(*this, &AutomaticGraphicsLevelManager::Config_DefaultFps));
	mConfigListenerContainer->registerConfigListenerWithDefaults("graphics", "autoadjust", sigc::mem_fun(*this, &AutomaticGraphicsLevelManager::Config_Enabled), false);
}

AutomaticGraphicsLevelManager::~AutomaticGraphicsLevelManager() {
	mFpsUpdatedConnection.disconnect();
}

void AutomaticGraphicsLevelManager::setFps(float fps) {
	mDefaultFps = fps;
}

void AutomaticGraphicsLevelManager::checkFps(float currentFps) {
	float changeRequired = mDefaultFps - currentFps;
	//This factor is used to adjust the required fps difference before a change is triggered. Lower required fpses eg. 30 will need to respond to smaller changes.
	float factor = mDefaultFps / 60.0f;
	if (std::abs(changeRequired) >= factor * 8.0f) {
		changeGraphicsLevel(changeRequired);
	}
}

void AutomaticGraphicsLevelManager::averageTimePerFrameUpdated(std::chrono::nanoseconds timePerFrame) {
	//Convert microseconds per frame to fps.
	checkFps(1000000000.0f / (float) timePerFrame.count());
}

void AutomaticGraphicsLevelManager::changeGraphicsLevel(float changeInFpsRequired) {
        auto outcome = Detail::processGraphicsLevelChange(
                changeInFpsRequired,
                mAtMaximumLevel,
                mAtMinimumLevel,
                [this](float change) { return mGraphicalChangeAdapter.fpsChangeRequired(change); });

        switch (outcome) {
                case Detail::GraphicsLevelChangeResult::NoChange:
                case Detail::GraphicsLevelChangeResult::Applied:
                        break;
                case Detail::GraphicsLevelChangeResult::ReachedMaximum:
                        logger->warn("Reached maximum graphics level; cannot increase details further.");
                        break;
                case Detail::GraphicsLevelChangeResult::ReachedMinimum:
                        logger->warn("Reached minimum graphics level; cannot decrease details further.");
                        break;
                case Detail::GraphicsLevelChangeResult::AlreadyMaximum:
                        logger->warn("Requested graphics increase but level is already at maximum; skipping adjustment.");
                        break;
                case Detail::GraphicsLevelChangeResult::AlreadyMinimum:
                        logger->warn("Requested graphics decrease but level is already at minimum; skipping adjustment.");
                        break;
        }
}

GraphicalChangeAdapter& AutomaticGraphicsLevelManager::getGraphicalAdapter() {
	return mGraphicalChangeAdapter;
}

void AutomaticGraphicsLevelManager::setEnabled(bool newEnabled) {
	mEnabled = newEnabled;
	if (!newEnabled) {
		mFpsUpdatedConnection.block();
	} else {
		mFpsUpdatedConnection.unblock();
	}
}

bool AutomaticGraphicsLevelManager::isEnabled() const {
	return mEnabled;
}

void AutomaticGraphicsLevelManager::Config_DefaultFps(const std::string&, const std::string&, varconf::Variable& variable) {
	if (variable.is_double()) {
		auto fps = static_cast<double>(variable);
		//If set to 0, the fps the manager tries to achieve is 60
		if (fps == 0.0f) {
			fps = 60.0f;
		}
		mDefaultFps = (float) fps;
	}
}

void AutomaticGraphicsLevelManager::Config_Enabled(const std::string&, const std::string&, varconf::Variable& variable) {
	if (variable.is_bool() && static_cast<bool>(variable)) {
		setEnabled(true);
	} else {
		setEnabled(false);
	}
}

}

