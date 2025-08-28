#include <cassert>
#include <fstream>
#include <vector>
#include <cstdlib>

#define main mesh_preprocessor_main
#include "../MeshPreprocessor.cpp"
#undef main

int main() {
    const char* objPath = "test.obj";
    const char* outPath = "out.nanite";
    {
        std::ofstream obj(objPath);
        obj << "v 0 0 0\n"
            << "v 1 0 0\n"
            << "v 1 1 0\n"
            << "v 0 1 0\n"
            << "f 1 2 3\n"
            << "f 1 3 4\n";
    }
    char* argv[] = {(char*)"MeshPreprocessor", (char*)objPath, (char*)outPath};
    int ret = mesh_preprocessor_main(3, argv);
    assert(ret == 0);

    std::ifstream in(outPath, std::ios::binary);
    assert(in.good());
    uint32_t clusterCount;
    in.read(reinterpret_cast<char*>(&clusterCount), sizeof(clusterCount));
    assert(clusterCount == 1);
    Ogre::Vector3 min, max;
    in.read(reinterpret_cast<char*>(&min), sizeof(min));
    in.read(reinterpret_cast<char*>(&max), sizeof(max));
    assert(min == Ogre::Vector3(0,0,0));
    assert(max == Ogre::Vector3(1,1,0));
    uint32_t icount;
    in.read(reinterpret_cast<char*>(&icount), sizeof(icount));
    assert(icount == 6);
    std::vector<unsigned int> indices(icount);
    in.read(reinterpret_cast<char*>(indices.data()), icount * sizeof(unsigned int));
    std::vector<unsigned int> expected{0,1,2,0,2,3};
    assert(indices == expected);
    uint32_t childCount;
    in.read(reinterpret_cast<char*>(&childCount), sizeof(childCount));
    assert(childCount == 0);
    return 0;
}
