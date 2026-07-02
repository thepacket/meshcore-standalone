# Roadmap

> ⚠️ **Early development.** A direction, not a promise — ordering and scope will change.

**Goal:** feature parity with **MeshOS** (the closed/paid standalone MeshCore firmware) as
an open, MIT alternative — a fully smartphone-independent on-device experience. We take
MeshOS / MeshUltra only as *feature & look references* (no code is copied; they're closed).

Status legend: ✅ done · 🟡 partial · ⏳ planned

---

## UI redesign on LVGL (current focus)

The feature milestones below (M1–M8) were first built in a portable bitmap UI
(`examples/companion_radio/ui-new/`). To reach a professional, Android-app-quality look,
the standalone UI is being **rebuilt on [LVGL](https://lvgl.io)** (`examples/companion_radio/ui-lvgl/`,
desktop prototype in `sim-lvgl/`): black theme, per-feature colour-coded icon chips
(FontAwesome), charts/gauges, and real widgets (switches, sliders, dropdowns, on-screen
keyboard, QR). Design language reference: **MeshUltra** (look only).

- ✅ Shared UI kit, home launcher, chat (list + conversation), contacts (list + ops),
  settings (full field set + keyboard editors + Data export/import), diagnostics
  (noise/signal/heard), stats dashboard (telemetry/noise/radio/core/packet counters),
  repeaters (list + detail gauges), peer-details panel (+ Share/Reset-path/Export/Remove),
  trace, tools/discover (companions/repeaters/rooms/sensors), terminal (console + packet
  monitor) — all live in the LVGL desktop sim. Screen set now mirrors the Android client,
  and the feature panels share a Material panel kit (`lv_ui_md_*`: flat top app bar, solid
  cards, titleMedium headers, label/value rows, cyan accent) matching the Android look. The
  home launcher keeps its glass-card grid style.
- ✅ **On-device LVGL bring-up — done and hardware-validated** on the T-Deck Plus (env
  `LilyGo_TDeck_companion_radio_usb_lvgl`): LVGL 9.2 + LovyanGFX flush + GT911 touch + tick +
  screen-manager all run on hardware (`ui-lvgl/UITask`), and **every** screen is bound to live
  `MyMesh` data through the C view-model bridge (`ui-lvgl/lv_data.h`) — home, Heard, Contacts
  (search), Stats, Packet monitor (+ tap-for-detail), Discover, Signal, Settings (incl. radio
  params, persisted), Chat (Public + DMs), Trace, and repeater admin (login/status/CLI).
- ✅ **LoRa RX and TX both work on the T-Deck.** Fixed a shared-SPI bus conflict: the radio
  (Arduino-HAL SPI) and the LovyanGFX display (ESP-IDF spi_master) were both on SPI2, so
  `display.begin()` tore down the radio's bus. Moving the display to its own host (SPI3) and
  handing the shared output pins back to the radio once per frame restored RX/TX. The bitmap
  UI stays as the fallback for mono / non-touch / companion boards.
- ✅ **On-device extras built directly in the LVGL UI:** a live **RF signal scope**
  (home panel + full-screen scope with a scale slider); **channel management**
  (list / share-key-as-QR / add-or-join); **active node discovery** (paced zero-hop
  `NODE_DISCOVER_REQ` every 60s, responders auto-added) with zero-hop/flood advert
  actions; peer-details **contact ops** (Share / Reset-path / Export-QR / Remove);
  the **physical keyboard** wired into LVGL with an on-screen-keyboard toggle; and
  global **tap feedback + toasts**. The T-Deck **trackball is disabled** — its
  hardware is too unreliable (missed rolls + false clicks) to be useful.

The protocol/firmware work from each milestone (config ops, ChatStore, RF hooks, login/
status/candidate cache) is UI-framework-agnostic and carries straight over.

---

## M1 — Settings ✅ (done)
Full on-device companion configuration: data-driven settings UI (Public info, Radio +
region presets, Contacts, Message, Position, Telemetry, Experimental, Device); app-style
look; cycle-or-keyboard input; frequency-band validation.

## M2 — Messaging & chat  🟡 (core done)
A smartphone-style chat, fully on-device.
- ✅ **Channels + DMs tabs** — channels-first (Public + configured), plus a DM list with
  last-message previews + unread badges (lists from live mesh enumeration).
- ✅ **Speech-bubble threads**, scrollable, **colour-coded usernames** (hashed palette).
- ✅ **Delivery status** — sending/sent/delivered/failed (red), via the ACK path
  (`sendTextTo` → `expected_ack` → `onMsgSendConfirmed`); send timeout → failed.
- ✅ **Compose** via physical keyboard + on-screen keyboard; word-wrapped bubbles.
- ✅ Volatile in-RAM `ChatStore` (threads lost on reboot; see M7 for persistence).
- ✅ **Tap-to-reply** quoting — tap a bubble to quote it; quoted lines render dimmed.
- ✅ **Emoji** — curated emoticon set as inline monochrome glyphs + picker (sent over-air
  as shortcodes like `:)`; word-boundary matched so URLs stay intact).
- ✅ **URL → QR code** — tap a message containing a URL to show it as a scannable
  on-screen QR (vendored MIT qrcodegen; see `THIRD_PARTY.md`).
- ⏳ **Contact-type-aware** room post/read niceties (rooms currently treated as DMs).

## M3 — RF diagnostics & tools  ✅ (done)
- ✅ **Packet monitor** — decoded raw packet stream + per-packet detail + path search.
- ✅ **Live RF scope** — fast scrolling plot of the instantaneous channel RSSI
  (`radio_driver.getCurrentRSSI()`) on the home panel, tapping through to a
  full-screen scope with a max-scale slider (shared scale, −120 dBm floor).
- ✅ **Last-heard list** — recent stations with SNR/RSSI + estimated distance
  (extended `AdvertPath`/`getRecentlyHeard`; haversine from advert lat/lon vs our GPS).
- ✅ **Signal meter** — per-repeater mesh-coverage bars + last-heard age.
- ✅ **Trace-route tool** — hop-by-hop path with per-hop SNR
  (`MyMesh::sendTrace` + `onTraceResult`).

## M3.5 — MeshOS-style home & theme ✅ (done)
Delivered alongside M3 to host the diagnostic tiles:
- ✅ **Icon-grid launcher** home (4-col tiles, line-art XBM icons + labels), replacing the
  paginated home; future tiles (Chat/Contacts/Repeaters/Finder/Map) shown dimmed.
- ✅ **Cyan-on-black theme** (shared `UIStyle.h` palette; settings + packet monitor inherit it).
- ✅ **Status bars** — top (menu · channel · clock), bottom (node · device · signal bars · battery).
- ✅ **Hamburger menu** — Send advert · Bluetooth · Reboot · Shutdown.

## M4 — GPS & offline mapping
- ⏳ **Offline maps** from SD-card tiles (needs tile renderer + projection).
- ⏳ **Node/repeater plotting** from advert lat/lon (`onAdvertRecv`).
- ⏳ **Geographic route trace** (ties to M3 trace) + **share location** over RF.
- ⏳ Touch pan/zoom (the trackball is disabled — unreliable hardware).
- Requires the SD card (T-Deck Plus has one) — gate behind storage availability.

## M5 — Remote infrastructure / repeater management  🟡 (core done)
- ✅ **Remote login** (password) to repeaters/rooms → pull **stats** (parsed RepeaterStats:
  battery, uptime, airtime tx/rx, packet counts, noise, RSSI/SNR, errors) via
  `uiLogin`/`uiRequestStatus` + new `onLoginResult`/`onStatusResponse` hooks. Login
  remembers a **per-repeater password** (persisted) for silent auto-login, and
  auto-fetches status on success.
- ✅ **Admin lives on the contact** — the standalone Repeaters section was retired;
  repeater/room management is reached from the node's peer card (**Manage**), and
  discovery of new nodes is handled by the Discover screen (not a separate Scan tab).
- ✅ **Remote triggers** — send `advert` / `clock sync` CLI commands over the air; the
  repeater's CLI reply is surfaced (`onCommandReply`).
- ⏳ Remote neighbour-list view (repeaters don't return neighbours in the status blob;
  would need a dedicated request).

## M6 — Terminal & system
- ✅ **On-device terminal** — the repeater/room **Console** (reached from a node's
  peer card → Manage): a scrollback console with an inline prompt that stays open,
  streaming remote CLI replies (`uiSendCommand` + `onCommandReply`). Login uses a
  **remembered per-repeater password** (silent auto-login + auto status fetch).
- ⏳ **Notifications** — alerts, optional audio tones (boards with a buzzer), and
  configurable screen-wake / backlight-dim rules (extend the auto-off logic).
- ⏳ **Fast boot** — trim the cold-boot path (MeshOS targets ~4 s).
- ✅ **Security** — end-to-end encryption is inherent to the MeshCore stack.

## M7 — Persistence
- ⏳ Optional **SD/flash storage** for message history, drafts, and last-heard, surviving
  reboot — enhancement on top of the volatile baseline; gated by storage availability.

## M8 — Connectivity (advanced)
- ⏳ **Unified transport image** (BLE + USB + Wi-Fi in one build) and optional **AirLink**
  bridging (relay a phone into the mesh without tethering). Largest/most speculative.

## Cross-cutting
- Portability across boards (T-Deck/T-Deck Plus first; keep the UI abstraction clean).
- Everything validated on real hardware before being called stable.
- New deps must be MIT-compatible (e.g. QR encoder); no GPL code pulled in.

## Done so far
M1 settings; the MeshOS-style cyan-on-black icon-grid launcher home with status bars; all of
M3 (packet monitor, **live RF scope**, last-heard, signal meter, trace route); M2 chat (Channels/DMs
tabs, speech-bubble threads, colour-coded names, delivery status, compose, tap-to-reply, inline
emoji, URL→QR); the M5 core (remote login, repeater stats, scanner/whitelist, remote triggers);
**channel management**, **active node discovery** (auto-add), peer-details **contact ops**
(Share/Reset/Export-QR/Remove), **physical keyboard** input + on-screen-keyboard toggle, and
global tap feedback/toasts; and the desktop simulator. (The T-Deck trackball is disabled —
unreliable hardware.)
