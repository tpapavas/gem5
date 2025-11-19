#include <fcntl.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

 #include "gem5/m5ops.h"

#define REGION_NVDLA (1024*1024)

// Αν θες να τρέξεις τοπικά, άφησέ το ορισμένο.
// Αν θες πραγματικά m5 ops, σβήστο το define και ξε-κάνε comment τις κλήσεις.
#define LOCAL_SIM 1

enum TASK_STATUS
{
    NOT_LAUNCHED,
    LAUNCHED,
    FINISHED
};

static inline double wall_time() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return 1. * t.tv_sec + 1.e-9 * t.tv_nsec;
}

size_t get_trace_from_file(const std::string& file_name_prefix, char* dst) {
    std::string trace_file_name = file_name_prefix + "_trace.bin";
    FILE* fp = fopen(trace_file_name.c_str(), "rb");

    if (!fp) {
        std::cout << "[ERR ] Trace file open failed: "
                  << trace_file_name << "\n";
        exit(1);
    }

    int size = 0;
    while (true) {
        int c = fgetc(fp);
        if (feof(fp)) break;
        dst[size++] = (char)c;
    }
    fclose(fp);

    // optional rd_only_var_log
    std::string var_log_file_name = file_name_prefix + "_rd_only_var_log";
    fp = fopen(var_log_file_name.c_str(), "rb");
    if (fp) {
        while (true) {
            int c = fgetc(fp);
            if (feof(fp)) break;
            dst[size++] = (char)c;
        }
        fclose(fp);
    }

    // 2 x 0xffffffff για end marker
    for (int k = 0; k < 8; ++k) dst[size++] = (char)0xff;

    return size;
}

// -------- main --------
int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cout << "usage: " << argv[0]
                  << " <trace_prefix> <batches> <workers>\n";
        return 1;
    }

    std::string trace_file_prefix(argv[1]);
    int batch_num  = atoi(argv[2]);  // π.χ. 4
    int worker_num = atoi(argv[3]);  // π.χ. 2

    // στήλες χρόνου
    int map_time_size  = batch_num + worker_num;
    // γραμμές (dummy + accelerators)
    int map_space_size = worker_num + 1;

    std::cout << "[CFG ] prefix=\"" << trace_file_prefix
              << "\" batches=" << batch_num
              << " workers=" << worker_num << "\n";
    std::cout << "[CFG ] map_time_size=" << map_time_size
              << " map_space_size=" << map_space_size
              << " cells=" << (map_time_size*map_space_size) << "\n\n";

    std::vector<uint8_t> task_map(map_time_size * map_space_size, FINISHED);

    // dump αρχικής κατάστασης (όλα F)
    std::cout << "[INIT] task_map (initial, all F)\n";
    for (int r = 0; r < map_space_size; ++r) {
        for (int c = 0; c < map_time_size; ++c) {
            std::cout << (int)task_map[r*map_time_size + c] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // *** ΣΚΟΠΙΜΑ ΔΕΝ αλλάζω το δικό σου indexing (+1) ***
    for (int i = 1; i <= worker_num; i++) {
        for (int j = 0; j < batch_num; j++) {
            int idx = i * (map_time_size + 1) + j; // το δικό σου
            if (idx >= 0 && idx < (int)task_map.size()) {
                task_map[idx] = NOT_LAUNCHED;
            }
            std::cout << "[MARK] i=" << i << " j=" << j << " -> idx=" << idx
                      << " set=NOT_LAUNCHED\n";
        }
    }
    std::cout << "\n";

    std::cout << "[INIT] task_map (after NOT_LAUNCHED marks)\n";
    for (int r = 0; r < map_space_size; ++r) {
        for (int c = 0; c < map_time_size; ++c) {
            uint8_t v = task_map[r*map_time_size + c];
            char ch = (v==FINISHED?'F':(v==NOT_LAUNCHED?'N':'L'));
            std::cout << ch << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    std::vector<uint32_t> wave_front(worker_num);
    for (int i = 0; i < worker_num; i++) wave_front[i] = i + 1;

    std::vector<uint8_t> accel_busy(worker_num, 0);

    std::vector<void*> region_nvdlas(worker_num, nullptr);
    for (int i = 0; i < worker_num; i++) {
        region_nvdlas[i] = aligned_alloc(sizeof(int)*16,
            REGION_NVDLA * batch_num);
        std::cout << "[BUF ] accel#" << (i+1)
                  << " base=" << region_nvdlas[i]
                  << " span=" << (REGION_NVDLA*batch_num) << " bytes\n";
    }
    std::cout << "\n";

    std::vector<uint32_t> trace_sizes(worker_num * batch_num, 0);
    for (int i = 0; i < worker_num * batch_num; i++) {
        int batch_id = i / worker_num + 1;   // 1..batch_num
        int accel_id = i % worker_num + 1;   // 1..worker_num
        std::string pfx = trace_file_prefix
            + std::to_string(batch_id) + '_' + std::to_string(accel_id);
        char* dst = (char*)(region_nvdlas[(accel_id-1)])
            + (batch_id-1) * REGION_NVDLA;
        size_t sz = get_trace_from_file(pfx, dst);
        trace_sizes[i] = (uint32_t)sz;
        std::cout << "[LOAD] batch=" << batch_id << " accel=" << accel_id
                  << " prefix=\"" << pfx << "\" dst=" << (void*)dst
                  << " size=" << sz << "\n";
    }
    std::cout << "\n";

    auto dump_wave = [&](const char* tag){
        std::cout << "[WAVE] " << tag
                  << " task_map rows(accel 1..N), cols(time 0..M-1)\n";
        // row 0 dummy
        for (int c = 0; c < map_time_size; ++c) std::cout << "F ";
        std::cout << "\n";
        for (int i = 0; i < worker_num; ++i) {
            for (int c = 0; c < map_time_size; ++c) {
                uint8_t v = task_map[(i+1)*map_time_size + c];
                char ch = (v==FINISHED?'F':(v==NOT_LAUNCHED?'N':'L'));
                std::cout << ch << " ";
            }
            std::cout << " | accel#" << (i+1)
                      << " wave_front=" << wave_front[i]
                      << " busy=" << (int)accel_busy[i] << "\n";
        }
        std::cout << "\n";
    };

    dump_wave("start");

    // main loop
    const int total_tasks = worker_num * batch_num;
    int finished_tasks = 0;
    int iter = 0;
    const int MAX_ITERS = 1000000; // safety

    while (true) {
        iter++;
        if (iter % 50 == 0) {
            std::cout << "[LOOP] iter=" << iter << " finished="
                      << finished_tasks << "/" << total_tasks << "\n";
        }

        // Try to launch
        for (int i = 0; i < worker_num; i++) {
            int t = wave_front[i];
            if (t < 0 || t >= map_time_size) continue;

            int idx_cur  = (i+1)*map_time_size + t;
            int idx_left = (i+1)*map_time_size + (t-1);
            int idx_diag = (i+1-1)*map_time_size + (t-1);

            uint8_t cur  = (idx_cur  >=0 && idx_cur  < (int)task_map.size())
                ? task_map[idx_cur]  : 255;
            uint8_t left = (idx_left >=0 && idx_left < (int)task_map.size())
                ? task_map[idx_left] : 255;
            uint8_t diag = (idx_diag >=0 && idx_diag < (int)task_map.size())
                ? task_map[idx_diag] : 255;

            std::cout << "[CHK ] accel#" << (i+1) << " t=" << t
                      << " idx(cur)=" << idx_cur
                      << " idx(left)=" << idx_left
                      << " idx(diag)=" << idx_diag
                      << " | cur=" << (int)cur
                      << " left=" << (int)left
                      << " diag=" << (int)diag
                      << " busy=" << (int)accel_busy[i] << "\n";

            if (!accel_busy[i]
                && cur  == NOT_LAUNCHED
                && left == FINISHED
                && diag == FINISHED)
            {
                int batch_id = t - i; // 1..batch_num
                if (batch_id >= 1 && batch_id <= batch_num) {
                    uint64_t trace_addr = (uint64_t)(region_nvdlas[i])
                        + (uint64_t)(batch_id-1) * REGION_NVDLA;
                    int vec_idx = worker_num * (batch_id - 1) + i;
                    uint32_t tsize = trace_sizes[vec_idx];

                    std::cout << std::fixed << std::setprecision(6);
                    std::cout << "[LAUN] accel#" << (i+1)
                              << " batch=" << batch_id
                              << " wave_t=" << t
                              << " trace_addr=0x" << std::hex << trace_addr
                              << std::dec
                              << " size=" << tsize
                              << " @t=" << wall_time() << "\n";

                    // m5_start_accel_id(trace_addr, tsize, trace_addr, i);
                    accel_busy[i] = 1;
                    task_map[idx_cur] = LAUNCHED;
                } else {
                    std::cout << "[SKIP] accel#" << (i+1) << " t=" << t
                              << " batch=" << (t - i) << " (out of 1.."
                              << batch_num << ")\n";
                }
            }
        }

        // Try to finish
        for (int i = 0; i < worker_num; i++) {
            if (accel_busy[i]) {
#ifdef LOCAL_SIM
                // τοπική προσομοίωση: “τελειώνει” άμεσα
                // με λίγο delay για να φαίνεται
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                int still_busy = 0;
#else
                int still_busy = m5_wait_accel_id(i);
#endif
                if (!still_busy) {
                    int t_done = wave_front[i];
                    int batch_id = t_done - i;

                    std::cout << std::fixed << std::setprecision(6);
                    std::cout << "[DONE] accel#" << (i+1)
                              << " batch=" << batch_id
                              << " wave_t=" << t_done
                              << " @t=" << wall_time() << "\n";

                    int idx_cur = (i+1)*map_time_size + t_done;
                    if (idx_cur >= 0 && idx_cur < (int)task_map.size())
                        task_map[idx_cur] = FINISHED;

                    if (batch_id >= 1 && batch_id <= batch_num)
                        finished_tasks++;

                    wave_front[i]++;
                    accel_busy[i] = 0;
                } else {
                    accel_busy[i] = 1;
                }
            }
        }

        // Optional visualization ανά n βήματα
        if (iter % 20 == 0) {
            dump_wave("progress");
        }

        // Έξοδος όταν ολοκληρωθούν όλα τα “valid” tasks
        if (finished_tasks >= total_tasks) {
            std::cout << "\n[EXIT] finished all ops ("
                      << finished_tasks << "/" << total_tasks
                      << ") @t=" << wall_time() << "\n";
            dump_wave("final");
            break;
        }

        // Safety guard για να μη μείνει σε infinite loop σε δοκιμές
        if (iter > MAX_ITERS) {
            std::cout << "\n[EXIT] max iters reached, breaking. finished="
                      << finished_tasks
                      << "/" << total_tasks << "\n";
            dump_wave("final-ish");
            break;
        }
    }

    return 0;
}
