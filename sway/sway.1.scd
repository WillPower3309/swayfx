swayfx(1)

# NAME

swayfx - An i3-compatible Wayland compositor

# SYNOPSIS

*swayfx* [options...] [command]

# OPTIONS

*-h, --help*
	Show help message and quit.

*-c, --config* <config>
	Specifies a config file.

*-C, --validate*
	Check the validity of the config file, then exit.

*-d, --debug*
	Enables full logging, including debug information.

*-v, --version*
	Show the version number and quit.

*-V, --verbose*
	Enables more verbose logging.

*--get-socketpath*
	Gets the IPC socket path and prints it, then exits.

# DESCRIPTION

SwayFX is a fork of the well-established Sway tiling window manager.
While Sway adheres to strict compatibility with the i3 window manager, SwayFX
liberates itself of this constraint to provide a richer set of features and a
better user experience. It introduces a new rendering engine to achieve fancy
effects such as blur, shadows, rounded corners, anti-aliasing and much more.

You can run swayfx directly from a tty, or via a Wayland-compatible login manager.

# CONFIGURATION

swayfx searches for a config file in the following locations, in this order:

. ~/.swayfx/config
. $XDG_CONFIG_HOME/swayfx/config (suggested location)
. ~/.i3/config
. $XDG_CONFIG_HOME/i3/config
. /etc/swayfx/config
. /etc/i3/config

If unset, $XDG_CONFIG_HOME defaults to *~/.config*.

An error is raised when no config file is found. The recommended default
configuration is usually installed to */etc/swayfx/config*; you are encouraged to
copy this to *~/.config/swayfx/config* and edit it from there.

For information on the config file format, see *swayfx*(5).

# IPC COMMANDS

Though *swayfxmsg*(1) is generally preferred, you may run *swayfx* _command_ to
send _command_ to the running instance of swayfx. You can also issue commands
with *i3-msg*(1) or even with *i3*(1).

# ENVIRONMENT

The following environment variables have an effect on swayfx:

_SWAYSOCK_
	Specifies the path to the swayfx IPC socket.

_XKB\_DEFAULT\_RULES_, _XKB\_DEFAULT\_MODEL_, _XKB\_DEFAULT\_LAYOUT_,
_XKB\_DEFAULT\_VARIANT_, _XKB\_DEFAULT\_OPTIONS_
	Configures the xkb keyboard settings. See *xkeyboard-config*(7). The
	preferred way to configure the keyboard is via the configuration file, see
	*swayfx-input*(5).

The following environment variables are set by swayfx:

_DISPLAY_
	If compiled with Xwayland support and Xwayland is not disabled by the
	config, this will be set to the name of the X display used for Xwayland.

_I3SOCK_
	For compatibility with i3, specifies the path to the swayfx IPC socket.

_SWAYSOCK_
	Specifies the path to the swayfx IPC socket.

_WAYLAND_DISPLAY_
	Specifies the name of the Wayland display that swayfx is running on.

_XCURSOR_SIZE_
	Specifies the configured cursor size.

_XCURSOR_THEME_
	Specifies the configured cursor theme.

# AUTHORS

Maintained by William McKinnon <contact@willmckinnon.com>, who is assisted by
other open source contributors. For more information about swayfx development, see
<https://github.com/WillPower3309/swayfx>.

# SEE ALSO

*swayfx*(5) *swayfxmsg*(1) *swayfx-input*(5) *swayfx-output*(5) *swayfx-bar*(5)
*swayfx-ipc*(7)
