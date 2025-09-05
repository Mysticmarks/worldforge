#pragma once

#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>

inline bool filterDomain(std::string& name, const std::vector<std::string>& whitelist) {
    for (const auto& domain : whitelist) {
        if (boost::algorithm::iends_with(name, domain)) {
            if (name.size() == domain.size()) {
                name.clear();
            } else if (name.size() > domain.size() && name[name.size() - domain.size() - 1] == '.') {
                name.erase(name.size() - domain.size() - 1);
            } else {
                continue;
            }
            return true;
        }
    }
    return false;
}

