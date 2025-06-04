# PhotoVault-Firmware
Firmware repository for PhotoVault

## Cloning Repo
Clone the project repo resursivly to clone all the git submodules
```
git clone --recurse-submodules git@github.com:ThenujaL/PhotoVault-Firmware.git
```
## Environment setup

### Windows

1. Install the toolchain [here] (https://dl.espressif.com/dl/esp-idf/?idf=4.4)
2. Run `install.bat` inside `PhotoVault-Firmware\esp-idf`. This will install all build and other dependencies
3. Run `export.bat` inside `PhotoVault-Firmware\esp-idf`. This will set all the environment variables.


### Run test app
1. Cd in to `PhotoVault-Firmware/examples/hello_world`
2. Run `idf.py build` to build the ssample project
3. Flash the build to the board <br>
    (Windows) Run `idf.py -p <PORT> flash` <br>
    ex: `idf.py -p COM3 flash`
    <br>
    <br>
    (Linux)
    <br>
    <br>
    (Mac)
4. View serial output <br>
    (Windows) Run `idf.py -p <PORT> monitor` <br>
    ex: `idf.py -p COM3 monitor`
    <br>
    <br>
    (Linux)
    <br>
    <br>
    (Mac)

If you see `Hello world!` being printed, you have successfully configured the board.

