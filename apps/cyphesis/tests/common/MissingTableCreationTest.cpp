#include "common/Storage.h"
#include "common/DatabaseNull.h"
#include <cassert>
#include <string>

class MissingTableDatabase : public DatabaseNull {
public:
    mutable bool entitiesMissing = true;
    mutable bool propertiesMissing = true;
    mutable bool thoughtsMissing = true;
    mutable bool accountsMissing = true;

    class ErrorWorker : public DatabaseNullResultWorker {
    public:
        bool error() const override { return true; }
    };

    DatabaseResult runSimpleSelectQuery(const std::string& query) const override {
        if (entitiesMissing && query.find("entities") != std::string::npos) {
            return DatabaseResult(std::make_unique<ErrorWorker>());
        }
        if (propertiesMissing && query.find("properties") != std::string::npos) {
            return DatabaseResult(std::make_unique<ErrorWorker>());
        }
        if (thoughtsMissing && query.find("thoughts") != std::string::npos) {
            return DatabaseResult(std::make_unique<ErrorWorker>());
        }
        if (accountsMissing && query.find("accounts") != std::string::npos) {
            return DatabaseResult(std::make_unique<ErrorWorker>());
        }
        return DatabaseNull::runSimpleSelectQuery(query);
    }

    int registerEntityTable() override { entitiesMissing = false; return 0; }
    int registerPropertyTable() override { propertiesMissing = false; return 0; }
    int registerThoughtsTable() override { thoughtsMissing = false; return 0; }
    int registerSimpleTable(const std::string& name, const Atlas::Message::MapType& row) override {
        if (name == "accounts") { accountsMissing = false; }
        return 0; 
    }
};

int main() {
    MissingTableDatabase db;
    Storage storage(db);
    assert(!db.entitiesMissing);
    assert(!db.propertiesMissing);
    assert(!db.thoughtsMissing);
    assert(!db.accountsMissing);
    return 0;
}

