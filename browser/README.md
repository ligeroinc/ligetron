# Run Ligetron from Browsers

## Installing Emscripten
If you don't already have Emscripten installed on your system, you can download and install it from the official website: https://emscripten.org/docs/getting_started/downloads.html.

Note: It's highly recommanded to install Emscripten SDK from GitHub instead of `apt install`.

Once the Emscripten SDK is installed, open a terminal or command prompt and run the following command to activate the Emscripten environment:

``` bash
source <path to emsdk>/emsdk_env.sh
```
Replace <path to emsdk> with the path to the directory where you installed the SDK.

Verify that Emscripten is installed and working by running the following command:

``` bash
emcc --version
```
This should display the version number of Emscripten.

With Emscripten installed and configured, you can now use emrun to run prover.html and verifier.html as described in the following section.


## Run with `emrun`

**Note: Currently Safari is not supported.**

Launch a prover or verifier with the following command

``` bash
emrun prover.html -- modules/<wasm module> <packing> <args...>
```

`modules` directory is a comile-time preloaded folder that contains three WASM modules: `edit.wasm`, `count.wasm`, and `mst.wasm`. Those WASM modules are provided as an example, more can be easily add either by preload at compile time or use Javascript to upload at runtime.

For example, to run an edit distance with packing size 8003:

``` bash
emrun prover.html -- modules/edit.wasm 8003 foo bar
```

When the prover finish, hit the button to download proof from browser and upload it into a `verifier.html` if desired. Note that the proof must named `proof.data`.

