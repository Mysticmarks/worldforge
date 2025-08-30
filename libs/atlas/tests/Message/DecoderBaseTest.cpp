#include <cassert>
#include <Atlas/Message/DecoderBase.h>
#include <Atlas/Message/Element.h>
#include <Atlas/Exception.h>

using namespace Atlas::Message;

class DummyDecoder : public DecoderBase {
protected:
    void messageArrived(MapType) override {}
};

int main() {
    DummyDecoder decoder;
    decoder.streamBegin();
    decoder.listListItem();
    bool threw = false;
    try {
        decoder.listEnd();
    } catch (const Atlas::Exception&) {
        threw = true;
    }
    assert(threw && "DecoderBase should throw on malformed listEnd");
    return 0;
}

