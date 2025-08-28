#pragma once
namespace Ogre {
struct Vector3 {
    float x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    bool operator==(const Vector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};
}
