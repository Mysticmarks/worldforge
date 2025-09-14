#include "CullingManager.h"

#include <OgreHardwareOcclusionQuery.h>
#include <OgreHardwareBufferManager.h>
#include <OgreMatrix4.h>
#include <OgreRenderSystem.h>
#include <OgreRoot.h>
#include <OgreVertexIndexData.h>

namespace Ember::OgreView {


CullingManager::CullingManager(Ogre::Camera* camera) : mCamera(camera) {}

CullingManager::~CullingManager() {
    if (mQuery) {
        if (auto* root = Ogre::Root::getSingletonPtr()) {
            if (auto* rs = root->getRenderSystem()) {
                rs->destroyHardwareOcclusionQuery(mQuery);
            }
        }
        mQuery = nullptr;
    }
    delete mVertexData;
    delete mIndexData;
}

void CullingManager::setCamera(Ogre::Camera* camera) { mCamera = camera; }

bool CullingManager::isVisible(const Ogre::AxisAlignedBox& bounds) const {
    if (!mCamera) {
        return true;
    }
    return mCamera->isVisible(bounds);
}

void CullingManager::ensureQueryObjects() const {
    auto* root = Ogre::Root::getSingletonPtr();
    if (!root) {
        return;
    }

    Ogre::RenderSystem* rs = root->getRenderSystem();
    if (!rs) {
        return;
    }

    if (!mQuery && rs->getCapabilities()->hasCapability(Ogre::RSC_HWOCCLUSION)) {
        mQuery = rs->createHardwareOcclusionQuery();
    }

    if (!mVertexData) {
        static const float cubeVerts[24] = {
            -0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f
        };

        mVertexData = OGRE_NEW Ogre::VertexData();
        mVertexData->vertexStart = 0;
        mVertexData->vertexCount = 8;
        mVertexData->vertexDeclaration =
            Ogre::HardwareBufferManager::getSingleton().createVertexDeclaration();
        mVertexData->vertexDeclaration->addElement(0, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
        mVertexData->vertexBufferBinding =
            Ogre::HardwareBufferManager::getSingleton().createVertexBufferBinding();
        Ogre::HardwareVertexBufferSharedPtr vbuf =
            Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
                3 * sizeof(float), 8, Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
        vbuf->writeData(0, vbuf->getSizeInBytes(), cubeVerts, true);
        mVertexData->vertexBufferBinding->setBinding(0, vbuf);
    }

    if (!mIndexData) {
        static const unsigned short cubeIdx[36] = {
            0,1,2, 2,3,0, // back
            4,5,6, 6,7,4, // front
            0,1,5, 5,4,0, // bottom
            2,3,7, 7,6,2, // top
            0,3,7, 7,4,0, // left
            1,2,6, 6,5,1  // right
        };
        mIndexData = OGRE_NEW Ogre::IndexData();
        mIndexData->indexStart = 0;
        mIndexData->indexCount = 36;
        Ogre::HardwareIndexBufferSharedPtr ibuf =
            Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
                Ogre::HardwareIndexBuffer::IT_16BIT, 36,
                Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
        ibuf->writeData(0, ibuf->getSizeInBytes(), cubeIdx, true);
        mIndexData->indexBuffer = ibuf;
    }
}

bool CullingManager::isOccluded(const Ogre::AxisAlignedBox& bounds) const {
    if (!mCamera) {
        return false;
    }

    auto* root = Ogre::Root::getSingletonPtr();
    if (!root) {
        return false;
    }

    Ogre::RenderSystem* rs = root->getRenderSystem();
    if (!rs || !rs->getCapabilities()->hasCapability(Ogre::RSC_HWOCCLUSION)) {
        return false;
    }

    ensureQueryObjects();
    if (!mQuery) {
        return false;
    }

    Ogre::Matrix4 world;
    world.makeTransform(bounds.getCenter(), bounds.getSize(), Ogre::Quaternion::IDENTITY);

    rs->_setWorldMatrix(world);
    rs->_setViewMatrix(mCamera->getViewMatrix(true));
    rs->_setProjectionMatrix(mCamera->getProjectionMatrixWithRSDepth());

    Ogre::RenderOperation op;
    op.operationType = Ogre::RenderOperation::OT_TRIANGLE_LIST;
    op.useIndexes = true;
    op.vertexData = mVertexData;
    op.indexData = mIndexData;

    mQuery->beginOcclusionQuery();
    rs->_render(op);
    mQuery->endOcclusionQuery();

    unsigned int visiblePixels = 0;
    if (mQuery->pullOcclusionQuery(&visiblePixels)) {
        return visiblePixels == 0;
    }

    return false;
}

} // namespace Ember::OgreView

