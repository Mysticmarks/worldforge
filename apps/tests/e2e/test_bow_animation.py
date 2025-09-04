import types


def test_bow_animations(bow_module, operation, entity, vector3d):
    Operation = operation
    Entity = entity
    Vector3D = vector3d

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

    Bow = bow_module
    actor = Actor()
    tool = Tool()
    usage = UsageInstance(actor, tool)
    res = Bow.Oplist()
    direction = Vector3D(1, 0, 0)

    Bow.shoot_in_direction(direction, usage, res)

    assert any(
        op.name == "start_action" and op.entity["action"] == "bow/releasing" for op in res
    )
    assert any(
        op.name == "sight"
        and op.entity.name == "activity"
        and op.entity.entity["action"] == "shoot"
        for op in res
    )

    task = Bow.DrawBow(usage, tick_interval=0, name="Draw")
    task.setup("t1")
    assert ("bow/drawing", None) in actor.actions
    assert any(
        op.name == "sight"
        and op.entity.name == "activity"
        and op.entity.entity["action"] == "draw"
        for op in tool.sent_world
    )

    task.tick()
    assert task.usages[0]["name"] == "release"
    assert actor.update_task_called == 0
