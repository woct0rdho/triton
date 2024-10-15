import functools
import os
import re
import subprocess
import sys
import sysconfig
import winreg
from glob import glob
from pathlib import Path


def parse_version(s, prefix=""):
    s = s.removeprefix(prefix)
    try:
        return tuple(int(x) for x in s.split("."))
    except ValueError:
        return None


def unparse_version(t, prefix=""):
    return prefix + ".".join([str(x) for x in t])


def max_version(versions, prefix=""):
    versions = [parse_version(x, prefix) for x in versions]
    versions = [x for x in versions if x is not None]
    if not versions:
        return None
    version = unparse_version(max(versions), prefix)
    return version


def find_msvc_base_vswhere():
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere_path = (
        Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    )
    if not vswhere_path.exists():
        vswhere_path = Path(
            r"C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
        )
    if not vswhere_path.exists():
        return None

    command = [
        str(vswhere_path),
        "-prerelease",
        "-products",
        "*",
        "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-requires",
        "Microsoft.VisualStudio.Component.Windows10SDK",
        "-latest",
        "-property",
        "installationPath",
    ]
    try:
        output = subprocess.check_output(command, text=True).strip()
    except subprocess.CalledProcessError:
        return None

    msvc_base_path = Path(output) / "VC" / "Tools" / "MSVC"
    if not msvc_base_path.exists():
        return None
    return msvc_base_path


def find_msvc_base_envpath():
    paths = os.environ.get("PATH", "").split(os.pathsep)
    for path in paths:
        path = path.replace("/", "\\")
        match = re.compile(r".*\\VC\\Tools\\MSVC\\").match(path)
        if not match:
            continue

        msvc_base_path = Path(match.group(0))
        if not msvc_base_path.exists():
            continue
        return msvc_base_path

    return None


def find_msvc_base_hardcoded():
    msvc_base_path = Path(r"C:\Program Files (x86)\Microsoft Visual Studio")
    if not msvc_base_path.exists():
        msvc_base_path = Path(r"C:\Program Files\Microsoft Visual Studio")
    if not msvc_base_path.exists():
        return None

    paths = glob(str(msvc_base_path / "*" / "*" / "VC" / "Tools" / "MSVC"))
    if not paths:
        return None
    # Heuristic to find the highest version
    return Path(sorted(paths)[-1])


def find_msvc():
    msvc_base_path = find_msvc_base_vswhere()
    if msvc_base_path is None:
        msvc_base_path = find_msvc_base_envpath()
    if msvc_base_path is None:
        msvc_base_path = find_msvc_base_hardcoded()
    if msvc_base_path is None:
        print("WARNING: Failed to find MSVC.")
        return [], []

    version = max_version(os.listdir(msvc_base_path))
    if version is None:
        print("WARNING: Failed to find MSVC.")
        return [], []

    return (
        [str(msvc_base_path / version / "include")],
        [str(msvc_base_path / version / "lib" / "x64")],
    )


def find_winsdk_base_registry():
    try:
        reg = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
        key = winreg.OpenKeyEx(
            reg, r"SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows\v10.0"
        )
        folder = winreg.QueryValueEx(key, "InstallationFolder")[0]
        winreg.CloseKey(key)
    except OSError:
        return None

    winsdk_base_path = Path(folder)
    if not winsdk_base_path.exists():
        return None
    return winsdk_base_path


def find_winsdk_base_hardcoded():
    winsdk_base_path = Path(r"C:\Program Files (x86)\Windows Kits\10")
    if not winsdk_base_path.exists():
        return None
    return winsdk_base_path


def find_winsdk():
    winsdk_base_path = find_winsdk_base_registry()
    if winsdk_base_path is None:
        winsdk_base_path = find_winsdk_base_hardcoded()
    if winsdk_base_path is None:
        print("WARNING: Failed to find Windows SDK.")
        return [], []

    version = max_version(os.listdir(winsdk_base_path / "Include"))
    if version is None:
        print("WARNING: Failed to find Windows SDK.")
        return [], []

    return (
        [
            str(winsdk_base_path / "Include" / version / "shared"),
            str(winsdk_base_path / "Include" / version / "ucrt"),
            str(winsdk_base_path / "Include" / version / "um"),
        ],
        [
            str(winsdk_base_path / "Lib" / version / "ucrt" / "x64"),
            str(winsdk_base_path / "Lib" / version / "um" / "x64"),
        ],
    )


@functools.cache
def find_msvc_winsdk():
    msvc_inc_dirs, msvc_lib_dirs = find_msvc()
    winsdk_inc_dirs, winsdk_lib_dirs = find_winsdk()
    return msvc_inc_dirs + winsdk_inc_dirs, msvc_lib_dirs + winsdk_lib_dirs


@functools.cache
def find_python():
    version = sysconfig.get_python_version().replace(".", "")
    python_lib_dirs = [
        os.path.join(os.path.dirname(sysconfig.get_paths()["stdlib"]), "libs"),
        os.path.join(os.path.dirname(sysconfig.get_paths()["platstdlib"]), "libs"),
        os.path.join(os.path.dirname(sys.executable), "libs"),
        rf"C:\Python{version}\libs",
    ]
    return python_lib_dirs


@functools.cache
def find_cuda():
    cuda_base_path = os.environ.get("CUDA_PATH")
    if cuda_base_path is not None:
        cuda_base_path = Path(cuda_base_path)
        if not cuda_base_path.exists():
            cuda_base_path = None

    if cuda_base_path is None:
        paths = glob(r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12*")
        if not paths:
            return None, [], []
        # Heuristic to find the highest version
        cuda_base_path = Path(sorted(paths)[-1])

    return (
        str(cuda_base_path / "bin"),
        [str(cuda_base_path / "include")],
        [str(cuda_base_path / "lib" / "x64")],
    )
