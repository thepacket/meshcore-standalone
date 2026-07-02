# meshcore-standalone

A standalone, **on-device UI** for [MeshCore](https://github.com/meshcore-dev/MeshCore)
companion devices — starting with the **LilyGo T-Deck Plus** — so the radio can be
operated directly on the device, **without a phone or web app**.

## Mission

MeshCore's companion firmware is normally a near-stateless radio that you drive
from a phone/web app. The mission of **meshcore-standalone** is to make a MeshCore
device **self-sufficient**: a **touch-first** on-device interface to configure and
use the radio with no external app — while staying **portable** across companion
devices and faithful to MeshCore's underlying protocol.

- **Standalone** — full control of the device from its own screen.
- **Touch-first** — designed for the T-Deck Plus touchscreen, with the built-in
  physical keyboard as an alternative for text entry. (The T-Deck trackball is
  too unreliable to be useful and is disabled.)
- **Beautiful** — a modern, colourful, widget-rich interface (built on **LVGL**),
  not raw text on a panel.
- **Open** — MIT licensed.

## Status

**Hardware-validated on the LilyGo T-Deck Plus.** The standalone [LVGL](https://lvgl.io)
UI runs on-device, bound to live MeshCore data, and LoRa **RX and TX both work**
(after fixing a shared-SPI bus conflict between the radio and the display, where the
LovyanGFX panel was tearing down the radio's SPI bus). You can operate the node
entirely from its screen — no phone or web app — over touch or the built-in keyboard.

Built for an Android-app-quality look (black theme, per-feature colour-coded icon
chips, charts/gauges, real widgets). Live on-device:

- **Home** — colour icon-grid launcher; live status bar (RX/RE/TX/CT counters,
  **Wi-Fi indicator**, clock, battery); live RF signal scope + free-RAM/flash
  hero cards; unread badge on the Chat tile
- **Chat** — Public channel **and** direct messages (send + live receive), speech
  bubbles with **timestamps**, per-message **delivery status** (sent → delivered
  → failed), per-conversation **unread badges**, and an incoming **hop tag**
  (direct / N hops). Long-press a message to **Resend** (failed) or **Delete**;
  **emergency position share** (confirm-guarded) to a channel
- **Contacts** — live contact list with name search (space-separated OR tokens),
  a **node-type filter** (All / Chats / Repeaters / Rooms), favourites filter,
  type pills
- **Channels** — manage group channels: list, view/share a channel's key as a QR,
  and add (join or create) by name + key — including **hashtag channels**
  (`#name` derives the key from `sha256`, matching the phone clients)
- **Heard** — recently-heard stations with SNR/RSSI, age, **distance + bearing**,
  **direct-vs-routed + hop count**, node-type colour, a saved-contact tick, and a
  Recent/Signal sort toggle; tap a row for its peer card
- **Discover** — active node discovery (paced zero-hop `NODE_DISCOVER_REQ`, auto
  every 60s + **Scan now**) with a live countdown + neighbour summary; rows show
  signal dot, age, RSSI, distance/bearing and a "new this scan" flag, and tap
  through to the peer card. Auto-add of responders is a Settings toggle.
- **Stats** — rolling noise-floor **and battery-trend** charts, last RSSI/SNR,
  packet counters + **loss rate** + decoded **radio-error flags**, **memory**
  (RAM/flash) and **mesh inventory** (contacts/channels), battery/uptime, and a
  **Reset counters** button
- **Packet monitor** — live decoded RX feed with a deep per-packet breakdown:
  advert name/type/**position/clock-skew**, trace per-hop SNRs, dest/src and
  channel-name resolution, **plaintext decrypt for channels you hold**, airtime,
  link margin, sender frequency error, rebroadcast counting and "ACK for our
  message"; raw hex, path search, and **capture export to USB serial**
- **Signal** — per-repeater coverage: RSSI bar + SNR + **link margin**,
  direct-vs-routed, distance/bearing, an **RSSI trend arrow** for walk-testing,
  and a Signal/Distance/Name sort toggle; tap a row for its peer card
- **Settings** — every companion config field, editable **and persisted**: radio
  params + **region presets**, RX-boosted-gain and **hardware CAD** toggles,
  telemetry modes, GPS + **time sources / GPS clock sync**, manual clock set,
  **factory reset** (confirm-guarded), **SD backup/restore of config and
  contacts+channels** (+ packet export to serial), contacts + BLE-pin info, and
  the on-screen-keyboard toggle
- **Wi-Fi** (internet access) — join a network from an on-device **scan picker**
  (or type an SSID), credentials persisted in NVS; auto-connect on boot,
  auto-reconnect, live status (IP + RSSI), an async **ping test** (4× ICMP to
  8.8.8.8), and **NTP clock sync** that sets the mesh RTC on connect. The 2.4 GHz
  radio stays fully off while disabled, so LoRa battery life is unaffected;
  independent of the companion transport
- **Files** — a **microSD browser** (read + write): navigate folders, see sizes
  and card usage, delete files, and **format the card as FAT32**. The card also
  backs the SD backup/restore in Settings and is the store for future features
  (message history, offline maps). Shares the radio's SPI bus, re-claiming it
  after each access so RX/TX is undisturbed
- **Trace** — build a path through chosen repeaters **or trace a saved contact
  over its learned path**; result shows a summary (hops · **RTT** · weakest link),
  per-hop SNR bars, a bottleneck flag, and a **Repeat** button; live elapsed timer
  and a path-length-adaptive timeout
- **Peer details** — tap a contact for type/path/RSSI/SNR/distance/last-heard/
  location/public key, plus Share / Reset-path / Export (QR) / Remove, **remote
  Telemetry**, one-tap **Trace**, **Add-contact** (for heard-only nodes), and
  **Manage** for repeaters/rooms
- **Repeater admin** — reached from a repeater/room's peer card (**Manage**):
  remote login with a **remembered per-repeater password** (silent auto-login +
  auto status fetch), live parsed status, and a real **Console** terminal
  (scrollback + inline prompt) for remote CLI
- **Input & feedback** — physical keyboard typing into focused fields; press
  feedback + confirmation toasts on every action

See [ROADMAP.md](ROADMAP.md) for the full milestone map.

## Build & flash (LilyGo T-Deck Plus)

The firmware builds with [PlatformIO](https://platformio.org/) (`pip install platformio`
or the VS Code extension). The standalone LVGL build is the
`LilyGo_TDeck_companion_radio_usb_lvgl` environment:

```sh
# build
pio run -e LilyGo_TDeck_companion_radio_usb_lvgl

# build + flash over USB (auto-detects the port; or pass --upload-port /dev/cu.usbmodemXXXX)
pio run -e LilyGo_TDeck_companion_radio_usb_lvgl -t upload

# serial monitor
pio device monitor -b 115200
```

Connect the T-Deck over USB-C first. On macOS the port shows up as
`/dev/cu.usbmodem*`; on Linux as `/dev/ttyACM*`.

## Credits & relationship to MeshCore

This is an **independent derivative** of [MeshCore](https://github.com/meshcore-dev/MeshCore)
by meshcore-dev (Scott Powell / rippleradios.com and contributors). **All credit for
the mesh stack, the companion firmware, and the protocol belongs to them** — this
project claims authorship of **only the standalone on-device UI layer and the desktop
simulator** added on top.

It is **not affiliated with or endorsed by** the MeshCore project, does **not** submit
changes upstream, and does **not** accept pull requests (see [CONTRIBUTING.md](CONTRIBUTING.md)).
The upstream repository is tracked read-only so this fork can pull in future MeshCore updates.

## License

MIT — see [`license.txt`](license.txt). MeshCore's original copyright is retained;
this project's copyright covers only the UI additions described above.

---

## About MeshCore (upstream project)

> The section below is from the upstream MeshCore project, included here for context.
> It describes MeshCore itself, not the additions made by meshcore-standalone.

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects using LoRa and other packet radios. It is designed for developers who want to create resilient, decentralized communication networks that work without the internet.

## 🔍 What is MeshCore?

MeshCore now supports a range of LoRa devices, allowing for easy flashing without the need to compile firmware manually. Users can flash a pre-built binary using tools like Adafruit ESPTool and interact with the network through a serial console.
MeshCore provides the ability to create wireless mesh networks, similar to Meshtastic and Reticulum but with a focus on lightweight multi-hop packet routing for embedded projects. Unlike Meshtastic, which is tailored for casual LoRa communication, or Reticulum, which offers advanced networking, MeshCore balances simplicity with scalability, making it ideal for custom embedded solutions, where devices (nodes) can communicate over long distances by relaying messages through intermediate nodes. This is especially useful in off-grid, emergency, or tactical situations where traditional communication infrastructure is unavailable.

## ⚡ Key Features

* Multi-Hop Packet Routing
  * Devices can forward messages across multiple nodes, extending range beyond a single radio's reach.
  * Supports up to a configurable number of hops to balance network efficiency and prevent excessive traffic.
  * Nodes use fixed roles where "Companion" nodes are not repeating messages at all to prevent adverse routing paths from being used.
* Supports LoRa Radios – Works with Heltec, RAK Wireless, and other LoRa-based hardware.
* Decentralized & Resilient – No central server or internet required; the network is self-healing.
* Low Power Consumption – Ideal for battery-powered or solar-powered devices.
* Simple to Deploy – Pre-built example applications make it easy to get started.

## 🎯 What Can You Use MeshCore For?

* Off-Grid Communication: Stay connected even in remote areas.
* Emergency Response & Disaster Recovery: Set up instant networks where infrastructure is down.
* Outdoor Activities: Hiking, camping, and adventure racing communication.
* Tactical & Security Applications: Military, law enforcement, and private security use cases.
* IoT & Sensor Networks: Collect data from remote sensors and relay it back to a central location.

## 🚀 How to Get Started

- Watch the [MeshCore QuickStart Playlist](https://www.youtube.com/watch?v=iaFltojJrAc&list=PLshzThxhw4O4WU_iZo3NmNZOv6KMrUuF9) by The Comms Channel
- Watch the [MeshCore Technical Presentation](https://www.youtube.com/watch?v=OwmkVkZQTf4) by Liam Cottle.
- Read through our [Frequently Asked Questions](./docs/faq.md) and [Documentation](https://docs.meshcore.io).
- Flash the MeshCore firmware on a supported device.
- Connect with a supported client.

For developers:

- Install [PlatformIO](https://docs.platformio.org) in [Visual Studio Code](https://code.visualstudio.com).
- Clone and open the MeshCore repository in Visual Studio Code.
- See the example applications you can modify and run:
  - [Companion Radio](./examples/companion_radio) - For use with an external chat app, over BLE, USB or Wi-Fi.
  - [KISS Modem](./examples/kiss_modem) - Serial KISS protocol bridge for host applications. ([protocol docs](./docs/kiss_modem_protocol.md))
  - [Simple Repeater](./examples/simple_repeater) - Extends network coverage by relaying messages.
  - [Simple Room Server](./examples/simple_room_server) - A simple BBS server for shared Posts.
  - [Simple Secure Chat](./examples/simple_secure_chat) - Secure terminal based text communication between devices.
  - [Simple Sensor](./examples/simple_sensor) - Remote sensor node with telemetry and alerting.

The Simple Secure Chat example can be interacted with through the Serial Monitor in Visual Studio Code, or with a Serial USB Terminal on Android.

## ⚡️ MeshCore Flasher

We have prebuilt firmware ready to flash on supported devices.

- Launch https://meshcore.io/flasher
- Select a supported device
- Flash one of the firmware types:
  - Companion, Repeater or Room Server
- Once flashing is complete, you can connect with one of the MeshCore clients below.

## 📱 MeshCore Clients

**Companion Firmware**

The companion firmware can be connected to via BLE, USB or Wi-Fi depending on the firmware type you flashed.

- Web: https://app.meshcore.nz
- Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
- iOS: https://apps.apple.com/us/app/meshcore/id6742354151?platform=iphone
- NodeJS: https://github.com/liamcottle/meshcore.js
- Python: https://github.com/fdlamotte/meshcore-cli

**Repeater and Room Server Firmware**

The repeater and room server firmware can be set up via USB in the web config tool.

- https://config.meshcore.io

They can also be managed via LoRa in the mobile app by using the Remote Management feature.

## 🛠 Hardware Compatibility

MeshCore is designed for devices listed in the [MeshCore Flasher](https://meshcore.io/flasher)

## 📜 License

MeshCore is open-source software released under the MIT License. You are free to use, modify, and distribute it for personal and commercial projects.

## Contributing

Please submit PR's using 'dev' as the base branch!
For minor changes just submit your PR and we'll try to review it, but for anything more 'impactful' please open an Issue first and start a discussion. It is better to sound out what it is you want to achieve first, and try to come to a consensus on what the best approach is, especially when it impacts the structure or architecture of this codebase.

Here are some general principles you should try to adhere to:
* Keep it simple. Please, don't think like a high-level lang programmer. Think embedded, and keep code concise, without any unnecessary layers.
* No dynamic memory allocation, except during setup/begin functions.
* Use the same brace and indenting style that's in the core source modules. (A .clang-format is probably going to be added soon, but please do NOT retroactively re-format existing code. This just creates unnecessary diffs that make finding problems harder)

Help us prioritize! Please react with thumbs-up to issues/PRs you care about most. We look at reaction counts when planning work.

### Running unit tests

To run unit tests, run the following command:

```bash
pio test --environment native --verbose
```

## Road-Map / To-Do

There are a number of fairly major features in the pipeline, with no particular time-frames attached yet. In very rough chronological order:
- [X] Companion radio: UI redesign
- [X] Repeater + Room Server: add ACL's (like Sensor Node has)
- [X] Standardise Bridge mode for repeaters
- [ ] Repeater/Bridge: Standardise the Transport Codes for zoning/filtering
- [X] Core + Repeater: enhanced zero-hop neighbour discovery
- [ ] Core: round-trip manual path support
- [ ] Companion + Apps: support for multiple sub-meshes (and 'off-grid' client repeat mode)
- [ ] Core + Apps: support for LZW message compression
- [ ] Core: dynamic CR (Coding Rate) for weak vs strong hops
- [ ] Core: new framework for hosting multiple virtual nodes on one physical device
- [ ] V2 protocol spec: discussion and consensus around V2 packet protocol, including path hashes, new encryption specs, etc

## 📞 Get Support

- Report bugs and request features on the [GitHub Issues](https://github.com/ripplebiz/MeshCore/issues) page.
- Find additional guides and components on [my site](https://buymeacoffee.com/ripplebiz).
- Join [MeshCore Discord](https://meshcore.gg) to chat with the developers and get help from the community.
