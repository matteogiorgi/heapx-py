import sys
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

if sys.platform == "win32":
    extra_compile_args = [
        "/std:c11",
    ]
else:
    extra_compile_args = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-pedantic",
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
    extra_compile_args=extra_compile_args,
)

setup(ext_modules=[extension])
