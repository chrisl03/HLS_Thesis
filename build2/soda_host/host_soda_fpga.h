//  host_soda_fpga.h
#ifndef HOST_SODA_FPGA_H__
#define HOST_SODA_FPGA_H__

// Matrix dims (host-visible)
#include "host_visible.h"

#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_kernel.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

class FPGA_SODA {
public:
    int fpga_init(const std::string& xclbin_path, unsigned int device_index = 0);
    void warmup(unsigned int iterations = 1);
    void run();
    void run_benchmark(unsigned int iterations);
    void run_benchmark_hostdatatransfer(unsigned int iterations);
    void print_performance_timings() const;
    void save_results_to_csv(const std::string& filename) const;

    // ins outs -- 2 input halves, 2 output halves (interleaved)
    float* get_inA0_ptr();
    float* get_inA1_ptr();
    float* get_outB0_ptr();
    float* get_outB1_ptr();

    // sizes (in floats). Κάθε channel παίρνει τα μισά bursts/packets.
    static constexpr std::size_t A0_ELEMS = SODA_BURSTS_IN_0   * 16;
    static constexpr std::size_t A1_ELEMS = SODA_BURSTS_IN_1   * 16;
    static constexpr std::size_t B0_ELEMS = SODA_PACKETS_OUT_0 * SODA_K;
    static constexpr std::size_t B1_ELEMS = SODA_PACKETS_OUT_1 * SODA_K;

private:
    xrt::device device;
    xrt::kernel soda_kernel;

    // 2 input + 2 output buffer objects
    xrt::bo inA0_bo;
    xrt::bo inA1_bo;
    xrt::bo outB0_bo;
    xrt::bo outB1_bo;

    float* inA0_host_ptr;
    float* inA1_host_ptr;
    float* outB0_host_ptr;
    float* outB1_host_ptr;

    xrt::run run_soda;

    // Timing data
    std::chrono::duration<double, std::micro> time_program_fpga;
    std::chrono::duration<double, std::micro> time_allocate_buffers;
    std::vector<std::chrono::duration<double, std::micro>> warmup_timings;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_input_to_device;
    std::vector<std::chrono::duration<double, std::micro>> time_kernel_execution;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_output_to_host;
};

#endif // HOST_SODA_FPGA_H__