#
# SODA 2-SLR build (two kernels: architecture_top_a + architecture_top_b)
#   Changes vs the single-kernel Makefile are marked with  # <-- 2SLR
#

SHELL := /bin/bash

############################## Help Section ##############################
.PHONY: help all_full clean cleanall test run build host xclbin emconfig hls all all_no_hls report sd_card

help::
	@echo "Makefile Usage:"
	@echo ""
	@echo "  Default configuration:"
	@echo "    TARGET     = $(TARGET)"
	@echo "    HOST_ARCH  = $(HOST_ARCH)"
	@echo "    DEVICE     = $(DEVICE)"
	@echo "    KERNELS    = $(KA_NAME) + $(KB_NAME)   -> $(CONTAINER_NAME).xclbin"
	@echo ""
	@echo "  Typical U55C commands:"
	@echo "    make all_full        # host + 2 xo + linked/packaged xclbin + emconfig"
	@echo "    make hls             # compile BOTH kernels to .xo only"
	@echo "    make build           # 2 .xo -> linked xclbin -> packaged xclbin"
	@echo "    make host            # build only the x86 XRT host executable"
	@echo "    make run             # build everything and run the host"
	@echo ""

############################## Project Variables ##############################
TARGET     ?= hw
HOST_ARCH  ?= x86

PLATFORM_REPO_PATHS ?= /opt/xilinx/platforms
DEVICE_NAME ?= xilinx_u55c_gen3x16_xdma_3_202210_1
DEVICE ?= $(PLATFORM_REPO_PATHS)/$(DEVICE_NAME)/$(DEVICE_NAME).xpfm

EDGE_COMMON_SW_PATH ?= /opt/petalinux_files
EDGE_COMMON_SW ?= ${EDGE_COMMON_SW_PATH}/xilinx-versal-common-v2023.2
SYSROOT_PATH ?= /opt/petalinux/2023.2
SYSROOT      ?= $(SYSROOT_PATH)/sysroots/cortexa72-cortexa53-xilinx-linux

# --- 2SLR: two kernels + one container (xclbin) name ------------------  # <-- 2SLR
KA_NAME ?= architecture_top_a
KB_NAME ?= architecture_top_b
CONTAINER_NAME ?= soda_2slr
# kept so the report/sd_card targets still resolve (they reference kernel A)
KERNEL_NAME ?= $(KA_NAME)

KERNEL_DIR  ?= soda_src
HOST_DIR    ?= soda_host

KERNEL_CFG  ?= hls.cfg
LINK_CFG    ?= system.cfg          # <-- 2SLR (was system.cfg)

# --- 2SLR: one source per kernel --------------------------------------  # <-- 2SLR
KA_SRC ?= $(KERNEL_DIR)/soda_top_a.cpp
KB_SRC ?= $(KERNEL_DIR)/soda_top_b.cpp

HOST_SRCS   ?= $(HOST_DIR)/tb_soda.cpp $(HOST_DIR)/host_soda_fpga.cpp

EXECUTABLE  ?= ./soda_host.exe

ifneq ($(HOST_ARCH),$(filter $(HOST_ARCH),x86 aarch64))
$(error HOST_ARCH must be either x86 or aarch64)
endif

device2xsa = $(strip $(patsubst %.xpfm,%,$(shell basename $(DEVICE))))
XSA := $(call device2xsa,$(DEVICE))

TEMP_DIR  := ./_x.$(TARGET).$(XSA)
BUILD_DIR := ./build_dir.$(TARGET).$(XSA)

# --- 2SLR: two .xo --------------------------------------------------- # <-- 2SLR
KA_XO := $(TEMP_DIR)/$(KA_NAME).xo
KB_XO := $(TEMP_DIR)/$(KB_NAME).xo
KERNEL_XO := $(KA_XO)                 # alias kept for the report target

# Detect DFX platform (rp.xsa exists next to the .xpfm)
DFX_RP_XSA := $(dir $(DEVICE))/hw/rp.xsa

ifneq ($(wildcard $(DFX_RP_XSA)),)
  LINK_OUTPUT := $(BUILD_DIR)/$(CONTAINER_NAME).xsa            # <-- 2SLR
else
  LINK_OUTPUT := $(BUILD_DIR)/$(CONTAINER_NAME).link.xclbin    # <-- 2SLR
endif

FINAL_XCLBIN := $(BUILD_DIR)/$(CONTAINER_NAME).xclbin          # <-- 2SLR
BINARY_CONTAINERS := $(FINAL_XCLBIN)

RUN_APP_SCRIPT = ./run_app.sh
PACKAGE_OUT    = ./package.$(TARGET)
LAUNCH_EMULATOR = $(PACKAGE_OUT)/launch_$(TARGET).sh

CMD_ARGS ?= $(FINAL_XCLBIN)

############################## Toolchain ##############################
VPP ?= v++
ECHO := @echo
CP := cp -rf
RM := rm -f
RMDIR := rm -rf

############################## Kernel Build Flags ##############################
VPP_FLAGS += -t $(TARGET) --platform $(DEVICE) --save-temps --optimize 2 --report_level estimate
ifneq ($(TARGET), hw)
VPP_FLAGS += -g
endif
KERNEL_INC_FLAGS += -I$(KERNEL_DIR)

############################## Host Build Flags ##############################
CXXFLAGS += -Wall -O0 -g -std=c++17 -fmessage-length=0
INC_FLAGS += -I./ -I./$(KERNEL_DIR) -I./$(HOST_DIR)

ifeq ($(HOST_ARCH), x86)
CXX ?= g++
check-xrt:
ifndef XILINX_XRT
	$(error XILINX_XRT variable is not set. Please set it and rerun.)
endif
INC_FLAGS += -I$(XILINX_XRT)/include
LDFLAGS   += -L$(XILINX_XRT)/lib -lOpenCL -lxrt_coreutil -pthread -lrt -ldl
else  # aarch64
check-xrt:
ifndef XILINX_VITIS
	$(error XILINX_VITIS variable is not set. Please set it and rerun.)
endif
ifndef EDGE_COMMON_SW
	$(error EDGE_COMMON_SW variable is not set.)
endif
ifndef SYSROOT
	$(error SYSROOT is not set.)
endif
ROOTFS  := $(EDGE_COMMON_SW)/rootfs.ext4
IMAGE   := $(EDGE_COMMON_SW)/Image
CXX := $(XILINX_VITIS)/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++
CXXFLAGS += -std=c++14 -Wno-int-to-pointer-cast --sysroot=$(SYSROOT)
INC_FLAGS += -I$(SYSROOT)/usr/include \
            -I$(SYSROOT)/usr/include/xrt \
            -I$(XILINX_VITIS)/aietools/include \
            -I$(XILINX_VITIS)/include
LDFLAGS += -ladf_api_xrt -lgcc -lc -lxrt_coreutil -lxilinxopencl \
           -lpthread -lrt -ldl -lcrypt -lstdc++ \
           -L$(SYSROOT)/usr/lib --sysroot=$(SYSROOT) \
           -L$(XILINX_VITIS)/aietools/lib/aarch64.o
endif

############################## Essential Checks ##############################
check-vitis:
ifndef XILINX_VITIS
	$(error XILINX_VITIS variable is not set. Please set it and rerun.)
endif

check-devices:
ifndef DEVICE
	$(error DEVICE not set. Please set DEVICE=<platform.xpfm> and rerun.)
endif

############################## Targets ##############################
all_full: check-devices $(EXECUTABLE) $(BINARY_CONTAINERS) emconfig

all:
	@$(MAKE) all_full; \
	RET=$$?; \
	$(MAKE) report; \
	exit $$RET

all_no_hls:
	@$(MAKE) NO_HLS=1 all_full; \
	RET=$$?; \
	$(MAKE) NO_HLS=1 report; \
	exit $$RET

host: $(EXECUTABLE)

build: check-vitis $(BINARY_CONTAINERS)

xclbin: build

# HLS-only: compile BOTH kernels to .xo                                 # <-- 2SLR
hls: check-vitis check-devices $(KA_XO) $(KB_XO)
	@echo "INFO: HLS compile complete -> $(KA_XO) + $(KB_XO)"

############################## Directory Rules ##############################
$(TEMP_DIR):
	mkdir -p $(TEMP_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

############################## Kernel Rules ##############################
KERNEL_DEPS := $(wildcard $(KERNEL_DIR)/*.h) $(wildcard $(KERNEL_DIR)/*.hpp)

ifeq ($(NO_HLS),1)
$(KA_XO):
	@test -f $@ || { echo "ERROR: Prebuilt kernel object not found: $@"; echo "Run 'make hls' first."; exit 1; }
$(KB_XO):
	@test -f $@ || { echo "ERROR: Prebuilt kernel object not found: $@"; echo "Run 'make hls' first."; exit 1; }
else
# compile each kernel separately into its own .xo                       # <-- 2SLR
$(KA_XO): $(KA_SRC) $(KERNEL_DEPS) | $(TEMP_DIR)
	$(VPP) $(VPP_FLAGS) -c -k $(KA_NAME) --config $(KERNEL_CFG) --temp_dir $(TEMP_DIR) $(KERNEL_INC_FLAGS) -o $@ $(KA_SRC)
$(KB_XO): $(KB_SRC) $(KERNEL_DEPS) | $(TEMP_DIR)
	$(VPP) $(VPP_FLAGS) -c -k $(KB_NAME) --config $(KERNEL_CFG) --temp_dir $(TEMP_DIR) $(KERNEL_INC_FLAGS) -o $@ $(KB_SRC)
endif

# link BOTH .xo into one xclbin using the 2-SLR connectivity cfg        # <-- 2SLR
$(LINK_OUTPUT): $(KA_XO) $(KB_XO) | $(BUILD_DIR)
	$(VPP) $(VPP_FLAGS) -l --config $(LINK_CFG) --temp_dir $(TEMP_DIR) $(VPP_LDFLAGS) --profile.trace_memory PLRAM[0] -o $@ $(KA_XO) $(KB_XO)
	
$(FINAL_XCLBIN): $(LINK_OUTPUT) | $(BUILD_DIR)
ifeq ($(HOST_ARCH), x86)
	$(VPP) -p $< -t $(TARGET) --platform $(DEVICE) --package.out_dir $(PACKAGE_OUT) -o $@
else
	$(VPP) -p $< -t $(TARGET) --platform $(DEVICE) --package.out_dir $(PACKAGE_OUT) -o $@
endif

############################## Host Rules ##############################
HOST_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(HOST_SRCS))

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -c -o $@ $<

$(EXECUTABLE): $(HOST_OBJS) | check-xrt
	$(CXX) -o $@ $^ $(LDFLAGS)

############################## Emulation Config ##############################
EMCONFIG_DIR = $(TEMP_DIR)
emconfig: $(EMCONFIG_DIR)/emconfig.json

$(EMCONFIG_DIR)/emconfig.json: | $(TEMP_DIR)
	emconfigutil --platform $(DEVICE) --od $(EMCONFIG_DIR)

############################## Run / Test ##############################
run: all_full
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
ifeq ($(HOST_ARCH), x86)
	$(CP) $(EMCONFIG_DIR)/emconfig.json .
	XCL_EMULATION_MODE=$(TARGET) $(EXECUTABLE) $(CMD_ARGS)
else
	$(LAUNCH_EMULATOR) -run-app $(RUN_APP_SCRIPT) | tee run_app.log; exit $${PIPESTATUS[0]}
endif
else
ifeq ($(HOST_ARCH), x86)
	$(EXECUTABLE) $(CMD_ARGS)
else
	@echo "INFO: For hw on board, copy package.$(TARGET) contents to SD card and run on target."
endif
endif

test: $(EXECUTABLE)
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
ifeq ($(HOST_ARCH), x86)
	XCL_EMULATION_MODE=$(TARGET) $(EXECUTABLE) $(CMD_ARGS)
else
	$(LAUNCH_EMULATOR) -run-app $(RUN_APP_SCRIPT) | tee run_app.log; exit $${PIPESTATUS[0]}
endif
else
ifeq ($(HOST_ARCH), x86)
	$(EXECUTABLE) $(CMD_ARGS)
else
	@echo "INFO: Please run on target board (hw) or use hw_emu/sw_emu."
endif
endif

############################## SD Card / Packaging ##############################
SD_DIRS :=
ifneq ($(wildcard data),)
SD_DIRS += --package.sd_dir data
endif

sd_card: $(BINARY_CONTAINERS) | $(EXECUTABLE) gen_run_app
ifneq ($(HOST_ARCH), x86)
	$(VPP) -p $(LINK_OUTPUT) -t $(TARGET) --platform $(DEVICE) \
		--package.out_dir $(PACKAGE_OUT) \
		--package.rootfs $(ROOTFS) \
		--package.kernel_image $(IMAGE) \
		--package.boot_mode=sd \
		$(SD_DIRS) \
		--package.sd_file xrt.ini \
		--package.sd_file $(RUN_APP_SCRIPT) \
		--package.sd_file $(EXECUTABLE) \
		-o $(CONTAINER_NAME).xclbin
else
	@echo "INFO: HOST_ARCH=x86 => skipping embedded sd_card packaging."
endif

gen_run_app:
ifneq ($(HOST_ARCH), x86)
	$(RM) $(RUN_APP_SCRIPT)
	$(ECHO) 'export LD_LIBRARY_PATH=/mnt:/tmp:$$LD_LIBRARY_PATH' >> $(RUN_APP_SCRIPT)
	$(ECHO) 'export PATH=$$PATH:/sbin:$$PATH' >> $(RUN_APP_SCRIPT)
	$(ECHO) 'export XILINX_XRT=/usr' >> $(RUN_APP_SCRIPT)
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	$(ECHO) 'export XCL_EMULATION_MODE=$(TARGET)' >> $(RUN_APP_SCRIPT)
endif
	$(ECHO) '$(notdir $(EXECUTABLE)) $(CONTAINER_NAME).xclbin' >> $(RUN_APP_SCRIPT)
	$(ECHO) 'return_code=$$?' >> $(RUN_APP_SCRIPT)
	$(ECHO) 'if [ $$return_code -ne 0 ]; then' >> $(RUN_APP_SCRIPT)
	$(ECHO) '  echo "ERROR: host run failed, RC=$$return_code"' >> $(RUN_APP_SCRIPT)
	$(ECHO) 'fi' >> $(RUN_APP_SCRIPT)
	$(ECHO) 'echo "INFO: host run completed."' >> $(RUN_APP_SCRIPT)
	chmod +x $(RUN_APP_SCRIPT)
endif

############################## Cleaning ##############################
clean:
	-$(RMDIR) $(EXECUTABLE) $(HOST_OBJS)
	-$(RMDIR) profile_* TempConfig system_estimate.xtxt *.rpt *.csv
	-$(RMDIR) *v++* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb
	-$(RMDIR) $(TEMP_DIR) $(BUILD_DIR)

cleanall: clean
	-$(RMDIR) build_dir* package.* _x* emulation qemu-memory-* sd_card* *.xclbin.run_summary
	-$(RMDIR) run_app.sh run_app.log *.link.xclbin .ipcache

############################## Report Generation Target ##############################
# NOTE: report backs up kernel A's HLS reports only (B is identical). Prefer
#       'make all_full' for the 2-SLR flow; 'make all'/'report' run cleanall.
.PHONY: report
report:
	@echo "========================================================="
	@echo " Backing up requested reports and outputs..."
	@echo "========================================================="
	@mkdir -p .report_backup/$(TEMP_DIR)/link/int
	@mkdir -p .report_backup/$(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report

	@cp -a $(CONTAINER_NAME).xclbin .report_backup/ 2>/dev/null || true
	@mkdir -p .report_backup/$(BUILD_DIR)
	@cp -a $(FINAL_XCLBIN) .report_backup/$(BUILD_DIR)/ 2>/dev/null || true
	@cp -a $(EXECUTABLE) .report_backup/ 2>/dev/null || true

	@cp -a $(BUILD_DIR)/$(HOST_DIR) .report_backup/$(BUILD_DIR)/ 2>/dev/null || true
	@cp -a $(LINK_OUTPUT) .report_backup/$(BUILD_DIR)/ 2>/dev/null || true
	@cp -a $(KA_XO) .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a $(KB_XO) .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a $(EMCONFIG_DIR)/emconfig.json .report_backup/$(TEMP_DIR)/ 2>/dev/null || true

	@cp -a v++_$(CONTAINER_NAME)*.log .report_backup/ 2>/dev/null || true
	@cp -a -r $(TEMP_DIR)/reports .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a -r $(TEMP_DIR)/logs .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a $(TEMP_DIR)/link/int/$(CONTAINER_NAME).xclbin.info .report_backup/$(TEMP_DIR)/link/int/ 2>/dev/null || true

	@cp -a $(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report/csynth_design_size.rpt .report_backup/$(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report/ 2>/dev/null || true
	@cp -a $(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report/csynth.rpt .report_backup/$(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report/ 2>/dev/null || true

	@echo "========================================================="
	@echo " Running cleanall to remove other generated files..."
	@echo "========================================================="
	@$(MAKE) cleanall

	@echo "========================================================="
	@echo " Restoring preserved files..."
	@echo "========================================================="
	@cp -a .report_backup/$(CONTAINER_NAME).xclbin ./ 2>/dev/null || true
	@mkdir -p $(BUILD_DIR)
	@cp -a .report_backup/$(BUILD_DIR)/* $(BUILD_DIR)/ 2>/dev/null || true
	@cp -a .report_backup/$(notdir $(EXECUTABLE)) ./ 2>/dev/null || true
	@cp -a .report_backup/v++_$(CONTAINER_NAME)*.log ./ 2>/dev/null || true
	@mkdir -p $(TEMP_DIR)
	@cp -a -r .report_backup/$(TEMP_DIR)/* $(TEMP_DIR)/ 2>/dev/null || true
	@rm -rf .report_backup
	@echo "========================================================="
	@echo " Report generation and cleanup complete."
	@echo "========================================================="