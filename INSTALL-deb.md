## A guide to installing swayfx on Debian
( This guide assumes you have basic knowledge on bug fixing and compiling<br/>You may have to adapt this guide to install on other distros )

### First we must install dependencies and download source code

```bash
sudo apt install meson wayland-protocols wayland libpcre2-dev libjson-c-dev libpango-1.0-0 libcairo2-dev wget
```

Now lets setup our build enviroment. The author of this guide suggests to create a "build" folder at ~/build <br/>
**NOTE**: At the time of writing this is the latest version of wlroots and swayfx
```bash
cd build # Or whatever folder you made
# Downloading Wlroots
wget https://gitlab.freedesktop.org/wlroots/wlroots/-/archive/0.17.1/wlroots-0.17.1.tar.gz
tar -xf wlroots-0.17.1.tar.gz
rm wlroots-0.17.1.tar.gz

# Now lets get Swayfx
wget https://github.com/WillPower3309/swayfx/archive/refs/tags/0.3.2.tar.gz
tar -xf 0.3.2.tar.gz
rm 0.3.2.tar.gz
```
We now have Swayfx and wlroots in our directory
```
.
├── swayfx-0.3.2
├── wlroots-0.17.1
```
___
### Compiling
We must compile wlroots first
```bash
cd wlroots-0.17.1
meson setup build/
ninja -C build/
```
And we're done!

Now to compile swayfx
```bash
cd swayfx-0.3.2
meson build/
ninja -C build/
sudo ninja -C build/ install
```
Reboot and then add the desired effects in your ~/.config/sway/config file <br/>
eg. `blur enable|disable`

+ Guide created with ♥️ by babymusk
