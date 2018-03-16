
# Directory variable declarations

CUDA_PATH := /usr/local/cuda
TOP_DIR := /home/nvidia/tegra_multimedia_api
CLASS_DIR := /home/nvidia/tegra_multimedia_api/samples/common/classes
ALGO_CUDA_DIR := /home/nvidia/tegra_multimedia_api/samples/common/algorithm/cuda
ALGO_TRT_DIR := /home/nvidia/tegra_multimedia_api/samples/common/algorithm/trt
ARGUS_UTILS_DIR := /home/nvidia/tegra_multimedia_api/argus/samples/utils

# Clear the flags from env
CPPFLAGS :=
LDFLAGS :=

# CPP defintion
CPP     = @ $(CROSS_CLDFLAGS += \
        -Wl,-rpath-link=/lib/$(TEGRA_ARMABI) \
        -Wl,-rpath-link=/usr/lib/$(TEGRA_ARMABI) \
        -Wl,-rpath-link=/usr/lib/$(TEGRA_ARMABI)/tegra \
        -Wl,-rpath-link=/$(CUDA_PATH)/lib64OMPILE)g++

TEGRA_ARMABI := aarch64-linux-gnu

# Specify the logical root directory for headers and libraries
CPPFLAGS += --sysroot=
LDFLAGS += \
        -Wl,-rpath-link=/lib/$(TEGRA_ARMABI) \
        -Wl,-rpath-link=/usr/lib/$(TEGRA_ARMABI) \
        -Wl,-rpath-link=/usr/lib/$(TEGRA_ARMABI)/tegra \
        -Wl,-rpath-link=/$(CUDA_PATH)/lib64

# All common header files
CPPFLAGS += -std=c++11 \
        -I"$(TOP_DIR)/include" \
        -I"$(TOP_DIR)/include/libjpeg-8b" \
        -I"$(ALGO_CUDA_DIR)" \
        -I"$(ALGO_TRT_DIR)" \
        -I"/$(CUDA_PATH)/include" \
        -I"/usr/include/$(TEGRA_ARMABI)" \
        -I"/usr/include/libdrm"

# All common dependent libraries
LDFLAGS += \
        -lpthread -lv4l2 -lEGL -lGLESv2 -lX11 \
        -lnvbuf_utils -lnvjpeg -lnvosd -ldrm \
        -lcuda -lcudart \
        -lnvinfer -lnvparsers \
        -lopencv_objdetect \
        -L"/$(CUDA_PATH)/lib64" \
        -L"/usr/lib/$(TEGRA_ARMABI)" \
        -L"/usr/lib/$(TEGRA_ARMABI)/tegra"









#

APP := argus_test

SRCS := argus_test.cpp \
            $(wildcard $(CLASS_DIR)/*.cpp) \
            $(ARGUS_UTILS_DIR)/Thread.cpp

OBJS := $(SRCS:.cpp=.o)

CPPFLAGS += \
        -I"$(ARGUS_UTILS_DIR)"

LDFLAGS += \
        -ldl -lnveglstream_camconsumer -largus


# Rule Definitions

all: $(APP)

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	@echo "Testmsg - Compiling Classes: $@ $(MAKE)"
	@make -C $(CLASS_DIR)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(CPP) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS)

clean:
	@rm -rf $(APP) $(OBJS)
