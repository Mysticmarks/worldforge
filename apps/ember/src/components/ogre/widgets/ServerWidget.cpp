//
// C++ Implementation: ServerWidget
//
// Description:
//
//
// Author: Erik Ogenvik <erik@ogenvik.org>, (C) 2004
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
#include "ServerWidget.h"

#include <Atlas/Objects/RootEntity.h>

#include "ColouredListItem.h"
#include "ModelRenderer.h"
#include "EntityTextureManipulator.h"

#include "components/entitymapping/EntityMappingManager.h"
#include "EmberEntityMappingManager.h"
#include "components/ogre/GUIManager.h"
#include "components/ogre/model/Model.h"
#include "ModelActionCreator.h"

#include "services/server/ServerService.h"
#include "services/server/ProtocolVersion.h"
#include "services/server/LocalServerAdminCreator.h"
#include "services/config/ConfigService.h"
#include "services/serversettings/ServerSettings.h"
#include "services/serversettings/ServerSettingsCredentials.h"

#include <Eris/SpawnPoint.h>
#include <Eris/TypeService.h>

#include <CEGUI/widgets/Listbox.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/RadioButton.h>
#include <CEGUI/widgets/Combobox.h>
#include <CEGUI/widgets/TabControl.h>
#include <Eris/Account.h>


using namespace CEGUI;
namespace Ember::OgreView::Gui {

WidgetPluginCallback ServerWidget::registerWidget(GUIManager& guiManager) {
	struct State {
		std::unique_ptr<ServerWidget> instance;
		std::vector<Ember::AutoCloseConnection> connections;
	};
	auto state = std::make_shared<State>();

	auto connectFn = [&guiManager, state](Eris::Account* account) mutable {
		state->instance = std::make_unique<ServerWidget>(guiManager, *account);

		auto& connection = account->getConnection();
		state->connections.emplace_back(connection.Disconnected.connect([state]() mutable {
			state->instance.reset();
		}));

	};
	auto con = ServerService::getSingleton().GotAccount.connect(connectFn);

	if (ServerService::getSingleton().getAccount()) {
		connectFn(ServerService::getSingleton().getAccount());
	}

	return [con, state]() mutable {
		state->connections.clear();
		state->instance.reset();
		con.disconnect();
	};
}

ServerWidget::ServerWidget(GUIManager& guiManager, Eris::Account& account) :
                mWidget(guiManager.createWidget()),
                mAccount(account),
                mCharacterList(nullptr),
                mCreateChar(nullptr),
                mUseCreator(nullptr),
                mServerSettings() {

	buildWidget();

	mConnections.emplace_back(account.getConnection().GotServerInfo.connect([this]() { showServerInfo(mAccount.getConnection()); }));
	showServerInfo(mAccount.getConnection());

	if (ServerService::getSingleton().getAccount()->isLoggedIn()) {
		loginSuccess(ServerService::getSingleton().getAccount());
		if (ServerService::getSingleton().getAvatar()) {
			gotAvatar(ServerService::getSingleton().getAvatar());
		}
	}

	mWidget->show();
	mWidget->getMainWindow()->moveToFront();
}

ServerWidget::~ServerWidget() {
	mWidget->getGUIManager().removeWidget(mWidget);
}

void ServerWidget::buildWidget() {

	if (mWidget->loadMainSheet("ServerWidget.layout", "Server/")) {

		mWidget->getMainWindow()->getChild("OutdatedProtocolAlert/OkButton")->subscribeEvent(CEGUI::PushButton::EventClicked, [this]() {
			displayPanel("InfoPanel");
		});

		CEGUI::PushButton* okButton = dynamic_cast<CEGUI::PushButton*> (mWidget->getMainWindow()->getChild("NoCharactersAlert/OkButton"));
		if (okButton) {
			BIND_CEGUI_EVENT(okButton, CEGUI::PushButton::EventClicked, ServerWidget::OkButton_Click)
		}

		CEGUI::PushButton* entityDestroyedOkButton = dynamic_cast<CEGUI::PushButton*> (mWidget->getMainWindow()->getChild("EntityDestroyed/OkButton"));
		if (entityDestroyedOkButton) {
			BIND_CEGUI_EVENT(entityDestroyedOkButton, CEGUI::PushButton::EventClicked, ServerWidget::EntityDestroyedOkButton_Click)
		}

		CEGUI::PushButton* login = dynamic_cast<CEGUI::PushButton*> (mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/Login"));
		BIND_CEGUI_EVENT(login, CEGUI::PushButton::EventClicked, ServerWidget::Login_Click)
		CEGUI::PushButton* createAcc = dynamic_cast<CEGUI::PushButton*> (mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/CreateAcc"));
		BIND_CEGUI_EVENT(createAcc, CEGUI::PushButton::EventClicked, ServerWidget::CreateAcc_Click)

		mCharacterList = dynamic_cast<CEGUI::Listbox*> (mWidget->getMainWindow()->getChild("InfoPanel/LoggedInPanel/CharacterTabControl/ChooseCharacterPanel/CharacterList"));
		CEGUI::PushButton* chooseChar = dynamic_cast<CEGUI::PushButton*> (mWidget->getMainWindow()->getChild("InfoPanel/LoggedInPanel/CharacterTabControl/ChooseCharacterPanel/Choose"));
		mUseCreator = dynamic_cast<CEGUI::PushButton*> (mWidget->getWindow("UseCreator"));
		mCreateChar = dynamic_cast<CEGUI::PushButton*> (mWidget->getWindow("CreateButton"));

		auto chooseFn = [this]() {
			CEGUI::ListboxItem* item = mCharacterList->getFirstSelectedItem();
			if (item) {

				std::string id = mCharacterModel[mCharacterList->getItemIndex(item)];

				mAccount.takeCharacter(id);

			}
			return true;
		};

		chooseChar->subscribeEvent(CEGUI::PushButton::EventClicked, chooseFn);
		mCharacterList->subscribeEvent(CEGUI::PushButton::EventMouseDoubleClick, chooseFn);
		BIND_CEGUI_EVENT(mUseCreator, CEGUI::PushButton::EventClicked, ServerWidget::UseCreator_Click)
		mCreateChar->subscribeEvent(CEGUI::PushButton::EventClicked, [this]() {
			if (!mAccount.getSpawnPoints().empty()) {
				auto& spawnPoint = mAccount.getSpawnPoints().front();
				Atlas::Objects::Entity::RootEntity ent;
				for (auto& entry: mNewEntity) {
					ent->setAttr(entry.first, entry.second);
				}
				ent->setId(spawnPoint.id);
				mAccount.createCharacterThroughEntity(ent);
			}
			return true;
		});


		mWidget->getMainWindow()->getChild("InfoPanel/LoggedInPanel/LogoutButton")->subscribeEvent(CEGUI::PushButton::EventClicked, [=]() {
			ServerService::getSingleton().logout();
			return true;
		});

		mWidget->getMainWindow()->getChild("InfoPanel/LoggedInPanel/TeleportInfo/Yes")->subscribeEvent(CEGUI::PushButton::EventClicked, [this]() {
			mWidget->getWindow("TeleportInfo", true)->setVisible(false);
			if (mAvatarTransferInfo) {
				ServerService::getSingleton().takeTransferredCharacter(mAvatarTransferInfo->getTransferInfo());
			}
			return true;
		});

		mWidget->getMainWindow()->getChild("InfoPanel/LoggedInPanel/TeleportInfo/No")->subscribeEvent(CEGUI::PushButton::EventClicked, [this]() {
			mWidget->getWindow("TeleportInfo", true)->setVisible(false);
			return true;
		});

		updateNewCharacter();

		mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/Disconnect")->subscribeEvent(CEGUI::PushButton::EventClicked, [=]() {
			ServerService::getSingleton().disconnect();
			return true;
		});


		ServerService::getSingleton().LoginSuccess.connect(sigc::mem_fun(*this, &ServerWidget::loginSuccess));
		ServerService::getSingleton().GotAvatar.connect(sigc::mem_fun(*this, &ServerWidget::gotAvatar));
		ServerService::getSingleton().GotAllCharacters.connect(sigc::mem_fun(*this, &ServerWidget::gotAllCharacters));
		ServerService::getSingleton().LoginFailure.connect(sigc::mem_fun(*this, &ServerWidget::showLoginFailure));
		ServerService::getSingleton().TransferInfoAvailable.connect(sigc::mem_fun(*this, &ServerWidget::server_TransferInfoAvailable));

		mWidget->addTabbableWindow(mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/NameEdit"));
		mWidget->addTabbableWindow(mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/PasswordEdit"));

		mWidget->addEnterButton(login);
		mWidget->closeTabGroup();

		mWidget->addEnterButton(mCreateChar);
		mWidget->closeTabGroup();

		createPreviewTexture();
	}

}


void ServerWidget::server_TransferInfoAvailable(const std::vector<AvatarTransferInfo>& transferInfos) {
	if (!transferInfos.empty()) {
		CEGUI::Window* teleportInfo = mWidget->getWindow("TeleportInfo", true);
		teleportInfo->setVisible(true);
		mAvatarTransferInfo = transferInfos[0];
	}
}


void ServerWidget::showServerInfo(Eris::Connection& connection) {
	try {
		CEGUI::Window* info = mWidget->getWindow("Info");
		Eris::ServerInfo sInfo;
		connection.getServerInfo(sInfo);
		std::stringstream ss;
		ss << "Server name: " << sInfo.name << "\n";
		ss << "Ruleset: " << sInfo.ruleset << "\n";
		ss << "Server type: " << sInfo.server << " (v. " << sInfo.version << ")\n";
		ss << "Ping: " << sInfo.ping << "\n";
		ss << "Uptime: " << static_cast<int> (sInfo.uptime / (60 * 60 * 24)) << " days\n";
		ss << "Number of clients: " << sInfo.clients << "\n";
		info->setText(ss.str());

		/*
		 * Since we're using the server getHostname as a section name
		 * we must wait until there is a connection before we can fetch
		 * the credentials
		 */
		CEGUI::Window* nameBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/NameEdit");
		CEGUI::Window* passwordBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/PasswordEdit");
		std::string savedUser;
		std::string savedPass;
                if (fetchCredentials(connection, savedUser, savedPass)) {
                        nameBox->setText(savedUser);
                        passwordBox->setText(savedPass);
                }

		//Check if the protocol version from the server is newer than the one we support, and warn if that's the case.
		if (sInfo.protocol_version > Ember::protocolVersion) {
			showOutdatedProtocolAlert();
		}

	} catch (...) {
		logger->warn("Error when getting the server info window.");
		return;
	}
}

bool ServerWidget::fetchCredentials(Eris::Connection& connection, std::string& user, std::string& pass) {
        logger->debug("Fetching saved credentials.");

	Eris::ServerInfo sInfo;
	connection.getServerInfo(sInfo);

        ServerSettingsCredentials serverCredentials(sInfo);
        if (mServerSettings.findItem(serverCredentials, "username")) {
                user = static_cast<std::string>(mServerSettings.getItem(serverCredentials, "username"));
        }
        if (mServerSettings.findItem(serverCredentials, "password")) {
                pass = static_cast<std::string>(mServerSettings.getItem(serverCredentials, "password"));
        }
        return !pass.empty() && !user.empty();
}

bool ServerWidget::saveCredentials() {
	logger->debug("Saving credentials.");

	Eris::ServerInfo sInfo;
	mAccount.getConnection().getServerInfo(sInfo);

	// pull widget references

	try {
		auto nameBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/NameEdit");
		auto passwordBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/PasswordEdit");
		auto saveBox = dynamic_cast<CEGUI::ToggleButton*> (mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/SavePassCheck"));
		if (nameBox && passwordBox && saveBox) {

			// fetch info from widgets
			const CEGUI::String& name = nameBox->getText();
			const CEGUI::String& password = passwordBox->getText();
        ServerSettingsCredentials serverCredentials(sInfo);
        mServerSettings.setItem(serverCredentials, "username", name.c_str());
        mServerSettings.setItem(serverCredentials, "password", password.c_str());
        mServerSettings.writeToDisk();
			return true;
		}
		return false;
	} catch (const CEGUI::Exception& ex) {
		logger->error("Error when getting windows from CEGUI: {}", ex.what());
		return false;
	}
}

void ServerWidget::logoutComplete(bool) {
	mWidget->getWindow("LoginPanel")->setVisible(true);
	mWidget->getWindow("LoggedInPanel")->setVisible(false);
	mTypeServiceConnection.disconnect();
}

void ServerWidget::loginSuccess(Eris::Account* account) {
	account->LogoutComplete.connect(sigc::mem_fun(*this, &ServerWidget::logoutComplete));
	mWidget->getWindow("LoginPanel")->setVisible(false);
	mWidget->getWindow("LoggedInPanel")->setVisible(true);
	account->refreshCharacterInfo();
	fillAllowedCharacterTypes(account);

	CEGUI::ToggleButton* saveBox = dynamic_cast<CEGUI::ToggleButton*> (mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/SavePassCheck"));
	if (saveBox->isSelected()) {
		try {
			saveCredentials();
		} catch (const std::exception& ex) {
			logger->error("Error when saving password: {}", ex.what());
		} catch (...) {
			logger->error("Unspecified error when saving password.");
		}
	}

	mTypeServiceConnection = account->getConnection().getTypeService().BoundType.connect(sigc::mem_fun(*this, &ServerWidget::typeService_TypeBound));

}

void ServerWidget::showLoginFailure(Eris::Account*, std::string msg) {
	auto helpText = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/HelpText");
	helpText->setYPosition(UDim(0.6f, 0));

	auto loginFailure = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/LoginFailure");
	loginFailure->setText(msg);
	loginFailure->setVisible(true);
}

bool ServerWidget::hideLoginFailure() {
	auto helpText = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/HelpText");
	helpText->setYPosition(UDim(0.55f, 0));

	auto loginFailure = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/LoginFailure");
	loginFailure->setVisible(false);

	return true;
}


void ServerWidget::fillAllowedCharacterTypes(Eris::Account* account) {

	auto& spawnPoints = account->getSpawnPoints();

	//If the account inherits from "admin" we're an admin and can create a creator entity. This also applies if we're a "system_account" account.
	if (account->getParent() == "admin" || account->getParent() == "system_account") {
		mUseCreator->setVisible(true);
		mUseCreator->setEnabled(true);
	} else {
		mUseCreator->setVisible(false);
		mUseCreator->setEnabled(false);
	}

	if (spawnPoints.empty()) {
		showNoCharactersAlert();
	} else {
		auto& spawnPoint = spawnPoints.front();
		auto createContainer = mWidget->getWindow("CreateContainer");
		while (createContainer->getChildCount()) {
			createContainer->destroyChild(createContainer->getChildAtIdx(0));
		}
		for (auto& propEntry: spawnPoint.properties) {
			if (propEntry.options.size() == 1) {
				//If there's only one option, then select it and don't show any widgets for it.
				mNewEntity[propEntry.name] = propEntry.options.front();
				updateNewCharacter();
			} else {
				auto propWindow = createContainer->createChild("HorizontalLayoutContainer");
				auto nameWindow = propWindow->createChild("EmberLook/StaticText");
				nameWindow->setText(propEntry.label);
				nameWindow->setSize({{0, 100},
									 {0, 20}});
				if (propEntry.options.empty()) {
					auto textWindow = propWindow->createChild("EmberLook/Editbox");
					textWindow->setSize({{0, 100},
										 {0, 20}});
					textWindow->subscribeEvent(CEGUI::Window::EventTextChanged, [textWindow, this, propEntry]() {
						mNewEntity[propEntry.name] = textWindow->getText().c_str();
						updateNewCharacter();
					});
				} else {
					auto selectWindow = dynamic_cast<CEGUI::Combobox*>(propWindow->createChild("EmberLook/Combobox"));
					selectWindow->setReadOnly(true);
					selectWindow->setSize({{0, 100},
										   {0, 100}});
					for (auto& option: propEntry.options) {
						auto item = new ColouredListItem(option.String());
						selectWindow->addItem(item);
					}
					selectWindow->subscribeEvent(CEGUI::Combobox::EventListSelectionAccepted, [selectWindow, this, propEntry]() {
						auto selected = selectWindow->getSelectedItem();
						if (selected) {
							mNewEntity[propEntry.name] = selected->getText().c_str();
							updateNewCharacter();
						}
					});
				}
				propWindow->setMinSize({{0, 200},
										{0, 30}});
				propWindow->setMaxSize({{0, 200},
										{0, 30}});
				propWindow->setTooltipText(propEntry.description);
			}
		}
	}
}

void ServerWidget::gotAllCharacters() {
	mCharacterList->resetList();
	mCharacterModel.clear();
	auto& cm = mAccount.getCharacters();
	auto I = cm.begin();
	auto I_end = cm.end();

	if (I == I_end) {
		//if the user has no previous characters, show the create character tab

		CEGUI::TabControl* tabControl = dynamic_cast<CEGUI::TabControl*> (mWidget->getWindow("CharacterTabControl"));
		if (tabControl) {
			//try {
			tabControl->setSelectedTab("CreateCharacterPanel");
			//} catch (...) {};
		}
	} else {

		for (; I != I_end; ++I) {
			const Atlas::Objects::Entity::RootEntity& entity = (*I).second;

			std::string itemText;
			if (!entity->getName().empty()) {
				itemText = entity->getName();
			} else {
				//If there's no name try to print the type of entity instead.
				if (!entity->getParent().empty()) {
					itemText = entity->getParent();
				} else {
					itemText = "<unknown>";
				}
			}
			auto* item = new Gui::ColouredListItem(itemText);

			mCharacterList->addItem(item);
			mCharacterModel.push_back(entity->getId());
		}
	}

}

bool ServerWidget::UseCreator_Click(const CEGUI::EventArgs&) {
	mAdminEntityCreator = std::make_unique<AdminEntityCreator>(mAccount);
	return true;
}

void ServerWidget::preparePreviewForTypeOrArchetype(std::string typeOrArchetype) {
	auto& typeService = mAccount.getConnection().getTypeService();
	Eris::TypeInfo* erisType = typeService.getTypeByName(typeOrArchetype);
	//If the type is an archetype, we need to instead check what kind of entity will be created and show a preview for that.
	if (erisType && erisType->isBound()) {
		if (erisType->getObjType() == "archetype") {
			if (!erisType->getEntities().empty()) {
				//Get the first entity and use that
				const Atlas::Message::Element& firstEntityElement = erisType->getEntities().front();
				//TODO: Make is possible to create a DetachedEntity from archetypes, and use that
				if (firstEntityElement.isMap()) {
					auto parentElementI = firstEntityElement.Map().find("parent");
					if (parentElementI != firstEntityElement.Map().end() && parentElementI->second.isString()) {
						mPreviewTypeName = parentElementI->second.String();
						preparePreviewForTypeOrArchetype(mPreviewTypeName);
					}
				}
			}
		} else {
			Authoring::DetachedEntity entity("0", erisType);
			entity.setFromMessage(mNewEntity);
			showPreview(entity);
		}
	}
}

void ServerWidget::showPreview(Ember::OgreView::Authoring::DetachedEntity& entity) {
	Mapping::ModelActionCreator actionCreator(entity, [this](const std::string& model) {
		//update the model preview window
		mModelPreviewRenderer->showModel(model);
		//mModelPreviewRenderer->showFull();
		//we want to zoom in a little
		mModelPreviewRenderer->setCameraDistance(0.7f);
	}, [this](const std::string& part) {
		if (mModelPreviewRenderer->getModel()) {
			mModelPreviewRenderer->getModel()->showPart(part);
		}
	});

	auto mapping = Mapping::EmberEntityMappingManager::getSingleton().getManager().createMapping(entity, actionCreator, entity.getType()->getTypeService(), nullptr);
	if (mapping) {
		mapping->initialize();
	}
}

void ServerWidget::typeService_TypeBound(Eris::TypeInfo* type) {
	if (type->getName() == mPreviewTypeName) {
		preparePreviewForTypeOrArchetype(type->getName());
	}
}

void ServerWidget::updateNewCharacter() {
	bool enableCreateButton = true;

	if (!mAccount.getSpawnPoints().empty()) {
		auto& spawnPoint = mAccount.getSpawnPoints().front();
		for (auto& prop: spawnPoint.properties) {
			auto I = mNewEntity.find(prop.name);
			if (I != mNewEntity.end()) {
				if (I->second.isNone()) {
					enableCreateButton = false;
				} else {
					if (I->second.isString() && I->second.String().empty()) {
						enableCreateButton = false;
					}
				}
			} else {
				enableCreateButton = false;
			}
		}
	} else {
		enableCreateButton = false;
	}

	mCreateChar->setEnabled(enableCreateButton);
	auto I = mNewEntity.find("parent");
	if (I != mNewEntity.end()) {
		mPreviewTypeName = I->second.String();
	}
	if (!mPreviewTypeName.empty()) {
		preparePreviewForTypeOrArchetype(mPreviewTypeName);
	}
}

bool ServerWidget::Login_Click(const CEGUI::EventArgs&) {
	CEGUI::Window* nameBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/NameEdit");
	CEGUI::Window* passwordBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/PasswordEdit");

	const CEGUI::String& name = nameBox->getText();
	const CEGUI::String& password = passwordBox->getText();

	mAccount.login(std::string(name.c_str()), std::string(password.c_str()));

	return true;
}

bool ServerWidget::CreateAcc_Click(const CEGUI::EventArgs&) {
	CEGUI::Window* nameBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/NameEdit");
	CEGUI::Window* passwordBox = mWidget->getMainWindow()->getChild("InfoPanel/LoginPanel/PasswordEdit");

	const CEGUI::String& name = nameBox->getText();
	const CEGUI::String& password = passwordBox->getText();

	mAccount.createAccount(name.c_str(), name.c_str(), password.c_str());
	return true;
}

bool ServerWidget::OkButton_Click(const CEGUI::EventArgs&) {
	displayPanel("InfoPanel");
	return true;
}

bool ServerWidget::EntityDestroyedOkButton_Click(const CEGUI::EventArgs&) {
	displayPanel("InfoPanel");
	return true;
}

void ServerWidget::gotAvatar(Eris::Avatar* avatar) {
	mTypeServiceConnection.disconnect();

	mAccount.AvatarDeactivated.connect(sigc::mem_fun(*this, &ServerWidget::avatar_Deactivated));
	avatar->CharacterEntityDeleted.connect(sigc::mem_fun(*this, &ServerWidget::avatar_EntityDeleted));
	mWidget->hide();
}


void ServerWidget::avatar_EntityDeleted() {
	CEGUI::Window* alert = mWidget->getWindow("EntityDestroyed");
	if (alert) {
		alert->show();
	}
}


void ServerWidget::avatar_Deactivated(const std::string&) {
	mCharacterList->resetList();
	mCharacterModel.clear();
	mAccount.refreshCharacterInfo();
	mWidget->show();
	mWidget->getMainWindow()->moveToFront();
	mWidget->getWindow("LoginPanel")->setVisible(false);
	mWidget->getWindow("LoggedInPanel")->setVisible(true);
	gotAllCharacters();
}

void ServerWidget::createPreviewTexture() {
	auto imageWidget = mWidget->getWindow("Image");
	if (!imageWidget) {
		logger->error("Could not find CreateCharacterPanel/Image, aborting creation of preview texture.");
	} else {
		mModelPreviewRenderer = std::make_unique<ModelRenderer>(imageWidget, "newCharacterPreview");
		mModelPreviewManipulator = std::make_unique<CameraEntityTextureManipulator>(*imageWidget, mModelPreviewRenderer->getEntityTexture());
	}

}

void ServerWidget::showOutdatedProtocolAlert() {
	displayPanel("OutdatedProtocolAlert");
}

void ServerWidget::showNoCharactersAlert() {
	displayPanel("NoCharactersAlert");
}

void ServerWidget::displayPanel(const std::string& windowName) {
	for (size_t i = 0; i < mWidget->getMainWindow()->getChildCount(); ++i) {
		auto child = mWidget->getMainWindow()->getChildAtIdx(i);
		if (!child->isAutoWindow()) {
			child->hide();
		}
	}
	auto child = mWidget->getMainWindow()->getChild(windowName);
	if (child) {
		child->show();
		//child->moveToFront();
	}
}


}
