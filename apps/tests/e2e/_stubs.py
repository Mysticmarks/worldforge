import math
from dataclasses import dataclass
from types import SimpleNamespace


@dataclass
class Operation:
    name: str
    entity: object | None = None
    to: str | None = None
    from_: str | None = None


class Entity(dict):
    def __init__(self, id: str | None = None, **kwargs):
        super().__init__(**kwargs)
        if id is not None:
            self["id"] = id
        self.id = id


class Oplist(list):
    """Simple list subclass used for operations."""
    pass


@dataclass
class Vector3D:
    x: float = 0
    y: float = 0
    z: float = 0

    def __init__(self, x: float = 0, y: float = 0, z: float = 0):
        if isinstance(x, Vector3D):
            self.x, self.y, self.z = x.x, x.y, x.z
        else:
            self.x, self.y, self.z = x, y, z

    def normalize(self) -> "Vector3D":
        mag = math.sqrt(self.x ** 2 + self.y ** 2 + self.z ** 2)
        if mag:
            self.x /= mag
            self.y /= mag
            self.z /= mag
        return self

    def __mul__(self, other: float) -> "Vector3D":
        return Vector3D(self.x * other, self.y * other, self.z * other)

    def __iadd__(self, other: "Vector3D") -> "Vector3D":
        self.x += other.x
        self.y += other.y
        self.z += other.z
        return self


class Quaternion:
    def __init__(self, *args, **kwargs):
        pass
