#pragma once
#include <algorithm>
#include "OgreVector3.h"
namespace Ogre {
class AxisAlignedBox {
    Vector3 mMin, mMax;
    bool mNull;
public:
    AxisAlignedBox() { setNull(); }
    void setNull() { mMin = Vector3(); mMax = Vector3(); mNull = true; }
    void merge(const Vector3& v) {
        if (mNull) {
            mMin = mMax = v;
            mNull = false;
        } else {
            mMin.x = std::min(mMin.x, v.x);
            mMin.y = std::min(mMin.y, v.y);
            mMin.z = std::min(mMin.z, v.z);
            mMax.x = std::max(mMax.x, v.x);
            mMax.y = std::max(mMax.y, v.y);
            mMax.z = std::max(mMax.z, v.z);
        }
    }
    Vector3 getMinimum() const { return mMin; }
    Vector3 getMaximum() const { return mMax; }
};
}
