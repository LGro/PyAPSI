"""Setuptools integration of C++ APSI and Python package.

This file is based on https://github.com/pybind/cmake_example/blob/master/setup.py
which is licensed under the following BSD style license terms.

Copyright (c) 2016 The Pybind Development Team, All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

You are under no obligation whatsoever to provide any bug fixes, patches, or
upgrades to the features, functionality or performance of the source code
("Enhancements") to anyone; however, if you choose to make your Enhancements
available either publicly, or directly to the author of this software, without
imposing a separate written license agreement for such Enhancements, then you
hereby grant the following license: a non-exclusive, royalty-free perpetual
license to install, use, modify, prepare derivative works, incorporate into
other computer software, distribute, and sublicense such enhancements or
derivative works thereof, in binary and source code form.
"""

import os
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext

__version__ = "0.2.0"

APSI_VERSION = "0.12.0"
APSI_COMMIT = "b967a126b4e1c682b039afc2d76a98ea2c993230"

PLAT_TO_CMAKE = {
    "win32": "Win32",
    "win-amd64": "x64",
    "win-arm32": "ARM",
    "win-arm64": "ARM64",
}


def _get_vcpkg_dir():
    """Return the directory where vcpkg should be installed."""
    base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_vcpkg")
    if sys.platform.startswith("darwin"):
        return os.path.join(base, "macos")
    elif sys.platform.startswith("linux"):
        return os.path.join(base, "linux")
    elif sys.platform.startswith("win"):
        return os.path.join(base, "windows")
    return base


def _vcpkg_exists(vcpkg_dir):
    """Check if vcpkg is already bootstrapped."""
    src_dir = os.path.join(vcpkg_dir, "src")
    if sys.platform.startswith("win"):
        return os.path.isfile(os.path.join(src_dir, "vcpkg.exe")) or os.path.isfile(
            os.path.join(vcpkg_dir, "vcpkg.exe")
        )
    return os.path.isfile(os.path.join(src_dir, "vcpkg")) or os.path.isfile(
        os.path.join(vcpkg_dir, "vcpkg")
    )


def _bootstrap_vcpkg(vcpkg_dir):
    """Download and bootstrap vcpkg."""
    print("Bootstrapping vcpkg...")

    if not os.path.isdir(vcpkg_dir):
        os.makedirs(vcpkg_dir, exist_ok=True)

    zip_url = f"https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip"
    zip_path = os.path.join(vcpkg_dir, "vcpkg-src.zip")

    if not os.path.isfile(zip_path):
        print(f"Downloading vcpkg from {zip_url}...")
        urllib.request.urlretrieve(zip_url, zip_path)

    extract_dir = os.path.join(vcpkg_dir, "src")
    if not os.path.isdir(extract_dir):
        print("Extracting vcpkg...")
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(vcpkg_dir)
        first_entry = os.listdir(vcpkg_dir)[0]
        extracted = os.path.join(vcpkg_dir, first_entry)
        if os.path.isdir(extracted):
            if os.path.isdir(extract_dir):
                shutil.rmtree(extract_dir)
            shutil.move(extracted, extract_dir)

    bootstrap_script = os.path.join(extract_dir, "bootstrap-vcpkg.sh")
    if sys.platform.startswith("win"):
        bootstrap_script = os.path.join(extract_dir, "bootstrap-vcpkg.bat")

    if not sys.platform.startswith("win"):
        os.chmod(bootstrap_script, 0o755)

    print("Running vcpkg bootstrap...")
    subprocess.check_call([bootstrap_script], cwd=extract_dir)

    vcpkg_exec = os.path.join(extract_dir, "vcpkg")
    if sys.platform.startswith("win"):
        vcpkg_exec = os.path.join(extract_dir, "vcpkg.exe")

    vcpkg_target = os.path.join(vcpkg_dir, "vcpkg")
    if sys.platform.startswith("win"):
        vcpkg_target += ".exe"

    shutil.copy2(vcpkg_exec, vcpkg_target)

    print("Installing APSI dependencies via vcpkg...")
    deps = [
        "seal[no-throw-tran]",
        "kuku",
        "log4cplus",
        "cppzmq",
        "flatbuffers",
        "jsoncpp",
    ]
    subprocess.check_call([vcpkg_exec, "install"] + deps, cwd=extract_dir)

    print("vcpkg bootstrap complete.")
    return extract_dir


def _get_vcpkg_toolchain(vcpkg_src_dir):
    """Return the path to the vcpkg CMake toolchain file."""
    return os.path.join(vcpkg_src_dir, "scripts/buildsystems/vcpkg.cmake")


def _ensure_apsi_source():
    """Download APSI source if not present (for sdist installs)."""
    apsi_dir = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "external", "apsi"
    )
    if os.path.isdir(os.path.join(apsi_dir, "CMakeLists.txt")):
        return

    print("APSI source not found, downloading...")
    os.makedirs(os.path.dirname(apsi_dir), exist_ok=True)

    tar_url = f"https://github.com/microsoft/APSI/archive/{APSI_COMMIT}.tar.gz"
    tar_path = os.path.join(os.path.dirname(apsi_dir), f"apsi-{APSI_COMMIT}.tar.gz")

    if not os.path.isfile(tar_path):
        print(f"Downloading APSI {APSI_VERSION} from {tar_url}...")
        urllib.request.urlretrieve(tar_url, tar_path)

    print(f"Extracting APSI {APSI_VERSION}...")
    with tarfile.open(tar_path, "r:gz") as tf:
        tf.extractall(path=os.path.dirname(apsi_dir))

    extracted = os.path.join(os.path.dirname(apsi_dir), f"APSI-{APSI_COMMIT}")
    if os.path.isdir(extracted):
        if os.path.isdir(apsi_dir):
            shutil.rmtree(apsi_dir)
        shutil.move(extracted, apsi_dir)

    print("APSI source ready.")


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        _ensure_apsi_source()

        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        cfg = "Debug" if debug else "Release"

        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")

        vcpkg_dir = _get_vcpkg_dir()
        vcpkg_env = os.environ.get("VCPKG_ROOT_DIR")

        if vcpkg_env and _vcpkg_exists(vcpkg_env):
            if os.path.isdir(os.path.join(vcpkg_env, "scripts")):
                vcpkg_src_dir = vcpkg_env
            elif os.path.isdir(os.path.join(vcpkg_env, "src", "scripts")):
                vcpkg_src_dir = os.path.join(vcpkg_env, "src")
            else:
                vcpkg_src_dir = vcpkg_env
        elif _vcpkg_exists(vcpkg_dir):
            if os.path.isdir(os.path.join(vcpkg_dir, "src", "scripts")):
                vcpkg_src_dir = os.path.join(vcpkg_dir, "src")
            else:
                vcpkg_src_dir = vcpkg_dir
        else:
            vcpkg_src_dir = _bootstrap_vcpkg(vcpkg_dir)

        toolchain = _get_vcpkg_toolchain(vcpkg_src_dir)

        cmake_args = [
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_BUILD_TYPE={cfg}",
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
        ]
        build_args = []

        if "CMAKE_ARGS" in os.environ:
            cmake_args += [item for item in os.environ["CMAKE_ARGS"].split(" ") if item]

        if self.compiler.compiler_type != "msvc":
            if not cmake_generator or cmake_generator == "Ninja":
                try:
                    import ninja  # noqa: F401

                    ninja_executable_path = os.path.join(ninja.BIN_DIR, "ninja")
                    cmake_args += [
                        "-GNinja",
                        f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja_executable_path}",
                    ]
                except ImportError:
                    pass
        else:
            single_config = any(x in cmake_generator for x in {"NMake", "Ninja"})
            contains_arch = any(x in cmake_generator for x in {"ARM", "Win64"})
            if not single_config and not contains_arch:
                cmake_args += ["-A", PLAT_TO_CMAKE[self.plat_name]]
            if not single_config:
                cmake_args += [
                    f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={extdir}"
                ]
                build_args += ["--config", cfg]

        if sys.platform.startswith("darwin"):
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args += ["-DCMAKE_OSX_ARCHITECTURES={}".format(";".join(archs))]

        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            if hasattr(self, "parallel") and self.parallel:
                build_args += [f"-j{self.parallel}"]

        build_temp = os.path.join(self.build_temp, ext.name)
        if not os.path.exists(build_temp):
            os.makedirs(build_temp)

        subprocess.check_call(["cmake", ext.sourcedir] + cmake_args, cwd=build_temp)
        subprocess.check_call(["cmake", "--build", "."] + build_args, cwd=build_temp)


setup(
    name="apsi",
    version=__version__,
    author="Lukas Grossberger",
    author_email="code@grossberger.xyz",
    url="https://github.com/LGro/PyAPSI",
    description="Python wrapper for labeled and unlabeled asymmetric private set "
    + "intersection (APSI).",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    packages=find_packages(),
    ext_modules=[CMakeExtension("_pyapsi")],
    cmdclass={"build_ext": CMakeBuild},
    extras_require={"test": "pytest"},
    zip_safe=False,
    python_requires=">=3.11",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Utilities",
        "License :: OSI Approved :: MIT License",
        "Natural Language :: English",
        "Operating System :: POSIX :: Linux",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: Microsoft :: Windows",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: Python :: 3.14",
        "Typing :: Typed",
    ],
)
