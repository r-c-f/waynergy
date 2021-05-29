# waynergy

An implementation of a synergy client for wlroots compositors. Based on the 
upstream uSynergy library (heavily modified for more protocol support and
a bit of paranoia).

## Getting started

### Prerequisites

* wayland, including wayland-scanner and the base protocols
* libxkbcommon
* libtls (either from libressl, or libretls)
* A compositor making use of [wlroots](https://github.com/swaywm/wlroots)
* [wl-clipboard](https://github.com/bugaevc/wl-clipboard) for clipboard support

### Building

```
meson build
cd build
ninja
ninja install
```

### Running

See 
```
waynergy -h
```
output:
```
waynergy: Synergy client for wlroots compositors

USAGE: waynergy [-h|--help] [-c|--host host] [-p|--port port] [-W|--width width] [-H|--height height] [-N|--name name] [-l|--logfile file] [-L|--loglevel level] [-n|--no-clip] [--fatal-none] [--fatal-ebad] [--fatal-ebsy] [--fatal-timeout]
	-h|--help:
		Help text
	-c|--host host:
		Server to connect to
	-p|--port port:
		Port
	-W|--width width:
		Width of screen in pixels (manual override, must be given with height)
	-H|--height height:
		Height of screen in pixels (manual override, must be given with width)
	-N|--name name:
		Name of client screen
	-l|--logfile file:
		Name of logfile to use
	-L|--loglevel level:
		Log level -- number, increasing from 0 for more verbosity
	-n|--no-clip:
		Don't synchronize the clipboard
	-e|--enable-crypto:
		Enable TLS encryption
	-t|--enable-tofu:
		Enable trust-on-first-use for TLS certificate
	--fatal-none:
		Consider *normal* disconnect (i.e. CBYE) to be fatal
	--fatal-ebad:
		Protocol errors are fatal
	--fatal-ebsy:
		EBSY (client already exists with our name) errors are fatal
	--fatal-timeout:
		timeouts are fatal

```

Also note that `SIGUSR1` triggers re-execution. Useful until proper recconect
procedures exist. 
### Configuration

The configuration files are stored in `$XDG_CONFIG_HOME/waynergy`, which is
probably at `~/.config/waynergy` in most cases. A single variable goes into 
each file named for the setting, because parsing is for those who are not lazy.

The basics are `port`, `host`, `name`, `width`, and `height`, which do exactly
what the command line option would do. 

#### Keymap

There's also an xkb format keymap to provide, if the default is not sufficient;
it should be placed in `xkb_keymap`. The easiest way to obtain this if the
default is insufficient is to use the output of
```
setxkbmap -print
```
For custom keycodes, one may run into issues with Xwayland if the minimum
keycode is below 8. To work around this, an offset may be provided in 
`xkb_key_offset`. 

##### Windows primary

Unfortunately there is no existing `xkb_keycodes` section included in
xkbcommon that will work with a Windows primary. To deal with this I've
included one that mostly works (minus the keys I don't actually have to test
on my own systems) in `doc/xkb/keycodes/win`. 

##### macOS primary

The same issue of keycodes applies here; see `doc/xkb/keycodes/mac` for
a usable configuration. 

#### Screensaver

`screensaver/start` should contain a command to be run when the screensaver is
activated remotely, `screensaver/stop` should contain a command to terminate
it when it is deactivated. 

#### Idle inhibition hack

Due to issues with the idle inhibition protocol, idle is actually inhibited by
sending a hopefully-meaningless event to the compositor: if `idle-inhibit/method`
is `key`, the key associated with the xkb-style name in `idle-inhibit/keyname` is
pressed (defaults to `HYPR`). If `idle-inhbibit/method` is `mouse`, then a relative 
move of 0,0 is sent (this is the default). 

The mouse approach prevents any clashes with keys, but will prevent cursor
hiding.

#### TLS

Enabled or disabled with `tls/enable`. Certificate hashes (for any given host) 
are stored in the directory `tls/hash/`, one hash per file named after the
host. These can be obtained by running something like 
```
printf "SHA256:%s\n" $(openssl x509 -outform der -in $certpath | openssl dgst -sha256 | cut -d ' ' -f 2)
```
on the host. Comman values of $certpath would be
- `%LocalAppData%\Barrier\SSL\Barrier.pem`
- `~/.local/share/barrier/SSL/Barrier.pem`
- `$XDG_DATA_HOME/barrier/SSL/Barrier.pem`
- `~/.synergy/SSL/Synergy.pem`

There is also the option to trust on first use by setting `tls/tofu` or 
running with the `--enable-tofu` option on the command line, which will allow
verification that it has not changed on subsequent connections. 

## Acknowledgements

* [uSynergy](https://github.com/symless/synergy-micro-client) for the protocol library
* The swaywm people, who've provided the protocols to make something like this
possible
* wl-clipboard, because its watch mode turns it into a clipboard manager so I
I don't have to.

## TODO

* use the wayland protocols for clipboard management. wl-clipboard already existed
and is mostly fine, but Synergy specifies the format of the data (negating the 
need to guess at mimetypes) and multi-process coordination is annoying. 
* De-uglify. This was one of those let's-not-really-plan-this-out-but-write-vaguely-working-code
sort of things, and it shows, quite noticeably. 
