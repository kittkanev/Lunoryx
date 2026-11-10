TESTING ?= 0
CPPFLAGS += -I. $(if $(filter 1,$(TESTING)),-DLUNORYX_TESTING,)
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++17 -MMD -MP -I/usr/include/lua5.4 -I/usr/include/SDL2 -I./third_party/imgui -I./third_party/imgui/backends -isystem ./third_party/include -isystem ./third_party/include/nlohmann
RUST_LIB = build/rust-linux/release/liblunoryx_kernels.a
RUST_BENCH_LIB = build/rust-bench/release/liblunoryx_kernels.a
RUST_SOURCES = $(wildcard extensions/rust-kernels/src/*.rs) extensions/rust-kernels/Cargo.toml extensions/rust-kernels/Cargo.lock rust-toolchain.toml
CARGO ?= cargo
LDFLAGS = $(RUST_LIB) build/deps/manifold-linux/src/libmanifold.a -llua5.4 -lSDL2 -lGL -lm -ldl -lpthread

CXXFLAGS += -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE $(EXTRA_CXXFLAGS)
LDFLAGS += -pie -Wl,-z,relro,-z,now $(EXTRA_LDFLAGS)

IMGUI_SRC = \
	third_party/imgui/imgui.cpp \
	third_party/imgui/imgui_demo.cpp \
	third_party/imgui/imgui_draw.cpp \
	third_party/imgui/imgui_tables.cpp \
	third_party/imgui/imgui_widgets.cpp \
	third_party/imgui/backends/imgui_impl_sdl2.cpp \
	third_party/imgui/backends/imgui_impl_opengl3.cpp

SRC = extensions/text-editor/TextEditor.cpp src/main.cpp extensions/ImGuizmo/ImGuizmo.cpp $(IMGUI_SRC)
OBJS = $(SRC:.cpp=.o) extensions/audio/audio_backend.o extensions/modeling/boolean_adapter.o extensions/xatlas/xatlas.o
DEPS = $(OBJS:.o=.d) tests/engine_tests.d build/main-testing.d

all: engine

engine: $(OBJS) $(RUST_LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f engine engine-testing build/main-testing.o build/audio-testing.o $(OBJS) $(DEPS) tests/core_tests tests/engine_tests tests/rust_kernel_tests tests/physics_differential tests/viewport_postprocess_tests tests/render_parity_tests tests/scene_shadow_tests tests/modeling_tests tests/animation_tests tests/studio_audio_tests


.PHONY: all clean test test-integration test-rust test-kernels benchmark

test: tests/core_tests tests/modeling_tests tests/animation_tests
	./tests/animation_tests
	./tests/core_tests
	./tests/modeling_tests

tests/core_tests: tests/core_tests.cpp src/core/scene_core.h src/core/scene_tools.h src/editor/editor_transform.h src/core/animation_core.h
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -pedantic -Werror -O2 $< -o $@

test-integration: tests/engine_tests
	./tests/engine_tests

tests/engine_tests: tests/engine_tests.cpp src/main.cpp src/core/scene_core.h src/core/scene_tools.h engine_version.h $(filter-out src/main.o extensions/audio/audio_backend.o,$(OBJS)) build/audio-testing.o $(RUST_LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DLUNORYX_TESTING tests/engine_tests.cpp $(filter-out src/main.o extensions/audio/audio_backend.o,$(OBJS)) build/audio-testing.o -o $@ $(LDFLAGS)

extensions/audio/audio_backend.o: extensions/audio/audio_backend.c extensions/audio/audio_backend.h extensions/audio/miniaudio.h extensions/audio/stb_vorbis.c
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Werror -Wno-unused-function -O2 $(EXTRA_CXXFLAGS) -c $< -o $@

tests/studio_audio_tests: tests/studio_audio_tests.c build/audio-testing.o
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Werror -O2 $^ -lm -ldl -lpthread $(EXTRA_LDFLAGS) -o $@

extensions/modeling/boolean_adapter.o: extensions/modeling/boolean_adapter.cpp extensions/modeling/boolean_adapter.h build/deps/manifold-linux/src/libmanifold.a
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Werror -Ibuild/deps/manifold/include -Ibuild/deps/manifold-linux/include $(EXTRA_CXXFLAGS) -c $< -o $@

build/deps/manifold-linux/src/libmanifold.a:
	bash tools/prepare_modeling.sh

tests/modeling_tests: tests/modeling_tests.cpp src/core/scene_core.h src/core/scene_tools.h src/core/mesh_modeling.h src/core/studio_mesh_types.h extensions/rust-kernels/kernels.h extensions/modeling/boolean_adapter.o $(RUST_LIB)
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -O2 $(EXTRA_CXXFLAGS) $< extensions/modeling/boolean_adapter.o build/deps/manifold-linux/src/libmanifold.a $(RUST_LIB) -lpthread $(EXTRA_LDFLAGS) -o $@

extensions/xatlas/xatlas.o: extensions/xatlas/xatlas.cpp extensions/xatlas/xatlas.h
	$(CXX) $(CPPFLAGS) -std=c++11 -O2 -w $(EXTRA_CXXFLAGS) -c $< -o $@

# The production archive contains only the allocation-free weld kernel.
$(RUST_LIB): $(RUST_SOURCES)
	$(CARGO) build --locked --offline --release --manifest-path extensions/rust-kernels/Cargo.toml --target-dir $(abspath build/rust-linux)

$(RUST_BENCH_LIB): $(RUST_SOURCES)
	$(CARGO) build --locked --offline --release --features bench --manifest-path extensions/rust-kernels/Cargo.toml --target-dir $(abspath build/rust-bench)

test-rust:
	$(CARGO) test --locked --offline --manifest-path extensions/rust-kernels/Cargo.toml --target-dir $(abspath build/rust-test)

tests/rust_kernel_tests: tests/rust_kernel_tests.cpp src/core/scene_core.h src/core/scene_tools.h src/core/studio_mesh_types.h extensions/rust-kernels/kernels.h src/core/mesh_modeling.h $(RUST_LIB) extensions/modeling/boolean_adapter.o
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -O2 $(EXTRA_CXXFLAGS) $< $(RUST_LIB) extensions/modeling/boolean_adapter.o build/deps/manifold-linux/src/libmanifold.a -lpthread $(EXTRA_LDFLAGS) -o $@

test-kernels: tests/rust_kernel_tests
	./tests/rust_kernel_tests

build/performance: benchmarks/performance.cpp src/core/scene_core.h src/core/scene_tools.h extensions/rust-kernels/kernels.h $(RUST_BENCH_LIB)
	mkdir -p build
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -O2 $< $(RUST_BENCH_LIB) -o $@

benchmark: build/performance
	./build/performance > build/performance-linux.csv

tests/physics_differential: tests/physics_differential.cpp tests/physics_reference.h src/core/scene_core.h src/core/scene_tools.h
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -O2 $(EXTRA_CXXFLAGS) $< $(EXTRA_LDFLAGS) -o $@

.PHONY: test-physics benchmark-physics
test-physics: tests/physics_differential
	./tests/physics_differential

build/physics_benchmark: benchmarks/physics_benchmark.cpp tests/physics_reference.h src/core/scene_core.h src/core/scene_tools.h
	mkdir -p build
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -O2 $< -o $@

benchmark-physics: build/physics_benchmark
	./build/physics_benchmark > build/physics-linux.csv

tests/viewport_postprocess_tests: tests/viewport_postprocess_tests.cpp src/render/viewport_target.h src/render/viewport_postprocess.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -lSDL2 -lGL $(EXTRA_LDFLAGS) -o $@

.PHONY: test-postprocess
test-postprocess: tests/viewport_postprocess_tests
	SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./tests/viewport_postprocess_tests

tests/render_parity_tests: tests/render_parity_tests.cpp src/render/material_render.h src/render/render_geometry.h tests/render_cache_regressions.h src/core/scene_tools.h src/core/mesh_modeling.h src/core/studio_mesh_types.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -lSDL2 -lGL $(EXTRA_LDFLAGS) -o $@

.PHONY: test-render
test-render: tests/render_parity_tests
	SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./tests/render_parity_tests > build/render-parity-linux.csv

build/weld_operation: benchmarks/weld_operation.cpp tests/reference_weld.h src/core/scene_tools.h src/core/mesh_modeling.h src/core/studio_mesh_types.h extensions/rust-kernels/kernels.h $(RUST_LIB)
	mkdir -p build
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -O2 $< $(RUST_LIB) -o $@

.PHONY: benchmark-weld
benchmark-weld: build/weld_operation
	./build/weld_operation > build/weld-linux.csv

tests/scene_shadow_tests: tests/scene_shadow_tests.cpp src/render/scene_shadows.h src/render/viewport_target.h src/render/viewport_postprocess.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -lSDL2 -lGL $(EXTRA_LDFLAGS) -o $@

.PHONY: test-shadows
test-shadows: tests/scene_shadow_tests
	SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./tests/scene_shadow_tests

tests/animation_tests: tests/animation_tests.c src/core/animation_core.h
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Werror -O2 $< -lm -o $@

build/audio-testing.o: extensions/audio/audio_backend.c extensions/audio/audio_backend.h extensions/audio/miniaudio.h extensions/audio/stb_vorbis.c
	mkdir -p build
	$(CC) $(CPPFLAGS) -DLUNORYX_TESTING -std=c11 -Wall -Wextra -Werror -Wno-unused-function -O2 $(EXTRA_CXXFLAGS) -c $< -o $@

build/main-testing.o: src/main.cpp $(wildcard src/*/*.h)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DLUNORYX_TESTING -c $< -o $@

engine-testing: build/main-testing.o build/audio-testing.o $(filter-out src/main.o extensions/audio/audio_backend.o,$(OBJS)) $(RUST_LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter-out $(RUST_LIB),$^) $(LDFLAGS) -o $@

extensions/text-editor/TextEditor.o: extensions/text-editor/TextEditor.cpp extensions/text-editor/TextEditor.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Wno-error -c $< -o $@

tests/worker_process_tests: tests/worker_process_tests.cpp src/scripting/worker_process.h src/io/secure_asset_io.h
	$(CXX) $(CPPFLAGS) -std=c++17 -Wall -Wextra -Werror -O2 $< -lpthread -o $@
test-workers: tests/worker_process_tests
	./tests/worker_process_tests
