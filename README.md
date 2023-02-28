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
| 2^10         | 333                | 60,828           | N/A           | 48,828            | 19,395                | N/A                | 16,121                 |
| 2^11         | 333                | 60,844           | N/A           | 55,812            | 19,391                | N/A                | 16,833                 |
| 2^12         | 333                | 60,884           | N/A           | 79,460            | 19,391                | N/A                | 22,023                 |
| 2^13         | 333                | 62,308           | N/A           | 108,156           | 19,392                | N/A                | 31,002                 |
| 2^14         | 840                | 62,316           | 15,352        | 182,500           | 19,395                | 3,152              | 51,472                 |
| 2^15         | 840                | 62,036           | 18,108        | 298,308           | 19,391                | 3,838              | 81,127                 |
| 2^16         | 840                | 65,116           | 23,356        | 548,180           | 20,445                | 5,199              | 149,395                |
| 2^17         | 1861               | 70,720           | 34,348        | 991,040           | 21,954                | 8,057              | 273,156                |
| 2^18         | 1861               | 70,360           | 57,328        | 1,916,228         | 21,916                | 14,053             | 531,482                |
| 2^19         | 3908               | 80,792           | 102,612       | 3,661,044         | 24,954                | 25,688             | 1,035,395              |
| 2^20         | 3908               | 81,232           | 196,564       | 7,208,832         | 24,993                | 49,892             | 2,120,920              |
| 2^21         | 8003               | 103,812          | 379,868       | 14,050,096        | 31,250                | 97,141             | 4,588,409              |
| 2^22         | 8003               | 104,212          | 756,848       | N/A               | 31,315                | 194,167            | N/A                    |
| 2^23         | 16195              | 148,076          | 1,500,508     | N/A               | 43,169                | 385,644            | N/A                    |
| 2^24         | 16195              | 149,432          | 3,028,280     | N/A               | 43,444                | 782,885            | N/A                    |
| 2^25         | 32579              | 246,324          | 6,042,844     | N/A               | 69,589                | 1,563,046          | N/A                    |

	
