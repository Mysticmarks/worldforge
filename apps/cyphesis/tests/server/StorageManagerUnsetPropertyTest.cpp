#include "server/StorageManager.h"
#include "server/Persistence.h"
#include "rules/simulation/WorldRouter.h"
#include "rules/simulation/LocatedEntity.h"
#include "common/TypeNode_impl.h"
#include "common/Property_impl.h"
#include "../TestPropertyManager.h"
#include "../DatabaseNull.h"

#include <cassert>
#include <vector>
#include <string>

struct RecordingDatabase : public DatabaseNull {
    std::vector<std::string> queries;
    int scheduleCommand(const std::string& query) override {
        queries.push_back(query);
        return 0;
    }
};

struct TestStorageManager : public StorageManager {
    TestStorageManager(WorldRouter& w, Database& db, EntityBuilder& eb, PropertyManager<LocatedEntity>& pm)
        : StorageManager(w, db, eb, pm) {}
    void test_insertEntity(LocatedEntity& e) { insertEntity(e); }
    void test_updateEntity(LocatedEntity& e) { updateEntity(e); }
};

int main() {
    RecordingDatabase db;
    Persistence persistence(db);
    EntityBuilder eb;
    TestPropertyManager<LocatedEntity> propertyManager;
    Ref<LocatedEntity> root(new LocatedEntity(0));
    WorldRouter world(root, eb, {});
    TestStorageManager store(world, db, eb, propertyManager);

    Ref<LocatedEntity> ent(new LocatedEntity(1));
    TypeNode<LocatedEntity> type("test_type");
    ent->setType(&type);
    ent->setAttr("foo", 1);
    store.test_insertEntity(*ent);

    db.queries.clear();

    ent->setAttr("foo", Atlas::Message::Element());
    store.test_updateEntity(*ent);

    bool deleted = false;
    for (const auto& q : db.queries) {
        if (q.find("DELETE FROM properties") != std::string::npos &&
            q.find("name = 'foo'") != std::string::npos) {
            deleted = true;
            break;
        }
    }
    assert(deleted);
    return 0;
}

