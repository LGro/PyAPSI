import os
from glob import glob

# Available at setup time due to pyproject.toml
from pybind11.setup_helpers import Pybind11Extension
from setuptools import setup

__version__ = "0.0.1"

vcpkg_installed_dir = os.environ["VCPKG_INSTALLED_DIR"]

ext_modules = [
    Pybind11Extension(
        "pyapsi",
        sorted(glob("src/*.cpp")),
        define_macros=[
            ("VERSION_INFO", __version__),
            ("__LINUX__", 1),
            ("_X86_", 1),
            ("GENERIC_IMPLEMENTATION", 1),
        ],
        include_dirs=[
            f"{vcpkg_installed_dir}/x64-linux/include",
            f"{vcpkg_installed_dir}/x64-linux/include/gsl",
            f"{vcpkg_installed_dir}/x64-linux/include/Kuku-2.1",
            f"{vcpkg_installed_dir}/x64-linux/include/SEAL-3.7",
            f"{vcpkg_installed_dir}/x64-linux/include/APSI-0.7",
        ],
        extra_objects=sorted(glob(f"{vcpkg_installed_dir}/x64-linux/lib/*.a")),
        cxx_std=17,
        extra_compile_args=[],
    ),
]

setup(
    name="pyapsi",
    version=__version__,
    author="Lukas Grossberger",
    author_email="code@grossberger.xyz",
    url="https://github.com/LGro/pyapsi",
    description="Python wrapper for APSI",
    long_description="",
    ext_modules=ext_modules,
    extras_require={"test": "pytest"},
    zip_safe=False,
    python_requires=">=3.8",
)
