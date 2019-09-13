MAKEFLAGS += --no-print-directory

TARGET	:= $(shell cmake -P res/scripts/target.cmake 2>&1)
SOURCE	:= $(shell cmake -P res/scripts/source.cmake 2>&1)

ifneq ($(OS),Windows_NT)

SYSTEM	:= llvm
SCRIPT	:= $(dir $(shell which vcpkg))scripts
CONFIG	:= -DCMAKE_TOOLCHAIN_FILE="$(SCRIPT)/buildsystems/vcpkg.cmake"
CONFIG	+= -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$(SCRIPT)/toolchains/linux.cmake"
CONFIG	+= -DVCPKG_TARGET_TRIPLET="$(VCPKG_DEFAULT_TRIPLET)"

all: build/$(SYSTEM)/debug/CMakeCache.txt
	@cmake --build build/$(SYSTEM)/debug --target $(TARGET)

run: build/$(SYSTEM)/debug/CMakeCache.txt
	@cmake --build build/$(SYSTEM)/debug --target $(TARGET)
	@build/$(SYSTEM)/debug/$(TARGET) 0.0.0.0 8082 html

install: build/$(SYSTEM)/release/CMakeCache.txt
	@cmake --build build/$(SYSTEM)/release --target install

build/$(SYSTEM)/debug/CMakeCache.txt: CMakeLists.txt
	@cmake -GNinja $(CONFIG) -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_INSTALL_PREFIX="$(CURDIR)/install/$(SYSTEM)/debug" \
	  -B build/$(SYSTEM)/debug

build/$(SYSTEM)/release/CMakeCache.txt: CMakeLists.txt
	@cmake -GNinja $(CONFIG) -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX="$(CURDIR)/install/$(SYSTEM)/release" \
	  -B build/$(SYSTEM)/release

ports: ports.txt
	@vcpkg install @$<

.PHONY: all run install

else

SYSTEM	:= msvc

build/$(SYSTEM)/debug/CMakeCache.txt: CMakeLists.txt
	$(error Please configure the project in Visual Studio (Debug))

build/$(SYSTEM)/release/CMakeCache.txt: CMakeLists.txt
	$(error Please configure the project in Visual Studio (Release))

ports: ports.txt
	@vcpkg install tbb @$<

endif

package: build/$(SYSTEM)/release/CMakeCache.txt
	@cmake --build build/$(SYSTEM)/release --target package

format:
	@clang-format -i $(SOURCE)

clean:
	@cmake -E remove_directory build/$(SYSTEM) install/$(SYSTEM) bin lib share

.PHONY: ports package format clean
