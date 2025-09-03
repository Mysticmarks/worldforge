#include "../TestBase.h"
#include "common/AssetsManager.h"
#include "common/FileSystemObserver.h"
#include "common/globals.h"

#include <boost/asio/io_context.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

class AssetsManagerIntegrationTest : public Cyphesis::TestBase {
public:
    void setup() override {}
    void teardown() override {}

    void test_allPathsTriggerReload() {
        namespace fs = std::filesystem;
        auto temp = fs::temp_directory_path();

        fs::path share1 = temp / "am_share1";
        fs::path share2 = temp / "am_share2";
        fs::path etc1 = temp / "am_etc1";
        fs::path asset1 = temp / "am_asset1";
        fs::path asset2 = temp / "am_asset2";

        fs::create_directories(share1 / "cyphesis" / "scripts");
        fs::create_directories(share2 / "cyphesis" / "scripts");
        fs::create_directories(etc1 / "cyphesis");
        fs::create_directories(asset1);
        fs::create_directories(asset2);

        share_directory = (share1.string() + ":" + share2.string());
        etc_directory = etc1.string();
        assets_directory = (asset1.string() + ":" + asset2.string());

        boost::asio::io_context io_context;
        auto observer = std::make_unique<FileSystemObserver>(io_context);
        AssetsManager manager(std::move(observer));
        manager.observeAssetsDirectory();

        fs::path shareFile1 = share1 / "cyphesis" / "scripts" / "file1.txt";
        fs::path shareFile2 = share2 / "cyphesis" / "scripts" / "file2.txt";
        fs::path assetFile1 = asset1 / "asset1.txt";
        fs::path assetFile2 = asset2 / "asset2.txt";

        bool share1Triggered = false;
        bool share2Triggered = false;
        bool asset1Triggered = false;
        bool asset2Triggered = false;

        manager.observeFile(shareFile1, [&](const fs::path&){ share1Triggered = true; });
        manager.observeFile(shareFile2, [&](const fs::path&){ share2Triggered = true; });
        manager.observeFile(assetFile1, [&](const fs::path&){ asset1Triggered = true; });
        manager.observeFile(assetFile2, [&](const fs::path&){ asset2Triggered = true; });

        std::ofstream(shareFile1) << "data";
        std::ofstream(shareFile2) << "data";
        std::ofstream(assetFile1) << "data";
        std::ofstream(assetFile2) << "data";

        for (int i = 0; i < 20; ++i) {
            io_context.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_TRUE(share1Triggered);
        ASSERT_TRUE(share2Triggered);
        ASSERT_TRUE(asset1Triggered);
        ASSERT_TRUE(asset2Triggered);
    }

    AssetsManagerIntegrationTest() {
        ADD_TEST(AssetsManagerIntegrationTest::test_allPathsTriggerReload);
    }
};

int main() {
    AssetsManagerIntegrationTest t;
    return t.run();
}
