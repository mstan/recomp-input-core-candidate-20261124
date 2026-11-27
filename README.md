# recomp-input-core

Current licensing and the grant for existing owner contributions are documented in [LICENSE-SCOPE.md](LICENSE-SCOPE.md).

Host controller selection, remapping and input policy extracted from shared
recomp-ui and Genesis runner code. It operates on caller-supplied standard
controller/keyboard snapshots; SDL2/SDL3 handles, event pumps, native device
names and gyro activation remain backend responsibilities.

Link `recomp-input::core` and include `recomp_input.h`. Create one context for
1-8 players and 1-24 logical actions. Publish a snapshot with `ri_update`, then
read independent per-player action masks with `ri_sample`. Every context owns
its settings, snapshots and capture state. Serialize access to one context;
different contexts have no shared mutable storage. No threads or disk I/O are
created by the service.

## Existing contracts preserved

- The launcher button/axis/combo encoding is retained: 1 + button index,
  100 + axis index * 2 + direction, and 1000 + button mask. Indices are SDL
  standard game-controller indices. Action names and guest controller protocols
  remain console-owned.
- Genesis percentage deadzones use `pct * 32767 / 100` and strict comparisons.
  Keyboard and gamepad sources may be additive. Gameplay blocking returns no
  actions while preserving held physical inputs for normal resumption.
- Full-release detection uses recomp-ui's 8000 axis band. Capture waits for a
  release, then accepts a new button or an axis strictly beyond 20000.
- A deliberate controller assignment transfers that physical instance from a
  previous seat. Other controller assignments remain stable when devices
  disconnect. Duplicate enumeration rows with one live instance id are removed;
  two actual identical-model devices stay distinct.
- Friendly driver names and custom labels are retained; opaque GUIDs are never
  primary labels. Identical live names receive readable numbered list labels.

`RI_SOURCE_KEYBOARD` and `RI_SOURCE_GAMEPAD` are source bits; their OR means both.
Do not pass launcher mouse/GunCon source kinds 3/4 directly as this bitset. Those
native peripherals, touch and guest-specific analog conventions stay in the
console adapter. Gyro metadata is carried in device snapshots without polling
or enabling the sensor itself.

## Identity, hotplug and persistence

Live selection uses a backend instance id, including id zero. A GUID is a
model/profile key and may identify multiple physical controllers. Supply a
stable physical key from a serial/path when the backend has one; it is optional.
After disconnect, the service resolves an exact physical key, or an unambiguous
unclaimed matching GUID. Ambiguous identical-model reconnects remain disconnected
until explicitly selected, preserving input isolation rather than assigning an
unrelated controller. Saved/custom labels survive disconnects. Reuse of a live
id for a different physical key or model does not inherit the old seat.

Explicit edits go through a caller persistence callback before publication.
Return zero if saving fails; the entire edit, including a physical-seat transfer,
remains unapplied. Polling, sampling and ordinary reconnects do not write
settings. Read settings with `ri_get_player`, restore them through `ri_restore`,
and use existing engine/launcher file formats in the callback. There is no new
INI format or migration. The launcher adapter maps `player_key_bind`,
`player_pad_bind`, `player_gamepad_guid`, deadzone and friendly-name fields to
`ri_player`; its live `player_gamepad_instance` convention is id + 1 and must be
converted explicitly. Production adapters/pins are deferred to integration.

## Component checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --timeout 0
```

Tests compile the unchanged pinned launcher_input.c against a deterministic SDL2
snapshot backend and compile the exact pinned Genesis deadzone/bind helpers.
They compare release bands and binding decisions over six axes, both signs,
four deadzones and all important extreme/threshold values. Realistic fixtures
exercise two same-model controllers, per-player button/key isolation, custom
labels, physical transfer, failed persistence, optional physical-key reconnect,
ambiguous GUIDs, instance-id reuse, capture, invalid records and settings restore.
This validates service policy; actual SDL2/SDL3 hardware, native peripherals,
launcher UI and gameplay integration remain later checks.

## Provenance

`PROVENANCE.json` records donor revisions and source hashes. The frozen launcher
oracle retains its MIT license under `licenses/`. Owner contributions are also
available under MIT as documented in `LICENSE-SCOPE.md`; historical donor
notices remain available. No game files, profiles, owner device data or captured
input is included.

## Analog profiles and finite haptics

`recomp_input_effects.h` adds native axis shaping and a serialized haptic owner.
The byte rescale, clipped pointer, and half-axis profiles preserve their
individual deadbands, signed endpoints, centres, and integer rounding. They do
not change stored bindings or guest pad layouts.

The haptic context combines two sources per player, routes their maximum motor
strengths to physical device IDs, expires frame or millisecond sources, renews
finite effects, and stops devices on transfer or release. Its backend callback
only applies the resulting motor command. Use the existing input owner or lock;
callbacks must not reenter that context. Release each device before closing its
handle, then destroy the context after its callers stop. Frame lifetimes use
unsigned 32-bit subtraction. Millisecond timestamps use one monotonic clock.
No worker, lock, SDL dependency, handle ownership, or hardware capabilities are
added by the provider.

