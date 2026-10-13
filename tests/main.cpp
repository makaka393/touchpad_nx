#include "tinytest.hpp"

int main() {
    for (const TestCase& t : testRegistry()) {
        testCurrent() = t.name;
        t.fn();
    }
    std::printf("%zu tests, %d failed checks\n", testRegistry().size(), testFailures());
    return testFailures() ? 1 : 0;
}
