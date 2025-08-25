#include "apps/ember/src/components/ogre/lod/NaniteLodDefinition.h"
#include <OgreVector3.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace Ember::OgreView::Lod;

struct ObjMesh {
    std::vector<Ogre::Vector3> vertices;
    std::vector<unsigned int> indices;
};

static ObjMesh loadObj(const std::string& path) {
    ObjMesh mesh;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string tag;
        if (!(iss >> tag)) continue;
        if (tag == "v") {
            float x, y, z; iss >> x >> y >> z;
            mesh.vertices.emplace_back(x, y, z);
        } else if (tag == "f") {
            unsigned int a, b, c;
            if (iss >> a >> b >> c) {
                // OBJ indices are 1-based
                mesh.indices.push_back(a - 1);
                mesh.indices.push_back(b - 1);
                mesh.indices.push_back(c - 1);
            }
        }
    }
    return mesh;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " input.obj output.nanite" << std::endl;
        return 1;
    }

    ObjMesh mesh = loadObj(argv[1]);
    NaniteLodDefinition def(nullptr, argv[2], 0, "General");

    const size_t clusterTris = 64; // simple fixed cluster size
    for (size_t i = 0; i < mesh.indices.size(); i += clusterTris * 3) {
        NaniteLodDefinition::Cluster c;
        for (size_t j = i; j < std::min(mesh.indices.size(), i + clusterTris * 3); j += 3) {
            unsigned int ia = mesh.indices[j];
            unsigned int ib = mesh.indices[j + 1];
            unsigned int ic = mesh.indices[j + 2];
            c.indices.push_back(ia);
            c.indices.push_back(ib);
            c.indices.push_back(ic);
            c.bounds.merge(mesh.vertices[ia]);
            c.bounds.merge(mesh.vertices[ib]);
            c.bounds.merge(mesh.vertices[ic]);
        }
        def.addCluster(std::move(c));
    }

    std::ofstream out(argv[2], std::ios::binary);
    const auto& clusters = def.getClusters();
    uint32_t count = static_cast<uint32_t>(clusters.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& c : clusters) {
        Ogre::Vector3 min = c.bounds.getMinimum();
        Ogre::Vector3 max = c.bounds.getMaximum();
        out.write(reinterpret_cast<const char*>(&min), sizeof(min));
        out.write(reinterpret_cast<const char*>(&max), sizeof(max));
        uint32_t icount = static_cast<uint32_t>(c.indices.size());
        out.write(reinterpret_cast<const char*>(&icount), sizeof(icount));
        out.write(reinterpret_cast<const char*>(c.indices.data()), icount * sizeof(unsigned int));
        uint32_t childCount = static_cast<uint32_t>(c.children.size());
        out.write(reinterpret_cast<const char*>(&childCount), sizeof(childCount));
        if (childCount)
            out.write(reinterpret_cast<const char*>(c.children.data()), childCount * sizeof(size_t));
    }
    return 0;
}

