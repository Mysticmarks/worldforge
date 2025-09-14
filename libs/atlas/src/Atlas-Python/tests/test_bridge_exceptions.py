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


class FailingNegotiation:
    def __init__(self, raise_error=False):
        self.state = 1 if raise_error else 0
        self.result_code = "" if raise_error else "fail"
        self.str = ""
        self.raise_error = raise_error

    def __call__(self, _):
        if self.raise_error:
            raise ValueError("bad negotiation")

    def get_send_str(self):
        return ""

    def get_codec(self):
        return FailingCodec()


def test_negotiation_fail_state():
    bridge = Bridge(negotiation=FailingNegotiation())
    with pytest.raises(BridgeException, match="Negotiation failed"):
        bridge.process_string("")


def test_negotiation_exception():
    bridge = Bridge(negotiation=FailingNegotiation(raise_error=True))
    with pytest.raises(BridgeException, match="Negotiation failed: bad negotiation"):
        bridge.process_string("data")
