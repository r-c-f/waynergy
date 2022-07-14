# waynergy

An implementation of a synergy client for wayland compositors. Based on the 
upstream uSynergy library (heavily modified for more protocol support and
a bit of paranoia).

## Getting started

### Prerequisites

* wayland, including wayland-scanner and the base protocols
* libxkbcommon
* libtls (either from libressl, or libretls)
* A compositor making use of [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots), or
(on an experimental basis) KDE, or (if all else fails) the willingness to run
questionable networking utilities with the privileges to access /dev/uinput
* [wl-clipboard](https://github.com/bugaevc/wl-clipboard) for clipboard support (*may not work on all compositors*)

### Building/Installation

#### Packages

- [AUR (release)](https://aur.archlinux.org/packages/waynergy)
- [AUR (git master)](https://aur.archlinux.org/packages/waynergy-git)
- [Gentoo ebuilds (by nrndda)](https://github.com/nrndda/nrndda-overlay/tree/master/gui-apps/waynergy)

#### Manual

```
meson build
cd build
ninja
ninja install
```

Note that KDE users may need to adjust the absolute path in `waynergy.desktop`
to satisfy kwin's trust checks; a mismatch will prevent the server from 
offering the required interface. 


### Running

#### Permissions/Security

Granting networking software written in questionable C the ability to 
generate arbitrary inputs isn't exactly a recipe for success if people are 
out to get you. Using TLS is essential in any case.

__For anyone posting an issue or otherwise seeking assistance: *debug logs
are key logs*__. So far people have been prudent but it pays to make this
clear I think. Posting a full debug log will make any targeted network attack
far less concerning than your bank login credentials ending up on a public
issue page, even if they are slightly obfuscated by key mapping translations. 

##### wlroots (sway et al.)

There are no restrictions on privileged protocols at the moment (though that
[may eventually change](https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3339)).
As such it should Just Work™. 

##### KDE

`waynergy.desktop` must be installed, and the path must be absolute or the
required interface will not be offered. 

##### uinput (everyone else)

First and foremost it should be kept in mind that kernel-level virtual input
may have undesirable side effects because the compositor is being completely
bypassed. This most notably means that virtual consoles are no longer isolated
as far as input is concerned, so simple lock screens can be easily bypassed if
more than one session is active. I've not tested the usability of magic 
SysRq, but that could also potentially be a great annoyance. With that out
of the way....

waynergy must be able to open `/dev/uinput` for this to work. Doing this for
the general case as part of the installation would require setuid, which I'm
not inclined to go with given obvious security implications (though it *will*
still work, i.e. drop privileges as expected). The alternative approach is to 
create an additional group specifically for uinput access, greatly reducing the 
risk involved, but possibly varying between systems.

In my case, a 'uinput' group is created, and a udev rule is used to modify the 
permissions appropriately:
```
# /etc/udev/rules.d/49-input.rules, in my case
KERNEL=="uinput",GROUP:="uinput",MODE:="0660"
```
From here, one *could* assign one's users to this group, but doing so would
open up uinput to every program, with all the potential issues noted in the 
first paragraph. The safest approach is probably setgid: 
```
# as root -- adjust path as needed
chown :uinput waynergy
chmod g+s waynergy
```

If this doesn't still doesn't seem to work (as in #38) be sure that the
`uintput` module is loaded properly. This might be done by creating a file
`/etc/modules-load.d/uinput.conf` with the contents of `uinput`.

#### CLI

See 
```
waynergy -h
```
output:
```
./waynergy: Synergy client for wayland compositors

USAGE: ./waynergy [-h|--help] [-b|--backend backend] [-C|--config path] [-c|--host host] [-p|--port port] [-W|--width width] [-H|--height height] [-N|--name name] [-l|--logfile file] [-L|--loglevel level] [-n|--no-clip] [-e|--enable-crypto] [-t|--enable-tofu] [--fatal-none] [--fatal-ebad] [--fatal-ebsy] [--fatal-timeout]
	-h|--help:
		Help text
	-b|--backend backend:
		Input backend -- one of wlr, kde, uinput
	-C|--config path:
		Configuration directory
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
	-E|--disable-crypto:
		Force disable TLS encryption
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

Also note that `SIGUSR1` triggers re-execution. Useful until proper reconnect
procedures exist. 
### Configuration
The configuration files are stored in `$XDG_CONFIG_HOME/waynergy`, which is
probably at `~/.config/waynergy` in most cases. Aside from keymaps and hashes,
most things should go in `config.ini`; the old approach of
single-value-per-file is retained strictly for compatibility, and because 
ripping it out would be annoying for cases where that approach is really
genuinely useful (i.e. xkb keymaps which have their file format). The basic 
global values are `port`, `host`, `name`, `width`, and `height`, which do exactly
what the command line option would do. Options within sections are referred to
in the form `section/property` for the purposes of this document. 

In addition to `config.ini`, there is now support for snippets (with `.ini`
extension) under `config.ini.d/`. Any duplicate options will overwrite any
read previously. 

#### Keymap

For the time being there are two key mapping mechanisms: xkb, which works
with the wlr backend, and raw, which will work on everything. xkb appears a
bit more complex, but has the advantage that the targets of the keycodes will
remain effectively the same across any client that can run waynergy. For 
raw, both the source and the target will vary, based on the underlying
operating systems and layout configurations. 

There is also now support for using the Synergy mapped key codes rather
than the raw buttons, by setting `syn_raw_key_codes` to `false`. Eventually
this may make a more universal approach possible for the defaults, though
I'll leave that for anyone who wants to have a go at doing it. 

##### XKB

If the default is not sufficient; it should be placed in `xkb_keymap`. 
The easiest way to obtain this is to use the output of
```
setxkbmap -print
```
For custom keycodes, one may run into issues with Xwayland if the minimum
keycode is below 8. To work around this, an offset may be provided in 
the configuration as `xkb_key_offset`. 

###### Windows primary

Unfortunately there is no existing `xkb_keycodes` section included in
xkbcommon that will work with a Windows primary. To deal with this I've
included one that mostly works (minus the keys I don't actually have to test
on my own systems) in `doc/xkb/keycodes/win`. 

###### macOS primary

The same issue of keycodes applies here; see `doc/xkb/keycodes/mac` for
a usable configuration.

##### Raw keymapping

Because the fake input protocol used by KDE doesn't support custom keymaps, 
while uinput doesn't involve xkb at all, using xkb for this doesn't work; 
instead, one must use the `raw-keymap` section to map the remote keycodes to 
the local keymap using the form `remote = local`; for example, to use an 
OpenBSD server I must specify the following section to get arrow keys working 
properly:
```
[raw-keymap]
98 = 111
100 = 113
104 = 116
102 = 114
```
Because these will vary on the source and target end, providing
general-purpose mappings is more difficult, but if anyone wants to
contribute some under `doc` with clearly-defined server and client 
targets they would be appreciated by somebody probably. 

The process of generating them can probably best be seen in
[issue 27](https://github.com/r-c-f/waynergy/issues/27) toward the 
end of the thread, the gist of it is: 
- increase log level in Barrier/Synergy on the server to display the button being 
sent,
- compare to the keycode displayed in xev/wev on the client system when
using a hardware keyboard,
- add them to the `[raw-keymap]` section as `server = client` pair,
- repeat for any keys that don't work right. 

The offset functionality is enabled through `raw-keymap/offset`, though it
can also be disabled for explicit mappings by setting `raw-keymap/offset_on_explicit`
to `false`. 

###### Synergy ID keymapping

In some cases (see recent comments on #35) Windows servers are sending 
duplicate button values for keys which are clearly different. In these cases
using the synergy key IDs might make sense in an `id-keymap` section:
```
[id-keymap]
57218 = 199
57219 = 200
```
with actual values based on the above process, using the `id` parameter in the
server log instead of `button`.

#### Screensaver

`screensaver/start` should contain a command to be run when the screensaver is
activated remotely, `screensaver/stop` should contain a command to terminate
it when it is deactivated. 

#### Idle inhibition hack

Due to issues with the idle inhibition protocol, idle is actually inhibited by
sending a hopefully-meaningless event to the compositor: if `idle-inhibit/method`
is `key`, the raw keycode given in `idle-inhibit/keycode` is sent if defined, 
falling back on the xkb-style key name in `idle-inhibit/keyname`, which will
default to `HYPR`. If `idle-inhbibit/method` is `mouse`, then a relative 
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
on the host. Common values of $certpath would be
- `%LocalAppData%\Barrier\SSL\Barrier.pem`
- `~/.local/share/barrier/SSL/Barrier.pem`
- `$XDG_DATA_HOME/barrier/SSL/Barrier.pem`
- `~/.synergy/SSL/Synergy.pem`

There is also the option to trust on first use by setting `tls/tofu` or 
running with the `--enable-tofu` option on the command line, which will allow
verification that it has not changed on subsequent connections. 

Client certificates are now supported as well; simply place the certificate at
`tls/cert`.

#### wlroots wheel issues

The latest version of wlroots has an issue where discrete axis events are 
accumulated rather than sent directly; to correct for this the discrete
parameter is now a multiple of `120` rather than `1`. For older wlroots
versions experiencing abnormally-large scroll steps, set `wlr/wheel_mult` to `1`. 

## Acknowledgements
I would like to thank
* [uSynergy](https://github.com/symless/synergy-micro-client) for the protocol library
* The swaywm people, who've provided the protocols to make something like this
possible
* wl-clipboard, because its watch mode turns it into a clipboard manager so I
I don't have to.
* [mattiasgustavsson](https://github.com/mattiasgustavsson/libs) for the INI 
implementation.
* kwin developers for supporting a fake input protocol these days 
* the kernel developers for supplying uinput on Linux
* and __users like you__, whose contributions (even through simple reporting
of issues) are instrumental in making this suck just a little bit less over
time. Adding multiple backend support, for compositors I don't even use,
has proven entirely worthwhile for this reason, for example. 

## TODO

At this point I'm content with most things here save for the following

* KDE likes to hide the mouse pointer if there is no pointing device 
currently installed. I don't think it's too much work to plug in a mouse,
but some would probably prefer a better workaround. 
