# Releasing

> ⚠️ **Early development.** There are no releases yet, and there is no automated
> release pipeline. (The upstream MeshCore firmware-build/release GitHub Actions
> were removed from this fork.)

For now, builds are produced manually with PlatformIO, e.g.:

```sh
pio run -e LilyGo_TDeck_companion_radio_usb
pio run -e LilyGo_TDeck_companion_radio_ble
```

The desktop UI simulator is built separately — see [`sim/README.md`](sim/README.md).

A tagged-release process may be added later once the project stabilizes and has
been validated on hardware.
