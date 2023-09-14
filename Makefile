OUT_DIR = out

HEADERS := $(shell find . -name '*.h')

COMMON_SRC := $(shell find src/common -name '*.cpp')
LIB_SRC := $(shell find src/client -name '*.cpp')
SRV_SRC := $(shell find src/server -name '*.cpp')
MATMUL_SRC := src/examples/matmul.cpp
MATMUL_NATIVE_SRC := src/examples/matmul.cpp

COMMON_OBJS := $(COMMON_SRC:%.cpp=$(OUT_DIR)/%.o)
LIB_OBJS := $(LIB_SRC:%.cpp=$(OUT_DIR)/%.o)
SRV_OBJS := $(SRV_SRC:%.cpp=$(OUT_DIR)/%.o)
MATMUL_OBJS := $(MATMUL_SRC:%.cpp=$(OUT_DIR)/%-webgpudd.o)
MATMUL_NATIVE_OBJS := $(MATMUL_NATIVE_SRC:%.cpp=$(OUT_DIR)/%-native.o)

DAWN_BUILD_DIR ?= ../dawn/out/Release
INCLUDES := -I $(DAWN_BUILD_DIR)/gen/include -I ../dawn/include

CXXFLAGS = -fPIC -O2 -arch arm64 -std=c++17 -fno-exceptions -fno-rtti
CXX = g++

DAWN_CLIENT_DEPS=-Wl,-force_load $(DAWN_BUILD_DIR)/src/dawn/wire/libdawn_wire.a $(DAWN_BUILD_DIR)/src/dawn/common/libdawn_common.a $(DAWN_BUILD_DIR)/src/dawn/libdawn_proc.a
DAWN_SERVER_DEPS=$(DAWN_BUILD_DIR)/src/dawn/libdawn_proc.a $(DAWN_BUILD_DIR)/src/dawn/libdawncpp.a $(DAWN_BUILD_DIR)/src/dawn/libdawncpp_headers.a $(DAWN_BUILD_DIR)/src/dawn/libdawn_headers.a $(DAWN_BUILD_DIR)/src/dawn/wire/libdawn_wire.a $(DAWN_BUILD_DIR)/src/dawn/common/libdawn_common.a $(DAWN_BUILD_DIR)/src/dawn/native/libdawn_native.a $(DAWN_BUILD_DIR)/src/dawn/platform/libdawn_platform.a $(DAWN_BUILD_DIR)/src/dawn/libdawn_proc.a $(DAWN_BUILD_DIR)/src/tint/libtint_api.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_hlsl_writer_common.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_spirv_reader_common.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_reader.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_reader_parser.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_writer.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_writer_ast_printer.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_writer_syntax_tree_printer.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_msl_writer.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_msl_writer_raise.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_reader_program_to_ir.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_msl_writer_ast_printer.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_helpers.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_ast_transform.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_resolver.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_core_intrinsic_data.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_core_intrinsic.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_inspector.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_program.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_sem.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_wgsl_ast.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_msl_writer_printer.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_generator.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_core_ir.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_core_constant.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_msl_writer_common.a $(DAWN_BUILD_DIR)/src/tint/libtint_api_options.a $(DAWN_BUILD_DIR)/src/tint/libtint_api_common.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_core_type.a $(DAWN_BUILD_DIR)/src/tint/libtint_lang_core.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_diagnostic.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_reflection.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_symbol.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_id.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_strconv.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_result.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_text.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_containers.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_ice.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_debug.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_rtti.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_macros.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_memory.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_math.a $(DAWN_BUILD_DIR)/src/tint/libtint_utils_traits.a -L $(DAWN_BUILD_DIR)/third_party/abseil/absl/strings -l absl_str_format_internal -l absl_strings -l absl_strings_internal -L $(DAWN_BUILD_DIR)/third_party/abseil/absl/base -l absl_base -l absl_spinlock_wait -l absl_throw_delegate -l absl_raw_logging_internal -l absl_log_severity -L $(DAWN_BUILD_DIR)/third_party/abseil/absl/numeric -l absl_int128 -framework CoreFoundation -framework IOKit -framework IOSurface -framework Metal -framework QuartzCore -framework Cocoa

.PHONY: clean all
all: server matmul-dd

libwebgpudd: $(LIB_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DAWN_CLIENT_DEPS) -shared $(LIB_OBJS) $(COMMON_OBJS) -o $(OUT_DIR)/libwebgpudd.so

server: $(SRV_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DAWN_SERVER_DEPS) $(SRV_OBJS) $(COMMON_OBJS) -o $(OUT_DIR)/server

matmul-dd: $(MATMUL_OBJS) libwebgpudd
	$(CXX) $(CXXFLAGS) -L $(OUT_DIR) -lwebgpudd $(MATMUL_OBJS) -o $(OUT_DIR)/matmul-dd

matmul-native: $(MATMUL_NATIVE_OBJS)
	$(CXX) $(CXXFLAGS) $(DAWN_SERVER_DEPS) $(MATMUL_NATIVE_OBJS) -o $(OUT_DIR)/matmul-native

$(OUT_DIR):
	mkdir $(OUT_DIR)

$(OUT_DIR)/%.o: %.cpp $(HEADERS) | $(OUT_DIR)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ -c $<

$(OUT_DIR)/%-native.o: %.cpp $(HEADERS) | $(OUT_DIR)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DBACKEND_DAWN_NATIVE -o $@ -c $<

$(OUT_DIR)/%-webgpudd.o: %.cpp $(HEADERS) | $(OUT_DIR)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I ./include -DBACKEND_WEBGPUDD -o $@ -c $<

clean:
	rm -r $(OUT_DIR)
