from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).parent))
from _stubs import Operation, Entity, Oplist, Vector3D, Quaternion


@pytest.fixture
def operation() -> type[Operation]:
    return Operation


@pytest.fixture
def entity() -> type[Entity]:
    return Entity


@pytest.fixture
def vector3d() -> type[Vector3D]:
    return Vector3D


class _StoppableTask:
    def __init__(self, usage, **kwargs):
        self.usage = usage
        self.usages = [{"name": "stop"}]

    def start_action(self, name, duration=None):
        return self.usage.actor.start_action(name, duration)

    def irrelevant(self, *args):
        return None


class _Usage:
    @staticmethod
    def set_cooldown_on_attached(tool, actor):
        pass

    @staticmethod
    def delay_task_if_needed(task):
        return task


@pytest.fixture
def bow_module(monkeypatch, operation, entity, vector3d):
    atlas = SimpleNamespace(Operation=operation, Entity=entity, Oplist=Oplist)
    physics = SimpleNamespace(Vector3D=vector3d, Quaternion=Quaternion)
    entity_filter = SimpleNamespace(Filter=lambda expr: SimpleNamespace(expr=expr))
    server = SimpleNamespace(OPERATION_BLOCKED="blocked")

    monkeypatch.setitem(sys.modules, "atlas", atlas)
    monkeypatch.setitem(sys.modules, "physics", physics)
    monkeypatch.setitem(sys.modules, "entity_filter", entity_filter)
    monkeypatch.setitem(sys.modules, "server", server)
    monkeypatch.setitem(sys.modules, "world", SimpleNamespace())
    monkeypatch.setitem(sys.modules, "world.StoppableTask", SimpleNamespace(StoppableTask=_StoppableTask))
    monkeypatch.setitem(sys.modules, "world.utils", SimpleNamespace(Usage=_Usage))

    root = Path(__file__).resolve().parents[3]
    bow_path = root / "apps/cyphesis/data/rulesets/deeds/scripts/world/objects/tools/Bow.py"
    spec = importlib.util.spec_from_file_location("Bow", bow_path)
    Bow = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(Bow)
    return Bow
