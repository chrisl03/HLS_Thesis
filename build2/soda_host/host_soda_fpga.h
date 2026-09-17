//  host_soda_fpga.h  (K=64, 8 read + 8 write channels)
#ifndef HOST_SODA_FPGA_H__
#define HOST_SODA_FPGA_H__

#include "host_visible.h"
#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_kernel.h>
#include <cmath>
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
    void print_performance_timings() const;
    void save_results_to_csv(const std::string& filename) const;

    float* get_inA0_ptr();
    float* get_inA1_ptr();
    float* get_inA2_ptr();
    float* get_inA3_ptr();
    float* get_inA4_ptr();
    float* get_inA5_ptr();
    float* get_inA6_ptr();
    float* get_inA7_ptr();
    float* get_outB0_ptr();
    float* get_outB1_ptr();
    float* get_outB2_ptr();
    float* get_outB3_ptr();
    float* get_outB4_ptr();
    float* get_outB5_ptr();
    float* get_outB6_ptr();
    float* get_outB7_ptr();

    // Καθε channel: BURSTS_PER_CH_EVEN bursts x 16 floats (even channels, το max)
    static constexpr std::size_t AIN_ELEMS = (std::size_t)BURSTS_PER_CH_EVEN * 16;
    static constexpr std::size_t BOUT_ELEMS = (std::size_t)BURSTS_PER_CH_EVEN * 16;

private:
    xrt::device device;
    xrt::kernel soda_kernel;
    xrt::run run_soda;

    xrt::bo inA0_bo;
    xrt::bo inA1_bo;
    xrt::bo inA2_bo;
    xrt::bo inA3_bo;
    xrt::bo inA4_bo;
    xrt::bo inA5_bo;
    xrt::bo inA6_bo;
    xrt::bo inA7_bo;
    xrt::bo outB0_bo;
    xrt::bo outB1_bo;
    xrt::bo outB2_bo;
    xrt::bo outB3_bo;
    xrt::bo outB4_bo;
    xrt::bo outB5_bo;
    xrt::bo outB6_bo;
    xrt::bo outB7_bo;
    float* inA0_host_ptr;
    float* inA1_host_ptr;
    float* inA2_host_ptr;
    float* inA3_host_ptr;
    float* inA4_host_ptr;
    float* inA5_host_ptr;
    float* inA6_host_ptr;
    float* inA7_host_ptr;
    float* outB0_host_ptr;
    float* outB1_host_ptr;
    float* outB2_host_ptr;
    float* outB3_host_ptr;
    float* outB4_host_ptr;
    float* outB5_host_ptr;
    float* outB6_host_ptr;
    float* outB7_host_ptr;

    std::chrono::duration<double, std::micro> time_program_fpga;
    std::chrono::duration<double, std::micro> time_allocate_buffers;
    std::vector<std::chrono::duration<double, std::micro>> warmup_timings;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_input_to_device;
    std::vector<std::chrono::duration<double, std::micro>> time_kernel_execution;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_output_to_host;
};

#endif