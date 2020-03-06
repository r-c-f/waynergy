# swaynergy

An unspeakably-horrible implementation of a synergy client for wlroots
compositors. Based on the upstream uSynergy library (modified for more protocol
support). Not ready for primetime (by any means) but maybe useful to somebody

## Getting started

### Prerequisites

* wayland, including wayland-scanner and the base protocols
* libxkbcommon
* A compositor making use of [wlroots](https://github.com/swaywm/wlroots)
* [wl-clipboard](https://github.com/bugaevc/wl-clipboard) for clipboard support

### Building

```
make
make install PREFIX=/where/you/want/this
```

### Running

See 
```
swaynergy -h
```
output:
```
swaynergy: Synergy client for wlroots compositors

USAGE: swaynergy [-h|--help] [-c|--host host] [-p|--port port] [-W|--width width] [-H|--height height] [-N|--name name]
	-h|--help:
		Help text
	-c|--host host:
		Server to connect to
	-p|--port port:
		Port
	-W|--width width:
		Width of screen in pixels
	-H|--height height:
		Height of screen in pixels
	-N|--name name:
		Name of client screen
```
### Configuration

The configuration files are stored in `$XDG_CONFIG_HOME/swaynergy`, which is
probably at `~/.config/swaynergy` in most cases. A single variable goes into 
each file named for the setting, because parsing is for those who are not lazy.

The basics are `port`, `host`, `name`, `width`, and `height`, which do exactly
what the command line option would do. 

#### Modifier keys

Synergy has a habit of setting the modifier mask for a given key on *release*.
To get around this, we support the notion of an intrinsic mask, where a given
keycode will trigger the modifier mask update on its own. To make use of this,
add one xkbcommon name per line in the following files in the `intrinsic_mask` 
folder:

* `alt`
* `control`
* `meta`
* `shift`
* `super`

Decent-enough defaults would probably be
##### alt
```
LALT
RALT
```
##### control
```
LCTL
RCTL
```
##### shift
```
LFSH
RTSH
```
##### super
```
LWIN
RWIN
```

I don't have a use for meta, but maybe somebody does. 

#### General keymap

There's also an xkb format keymap to provide, if the default is not sufficient;
it should be placed in `xkb_keymap`. The easiest way to obtain this if the
default is insufficient is to use the output of
```
setxkbmap -print
```

#### Screensaver

`screensaver/start` should contain a command to be run when the screensaver is
activated remotely, `screensaver/stop` should contain a command to terminate
it when it is deactivated. 

#### Idle inhibition hack

`idle-inhibit` should contain a 'start' and a 'stop' file, with commands to
be run to stop whatever idle monitor is working when swaynergy starts, and 
to restart it before swaynergy exits. This is to bypass the problem of needing
an active surface to inhibit idle in the actual underlying protocols, which 
we must do in order to prevent the screensaver/lock screen/whatever from being
triggered except by the Synergy screensaver message. In my case, this means
killing swayidle, and restarting swayidle.

## Acknowledgements

* [uSynergy](https://github.com/symless/synergy-micro-client) for the protocol library
* The swaywm people, who've provided the protocols to make something like this
possible
* wl-clipboard, because its watch mode turns it into a clipboard manager so I
I don't have to.

##TODO

* use the wayland protocols for clipboard management. wl-clipboard already existed
and is mostly fine, but Synergy specifies the format of the data (negating the 
need to guess at mimetypes) and multi-process coordination is annoying. 
* Proper support for network timeouts. This likely means rewriting the network
handling code so that it's not just a crusty hack meant to play nice with 
uSynergy, which could be changed to reflect the fact that we already read whole
packets.
* Improve idle handling
* De-uglify. This was one of those let's-not-really-plan-this-out-but-write-vaguely-working-code
sort of things, and it shows, quite noticeably. 
