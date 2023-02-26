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
cd ../..
cd ligerovm/external
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
cd ../../..
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
