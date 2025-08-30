#include "common/Storage.h"
#include "common/DatabaseNull.h"
#include <cassert>
#include <memory>

class FailingDatabase : public DatabaseNull {
public:
    int initConnection() override { return -1; }
};

int main() {
    std::unique_ptr<Database> db;
    try {
        db = std::make_unique<FailingDatabase>();
        Storage storage(*db);
        assert(false && "Expected failure to initialize storage");
    } catch (...) {
        db = std::make_unique<DatabaseNull>();
        Storage storage(*db);
    }
    return 0;
}

