import pytest
import atlas
from atlas.transport.bridge import Bridge, BridgeException


class FailingCodec:
    def encode(self, _):
        raise ValueError("cannot encode")

    def decode(self, _):
        raise ValueError("cannot decode")

    def close(self):
        return ''


def test_decode_invalid_data():
    bridge = Bridge()
    bridge.codec = FailingCodec()
    with pytest.raises(BridgeException, match="Decoding failed"):
        bridge.process_string("bad data")


def test_encode_invalid_operation():
    bridge = Bridge()
    bridge.codec = FailingCodec()
    op = atlas.Operation("foo")
    with pytest.raises(BridgeException, match="Encoding failed"):
        bridge.process_operation(op)
