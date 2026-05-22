#include "host_visible.h"

#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_bo.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void compute_golden(
    const std::vector<float>& A_vec,
    std::vector<float>& B_golden_vec
) {
    B_golden_vec.clear();
    B_golden_vec.reserve(KERNEL_ITERATIONS);

    for (int i = 1; i < ROWS - 1; i++) {
        for (int j = 1; j < COLUMNS - 1; j++) {
            float center = A_vec[i * COLUMNS + j];
            float down   = A_vec[(i + 1) * COLUMNS + j];
            float right  = A_vec[i * COLUMNS + (j + 1)];
            float left   = A_vec[i * COLUMNS + (j - 1)];
            float up     = A_vec[(i - 1) * COLUMNS + j];

            float res_0 = center - left;
            float res_1 = center - right;
            float res_2 = center - up;
            float res_3 = center - down;

            float b_val =
                (res_0 * res_0) +
                (res_1 * res_1) +
                (res_2 * res_2) +
                (res_3 * res_3);

            B_golden_vec.push_back(b_val);
        }
    }
}

static xrt::kernel open_kernel(
    const xrt::device& device,
    const xrt::uuid& uuid
) {
    try {
        return xrt::kernel(
            device,
            uuid,
            "architecture_top_level:{architecture_top_level}"
        );
    } catch (const std::exception&) {
        return xrt::kernel(
            device,
            uuid,
            "architecture_top_level"
        );
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr
            << "Usage: " << argv[0] << " <architecture_top_level.xclbin> [device_index]\n";
        return 1;
    }

    const std::string xclbin_path = argv[1];
    const unsigned int device_index =
        (argc >= 3) ? static_cast<unsigned int>(std::stoi(argv[2])) : 0;

    std::cout << "[HOST] SODA LCS XRT host\n";
    std::cout << "[HOST] K=" << K
              << ", ROWS=" << ROWS
              << ", COLUMNS=" << COLUMNS << "\n";

    std::vector<float> input(TOTAL_PIXELS);
    std::vector<float> golden;

    for (int i = 0; i < TOTAL_PIXELS; i++) {
        input[i] = static_cast<float>(i % 256) / 10.0f;
    }

    compute_golden(input, golden);

    std::cout << "[HOST] Golden outputs: " << golden.size() << "\n";
    std::cout << "[HOST] Input bytes    : " << INPUT_BYTES << "\n";
    std::cout << "[HOST] Output bytes   : " << OUTPUT_BYTES << "\n";

    try {
        auto device = xrt::device(device_index);

        std::cout << "[HOST] Programming device " << device_index << " with:\n";
        std::cout << "       " << xclbin_path << "\n";

        auto uuid = device.load_xclbin(xclbin_path);
        auto kernel = open_kernel(device, uuid);

        std::cout << "[HOST] Kernel opened successfully.\n";

        auto in_bo = xrt::bo(device, INPUT_BYTES, kernel.group_id(0));
        auto out_bo = xrt::bo(device, OUTPUT_BYTES, kernel.group_id(1));

        auto in_ptr = in_bo.map<float*>();
        auto out_ptr = out_bo.map<float*>();

        if (in_ptr == nullptr || out_ptr == nullptr) {
            std::cerr << "[HOST] ERROR: Failed to map XRT BOs.\n";
            return 1;
        }

        std::copy(input.begin(), input.end(), in_ptr);
        std::fill(out_ptr, out_ptr + OUTPUT_FLOATS, 0.0f);

        in_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        auto run = xrt::run(kernel);
        run.set_arg(0, in_bo);
        run.set_arg(1, out_bo);

        std::cout << "[HOST] Running kernel...\n";

        auto t0 = std::chrono::high_resolution_clock::now();

        run.start();
        run.wait();

        auto t1 = std::chrono::high_resolution_clock::now();

        out_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

        double kernel_ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "[HOST] Kernel execution time: "
                  << kernel_ms << " ms\n";

        int errors = 0;

        for (int i = 0; i < KERNEL_ITERATIONS; i++) {
            int pack_idx = i / K;
            int elem_idx = i % K;

            float hls_result = out_ptr[pack_idx * K + elem_idx];
            float golden_result = golden[i];

            if (std::fabs(hls_result - golden_result) > 0.001f) {
                errors++;

                if (errors <= 10) {
                    int row = (i / KERNEL_COLS) + 1;
                    int col = (i % KERNEL_COLS) + 1;

                    std::cout
                        << "[ERROR] index=" << i
                        << " row=" << row
                        << " col=" << col
                        << " FPGA=" << hls_result
                        << " GOLDEN=" << golden_result
                        << "\n";
                }
            }
        }

        if (errors == 0) {
            std::cout << "\n=======================================\n";
            std::cout << "  TEST PASSED! 0 errors detected.\n";
            std::cout << "=======================================\n";
            return 0;
        } else {
            std::cout << "\n=======================================\n";
            std::cout << "  TEST FAILED! " << errors << " mismatches found.\n";
            std::cout << "=======================================\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "[HOST] ERROR: " << e.what() << "\n";
        return 1;
    }
}