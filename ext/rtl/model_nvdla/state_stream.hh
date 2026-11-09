#ifndef __STATE_STREAM_HH__
#define __STATE_STREAM_HH__

// Binary streams for the wrapper checkpoint state. They do not depend
// on gem5, so the wrapper library can be built on its own.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

class StateOut {
    std::ofstream os;
    std::string path;

public:
    explicit StateOut(const std::string &_path) :
            os(_path, std::ios::binary | std::ios::trunc), path(_path) {
        if (!os) {
            fprintf(stderr, "StateOut: cannot open %s\n", path.c_str());
            abort();
        }
    }

    void bytes(const void *p, size_t n) {
        os.write(static_cast<const char *>(p), n);
        if (!os) {
            fprintf(stderr, "StateOut: write failed on %s\n", path.c_str());
            abort();
        }
    }

    template <typename T>
    void pod(const T &v) {
        static_assert(std::is_trivially_copyable<T>::value, "not POD");
        bytes(&v, sizeof(T));
    }

    template <typename T>
    void podVec(const std::vector<T> &v) {
        pod<uint64_t>(v.size());
        bytes(v.data(), v.size() * sizeof(T));
    }

    // Marks the start of a component, so a layout mismatch is caught
    // at load time instead of giving garbage state
    void tag(uint32_t t) { pod(t); }
};

class StateIn {
    std::ifstream is;
    std::string path;

public:
    explicit StateIn(const std::string &_path) :
            is(_path, std::ios::binary), path(_path) {
        if (!is) {
            fprintf(stderr, "StateIn: cannot open %s\n", path.c_str());
            abort();
        }
    }

    void bytes(void *p, size_t n) {
        is.read(static_cast<char *>(p), n);
        if (!is) {
            fprintf(stderr, "StateIn: short read on %s\n", path.c_str());
            abort();
        }
    }

    template <typename T>
    void pod(T &v) {
        static_assert(std::is_trivially_copyable<T>::value, "not POD");
        bytes(&v, sizeof(T));
    }

    template <typename T>
    T get() {
        T v;
        pod(v);
        return v;
    }

    template <typename T>
    void podVec(std::vector<T> &v) {
        v.resize(get<uint64_t>());
        bytes(v.data(), v.size() * sizeof(T));
    }

    void tag(uint32_t t) {
        uint32_t got = get<uint32_t>();
        if (got != t) {
            fprintf(stderr, "StateIn: %s: expected tag %#x, got %#x\n",
                    path.c_str(), t, got);
            abort();
        }
    }

    // Reads a count that must match the one of this instance
    void expect(uint64_t want, const char *what) {
        uint64_t got = get<uint64_t>();
        if (got != want) {
            fprintf(stderr, "StateIn: %s: %s is %lu, expected %lu\n",
                    path.c_str(), what, (unsigned long)got,
                    (unsigned long)want);
            abort();
        }
    }

    // The whole file must be consumed, anything left means a mismatch
    void expectEnd() {
        is.peek();
        if (!is.eof()) {
            fprintf(stderr, "StateIn: trailing data in %s\n", path.c_str());
            abort();
        }
    }
};

#endif // __STATE_STREAM_HH__
