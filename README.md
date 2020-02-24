# swaynergy

An unspeakably-horrible implementation of a synergy client for wlroots
compositors. Based on the upstream uSynergy library (modified for more protocol
support). Not ready for primetime (by any means) but maybe useful to somebody
who -- not knowing C -- would be willing to try it. 

## Getting started

### Prerequisites

* wayland, including wayland-scanner and the base protocols
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

### Configuration

The configuration files are stored in `$XDG_CONFIG_HOME/swaynergy`, which is
probably at `~/.config/swaynergy` in most cases. A single variable goes into 
each file named for the setting, because parsing is for those who are not lazy.

#### Modifier keys

Due to the annoying nature of the synergy protocol and an unwillingess to work 
too much with xkbcommon, we support intrinsic masks for a given set of keys. 
One scancode per line in the following files:

* `alt`
* `control`
* `shift`
* `super`

#### General keymap

There's also an xkb format keymap to provide, if the default is not sufficient;
it it should be placed in `xkb_keymap`. 


## Acknowledgements

* [uSynergy](https://github.com/symless/synergy-micro-client) for the protocol library
