# PhotoVault-Firmware
Firmware repository for PhotoVault

## Cloning Repo
Clone the project repo resursivly to clone all the git submodules
```
git clone --recurse-submodules git@github.com:ThenujaL/PhotoVault-Firmware.git
```
## Environment setup

### Windows
1. Install the toolchain [here](https://dl.espressif.com/dl/esp-idf/?idf=4.4)
2. Run `install.bat` inside `PhotoVault-Firmware\esp-idf`. This will install all build and other dependencies
3. Run `export.bat` inside `PhotoVault-Firmware\esp-idf`. This will set all the environment variables. *NOTE:* this step must be done everytime you reopen the project environment

### Mac
1. Install CMake & Ninja build<br>
    **[Homebrew](https://brew.sh/)**<br>
    `brew install cmake ninja dfu-util`

    **[Macports](https://www.macports.org/install.php)**<br>
    `sudo port install cmake ninja dfu-util`

    It is strongly recommended to also install [ccache](https://ccache.dev/) for faster builds. If you have [Homebrew](https://brew.sh/), this can be done via `brew install ccache` or `sudo port install ccache` on [Macports](https://www.macports.org/install.php)

    Apple M1 Users - If you use Apple M1 platform and see an error like this:
    ```
    WARNING: directory for tool xtensa-esp32-elf version esp-2021r2-patch3-8.4.0 is present, but tool was not found
    ERROR: tool xtensa-esp32-elf has no installed versions. Please run 'install.sh' to install it.
    ```

    Or

    ```
    zsh: bad CPU type in executable: ~/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc
    ```

    Then you need to install Apple Rosetta 2 by running `/usr/sbin/softwareupdate --install-rosetta --agree-to-license`

2. Install Python 3
    **[Homebrew](https://brew.sh/)**<br>
    `brew install python3`

    **[Macports](https://www.macports.org/install.php)**<br>
    `sudo port install python38`

3. Install the tools
    ```
    cd PhotoVault-Firmware/esp-idf
    ./install.sh esp32
    ```
    This will install the tools used by ESP-IDF, such as the compiler, debugger, Python packages, etc, for projects supporting ESP32

4. Set the environment variables *NOTE:* this step must be done everytime you reopen the project environment
    `. PhotoVault-Firmware/esp-idf/export.sh`



Sources:
- [Windows Setup](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/windows-setup.html)
- [Mac Setup](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html)


### Run test app
1. cd in to `PhotoVault-Firmware/examples/hello_world`
2. Connect the ESP32 board
3. Run `idf.py build` to build the sample project
4. Flash the build to the board <br>
    `idf.py -p <PORT> flash` <br><br>
    ex: `idf.py -p COM3 flash`<br>

    To see the port number on your machine, follow the corresponsing [Windows](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/establish-serial-connection.html#check-port-on-windows) or [Mac/Linux](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/establish-serial-connection.html#check-port-on-linux-and-macos) instructions.

4. View serial output <br>
    Run `idf.py -p <PORT> monitor` <br><br>
    ex: `idf.py -p COM3 monitor`

If you see `Hello world!` being printed, you have successfully configured the board.

