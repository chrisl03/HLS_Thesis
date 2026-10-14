//  host_soda_fpga.h  (2-SLR: two kernels A & B, each 8R+8W, run concurrently)
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

// One kernel instance (8 read + 8 write HBM channels).
struct KernelUnit {
    xrt::kernel kern;
    xrt::run    run;
    xrt::bo     inA[NUM_READ_CH];
    xrt::bo     outB[NUM_WRITE_CH];
    float*      inA_ptr[NUM_READ_CH];
    float*      outB_ptr[NUM_WRITE_CH];

    // per-channel buffer size (even channel count is the max of even/odd)
    static constexpr std::size_t AIN_ELEMS  = (std::size_t)BURSTS_PER_CH_EVEN * 16;
    static constexpr std::size_t BOUT_ELEMS = (std::size_t)BURSTS_PER_CH_EVEN * 16;

    void init(xrt::device& dev, const xrt::uuid& uuid, const char* name) {
        kern = xrt::kernel(dev, uuid, name);
        for (int i = 0; i < NUM_READ_CH; i++) {
            inA[i]     = xrt::bo(dev, AIN_ELEMS * sizeof(float), kern.group_id(i));
            inA_ptr[i] = inA[i].map<float*>();
        }
        for (int i = 0; i < NUM_WRITE_CH; i++) {
            outB[i]     = xrt::bo(dev, BOUT_ELEMS * sizeof(float), kern.group_id(NUM_READ_CH + i));
            outB_ptr[i] = outB[i].map<float*>();
        }
        run = xrt::run(kern);
        for (int i = 0; i < NUM_READ_CH;  i++) run.set_arg(i, inA[i]);
        for (int i = 0; i < NUM_WRITE_CH; i++) run.set_arg(NUM_READ_CH + i, outB[i]);
    }
    void sync_in()  { for (int i=0;i<NUM_READ_CH;i++)  inA[i].sync(XCL_BO_SYNC_BO_TO_DEVICE); }
    void sync_out() { for (int i=0;i<NUM_WRITE_CH;i++) outB[i].sync(XCL_BO_SYNC_BO_FROM_DEVICE); }
};

class FPGA_SODA {
public:
    int  fpga_init(const std::string& xclbin_path, unsigned int device_index = 0);
    void warmup(unsigned int iterations = 1);
    void run();                       // launches A and B concurrently
    void print_performance_timings() const;
    void save_results_to_csv(const std::string& filename) const;

    // channel pointers for each kernel block
    float* inA_ptr (int kernel, int ch) { return (kernel==0?A:B).inA_ptr[ch]; }
    float* outB_ptr(int kernel, int ch) { return (kernel==0?A:B).outB_ptr[ch]; }

    KernelUnit A;   // kernel 0 -> architecture_top_a (SLR0)
    KernelUnit B;   // kernel 1 -> architecture_top_b (SLR1)

private:
    xrt::device device;
    std::chrono::duration<double, std::micro> time_program_fpga{};
    std::chrono::duration<double, std::micro> time_allocate_buffers{};
    std::vector<std::chrono::duration<double, std::micro>> warmup_timings;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_input_to_device;
    std::vector<std::chrono::duration<double, std::micro>> time_kernel_execution;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_output_to_host;
};

#endif