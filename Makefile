# Convenience wrapper. The real build is CMake + Ninja.

BUILDDIR = build

.PHONY: all clean selftest

all:
	cmake -S . -B $(BUILDDIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
	cmake --build $(BUILDDIR) --parallel

clean:
	cmake -E rm -rf $(BUILDDIR)

selftest: all
	$(BUILDDIR)/main --selftest
