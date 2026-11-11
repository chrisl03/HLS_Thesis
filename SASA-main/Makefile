#
# SASA DENOISE2D single-PE (COUNT=1) on Alveo U55C — same Makefile as your K=16 SODA flow,
# only KERNEL_NAME / dirs / sources changed.
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
	@echo "    KERNEL     = $(KERNEL_NAME)"
	@echo ""
	@echo "  make hls        -> compile kernel only -> .xo"
	@echo "  make build      -> .xo -> linked xclbin -> packaged xclbin"
	@echo "  make host       -> build the x86 XRT host executable"
	@echo "  make all_full   -> host + xclbin + emconfig"
	@echo "  make run        -> build everything and run the host"
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

# IMPORTANT: must match the top function name in unikernel.cpp
KERNEL_NAME ?= unikernel

KERNEL_DIR  ?= sasa_src
HOST_DIR    ?= sasa_host

KERNEL_CFG  ?= hls.cfg
LINK_CFG    ?= system.cfg

# Sources
KERNEL_SRCS ?= $(KERNEL_DIR)/unikernel.cpp
HOST_SRCS   ?= $(HOST_DIR)/host.cpp

# Output names
EXECUTABLE  ?= ./sasa_host.exe

ifneq ($(HOST_ARCH),$(filter $(HOST_ARCH),x86 aarch64))
$(error HOST_ARCH must be either x86 or aarch64)
endif

device2xsa = $(strip $(patsubst %.xpfm,%,$(shell basename $(DEVICE))))
XSA := $(call device2xsa,$(DEVICE))

TEMP_DIR  := ./_x.$(TARGET).$(XSA)
BUILD_DIR := ./build_dir.$(TARGET).$(XSA)

KERNEL_XO := $(TEMP_DIR)/$(KERNEL_NAME).xo
DFX_RP_XSA := $(dir $(DEVICE))/hw/rp.xsa

ifneq ($(wildcard $(DFX_RP_XSA)),)
  LINK_OUTPUT := $(BUILD_DIR)/$(KERNEL_NAME).xsa
else
  LINK_OUTPUT := $(BUILD_DIR)/$(KERNEL_NAME).link.xclbin
endif

FINAL_XCLBIN := $(BUILD_DIR)/$(KERNEL_NAME).xclbin
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

# DENOISE2D.h pulls in ap_int.h (ap_uint<512>), which lives in the Vitis HLS include dir,
# NOT in XRT. Add it so the x86 host can compile. Needs XILINX_HLS (set by Vitis settings64).
HLS_INC ?= $(XILINX_HLS)/include
INC_FLAGS += -I$(HLS_INC)

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
CXX := $(XILINX_VITIS)/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++
CXXFLAGS += -std=c++17 -Wno-int-to-pointer-cast --sysroot=$(SYSROOT)
INC_FLAGS += -I$(SYSROOT)/usr/include -I$(SYSROOT)/usr/include/xrt -I$(XILINX_VITIS)/include
LDFLAGS += -lxrt_coreutil -lxilinxopencl -lpthread -lrt -ldl -lstdc++ \
           -L$(SYSROOT)/usr/lib --sysroot=$(SYSROOT)
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
	@$(MAKE) all_full; RET=$$?; $(MAKE) report; exit $$RET

all_no_hls:
	@$(MAKE) NO_HLS=1 all_full; RET=$$?; $(MAKE) NO_HLS=1 report; exit $$RET

host: $(EXECUTABLE)
build: check-vitis $(BINARY_CONTAINERS)
xclbin: build
hls: check-vitis check-devices $(KERNEL_XO)
	@echo "INFO: HLS compile complete -> $(KERNEL_XO)"

############################## Directory Rules ##############################
$(TEMP_DIR):
	mkdir -p $(TEMP_DIR)
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

############################## Kernel Rules ##############################
KERNEL_DEPS := $(wildcard $(KERNEL_DIR)/*.h) $(wildcard $(KERNEL_DIR)/*.hpp)

ifeq ($(NO_HLS),1)
$(KERNEL_XO):
	@test -f $@ || { echo "ERROR: Prebuilt kernel object not found: $@"; echo "Run 'make hls' or 'make all' first."; exit 1; }
else
$(KERNEL_XO): $(KERNEL_SRCS) $(KERNEL_DEPS) | $(TEMP_DIR)
	$(VPP) $(VPP_FLAGS) -c -k $(KERNEL_NAME) --config $(KERNEL_CFG) --temp_dir $(TEMP_DIR) $(KERNEL_INC_FLAGS) -o $@ $(KERNEL_SRCS)
endif

$(LINK_OUTPUT): $(KERNEL_XO) | $(BUILD_DIR)
	$(VPP) $(VPP_FLAGS) -l --config $(LINK_CFG) --temp_dir $(TEMP_DIR) $(VPP_LDFLAGS) -o $@ $^

$(FINAL_XCLBIN): $(LINK_OUTPUT) | $(BUILD_DIR)
	$(VPP) -p $< -t $(TARGET) --platform $(DEVICE) --package.out_dir $(PACKAGE_OUT) -o $@

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
ifeq ($(HOST_ARCH), x86)
	$(EXECUTABLE) $(CMD_ARGS)
else
	@echo "INFO: Please run on target board (hw) or use hw_emu/sw_emu."
endif

############################## Cleaning ##############################
clean:
	-$(RMDIR) $(EXECUTABLE) $(HOST_OBJS)
	-$(RMDIR) profile_* TempConfig system_estimate.xtxt *.rpt *.csv
	-$(RMDIR) *v++* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb
	-$(RMDIR) $(TEMP_DIR) $(BUILD_DIR)

cleanall: clean
	-$(RMDIR) build_dir* package.* _x* emulation qemu-memory-* sd_card* *.xclbin.run_summary
	-$(RMDIR) run_app.sh run_app.log *.link.xclbin .ipcache hls_project

############################## Report (same as your SODA Makefile) ##############################
.PHONY: report
report:
	@echo "========================================================="
	@echo " Backing up requested reports and outputs..."
	@echo "========================================================="
	@mkdir -p .report_backup/$(TEMP_DIR)/link/int
	@mkdir -p .report_backup/$(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report
	@cp -a $(KERNEL_NAME).xclbin .report_backup/ 2>/dev/null || true
	@mkdir -p .report_backup/$(BUILD_DIR)
	@cp -a $(FINAL_XCLBIN) .report_backup/$(BUILD_DIR)/ 2>/dev/null || true
	@cp -a $(EXECUTABLE) .report_backup/ 2>/dev/null || true
	@cp -a $(BUILD_DIR)/$(HOST_DIR) .report_backup/$(BUILD_DIR)/ 2>/dev/null || true
	@cp -a $(LINK_OUTPUT) .report_backup/$(BUILD_DIR)/ 2>/dev/null || true
	@cp -a $(KERNEL_XO) .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a $(EMCONFIG_DIR)/emconfig.json .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a v++_$(KERNEL_NAME)*.log .report_backup/ 2>/dev/null || true
	@cp -a -r $(TEMP_DIR)/reports .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a -r $(TEMP_DIR)/logs .report_backup/$(TEMP_DIR)/ 2>/dev/null || true
	@cp -a $(TEMP_DIR)/link/int/$(KERNEL_NAME).xclbin.info .report_backup/$(TEMP_DIR)/link/int/ 2>/dev/null || true
	@cp -a $(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report/csynth.rpt .report_backup/$(TEMP_DIR)/$(KERNEL_NAME)/$(KERNEL_NAME)/$(KERNEL_NAME)/solution/syn/report/ 2>/dev/null || true
	@$(MAKE) cleanall
	@cp -a .report_backup/$(KERNEL_NAME).xclbin ./ 2>/dev/null || true
	@mkdir -p $(BUILD_DIR)
	@cp -a .report_backup/$(BUILD_DIR)/* $(BUILD_DIR)/ 2>/dev/null || true
	@cp -a .report_backup/$(notdir $(EXECUTABLE)) ./ 2>/dev/null || true
	@mkdir -p $(TEMP_DIR)
	@cp -a -r .report_backup/$(TEMP_DIR)/* $(TEMP_DIR)/ 2>/dev/null || true
	@rm -rf .report_backup
	@echo " Report generation complete."