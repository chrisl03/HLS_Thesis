// host.cpp — XRT host, port of the TAPA host for the SASA DENOISE2D single-PE.
//
// WHAT WAS CHANGED (tapa -> hls/XRT only):
//   tapa::invoke(unikernel, FLAGS_bitstream, tapa::read_write_mmap(source_0),
//                tapa::read_write_mmap(product_0), iter)
//     -> xrt::device / load_xclbin / xrt::kernel("unikernel") / xrt::bo + sync + run.wait()
//   gflags --bitstream flag  -> argv[1] (XRT host has no FRT bitstream flag)
// The buffer sizes (MIDDLE_REGION words of ap_uint<512>) and iter (ITERATION) are kept as-is.
#include <iostream>
#include <vector>
#include <cstdint>
#include "math.h"
#include "DENOISE2D.h"

#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_kernel.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <xclbin> [device_id]\n";
    return 1;
  }
  const std::string xclbin_path = argv[1];
  const unsigned device_id = (argc > 2) ? std::atoi(argv[2]) : 0;

  std::cout << "Program start" << std::endl;

  // ---- sizes (identical to the TAPA host) --------------------------------
  const size_t   N      = MIDDLE_REGION;            // words of ap_uint<512>
  const size_t   nbytes = N * (DWIDTH / 8);         // 64 bytes / word
  const uint32_t iter    = ITERATION;
  printf("middle region size = %zu words (%zu bytes), iters = %u\n", N, nbytes, iter);

  // ---- device / kernel ----------------------------------------------------
  xrt::device device(device_id);
  auto uuid = device.load_xclbin(xclbin_path);
  auto krnl = xrt::kernel(device, uuid, "unikernel");

  // ---- buffers (source_0 = arg0, product_0 = arg1, iters = arg2) ----------
  auto bo_source  = xrt::bo(device, nbytes, krnl.group_id(0));
  auto bo_product = xrt::bo(device, nbytes, krnl.group_id(1));

  auto source_0  = bo_source.map<INTERFACE_WIDTH*>();
  auto product_0 = bo_product.map<INTERFACE_WIDTH*>();

  for (size_t i = 0; i < N; i++) {           // init input here (same spot as the TAPA host)
    source_0[i]  = 0;
    product_0[i] = 0;
  }

  bo_source.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_product.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  // ---- run ----------------------------------------------------------------
  std::cout << "kernel start" << std::endl;
  auto run = krnl(bo_source, bo_product, iter);
  run.wait();
  std::cout << "kernel finish" << std::endl;

  // ---- read result back ---------------------------------------------------
  bo_product.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

  std::cout << "Program end" << std::endl;
  return 0;
}