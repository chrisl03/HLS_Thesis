//  host_soda_fpga.cpp  (2-SLR: two kernels A & B, concurrent)
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
    time_program_fpga = clock::now() - start;

    std::cout << "Allocate buffers: kernel A (SLR0) + kernel B (SLR1), 8R+8W each" << std::endl;
    auto sa = clock::now();
    A.init(device, uuid, "architecture_top_a");
    B.init(device, uuid, "architecture_top_b");
    time_allocate_buffers = clock::now() - sa;
    return 0;
}

void FPGA_SODA::warmup(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    warmup_timings.clear();
    for (unsigned int i = 0; i < iterations; ++i) {
        auto s = clock::now();
        A.run.start(); B.run.start();     // both kernels run concurrently
        A.run.wait();  B.run.wait();
        warmup_timings.push_back(clock::now() - s);
        std::cout << "Warmed up (" << (i+1) << "/" << iterations << ")" << std::endl;
    }
}

void FPGA_SODA::run() {
    using clock = std::chrono::high_resolution_clock;
    auto sci = clock::now();
    A.sync_in(); B.sync_in();
    time_copy_input_to_device.push_back(clock::now() - sci);

    auto sk = clock::now();
    A.run.start(); B.run.start();         // launch both, then wait both
    A.run.wait();  B.run.wait();
    time_kernel_execution.push_back(clock::now() - sk);

    auto sco = clock::now();
    A.sync_out(); B.sync_out();
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
                  << " us, kernel(A||B) " << time_kernel_execution[i].count()
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