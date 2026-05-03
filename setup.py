from pathlib import Path

from setuptools import Extension, setup


ROOT = Path(__file__).parent
HEAPX_ROOT = ROOT / "extern" / "heapx"

heapx_sources = [
    "extern/heapx/src/heap.c",
    "extern/heapx/src/heaps/binary_heap.c",
    "extern/heapx/src/heaps/fibonacci_heap.c",
    "extern/heapx/src/heaps/kaplan_heap.c",
]

extension = Extension(
    "heapx._heapx",
    sources=[
        "src/heapx/_heapx.c",
        *heapx_sources,
    ],
    include_dirs=[
        str(HEAPX_ROOT / "include"),
        str(HEAPX_ROOT / "src"),
    ],
    extra_compile_args=["-std=c99", "-Wall", "-Wextra", "-pedantic"],
)


setup(ext_modules=[extension])
