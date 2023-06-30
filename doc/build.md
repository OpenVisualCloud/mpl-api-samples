Build on Ubuntu 22.04
# Build Guide
## 1.System Requirement
### 1.1 Hardware
IMPL supports multiple hardware which supports DPC++ acceleration. Currently it has been tested with Xeon ICL CPU and Intel ATS-M GPU.
### 1.2 Operating System
IMPL can run on Ubuntu 22.04 which have high version of glibc required by DPC++.

## 2.How to build
### 2.1 Install the build dependency
To build this project you will need:
- git
- pkg-config
- cmake 3.20.5 or newer
  
**basic tools install**
```shell
sudo apt install git pkg-config cmake
```

### 2.2 Install the GPU driver
You can follow the GPU driver install guide https://dgpu-docs.intel.com/driver/installation.html#. **Note: you must install developer packages**.

IMPL depends on compute and media runtime. The following packages must be installed.

| package                               | information                             |
| :---                                  | :----                                   |
| intel-i915-dkms                       | GPU driver mode                         |
| libigdgmm12                           | Intel Graphics Memory Management Library|
| intel-opencl-icd                      | OpenCL compute runtime                  |
| libigc-dev                            | OpenCL compiler development files       |
| libigdfcl-dev                         | OpenCL compiler development files       |
| intel-igc-cm                          | CM Frontend lib                         |
| level-zero                            | Level Zero compute runtimes             |
| intel-level-zero-gpu                  | Level Zero compute runtimes             | 
| level-zero-dev                        | level zero developer package            |
| intel-media-va-driver-non-free        | VAAPI driver                            | 
| va-driver-all                         | Video Acceleration (VA) API             |
| libva-dev                             | Video Acceleration (VA) API             |
| libmfx1                               | Intel Media SDK                         |
| libmfxgen1                            | Intel oneVPL GPU Runtime                |

You can use the scripts in tools/GPUDriverInstall to install the required packages. [Driver version](https://dgpu-docs.intel.com/releases/index.html) 7.2. 20230526 is provided (package_list_0526.txt).
```shell
./download_driver_package.sh <package list file>
./install.sh <package directory>
```

### 2.3 Install Intel oneAPI

You need to install Intel® oneAPI Base Toolkit (**version 2023.0.0**)
You can choose one of the two methods below:
- use the command line and follow the instructions in the installer.
    ```shell
    wget https://registrationcenter-download.intel.com/akdlm/irc_nas/19079/l_BaseKit_p_2023.0.0.25537.sh
    sudo sh ./l_BaseKit_p_2023.0.0.25537.sh
    ```
- For other installation methods, you can refer to https://www.intel.com/content/www/us/en/developer/tools/oneapi/base-toolkit-download.html. **Note: The installed version must be 2023.0.0**

### 2.4 Compiler Environment Check

A dependency issue may occur for C++ compilation through Intel® oneAPI compiler version 2023.0 on Linux* systems. Make sure the latest version of gcc installed on your machine also has the equivalent g++ package installed. For example, gcc 12 with g++ 12.For more information, please refer to https://www.intel.com/content/www/us/en/developer/articles/troubleshooting/error-c-header-file-not-found-with-dpc-c-compiler.html

For ubuntu 22.04, you can solve the isuuse by
   ```shell
   sudo apt install gcc-12 g++-12
   ```
### 2.5 Build IMPL Library
```shell
git clone https://github.com/OpenVisualCloud/mpl-api-samples.git
cd mpl-api-samples/
./build.sh
```
### 2.6 Use CMake with IMPL Library
Using CMake with IMPL Library is supported. Use IMPL Library in your project with the following steps:
```cmake
find_package(Impl REQUIRED)
```

```cmake 
target_link_libraries(<target_name> Impl::Impl)
```


