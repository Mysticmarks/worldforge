import collections

import pytest

from atlas.typemap import get_atlas_type


@pytest.mark.parametrize(
    "value,expected",
    [
        (0, "int"),
        (True, "int"),
        (3.14, "float"),
        ("hello", "string"),
        ([1, 2], "list"),
        ((1, 2), "list"),
        ({"k": "v"}, "map"),
    ],
)
def test_builtin_mappings(value, expected):
    assert get_atlas_type(value) == expected


def test_mapping_duck_typing_support():
    class CustomMapping:
        def __init__(self):
            self._data = {"a": 1}

        def items(self):
            return self._data.items()

    assert get_atlas_type(collections.UserDict()) == "map"
    assert get_atlas_type(CustomMapping()) == "map"


def test_sequence_duck_typing_support():
    class CustomSequence(collections.UserList):
        pass

    assert get_atlas_type(range(3)) == "list"
    assert get_atlas_type(CustomSequence([1, 2, 3])) == "list"
