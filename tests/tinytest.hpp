// Minimal test harness: no dependencies, runs on the host.
#pragma once
#include <cstdio>
#include <vector>

struct TestCase { const char* name; void (*fn)(); };
inline std::vector<TestCase>& testRegistry() { static std::vector<TestCase> r; return r; }
inline int& testFailures() { static int f = 0; return f; }
inline const char*& testCurrent() { static const char* n = ""; return n; }
struct TestRegistrar { TestRegistrar(const char* n, void (*f)()) { testRegistry().push_back({n, f}); } };

#define TEST(name) \
    static void name(); \
    static TestRegistrar reg_##name(#name, name); \
    static void name()

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL %s:%d [%s] CHECK(%s)\n", __FILE__, __LINE__, testCurrent(), #cond); \
            ++testFailures(); \
        } \
    } while (0)

#define CHECK_EQ(got, want) \
    do { \
        const long long g_ = static_cast<long long>(got); \
        const long long w_ = static_cast<long long>(want); \
        if (g_ != w_) { \
            std::printf("FAIL %s:%d [%s] %s: got %lld, want %lld\n", __FILE__, __LINE__, testCurrent(), #got, g_, w_); \
            ++testFailures(); \
        } \
    } while (0)
