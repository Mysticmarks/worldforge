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
#pragma once

#include "GraphicalChangeAdapter.h"

#include <sigc++/signal.h>
#include <sigc++/connection.h>
#include <sigc++/trackable.h>

#include <boost/circular_buffer.hpp>

#include <string>
#include <chrono>
#include <functional>

namespace varconf {
class Variable;
}

namespace Ember {
struct TimeFrame;

class MainLoopController;

class ConfigListenerContainer;
namespace OgreView {

class GraphicalChangeAdapter;

/**
 * @brief Records the average time per frame.
 * 
 */
class FrameTimeRecorder : public virtual sigc::trackable {
public:
	/**
	 * @brief Constructor
	 */
	explicit FrameTimeRecorder(MainLoopController& mainLoopController);

	/**
	 * @brief Destructor
	 */
	virtual ~FrameTimeRecorder();

	/**
	 * @brief Signal sent out with the updated average time per frame.
	 */
	sigc::signal<void(const std::chrono::steady_clock::duration)> EventAverageTimePerFrameUpdated;

protected:

	/**
	 * The amount of time in microseconds that the fps should be averaged over.
	 */
	std::chrono::steady_clock::duration mRequiredTimeSamples;

	/**
	 * Stores averaged time frames.
	 */
	boost::circular_buffer<std::chrono::steady_clock::duration> mTimePerFrameStore;

	/**
	 * @brief Accumulates frame times since last calculation.
	 */
	std::chrono::steady_clock::duration mAccumulatedFrameTimes;

	/**
	 * @brief Accumulates number of frames since last calculation.
	 */
	int mAccumulatedFrames;

	void frameCompleted(const TimeFrame& timeFrame, unsigned int frameActionMask);

};

namespace Detail {

enum class GraphicsLevelChangeResult {
        NoChange,
        Applied,
        ReachedMaximum,
        ReachedMinimum,
        AlreadyMaximum,
        AlreadyMinimum
};

inline GraphicsLevelChangeResult processGraphicsLevelChange(float changeInFpsRequired,
                                                            bool& atMaximumLevel,
                                                            bool& atMinimumLevel,
                                                            const std::function<bool(float)>& applyChange) {
        if (changeInFpsRequired == 0.0f) {
                return GraphicsLevelChangeResult::NoChange;
        }

        if (changeInFpsRequired > 0.0f) {
                if (atMaximumLevel) {
                        return GraphicsLevelChangeResult::AlreadyMaximum;
                }

                bool furtherChangePossible = applyChange(changeInFpsRequired);
                atMinimumLevel = false;
                atMaximumLevel = !furtherChangePossible;
                if (atMaximumLevel) {
                        return GraphicsLevelChangeResult::ReachedMaximum;
                }
                return GraphicsLevelChangeResult::Applied;
        }

        if (atMinimumLevel) {
                return GraphicsLevelChangeResult::AlreadyMinimum;
        }

        bool furtherChangePossible = applyChange(changeInFpsRequired);
        atMaximumLevel = false;
        atMinimumLevel = !furtherChangePossible;
        if (atMinimumLevel) {
                return GraphicsLevelChangeResult::ReachedMinimum;
        }
        return GraphicsLevelChangeResult::Applied;
}

}

/**
 *@brief Central class for automatic adjustment of graphics level
 *
 * This class maintains a current Graphics level. It connects to the fpsUpdated signal and thus
 * checks the fps for a significant increase or decrease and then asks for a change in the level
 * by using the GraphicalChangeAdapter.
 */

class AutomaticGraphicsLevelManager {
public:
	/**
	 * @brief Constructor
	 * @param mainLoopController The main loop controller.
	 */
	explicit AutomaticGraphicsLevelManager(MainLoopController& mainLoopController);

	/**
	 * @brief Destructor
	 */
	~AutomaticGraphicsLevelManager();

	/**
	 * @brief Sets whether automatic adjustment is enabled
	 */
	void setEnabled(bool newEnabled);

	/**
	 * @brief Sets the FPS that the component tries to achieve.
	 * @param fps The fps that the manager tries to achieve.
	 */
	void setFps(float fps);

	/**
	 * @brief Used to check if automatic adjustment is enabled
	 */
	bool isEnabled() const;

	/**
	 * @brief Used to trigger a change in graphics level
	 * @param changeInFpsRequired Used to pass how much of a change in fps is required, positive for an increase in fps, negative for a decrease in fps
	 */
	void changeGraphicsLevel(float changeInFpsRequired);

	/**
	 * @brief Used to access the instance GraphicalChangeAdapter owned by this class.
	 */
	GraphicalChangeAdapter& getGraphicalAdapter();

protected:
	/**
	 * The fps this module will try to achieve once enabled
	 */
	float mDefaultFps;

        /**
         * Boolean that holds whether automatic adjustment is enabled.
         */
        bool mEnabled;

        /**
         * Tracks whether the current graphics level is at the maximum allowed level.
         */
        bool mAtMaximumLevel;

        /**
         * Tracks whether the current graphics level is at the minimum allowed level.
         */
        bool mAtMinimumLevel;

	/**
	 * Instance of FpsUpdater class owned by this class to get updates on when the fps is updated.
	 */
	FrameTimeRecorder mFrameTimeRecorder;

	/**
	 * The interface through which this central class communicates with the graphical subcomponents.
	 */
	GraphicalChangeAdapter mGraphicalChangeAdapter;

	/**
	 * @brief Used to listen for configuration changes.
	 */
	std::unique_ptr<ConfigListenerContainer> mConfigListenerContainer;

	/**
	 * @brief The connection through which the automatic graphics manager listens for fps updates.
	 */
        sigc::connection mFpsUpdatedConnection;

	/**
	 * @brief This function is used to check if the fps is optimum, higher or lower as compared to mDefaultFps.
	 */
	void checkFps(float);

	/**
	 * Called from the FrameTimeRecorder when a new average time per frame has been calculated.
	 * @param timePerFrame Time per frame, in microseconds.
	 */
	void averageTimePerFrameUpdated(std::chrono::nanoseconds timePerFrame);

	/**
	 * @brief Connected to the config service to listen for derired fps settings.
	 */
	void Config_DefaultFps(const std::string& section, const std::string& key, varconf::Variable& variable);

	/**
	 * @brief Connected to the config service to listen for whether the automatic graphics manager should be enabled.
	 */
	void Config_Enabled(const std::string& section, const std::string& key, varconf::Variable& variable);

};

}
}
