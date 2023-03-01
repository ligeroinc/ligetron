# Downloading and Building Ligerovm on Ubuntu Using CMake

This README will guide you through the process of downloading the Ligerovm project from GitLab, building its dependencies, and building it using CMake.

## Prerequisites
Before proceeding, ensure that the following prerequisites are met:

* Ubuntu 22.04 is installed on your system.
* Update Ubuntu system by running the following command in your terminal:

``` bash
sudo apt-get update && sudo apt-get upgrade -y
```

* Install necessary dependencies by running the following command in your terminal:

```bash
sudo apt-get install g++-12 libtbb-dev cmake libssl-dev libboost-all-dev -y
```

* Git is installed on your system. If it is not installed, you can download and install it by running the following command in your terminal:

``` bash
sudo apt-get install git -y
```

## Building Dependencies
Before building the Ligerovm project, you need to build and install the HEXL and WABT dependencies using g++-12. Follow these steps:

``` bash
git clone https://github.com/intel/hexl.git
cd hexl
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=g++-12 ..
make -j
sudo make install
```

``` bash
git clone https://github.com/WebAssembly/wabt.git
cd wabt
git submodule update --init
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=g++-12 ..
make -j
sudo make install
```

## Downloading and Building Ligerovm

To download the Ligerovm project from GitLab and build it using CMake, follow these steps:

``` bash
git clone https://gitlab.stealthsoftwareinc.com/ruihanwang/ligerovm.git --branch non-batch
cd ligerovm
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=g++-12 ..
make -j
```

This will clone the Ligerovm project from GitLab, switch to the non-batch branch, build the dependencies, and build the project using CMake.

## Running the Prover/Verifier
To run the prover/verifier, follow these steps:

* Set the OMP_NUM_THREADS environment variable to the number of cores you want to use. For example, if you want to use 4 cores, run:

``` bash
export OMP_NUM_THREADS=4
```

* Navigate to the build directory and run the following command to run the prover:

``` bash
./prover <WASM module> <packing size> <args...>
```

* Navigate to the build directory and run the following command to run the verifier:

``` bash
./verifier <WASM module> <packing size> <args...>
```

* Replace <WASM module>, <packing size>, and <args...> with the appropriate arguments for your use case.


## Examples

### Example 1: Edit Distance
To run the Edit Distance program with the `edit.wasm` module located in `wasm/edit.wasm` and arguments `abcde` and `bcdef`, follow these steps:

* Navigate to the build directory and run the following command:

``` bash
./prover ../wasm/edit.wasm 893 abcde bcdef
```

This will run the Edit Distance program with the given packing size and arguments and produce the proof.


### Example 2: Minimal Spanning Tree
To run the Minimal Spanning Tree program with the `mst.wasm` module located in `wasm/mst.wasm` and arguments `003006777005003335006002649001002027000009763006000426002006211008007429002000862003007135 xxxxxxxxxx`, follow these steps:

* Navigate to the build directory and run the following command:

``` bash
./prover ../wasm/mst.wasm 893 003006777005003335006002649001002027000009763006000426002006211008007429002000862003007135 xxxxxxxxxx
```

This will run the Minimal Spanning Tree program with the given arguments and produce the proof.

## Benchmarks

### Space usage and Page Faults

All space usage is measured with command `/usr/bin/time -v` and displayed as peak memory usage in KB.

| Circuit Size | Ligetron - Packing | Ligetron - Space | Orion - Space | Brakedown - Space | Ligetron - Page Fault | Orion - Page Fault | Brakedown - Page Fault |
|--------------|--------------------|------------------|---------------|-------------------|-----------------------|--------------------|------------------------|
| 2^12         | 333                | 60,884           | 15,352        | 79,460            | 19,391                | 3,152              | 22,023                 |
| 2^13         | 333                | 62,308           | 18,108        | 108,156           | 19,392                | 3,838              | 31,002                 |
| 2^14         | 840                | 62,316           | 23,356        | 182,500           | 19,395                | 5,199              | 51,472                 |
| 2^15         | 840                | 62,036           | 34,348        | 298,308           | 19,391                | 8,057              | 81,127                 |
| 2^16         | 840                | 65,116           | 57,328        | 548,180           | 20,445                | 14,053             | 149,395                |
| 2^17         | 1861               | 70,720           | 102,612       | 991,040           | 21,954                | 25,688             | 273,156                |
| 2^18         | 1861               | 70,360           | 196,564       | 1,916,228         | 21,916                | 49,892             | 531,482                |
| 2^19         | 3908               | 80,792           | 379,868       | 3,661,044         | 24,954                | 97,141             | 1,035,395              |
| 2^20         | 3908               | 81,232           | 756,848       | 7,208,832         | 24,993                | 194,167            | 2,120,920              |
| 2^21         | 8003               | 103,812          | 1,500,508     | 14,050,096        | 31,250                | 385,644            | 4,588,409              |
| 2^22         | 8003               | 104,212          | 3,028,280     | N/A               | 31,315                | 782,885            | N/A                    |
| 2^23         | 16195              | 148,076          | 6,042,844     | N/A               | 43,169                | 1,563,046          | N/A                    |


### Hardware Acceleration

* `AVX512` support is disabled by setting the following runtime flags. Note the code still use `AVX2` acceleration.

``` bash
export HEXL_DISABLE_AVX512DQ=1
export HEXL_DISABLE_AVX512IFMA=1
export HEXL_DISABLE_AVX512VBMI2=1
```

* `sha` extension is disabled by setting the following runtime flags:

``` bash
export OPENSSL_ia32cap=~0x20000000
```

| Circuit Size   | Packing | AVX512 + SHA_NI | AVX512 | SHA_NI | None   |
|----------------|---------|-----------------|--------|--------|--------|
| 273,567        | 1,861   | 156             | 163    | 210    | 219    |
| 1,074,700      | 3,908   | 513             | 568    | 725    | 774    |
| 4,273,602      | 8,003   | 2,205           | 2,446  | 2,856  | 3,055  |
| 17,037,140     | 16,195  | 9,134           | 9,692  | 12,163 | 12,765 |
| 68,024,952     | 32,579  | 36,123          | 39,829 | 52,911 | 54,881 |
|----------------|---------|-----------------|--------|--------|--------|
| Avg. speedup   | N/A     | 1.43x           | 1.33x  | 1.05x  | 1x     |

All time are in milliseconds (ms).
