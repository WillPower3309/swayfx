---
name: Bugs
about: Crashes and other bugs
labels: 'bug'

---

### Please read the following before submitting:
- Proprietary graphics drivers, including nvidia, are not supported. Please use the open source equivalents, such as nouveau, if you would like to use SwayFX.
- Please do NOT submit issues for information from the github wiki. The github wiki is community maintained and therefore may contain outdated information, scripts that don't work or obsolete workarounds.
  If you fix a script or find outdated information, don't hesitate to adjust the wiki page.

### Please fill out the following:
- **SwayFX Version:**
  - `swayfx -v`

- **Debug Log:**
  - Run `swayfx -d 2> ~/swayfx.log` from a TTY and upload it to a pastebin, such as gist.github.com.
  - This will record information about swayfx's activity. Please try to keep the reproduction as brief as possible and exit swayfx.
  - Attach the **full** file, do not truncate it.

- **Configuration File:**
  - Please try to produce with the default configuration.
  - If you cannot reproduce with the default configuration, please try to find the minimal configuration to reproduce.
  - Upload the config to a pastebin such as gist.github.com.

- **Stack Trace:**
  - This is only needed if swayfx crashes.
  - If you use systemd, you should be able to open the coredump of the most recent crash with gdb with
    `coredumpctl gdb swayfx` and then `bt full` to obtain the stack trace.
  - If the lines mentioning swayfx or wlroots have `??` for the location, your binaries were built without debug symbols. Please compile both swayfx and wlroots from source and try to reproduce.

- **Description:**
  - The steps you took in plain English to reproduce the problem.
