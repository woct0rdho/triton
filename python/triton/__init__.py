"""isort:skip_file"""
__version__ = '3.0.0'

# Users may not know how to add cl and CUDA to PATH. Let's do it before loading anything
import os
if os.name == "nt":
    from .windows_utils import find_cuda, find_msvc_winsdk
    msvc_winsdk_inc_dirs, _ = find_msvc_winsdk()
    if msvc_winsdk_inc_dirs:
        cl_path = msvc_winsdk_inc_dirs[0].replace(r"\include", r"\bin\Hostx64\x64")
        os.environ["PATH"] = cl_path + os.pathsep + os.environ["PATH"]
    cuda_bin_path, _, _ = find_cuda()
    if cuda_bin_path:
        os.environ["PATH"] = cuda_bin_path + os.pathsep + os.environ["PATH"]

# ---------------------------------------
# Note: import order is significant here.

# submodules
from .runtime import (
    autotune,
    Config,
    heuristics,
    JITFunction,
    KernelInterface,
    reinterpret,
    TensorWrapper,
    OutOfResources,
    InterpreterError,
    MockTensor,
)
from .runtime.jit import jit
from .compiler import compile, CompilationError
from .errors import TritonError

from . import language
from . import testing
from . import tools

__all__ = [
    "autotune",
    "cdiv",
    "CompilationError",
    "compile",
    "Config",
    "heuristics",
    "impl",
    "InterpreterError",
    "jit",
    "JITFunction",
    "KernelInterface",
    "language",
    "MockTensor",
    "next_power_of_2",
    "ops",
    "OutOfResources",
    "reinterpret",
    "runtime",
    "TensorWrapper",
    "TritonError",
    "testing",
    "tools",
]

# -------------------------------------
# misc. utilities that  don't fit well
# into any specific module
# -------------------------------------


def cdiv(x: int, y: int):
    return (x + y - 1) // y


def next_power_of_2(n: int):
    """Return the smallest power of 2 greater than or equal to n"""
    n -= 1
    n |= n >> 1
    n |= n >> 2
    n |= n >> 4
    n |= n >> 8
    n |= n >> 16
    n |= n >> 32
    n += 1
    return n
