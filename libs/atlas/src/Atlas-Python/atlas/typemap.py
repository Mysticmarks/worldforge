# map type into atlas type string

# Copyright 2000, 2001 by Aloril

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

"""Helpers for mapping native Python values to Atlas type strings."""

from collections.abc import Mapping, Sequence
from numbers import Integral, Real

_SCALAR_TYPES = {
    bool: "int",  # bool must resolve before the more general Integral check
    int: "int",
    float: "float",
    str: "string",
}


def _is_sequence(value):
    """Return True when *value* should be represented as an Atlas list."""

    if isinstance(value, (str, bytes, bytearray)):
        return False
    return isinstance(value, Sequence)


def get_atlas_type(value):
    """Return the Atlas type string for *value*.

    Atlas distinguishes between integer, floating point, string, list, and
    map encodings.  Historically this helper only matched exact built-in types,
    causing subclasses (for example ``bool`` or custom mapping implementations)
    to fall back to ``"list"``.  We broaden the detection to accept duck-typed
    values while keeping compatibility with the legacy defaults.
    """

    value_type = type(value)
    if value_type in _SCALAR_TYPES:
        return _SCALAR_TYPES[value_type]

    if isinstance(value, bool):
        return "int"

    if isinstance(value, Integral):
        return "int"

    if isinstance(value, Real):
        return "float"

    if isinstance(value, str):
        return "string"

    if isinstance(value, Mapping) or hasattr(value, "items"):
        return "map"

    if isinstance(value, (list, tuple)) or _is_sequence(value):
        return "list"

    return "list"
