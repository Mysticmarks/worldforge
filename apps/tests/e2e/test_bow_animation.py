import importlib.util
import math
import types
import sys
from pathlib import Path

# --- stub modules ---

class Operation:
    def __init__(self, name, entity=None, *, to=None, from_=None):
        self.name = name
        self.entity = entity
        self.to = to
        self.from_ = from_

class Entity(dict):
    def __init__(self, id=None, **kwargs):
        super().__init__(**kwargs)
        if id is not None:
            self["id"] = id
        self.id = id

class Oplist(list):
    def append(self, obj):
        super().append(obj)

atlas = types.ModuleType("atlas")
atlas.Operation = Operation
atlas.Entity = Entity
atlas.Oplist = Oplist
sys.modules["atlas"] = atlas

class Vector3D:
    def __init__(self, x=0, y=0, z=0):
        if isinstance(x, Vector3D):
            self.x, self.y, self.z = x.x, x.y, x.z
        else:
            self.x, self.y, self.z = x, y, z
    def normalize(self):
        mag = math.sqrt(self.x**2 + self.y**2 + self.z**2)
        if mag:
            self.x/=mag; self.y/=mag; self.z/=mag
        return self
    def __mul__(self, other):
        return Vector3D(self.x * other, self.y * other, self.z * other)
    def __iadd__(self, other):
        self.x += other.x; self.y += other.y; self.z += other.z
        return self

class Quaternion:
    def __init__(self, *args, **kwargs):
        pass

physics = types.ModuleType("physics")
physics.Vector3D = Vector3D
physics.Quaternion = Quaternion
sys.modules["physics"] = physics

entity_filter = types.ModuleType("entity_filter")
class Filter:
    def __init__(self, expr):
        self.expr = expr
entity_filter.Filter = Filter
sys.modules["entity_filter"] = entity_filter

class Task:
    def __init__(self, usage=None, **kwargs):
        self.usage = usage
        self.actor = usage.actor if usage else None
        self.usages = []
    def start_action(self, name, duration=None):
        return self.actor.start_action(name, duration)
    def irrelevant(self, *args):
        return None

server = types.ModuleType("server")
server.Task = Task
server.OPERATION_BLOCKED = "blocked"
sys.modules["server"] = server

world_pkg = types.ModuleType("world")
sys.modules["world"] = world_pkg

stoppable_task_mod = types.ModuleType("world.StoppableTask")
class StoppableTask(Task):
    def __init__(self, usage, **kwargs):
        super().__init__(usage=usage, **kwargs)
        self.usages = self.usages + [{"name": "stop"}]
stoppable_task_mod.StoppableTask = StoppableTask
sys.modules["world.StoppableTask"] = stoppable_task_mod

utils_mod = types.ModuleType("world.utils")
class Usage:
    @staticmethod
    def set_cooldown_on_attached(tool, actor):
        pass
    @staticmethod
    def delay_task_if_needed(task):
        return task
utils_mod.Usage = Usage
sys.modules["world.utils"] = utils_mod

# --- import Bow module ---
root = Path(__file__).resolve().parents[3]
bow_path = root / "apps/cyphesis/data/rulesets/deeds/scripts/world/objects/tools/Bow.py"
spec = importlib.util.spec_from_file_location("Bow", bow_path)
Bow = importlib.util.module_from_spec(spec)
spec.loader.exec_module(Bow)

# --- stubs for actor, tool, usage ---

class Location:
    def __init__(self):
        self.pos = Vector3D()
        self.bbox = types.SimpleNamespace(high_corner=types.SimpleNamespace(y=1.0))
        self.orientation = None
    def copy(self):
        new = Location()
        new.pos = Vector3D(self.pos)
        return new

class Actor:
    def __init__(self):
        self.id = "actor"
        self.location = Location()
        self.actions = []
        self.sent_world = []
        self.update_task_called = 0
    def find_in_contains(self, _):
        return [types.SimpleNamespace(id="arrow")]
    def start_action(self, name, duration):
        self.actions.append((name, duration))
        return Operation("start_action", Entity(action=name, duration=duration))
    def send_world(self, op):
        self.sent_world.append(op)
    def update_task(self):
        self.update_task_called += 1

class Tool:
    def __init__(self):
        self.id = "bow"
        self.sent_world = []
        self.props = types.SimpleNamespace(cooldown=None)
    def send_world(self, op):
        self.sent_world.append(op)
    def get_prop_float(self, name, default=None):
        return None

class UsageInstance:
    def __init__(self, actor, tool):
        self.actor = actor
        self.tool = tool
        self.op = Operation("draw")
    def get_arg(self, name, index):
        return None
    def is_valid(self):
        return True, None


def test_bow_animations():
    actor = Actor()
    tool = Tool()
    usage = UsageInstance(actor, tool)
    res = Bow.Oplist()
    direction = Vector3D(1, 0, 0)

    Bow.shoot_in_direction(direction, usage, res)

    assert any(op.name == "start_action" and op.entity["action"] == "bow/releasing" for op in res)
    assert any(op.name == "sight" and op.entity.name == "activity" and op.entity.entity["action"] == "shoot" for op in res)

    task = Bow.DrawBow(usage, tick_interval=0, name="Draw")
    task.setup("t1")
    assert ("bow/drawing", None) in actor.actions
    assert any(op.name == "sight" and op.entity.name == "activity" and op.entity.entity["action"] == "draw" for op in tool.sent_world)

    task.tick()
    assert task.usages[0]["name"] == "release"
    assert actor.update_task_called == 0
