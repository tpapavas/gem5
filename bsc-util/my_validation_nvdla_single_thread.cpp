#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "gem5/m5ops.h"

static constexpr std::size_t ALIGNMENT = 64;

// 256 MiB — χωρίς * 8
static constexpr std::size_t REGION_NVDLA =
    256ULL * 1024ULL * 1024ULL;

double wall_time()
{
    struct timespec t {};

    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        std::perror("clock_gettime");
        return 0.0;
    }

    return static_cast<double>(t.tv_sec) +
           1.0e-9 * static_cast<double>(t.tv_nsec);
}

bool append_file(
    const char* path,
    const char* description,
    std::uint8_t* destination,
    std::size_t capacity,
    std::size_t& used)
{
    FILE* fp = std::fopen(path, "rb");

    if (fp == nullptr) {
        std::fprintf(
            stderr,
            "%s open failed: %s: %s\n",
            description,
            path,
            std::strerror(errno)
        );
        return false;
    }

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fprintf(
            stderr,
            "%s: fseek failed: %s\n",
            description,
            std::strerror(errno)
        );
        std::fclose(fp);
        return false;
    }

    const long file_size_signed = std::ftell(fp);

    if (file_size_signed < 0) {
        std::fprintf(
            stderr,
            "%s: ftell failed: %s\n",
            description,
            std::strerror(errno)
        );
        std::fclose(fp);
        return false;
    }

    const std::size_t file_size =
        static_cast<std::size_t>(file_size_signed);

    if (file_size > capacity - used) {
        std::fprintf(
            stderr,
            "%s is too large: file=%zu bytes, remaining=%zu bytes\n",
            description,
            file_size,
            capacity - used
        );
        std::fclose(fp);
        return false;
    }

    std::rewind(fp);

    const std::size_t bytes_read =
        std::fread(destination + used, 1, file_size, fp);

    if (bytes_read != file_size) {
        std::fprintf(
            stderr,
            "%s read failed: expected=%zu, received=%zu\n",
            description,
            file_size,
            bytes_read
        );
        std::fclose(fp);
        return false;
    }

    used += bytes_read;
    std::fclose(fp);

    std::printf(
        "%s opened successfully: %s (%zu bytes)\n",
        description,
        path,
        file_size
    );

    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::fprintf(
            stderr,
            "Usage: %s <trace.bin> [rd_only_var_log]\n",
            argv[0]
        );
        return EXIT_FAILURE;
    }

    std::printf(
        "REGION_NVDLA = %zu bytes (%zu MiB)\n",
        REGION_NVDLA,
        REGION_NVDLA / (1024ULL * 1024ULL)
    );

    void* region_nvdla = nullptr;

    const int allocation_result = posix_memalign(
        &region_nvdla,
        ALIGNMENT,
        REGION_NVDLA
    );

    if (allocation_result != 0 || region_nvdla == nullptr) {
        std::fprintf(
            stderr,
            "posix_memalign failed: %s\n",
            std::strerror(allocation_result)
        );
        return EXIT_FAILURE;
    }

    std::printf("region_nvdla = %p\n", region_nvdla);

    auto* buffer = static_cast<std::uint8_t*>(region_nvdla);
    std::size_t used = 0;

    if (!append_file(
            argv[1],
            "Trace file",
            buffer,
            REGION_NVDLA,
            used)) {
        std::free(region_nvdla);
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        if (!append_file(
                argv[2],
                "rd_only_var_log",
                buffer,
                REGION_NVDLA,
                used)) {
            std::free(region_nvdla);
            return EXIT_FAILURE;
        }
    }

    std::printf("Total input size = %zu bytes\n", used);
    std::printf("start_time = %f\n", wall_time());
    std::fflush(stdout);

    m5_start_accel_id(
        reinterpret_cast<std::uint64_t>(region_nvdla),
        used,
        reinterpret_cast<std::uint64_t>(region_nvdla),
        0
    );

    while (m5_wait_accel_id(0)) {
        // Wait for NVDLA completion.
    }

    std::printf("end_time = %f\n", wall_time());

    std::free(region_nvdla);
    return EXIT_SUCCESS;
}