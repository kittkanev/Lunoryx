CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++11 -MMD -MP -I/usr/include/lua5.4 -I/usr/include/SDL2 -I./imgui -I./imgui/backends -isystem ./win_deps -isystem ./win_deps/nlohmann
RUST_LIB = build/rust-linux/release/liblunoryx_kernels.a
RUST_BENCH_LIB = build/rust-bench/release/liblunoryx_kernels.a
RUST_SOURCES = $(wildcard extensions/rust-kernels/src/*.rs) extensions/rust-kernels/Cargo.toml extensions/rust-kernels/Cargo.lock rust-toolchain.toml
CARGO ?= cargo
LDFLAGS = $(RUST_LIB) build/deps/manifold-linux/src/libmanifold.a -llua5.4 -lSDL2 -lGL -lm -ldl -lpthread

CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

IMGUI_SRC = \
	imgui/imgui.cpp \
	imgui/imgui_demo.cpp \
	imgui/imgui_draw.cpp \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/backends/imgui_impl_sdl2.cpp \
	imgui/backends/imgui_impl_opengl3.cpp

SRC = main.cpp extensions/ImGuizmo/ImGuizmo.cpp $(IMGUI_SRC)
OBJS = $(SRC:.cpp=.o) extensions/audio/audio_backend.o extensions/modeling/boolean_adapter.o extensions/xatlas/xatlas.o
DEPS = $(OBJS:.o=.d) tests/engine_tests.d

all: engine

engine: $(OBJS) $(RUST_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f engine $(OBJS) $(DEPS) tests/core_tests tests/engine_tests tests/rust_kernel_tests tests/physics_differential tests/viewport_postprocess_tests tests/render_parity_tests tests/scene_shadow_tests


.PHONY: all clean test test-integration test-rust test-kernels benchmark

test: tests/core_tests tests/modeling_tests
	./tests/core_tests
	./tests/modeling_tests

tests/core_tests: tests/core_tests.cpp scene_core.h scene_tools.h editor_transform.h
	$(CXX) -std=c++11 -Wall -Wextra -pedantic -Werror -O2 $< -o $@

test-integration: tests/engine_tests
	./tests/engine_tests

tests/engine_tests: tests/engine_tests.cpp main.cpp scene_core.h scene_tools.h engine_version.h $(filter-out main.o,$(OBJS)) $(RUST_LIB)
	$(CXX) $(CXXFLAGS) tests/engine_tests.cpp $(filter-out main.o,$(OBJS)) -o $@ $(LDFLAGS)

extensions/audio/audio_backend.o: extensions/audio/audio_backend.c extensions/audio/audio_backend.h extensions/audio/miniaudio.h extensions/audio/stb_vorbis.c
	$(CC) -std=c11 -Wall -Wextra -Werror -Wno-unused-function -O2 $(EXTRA_CXXFLAGS) -c $< -o $@

tests/studio_audio_tests: tests/studio_audio_tests.c extensions/audio/audio_backend.o
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 $^ -lm -ldl -lpthread $(EXTRA_LDFLAGS) -o $@

extensions/modeling/boolean_adapter.o: extensions/modeling/boolean_adapter.cpp extensions/modeling/boolean_adapter.h build/deps/manifold-linux/src/libmanifold.a
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -Ibuild/deps/manifold/include -Ibuild/deps/manifold-linux/include $(EXTRA_CXXFLAGS) -c $< -o $@

build/deps/manifold-linux/src/libmanifold.a:
	bash tools/prepare_modeling.sh

tests/modeling_tests: tests/modeling_tests.cpp scene_core.h scene_tools.h mesh_modeling.h studio_mesh_types.h extensions/rust-kernels/kernels.h extensions/modeling/boolean_adapter.o $(RUST_LIB)
	$(CXX) -std=c++11 -Wall -Wextra -Werror -O2 $(EXTRA_CXXFLAGS) $< extensions/modeling/boolean_adapter.o build/deps/manifold-linux/src/libmanifold.a $(RUST_LIB) -lpthread $(EXTRA_LDFLAGS) -o $@

extensions/xatlas/xatlas.o: extensions/xatlas/xatlas.cpp extensions/xatlas/xatlas.h
	$(CXX) -std=c++11 -O2 -w $(EXTRA_CXXFLAGS) -c $< -o $@

# The production archive contains only the allocation-free weld kernel.
$(RUST_LIB): $(RUST_SOURCES)
	$(CARGO) build --locked --offline --release --manifest-path extensions/rust-kernels/Cargo.toml --target-dir $(abspath build/rust-linux)

$(RUST_BENCH_LIB): $(RUST_SOURCES)
	$(CARGO) build --locked --offline --release --features bench --manifest-path extensions/rust-kernels/Cargo.toml --target-dir $(abspath build/rust-bench)

test-rust:
	$(CARGO) test --locked --offline --manifest-path extensions/rust-kernels/Cargo.toml --target-dir $(abspath build/rust-test)

tests/rust_kernel_tests: tests/rust_kernel_tests.cpp scene_core.h scene_tools.h studio_mesh_types.h extensions/rust-kernels/kernels.h mesh_modeling.h $(RUST_LIB) extensions/modeling/boolean_adapter.o
	$(CXX) -std=c++11 -Wall -Wextra -Werror -O2 $(EXTRA_CXXFLAGS) $< $(RUST_LIB) extensions/modeling/boolean_adapter.o build/deps/manifold-linux/src/libmanifold.a -lpthread $(EXTRA_LDFLAGS) -o $@

test-kernels: tests/rust_kernel_tests
	./tests/rust_kernel_tests

build/performance: benchmarks/performance.cpp scene_core.h scene_tools.h extensions/rust-kernels/kernels.h $(RUST_BENCH_LIB)
	mkdir -p build
	$(CXX) -std=c++11 -Wall -Wextra -Werror -O2 $< $(RUST_BENCH_LIB) -o $@

benchmark: build/performance
	./build/performance > build/performance-linux.csv

tests/physics_differential: tests/physics_differential.cpp tests/physics_reference.h scene_core.h scene_tools.h
	$(CXX) -std=c++11 -Wall -Wextra -Werror -O2 $(EXTRA_CXXFLAGS) $< $(EXTRA_LDFLAGS) -o $@

.PHONY: test-physics benchmark-physics
test-physics: tests/physics_differential
	./tests/physics_differential

build/physics_benchmark: benchmarks/physics_benchmark.cpp tests/physics_reference.h scene_core.h scene_tools.h
	mkdir -p build
	$(CXX) -std=c++11 -Wall -Wextra -Werror -O2 $< -o $@

benchmark-physics: build/physics_benchmark
	./build/physics_benchmark > build/physics-linux.csv

tests/viewport_postprocess_tests: tests/viewport_postprocess_tests.cpp viewport_target.h viewport_postprocess.h
	$(CXX) $(CXXFLAGS) $< -lSDL2 -lGL $(EXTRA_LDFLAGS) -o $@

.PHONY: test-postprocess
test-postprocess: tests/viewport_postprocess_tests
	SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./tests/viewport_postprocess_tests

tests/render_parity_tests: tests/render_parity_tests.cpp material_render.h render_geometry.h tests/render_cache_regressions.h scene_tools.h mesh_modeling.h studio_mesh_types.h
	$(CXX) $(CXXFLAGS) $< -lSDL2 -lGL $(EXTRA_LDFLAGS) -o $@

.PHONY: test-render
test-render: tests/render_parity_tests
	SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./tests/render_parity_tests > build/render-parity-linux.csv

build/weld_operation: benchmarks/weld_operation.cpp tests/reference_weld.h scene_tools.h mesh_modeling.h studio_mesh_types.h extensions/rust-kernels/kernels.h $(RUST_LIB)
	mkdir -p build
	$(CXX) -std=c++11 -Wall -Wextra -Werror -O2 $< $(RUST_LIB) -o $@

.PHONY: benchmark-weld
benchmark-weld: build/weld_operation
	./build/weld_operation > build/weld-linux.csv

tests/scene_shadow_tests: tests/scene_shadow_tests.cpp scene_shadows.h viewport_target.h viewport_postprocess.h
	$(CXX) $(CXXFLAGS) $< -lSDL2 -lGL $(EXTRA_LDFLAGS) -o $@

.PHONY: test-shadows
test-shadows: tests/scene_shadow_tests
	SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./tests/scene_shadow_tests
