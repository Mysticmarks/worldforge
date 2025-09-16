//
// C++ Implementation: EntityMappingCreator
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
#include "EntityMappingCreator.h"

#include <memory>
#include <functional>
#include <unordered_map>
#include "EntityMapping.h"
#include "EntityMappingManager.h"

#include "components/entitymapping/Cases/EntityRefCase.h"
#include "Cases/AttributeCase.h"

#include "Cases/AttributeComparers/HeightComparerWrapper.h"
#include "Cases/AttributeComparers/NumericComparer.h"
#include "Cases/AttributeComparers/NumericComparerWrapper.h"
#include "Cases/AttributeComparers/NumericEqualsComparer.h"
#include "Cases/AttributeComparers/NumericEqualsOrGreaterComparer.h"
#include "Cases/AttributeComparers/NumericEqualsOrLesserComparer.h"
#include "Cases/AttributeComparers/NumericGreaterComparer.h"
#include "Cases/AttributeComparers/NumericLesserComparer.h"
#include "Cases/AttributeComparers/NumericRangeComparer.h"
#include "Cases/AttributeComparers/StringComparer.h"
#include "Cases/AttributeComparers/StringComparerWrapper.h"

#include "Matches/Observers/EntityCreationObserver.h"
#include "Matches/VirtualAttributeMatch.h"
#include "Matches/SingleAttributeMatch.h"

#include "IActionCreator.h"
#include "framework/Log.h"

#include <Eris/TypeService.h>

namespace {

const Ember::EntityMapping::Definitions::CaseDefinition::ParameterEntry*
findCaseParameter(const std::vector<Ember::EntityMapping::Definitions::CaseDefinition::ParameterEntry>& parameters, const std::string& type) {
        for (auto& entry: parameters) {
                if (entry.first == type) {
                        return &(entry);
                }
        }
        return nullptr;
}

}


namespace Ember::EntityMapping {

using namespace Definitions;
using namespace Matches;
using namespace Observers;
using namespace Cases;
using namespace AttributeComparers;


EntityMappingCreator::EntityMappingCreator(EntityMappingDefinition& definition, Eris::Entity& entity, IActionCreator& actionCreator, Eris::TypeService& typeService, Eris::View* view)
		: mActionCreator(actionCreator),
		  mEntity(entity),
		  mEntityMapping(nullptr),
		  mDefinition(definition),
		  mTypeService(typeService),
		  mView(view) {
}


std::unique_ptr<EntityMapping> EntityMappingCreator::create() {
	createMapping();
	return std::move(mEntityMapping);
}


void EntityMappingCreator::createMapping() {
	mEntityMapping = std::make_unique<EntityMapping>(mEntity);

	mActionCreator.createActions(*mEntityMapping, mEntityMapping->getBaseCase(), mDefinition.RootCase);

	for (auto& aMatch: mDefinition.RootCase.Matches) {
		addMatch(mEntityMapping->getBaseCase(), aMatch);
	}

	//since we already have the entity, we can perform a check right away
	mEntityMapping->getBaseCase().setEntity(&mEntity);
}

void EntityMappingCreator::addEntityTypeCases(EntityTypeMatch* entityTypeMatch, MatchDefinition& matchDefinition) {

	for (auto& aCase: matchDefinition.Cases) {
		auto entityCase = std::make_unique<EntityTypeCase>();

		for (auto& paramEntry: aCase.Parameters) {
			if (paramEntry.first == "equals") {
				entityCase->addEntityType(mTypeService.getTypeByName(paramEntry.second));
			}
		}

		mActionCreator.createActions(*mEntityMapping, *entityCase, aCase);

		for (auto& aMatch: aCase.Matches) {
			addMatch(*entityCase, aMatch);
		}
		entityCase->setParentMatch(entityTypeMatch);
		entityTypeMatch->addCase(std::move(entityCase));
	}
}

void EntityMappingCreator::addEntityRefCases(EntityRefMatch* match, MatchDefinition& matchDefinition) {
	for (auto& aCase: matchDefinition.Cases) {
		auto entityRefCase = std::make_unique<EntityRefCase>();

		for (auto& paramEntry: aCase.Parameters) {
			if (paramEntry.first == "equals") {
				entityRefCase->addEntityType(mTypeService.getTypeByName(paramEntry.second));
			}
		}
		mActionCreator.createActions(*mEntityMapping, *entityRefCase, aCase);

		for (auto& aMatch: aCase.Matches) {
			addMatch(*entityRefCase, aMatch);
		}
		entityRefCase->setParentMatch(match);
		match->addCase(std::move(entityRefCase));
	}
}


std::unique_ptr<AttributeComparers::AttributeComparerWrapper> EntityMappingCreator::getAttributeCaseComparer(AttributeMatch* match, MatchDefinition& matchDefinition, CaseDefinition& caseDefinition) {
        const std::string& matchType = matchDefinition.Properties["type"];

        if ((matchType.empty()) || (matchType == "string")) {
                //default is string comparison
		if (auto param = findCaseParameter(caseDefinition.Parameters, "equals")) {
			return std::make_unique<AttributeComparers::StringComparerWrapper>(std::make_unique<AttributeComparers::StringValueComparer>(param->second));
		} else if (findCaseParameter(caseDefinition.Parameters, "notempty")) {
			return std::make_unique<AttributeComparers::StringComparerWrapper>(std::make_unique<AttributeComparers::StringNotEmptyComparer>());
		} else {
			return std::make_unique<AttributeComparers::StringComparerWrapper>(std::make_unique<AttributeComparers::StringValueComparer>(""));
		}
        } else if (matchType == "numeric") {
                return std::make_unique<AttributeComparers::NumericComparerWrapper>(createNumericComparer(caseDefinition));
        } else if (matchType == "function") {
                using FunctionComparerCreator = std::unique_ptr<AttributeComparers::AttributeComparerWrapper>(EntityMappingCreator::*)(CaseDefinition&);
                static const std::unordered_map<std::string, FunctionComparerCreator> functionComparers{
                        {"height", &EntityMappingCreator::createHeightFunctionComparer},
                        {"verticalextent", &EntityMappingCreator::createHeightFunctionComparer}
                };

                const auto& attributeName = match->getAttributeName();
                if (const auto comparerIt = functionComparers.find(attributeName); comparerIt != functionComparers.end()) {
                        auto comparer = std::invoke(comparerIt->second, this, caseDefinition);
                        if (!comparer) {
                                logger->warn("Failed to create comparer for function attribute '{}'.", attributeName);
                        }
                        return comparer;
                }
                logger->warn("Unsupported function attribute '{}' in function match.", attributeName);
        }
        return nullptr;

}

std::unique_ptr<AttributeComparers::AttributeComparerWrapper> EntityMappingCreator::createHeightFunctionComparer(CaseDefinition& caseDefinition) {
        auto numericComparer = createNumericComparer(caseDefinition);
        if (!numericComparer) {
                return nullptr;
        }
        return std::make_unique<AttributeComparers::HeightComparerWrapper>(std::move(numericComparer), mEntity);
}

std::unique_ptr<AttributeComparers::NumericComparer> EntityMappingCreator::createNumericComparer(CaseDefinition& caseDefinition) {
        const CaseDefinition::ParameterEntry* param;

        if ((param = findCaseParameter(caseDefinition.Parameters, "equals"))) {
                return std::make_unique<AttributeComparers::NumericEqualsComparer>(std::stof(param->second));
	}

	//If both a min and max value is set, it's a range comparer
	std::unique_ptr<AttributeComparers::NumericComparer> mMin;
	std::unique_ptr<AttributeComparers::NumericComparer> mMax(nullptr);
	if ((param = findCaseParameter(caseDefinition.Parameters, "lesser"))) {
		mMin = std::make_unique<AttributeComparers::NumericLesserComparer>(std::stof(param->second));
	} else if ((param = findCaseParameter(caseDefinition.Parameters, "lesserequals"))) {
		mMin = std::make_unique<AttributeComparers::NumericEqualsOrLesserComparer>(std::stof(param->second));
	}

	if ((param = findCaseParameter(caseDefinition.Parameters, "greater"))) {
		mMax = std::make_unique<AttributeComparers::NumericGreaterComparer>(std::stof(param->second));
	} else if ((param = findCaseParameter(caseDefinition.Parameters, "greaterequals"))) {
		mMax = std::make_unique<AttributeComparers::NumericEqualsOrGreaterComparer>(std::stof(param->second));
	}

	//check if we have both min and max set, and if so we should use a range comparer
	if (mMin && mMax) {
		return std::make_unique<AttributeComparers::NumericRangeComparer>(std::move(mMin), std::move(mMax));
	} else if (!mMax && mMin) {
		return mMin;
	} else if (mMax) {
		return mMax;
	}
	//invalid, could not find anything to compare against
	return nullptr;
}


void EntityMappingCreator::addAttributeCases(AttributeMatch* match, MatchDefinition& matchDefinition) {
	for (auto& aCase: matchDefinition.Cases) {
		auto wrapper = getAttributeCaseComparer(match, matchDefinition, aCase);
		if (wrapper) {
			auto attrCase = std::make_unique<AttributeCase>(std::move(wrapper));

			mActionCreator.createActions(*mEntityMapping, *attrCase, aCase);

			for (auto& aMatch: aCase.Matches) {
				addMatch(*attrCase, aMatch);
			}

			attrCase->setParentMatch(match);
			match->addCase(std::move(attrCase));
		}
	}

}

void EntityMappingCreator::addMatch(CaseBase& aCase, MatchDefinition& matchDefinition) {
	if (matchDefinition.Type == "attribute") {
		addAttributeMatch(aCase, matchDefinition);
	} else if (matchDefinition.Type == "entitytype") {
		addEntityTypeMatch(aCase, matchDefinition);
	} else if (matchDefinition.Type == "entityref") {
		addEntityRefCase(aCase, matchDefinition);
	}
}

void EntityMappingCreator::addAttributeMatch(CaseBase& aCase, MatchDefinition& matchDefinition) {
	const std::string& attributeName = matchDefinition.Properties["attribute"];

        std::unique_ptr<AttributeMatch> match;
        const std::string& matchType = matchDefinition.Properties["type"];
        if (matchType == "function") {
                const auto createHeightMatch = [](const std::string& attribute) -> std::unique_ptr<AttributeMatch> {
                        auto virtualMatch = std::make_unique<VirtualAttributeMatch>(attribute, std::initializer_list<std::string>{"bbox", "scale"});
                        virtualMatch->addMatchAttributeObserver(std::make_unique<MatchAttributeObserver>(*virtualMatch, "bbox"));
                        virtualMatch->addMatchAttributeObserver(std::make_unique<MatchAttributeObserver>(*virtualMatch, "scale"));
                        return virtualMatch;
                };

                static const std::unordered_map<std::string, std::function<std::unique_ptr<AttributeMatch>(const std::string&)>> functionMatches{
                        {"height", createHeightMatch},
                        {"verticalextent", createHeightMatch}
                };

                if (const auto matchIt = functionMatches.find(attributeName); matchIt != functionMatches.end()) {
                        match = matchIt->second(attributeName);
                        if (!match) {
                                logger->warn("Failed to create function attribute match for '{}'.", attributeName);
                        }
                } else {
                        logger->warn("Unsupported function attribute '{}' in function match.", attributeName);
                }
        } else {
                auto singleMatch = std::make_unique<SingleAttributeMatch>(attributeName);
                singleMatch->setMatchAttributeObserver(std::make_unique<MatchAttributeObserver>(*singleMatch, attributeName));
                match = std::move(singleMatch);
        }
        if (match) {
		addAttributeCases(match.get(), matchDefinition);
		aCase.addMatch(std::move(match));

	}
}

void EntityMappingCreator::addEntityTypeMatch(CaseBase& aCase, MatchDefinition& matchDefinition) {
	auto match = std::make_unique<EntityTypeMatch>();
	addEntityTypeCases(match.get(), matchDefinition);
	aCase.addMatch(std::move(match));

	//since we already have the entity, we can perform a check right away
// 	match->testEntity(mEntity);
}

void EntityMappingCreator::addEntityRefCase(CaseBase& aCase, MatchDefinition& matchDefinition) {
	if (mView) {
		const std::string& attributeName = matchDefinition.Properties["attribute"];
		auto match = std::make_unique<EntityRefMatch>(attributeName, mView);

		addEntityRefCases(match.get(), matchDefinition);


		//observe the attribute by the use of an MatchAttributeObserver
		match->setMatchAttributeObserver(std::make_unique<MatchAttributeObserver>(*match, attributeName));

		match->setEntityCreationObserver(std::make_unique<EntityCreationObserver>(*match));
		aCase.addMatch(std::move(match));
	}

}

}


