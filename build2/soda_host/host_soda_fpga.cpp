//  host_soda_fpga.cpp
#include "host_soda_fpga.h"

#include <experimental/xrt_xclbin.h>
#include <iostream>

int FPGA_SODA::fpga_init(const std::string& xclbin_path, const unsigned int device_index) {
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    device = xrt::device(device_index);

    std::cout << "Trying to program device[" << device_index << "] with name: "
              << device.get_info<xrt::info::device::name>()
              << " and bdf: " << device.get_info<xrt::info::device::bdf>()
              << std::endl;

    auto uuid = device.load_xclbin(xclbin_path);

    soda_kernel = xrt::kernel(device, uuid, "architecture_top_level");

    auto end_program = clock::now();
    time_program_fpga = end_program - start;

    std::cout << "Device[" << device_index << "]: programmed successfully. Kernel handle: "
              << soda_kernel.get_handle() << std::endl;

    std::cout << "Allocate buffers in global memory (2R+2W interleaved)" << std::endl;
    auto start_alloc = clock::now();

    const std::size_t A0_BYTES = A0_ELEMS * sizeof(float);
    const std::size_t A1_BYTES = A1_ELEMS * sizeof(float);
    const std::size_t B0_BYTES = B0_ELEMS * sizeof(float);
    const std::size_t B1_BYTES = B1_ELEMS * sizeof(float);

    // group_id αντιστοιχεί στη σειρά των kernel arguments:
    //   arg0 = A_in_mem_0, arg1 = A_in_mem_1, arg2 = B_out_mem_0, arg3 = B_out_mem_1
    inA0_bo  = xrt::bo(device, A0_BYTES, soda_kernel.group_id(0));
    inA1_bo  = xrt::bo(device, A1_BYTES, soda_kernel.group_id(1));
    outB0_bo = xrt::bo(device, B0_BYTES, soda_kernel.group_id(2));
    outB1_bo = xrt::bo(device, B1_BYTES, soda_kernel.group_id(3));

    inA0_host_ptr  = inA0_bo.map<float*>();
    inA1_host_ptr  = inA1_bo.map<float*>();
    outB0_host_ptr = outB0_bo.map<float*>();
    outB1_host_ptr = outB1_bo.map<float*>();

    std::cout << "Mapped pointers: A0=" << (void*)inA0_host_ptr
              << " A1=" << (void*)inA1_host_ptr
              << " B0=" << (void*)outB0_host_ptr
              << " B1=" << (void*)outB1_host_ptr << std::endl;

    if (inA0_host_ptr == nullptr || inA1_host_ptr == nullptr ||
        outB0_host_ptr == nullptr || outB1_host_ptr == nullptr) {
        std::cerr << "Error: Failed to map XRT Buffer Objects to host memory!" << std::endl;
        return -1;
    }

    run_soda = xrt::run(soda_kernel);
    run_soda.set_arg(0, inA0_bo);
    run_soda.set_arg(1, inA1_bo);
    run_soda.set_arg(2, outB0_bo);
    run_soda.set_arg(3, outB1_bo);

    auto end_alloc = clock::now();
    time_allocate_buffers = end_alloc - start_alloc;

    return 0;
}

void FPGA_SODA::warmup(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    warmup_timings.clear();
    warmup_timings.reserve(iterations);

    for (unsigned int i = 0; i < iterations; ++i) {
        auto start = clock::now();
        run_soda.start();
        run_soda.wait();
        auto end = clock::now();
        warmup_timings.push_back(end - start);
        std::cout << "Warmed up (" << (i + 1) << "/" << iterations << ")" << std::endl;
    }
}

void FPGA_SODA::run() {
    using clock = std::chrono::high_resolution_clock;

    auto start_copy_in = clock::now();
    inA0_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA1_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto end_copy_in = clock::now();
    time_copy_input_to_device.push_back(end_copy_in - start_copy_in);

    auto start_kernel = clock::now();
    run_soda.start();
    run_soda.wait();
    auto end_kernel = clock::now();
    time_kernel_execution.push_back(end_kernel - start_kernel);

    auto start_copy_out = clock::now();
    outB0_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB1_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    auto end_copy_out = clock::now();
    time_copy_output_to_host.push_back(end_copy_out - start_copy_out);
}

void FPGA_SODA::run_benchmark(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    std::vector<double> kernel_ms;
    kernel_ms.reserve(iterations);

    for (unsigned int i = 0; i < iterations; ++i) {
        auto start = clock::now();
        run_soda.start();
        run_soda.wait();
        auto end = clock::now();
        kernel_ms.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    double sum = 0.0;
    for (auto v : kernel_ms) sum += v;
    std::cout << "Kernel-only benchmark over " << iterations
              << " iters, avg: " << (sum / iterations) << " ms" << std::endl;
}

void FPGA_SODA::run_benchmark_hostdatatransfer(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    std::vector<double> copy_in_ms, kernel_ms, copy_out_ms, total_ms;

    for (unsigned int i = 0; i < iterations; ++i) {
        auto start_total = clock::now();

        auto start_ci = clock::now();
        inA0_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        inA1_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        auto end_ci = clock::now();
        copy_in_ms.push_back(std::chrono::duration<double, std::milli>(end_ci - start_ci).count());

        auto start_k = clock::now();
        run_soda.start();
        run_soda.wait();
        auto end_k = clock::now();
        kernel_ms.push_back(std::chrono::duration<double, std::milli>(end_k - start_k).count());

        auto start_co = clock::now();
        outB0_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        outB1_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        auto end_co = clock::now();
        copy_out_ms.push_back(std::chrono::duration<double, std::milli>(end_co - start_co).count());

        auto end_total = clock::now();
        total_ms.push_back(std::chrono::duration<double, std::milli>(end_total - start_total).count());
    }

    double avg_copy_in_ms = 0, avg_kernel_ms = 0, avg_copy_out_ms = 0, avg_total_ms = 0;
    for (unsigned int i = 0; i < iterations; ++i) {
        avg_copy_in_ms += copy_in_ms[i];
        avg_kernel_ms += kernel_ms[i];
        avg_copy_out_ms += copy_out_ms[i];
        avg_total_ms += total_ms[i];
    }

    std::cout << "Benchmarking with data transfers over " << iterations << " iterations." << std::endl;
    std::cout << "Average Input Transfer ms: " << avg_copy_in_ms / iterations << " ms" << std::endl;
    std::cout << "Average Kernel Execution ms: " << avg_kernel_ms / iterations << " ms" << std::endl;
    std::cout << "Average Output Transfer ms: " << avg_copy_out_ms / iterations << " ms" << std::endl;
    std::cout << "Average Total ms: " << avg_total_ms / iterations << " ms" << std::endl;
}

void FPGA_SODA::print_performance_timings() const {
    std::cout << "FPGA Initialization Timings:" << std::endl;
    std::cout << "  Time to program FPGA: " << time_program_fpga.count() << " us" << std::endl;
    std::cout << "  Time to allocate buffers: " << time_allocate_buffers.count() << " us" << std::endl;

    if (!warmup_timings.empty()) {
        double sum = 0.0;
        for (auto& t : warmup_timings) sum += t.count();
        std::cout << "Warmup Timings:" << std::endl;
        std::cout << "  iters: " << warmup_timings.size()
                  << ", avg: " << (sum / warmup_timings.size()) << " us" << std::endl;
    }

    std::cout << "Run Timings:" << std::endl;
    for (std::size_t i = 0; i < time_kernel_execution.size(); ++i) {
        std::cout << "  Run " << (i + 1) << ":" << std::endl;
        std::cout << "    copy-in : " << time_copy_input_to_device[i].count() << " us" << std::endl;
        std::cout << "    kernel  : " << time_kernel_execution[i].count() << " us" << std::endl;
        std::cout << "    copy-out: " << time_copy_output_to_host[i].count() << " us" << std::endl;
    }
}

void FPGA_SODA::save_results_to_csv(const std::string& filename) const {
    std::ofstream csv_file(filename, std::ios::app);
    if (!csv_file.is_open()) return;

    size_t n = time_kernel_execution.size();
    if (n == 0) return;

    double avg_in = 0, avg_k = 0, avg_out = 0;
    for (size_t i = 0; i < n; ++i) {
        avg_in  += time_copy_input_to_device[i].count();
        avg_k   += time_kernel_execution[i].count();
        avg_out += time_copy_output_to_host[i].count();
    }

    avg_in /= n; avg_k /= n; avg_out /= n;
    csv_file << n << "," << avg_in << "," << avg_k << "," << avg_out << "," << (avg_in + avg_k + avg_out) << "\n";
    csv_file.close();
}

float* FPGA_SODA::get_inA0_ptr()  { return inA0_host_ptr; }
float* FPGA_SODA::get_inA1_ptr()  { return inA1_host_ptr; }
float* FPGA_SODA::get_outB0_ptr() { return outB0_host_ptr; }
float* FPGA_SODA::get_outB1_ptr() { return outB1_host_ptr; }