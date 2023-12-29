## A guide to installing swayfx on Debian
( You may have to adapt this guide to install on other distros )

### First install all required dependencies and download the source code

```bash
sudo apt install meson wayland-protocols wayland libpcre2-dev libjson-c-dev libpango-1.0-0 libcairo2-dev wget
```

### Next setup the build enviroment
**NOTE**: At the time of writing this is the latest version of wlroots and swayfx.
```bash
mkdir ~/build
cd ~/build # Or whatever you have named it
# Downloading Wlroots
wget https://gitlab.freedesktop.org/wlroots/wlroots/-/archive/0.17.1/wlroots-0.17.1.tar.gz
tar -xf wlroots-0.17.1.tar.gz
rm wlroots-0.17.1.tar.gz

# Downloaing Swayfx
wget https://github.com/WillPower3309/swayfx/archive/refs/tags/0.3.2.tar.gz
tar -xf 0.3.2.tar.gz
rm 0.3.2.tar.gz
```
swayfx and wlroots should now be located in the `build` directory.
```
.
├── swayfx-0.3.2
├── wlroots-0.17.1
```
___
### Compiling
You MUST compile wlroots first.
```bash
cd wlroots-0.17.1
meson setup build/
ninja -C build/
```

Now to compile swayfx.
```bash
cd swayfx-0.3.2
meson build/
ninja -C build/
sudo ninja -C build/ install
```
Reboot and then add the desired effects in your ~/.config/sway/config file <br/>
e.g. `blur enable|disable`

+ Guide created with ♥️ by babymusk
