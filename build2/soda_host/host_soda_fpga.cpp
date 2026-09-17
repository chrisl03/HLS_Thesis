//  host_soda_fpga.cpp  (K=64, 8 read + 8 write channels)
#include "host_soda_fpga.h"
#include <experimental/xrt_xclbin.h>
#include <iostream>

int FPGA_SODA::fpga_init(const std::string& xclbin_path, const unsigned int device_index) {
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    device = xrt::device(device_index);
    std::cout << "Programming device[" << device_index << "]: "
              << device.get_info<xrt::info::device::name>() << std::endl;
    auto uuid = device.load_xclbin(xclbin_path);
    soda_kernel = xrt::kernel(device, uuid, "architecture_top_level");
    time_program_fpga = clock::now() - start;

    std::cout << "Allocate buffers (8R+8W)" << std::endl;
    auto sa = clock::now();
    inA0_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(0));
    inA1_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(1));
    inA2_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(2));
    inA3_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(3));
    inA4_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(4));
    inA5_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(5));
    inA6_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(6));
    inA7_bo = xrt::bo(device, AIN_ELEMS*sizeof(float), soda_kernel.group_id(7));
    outB0_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(8));
    outB1_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(9));
    outB2_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(10));
    outB3_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(11));
    outB4_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(12));
    outB5_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(13));
    outB6_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(14));
    outB7_bo = xrt::bo(device, BOUT_ELEMS*sizeof(float), soda_kernel.group_id(15));
    inA0_host_ptr = inA0_bo.map<float*>();
    inA1_host_ptr = inA1_bo.map<float*>();
    inA2_host_ptr = inA2_bo.map<float*>();
    inA3_host_ptr = inA3_bo.map<float*>();
    inA4_host_ptr = inA4_bo.map<float*>();
    inA5_host_ptr = inA5_bo.map<float*>();
    inA6_host_ptr = inA6_bo.map<float*>();
    inA7_host_ptr = inA7_bo.map<float*>();
    outB0_host_ptr = outB0_bo.map<float*>();
    outB1_host_ptr = outB1_bo.map<float*>();
    outB2_host_ptr = outB2_bo.map<float*>();
    outB3_host_ptr = outB3_bo.map<float*>();
    outB4_host_ptr = outB4_bo.map<float*>();
    outB5_host_ptr = outB5_bo.map<float*>();
    outB6_host_ptr = outB6_bo.map<float*>();
    outB7_host_ptr = outB7_bo.map<float*>();
    run_soda = xrt::run(soda_kernel);
    run_soda.set_arg(0, inA0_bo);
    run_soda.set_arg(1, inA1_bo);
    run_soda.set_arg(2, inA2_bo);
    run_soda.set_arg(3, inA3_bo);
    run_soda.set_arg(4, inA4_bo);
    run_soda.set_arg(5, inA5_bo);
    run_soda.set_arg(6, inA6_bo);
    run_soda.set_arg(7, inA7_bo);
    run_soda.set_arg(8, outB0_bo);
    run_soda.set_arg(9, outB1_bo);
    run_soda.set_arg(10, outB2_bo);
    run_soda.set_arg(11, outB3_bo);
    run_soda.set_arg(12, outB4_bo);
    run_soda.set_arg(13, outB5_bo);
    run_soda.set_arg(14, outB6_bo);
    run_soda.set_arg(15, outB7_bo);
    time_allocate_buffers = clock::now() - sa;
    return 0;
}

void FPGA_SODA::warmup(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    warmup_timings.clear();
    for (unsigned int i = 0; i < iterations; ++i) {
        auto s = clock::now();
        run_soda.start(); run_soda.wait();
        warmup_timings.push_back(clock::now() - s);
        std::cout << "Warmed up (" << (i+1) << "/" << iterations << ")" << std::endl;
    }
}

void FPGA_SODA::run() {
    using clock = std::chrono::high_resolution_clock;
    auto sci = clock::now();
    inA0_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA1_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA2_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA3_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA4_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA5_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA6_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inA7_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    time_copy_input_to_device.push_back(clock::now() - sci);
    auto sk = clock::now();
    run_soda.start(); run_soda.wait();
    time_kernel_execution.push_back(clock::now() - sk);
    auto sco = clock::now();
    outB0_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB1_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB2_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB3_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB4_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB5_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB6_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    outB7_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    time_copy_output_to_host.push_back(clock::now() - sco);
}

void FPGA_SODA::print_performance_timings() const {
    std::cout << "FPGA Init: program " << time_program_fpga.count()
              << " us, alloc " << time_allocate_buffers.count() << " us" << std::endl;
    if (!warmup_timings.empty()) {
        double s=0; for (auto&t:warmup_timings) s+=t.count();
        std::cout << "Warmup avg: " << (s/warmup_timings.size()) << " us" << std::endl;
    }
    for (std::size_t i=0;i<time_kernel_execution.size();++i)
        std::cout << "  Run " << (i+1) << ": copy-in " << time_copy_input_to_device[i].count()
                  << " us, kernel " << time_kernel_execution[i].count()
                  << " us, copy-out " << time_copy_output_to_host[i].count() << " us" << std::endl;
}

void FPGA_SODA::save_results_to_csv(const std::string& filename) const {
    std::ofstream f(filename, std::ios::app);
    if (!f.is_open()) return;
    size_t n = time_kernel_execution.size(); if (n==0) return;
    double ai=0,ak=0,ao=0;
    for (size_t i=0;i<n;++i){ai+=time_copy_input_to_device[i].count();ak+=time_kernel_execution[i].count();ao+=time_copy_output_to_host[i].count();}
    ai/=n;ak/=n;ao/=n;
    f << n << "," << ai << "," << ak << "," << ao << "," << (ai+ak+ao) << "\n";
    f.close();
}

float* FPGA_SODA::get_inA0_ptr()  { return inA0_host_ptr; }
float* FPGA_SODA::get_inA1_ptr()  { return inA1_host_ptr; }
float* FPGA_SODA::get_inA2_ptr()  { return inA2_host_ptr; }
float* FPGA_SODA::get_inA3_ptr()  { return inA3_host_ptr; }
float* FPGA_SODA::get_inA4_ptr()  { return inA4_host_ptr; }
float* FPGA_SODA::get_inA5_ptr()  { return inA5_host_ptr; }
float* FPGA_SODA::get_inA6_ptr()  { return inA6_host_ptr; }
float* FPGA_SODA::get_inA7_ptr()  { return inA7_host_ptr; }
float* FPGA_SODA::get_outB0_ptr() { return outB0_host_ptr; }
float* FPGA_SODA::get_outB1_ptr() { return outB1_host_ptr; }
float* FPGA_SODA::get_outB2_ptr() { return outB2_host_ptr; }
float* FPGA_SODA::get_outB3_ptr() { return outB3_host_ptr; }
float* FPGA_SODA::get_outB4_ptr() { return outB4_host_ptr; }
float* FPGA_SODA::get_outB5_ptr() { return outB5_host_ptr; }
float* FPGA_SODA::get_outB6_ptr() { return outB6_host_ptr; }
float* FPGA_SODA::get_outB7_ptr() { return outB7_host_ptr; }