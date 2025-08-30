//
// C++ Interface: ConsoleAdapter
//
// Description: 
//
//
// Author: Erik Ogenvik <erik@ogenvik.org>, (C) 2007
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
#ifndef EMBEROGRE_GUICONSOLEADAPTER_H
#define EMBEROGRE_GUICONSOLEADAPTER_H

#include <sigc++/signal.h>
#include <string>

namespace CEGUI {
class Editbox;
}

namespace Ember {
class ConsoleBackend;
}


namespace Ember::OgreView::Gui {

/**
	@author Erik Ogenvik <erik@ogenvik.org>
*/
class ConsoleAdapter {
public:
        explicit ConsoleAdapter(CEGUI::Editbox* inputBox);

        ~ConsoleAdapter();

        /**
        Emit a user-visible message indicating that no commands matched the
        supplied prefix.
        */
        static void reportInvalidPrefix(ConsoleBackend& backend, const std::string& prefix);

	/**
	Emitted when a command has executed.
	@param the command that was executed
	*/
	sigc::signal<void(const std::string&)> EventCommandExecuted;

protected:
	CEGUI::Editbox* mInputBox;
	ConsoleBackend* mBackend;

	bool consoleInputBox_KeyUp(const CEGUI::EventArgs& args);

	bool consoleInputBox_KeyDown(const CEGUI::EventArgs& args);

	// the text of the current command line saved when browsing the history
	std::string mCommandLine;
	bool mTabPressed;
	int mSelected;
	bool mReturnKeyDown;

};

}


#endif
