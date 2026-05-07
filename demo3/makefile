#==============================================================================
# CMake shim for 2D Diffusion-Convection Solver
#==============================================================================

BUILDDIR ?= build
CONFIG ?= Release
TEST_BUILDDIR ?= build_make_tests

CMAKE_CONFIGURE_FLAGS ?= -DCMAKE_BUILD_TYPE=$(CONFIG) -DBUILD_TESTING=ON

all:
	cmake -S . -B $(BUILDDIR) $(CMAKE_CONFIGURE_FLAGS)
	cmake --build $(BUILDDIR) --config $(CONFIG) --target df2d

test:
	cmake -S . -B $(TEST_BUILDDIR) -DCMAKE_BUILD_TYPE=$(CONFIG) -DBUILD_TESTING=ON
	cmake --build $(TEST_BUILDDIR) --config $(CONFIG) --target adr_solver_tests
	cd $(TEST_BUILDDIR) && ctest --output-on-failure -C $(CONFIG)

clean:
	cmake --build $(BUILDDIR) --config $(CONFIG) --target clean

distclean:
	rm -rf $(BUILDDIR) $(TEST_BUILDDIR)

rebuild: distclean all

format:
	clang-format -i src/*.cpp include/*.h

clean-output:
	rm -rf output/data_* output/eta_ave_*.m output/remarks_*.m output/checkpoint_*.bin
	rm -rf data_* eta_ave_*.m remarks_*.m checkpoint_*.bin

help:
	@echo "Targets:"
	@echo "  all          - Configure and build df2d via CMake"
	@echo "  test         - Configure, build, and run adr_solver_tests via CMake"
	@echo "  clean        - Run CMake target clean for BUILDDIR"
	@echo "  distclean    - Remove CMake build directories"
	@echo "  rebuild      - distclean then all"
	@echo "  format       - Format source code"
	@echo "  clean-output - Remove simulation output files"
	@echo ""
	@echo "Variables:"
	@echo "  BUILDDIR=$(BUILDDIR)"
	@echo "  TEST_BUILDDIR=$(TEST_BUILDDIR)"
	@echo "  CONFIG=$(CONFIG)"

.PHONY: all test clean distclean rebuild format clean-output help
