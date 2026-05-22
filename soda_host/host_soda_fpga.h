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
    
    // ins outs
    float* get_inA_ptr();
    float* get_outB_ptr();

    // sizes (in floats)
    static constexpr std::size_t A_ELEMS = SODA_BURSTS_IN * 16;
    static constexpr std::size_t B_ELEMS = SODA_TOTAL_PACKETS_OUT * SODA_K;

private:
    xrt::device device;
    xrt::kernel soda_kernel;

    xrt::bo inA_bo;
    xrt::bo outB_bo;

    float* inA_host_ptr;
    float* outB_host_ptr;

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