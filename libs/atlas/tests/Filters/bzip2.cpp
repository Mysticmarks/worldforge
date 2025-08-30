#if defined(HAVE_BZLIB_H) && defined(HAVE_LIBBZ2)
#include <Atlas/Filters/Bzip2.h>
#include <cassert>
#include <stdexcept>
#include <string>

using Atlas::Filters::Bzip2;

void testDecodeMalformed() {
    Bzip2 filter;
    filter.begin();
    bool threw = false;
    try {
        filter.decode("not a bzip2 stream");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    filter.end();
    assert(threw);
}

void testEncodeAfterEnd() {
    Bzip2 filter;
    filter.begin();
    filter.end();
    bool threw = false;
    try {
        filter.encode(std::string(50000, 'x'));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

int main() {
    testDecodeMalformed();
    testEncodeAfterEnd();
    return 0;
}
#else
int main() { return 0; }
#endif
