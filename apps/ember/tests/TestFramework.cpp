#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "framework/TinyXmlCodec.h"
#include "framework/AtlasMessageLoader.h"
#include "tinyxml/tinyxml.h"

#include <Atlas/Objects/SmartPtr.h>
#include <Atlas/Objects/Root.h>
#include <Atlas/Message/QueuedDecoder.h>
#include <Atlas/Objects/Encoder.h>
#include <wfmath/timestamp.h>

#include <boost/date_time.hpp>
#include <Atlas/Objects/Factories.h>
#include <filesystem>

Atlas::Objects::Factories factories;
static std::filesystem::path g_dataPath;

namespace Ember {

class FrameworkTestCase : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(FrameworkTestCase);
	CPPUNIT_TEST(testTinyXmlCodec);

	CPPUNIT_TEST_SUITE_END();

public:
	void testTinyXmlCodec() {
		TiXmlDocument xmlDoc;
                bool result = xmlDoc.LoadFile((g_dataPath / "atlas.xml").string().c_str(), TIXML_DEFAULT_ENCODING);
		CPPUNIT_ASSERT(result);

		std::map <std::string, Atlas::Objects::Root> messages;

		{
			AtlasMessageLoader loader(factories, messages);
			TinyXmlCodec codec(*xmlDoc.RootElement(), loader);

			codec.poll();
		}
		CPPUNIT_ASSERT(messages.size() == 2);

		{

			TiXmlDocument xmlWriteDoc;
			Atlas::Message::QueuedDecoder decoder;
			TinyXmlCodec codec(xmlWriteDoc, decoder);
			Atlas::Objects::ObjectsEncoder encoder(codec);

			encoder.streamBegin();

			for (std::map<std::string, Atlas::Objects::Root>::const_iterator I = messages.begin(); I != messages.end(); ++I) {
				encoder.streamObjectsMessage(I->second);
			}

			encoder.streamEnd();

			//For simplicity we'll just compare the string output of the two documents.
			std::stringstream ssOriginal, ssCopy;

			ssOriginal << xmlDoc;
			ssCopy << xmlWriteDoc;

			CPPUNIT_ASSERT(ssOriginal.str() == ssCopy.str());

		}
	}

};

}

CPPUNIT_TEST_SUITE_REGISTRATION( Ember::FrameworkTestCase);

int main(int argc, char** argv) {
        g_dataPath = std::filesystem::absolute(argv[0]).parent_path();
        CppUnit::TextUi::TestRunner runner;
	CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());

	// Shows a message as each test starts
	CppUnit::BriefTestProgressListener listener;
	runner.eventManager().addListener(&listener);

	bool wasSuccessful = runner.run("", false);
	return !wasSuccessful;
}

