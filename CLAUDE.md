# CLAUDE.md

## Project Overview

ESPHome custom component for controlling Samsung air conditioning systems over RS-485. Supports two protocols:

- **NASA protocol** — newer Samsung units, dotted address format (`20.00.00`)
- **Non-NASA protocol** — legacy Samsung units, single hex byte address (`00`, `01`, `c8`)

Protocol is auto-detected at runtime from the first valid packet received.

Hardware: typically an ESP32 (e.g., M5STACK ATOM Lite) with an RS-485 transceiver connected to the AC system's F1/F2 bus.

## Repository Structure

```
esphome_samsung_ac/
├── components/samsung_ac/     # Core ESPHome component (C++ and Python)
│   ├── __init__.py            # ESPHome config schema + code generation (~500 lines)
│   ├── samsung_ac.h/cpp       # Main component: Samsung_AC (PollingComponent + UARTDevice)
│   ├── samsung_ac_device.h/cpp          # Per-unit device abstraction
│   ├── samsung_ac_device_custClim.h/cpp # Custom climate entity mapping
│   ├── protocol.h/cpp         # Protocol base class, enums, auto-detection logic
│   ├── protocol_nasa.h/cpp    # NASA protocol implementation (~1200 lines, largest file)
│   ├── protocol_non_nasa.h/cpp # Non-NASA protocol implementation (~680 lines)
│   ├── conversions.h/cpp      # Mode/fan/swing/preset <-> ESPHome climate mappings
│   ├── util.h/cpp             # Hex encoding, address parsing utilities
│   ├── debug_mqtt.h/cpp       # Optional MQTT debug forwarding
│   └── debug_number.h/cpp     # Debug number entity
├── test/                      # Unit tests
│   ├── test.sh                # Master test runner (runs both protocol tests)
│   ├── test_nasa.sh           # NASA protocol tests
│   ├── test_non_nasa.sh       # Non-NASA protocol tests
│   ├── build_and_run.sh       # Compile and execute a test binary
│   ├── main_test_nasa.cpp     # NASA test cases
│   ├── main_test_non_nasa.cpp # Non-NASA test cases
│   ├── main_serial.cpp        # Serial communication test
│   ├── main_readfile.cpp      # File reading test
│   └── esphome/               # Mock ESPHome headers for test compilation
├── .github/
│   ├── workflows/build.yml    # CI: Arduino build, ESP-IDF build, unit tests
│   ├── conf_arduino.yml       # ESPHome config for Arduino framework CI test
│   └── conf_esp_idf.yml       # ESPHome config for ESP-IDF framework CI test
├── example.yaml               # Full example ESPHome configuration
├── readme.md                  # User-facing documentation
└── nasa.md                    # NASA protocol notes
```

## Architecture

### Class Hierarchy

```
Samsung_AC (samsung_ac.h)
├── Inherits: PollingComponent, UARTDevice, MessageTarget
├── Owns: map<string, Samsung_AC_Device*> devices_
├── Manages: UART read/write, message routing, send queue
└── Lifecycle: setup() → loop() → update()

Samsung_AC_Device (samsung_ac_device.h)
├── Represents one indoor/outdoor unit
├── Owns ESPHome entities:
│   ├── Samsung_AC_Climate   (climate::Climate)
│   ├── Samsung_AC_Switch    (switch_::Switch)
│   ├── Samsung_AC_Mode_Select (select::Select)
│   └── Samsung_AC_Number    (number::Number)
├── Owns: vector<Samsung_AC_CustClim*> custom_climates
└── Delegates protocol work to Protocol* protocol

Protocol (protocol.h) — abstract base
├── NasaProtocol (protocol_nasa.h)
└── NonNasaProtocol (protocol_non_nasa.h)
```

### Data Flow

**Incoming (AC → ESPHome):**
```
UART RX → Samsung_AC::loop()
  → process_data() [protocol auto-detection]
  → try_decode_nasa_packet() or try_decode_non_nasa_packet()
  → process_messageset() / process_non_nasa_packet()
  → Samsung_AC_Device::update_*() methods
  → ESPHome entity publish (climate/sensor/switch state)
```

**Outgoing (ESPHome → AC):**
```
User action on entity (Climate/Switch/Select/Number)
  → Samsung_AC_Device::publish_request(ProtocolRequest)
  → Protocol::publish_request()
  → Build NASA or Non-NASA packet
  → Enqueue in Samsung_AC::send_queue_
  → UART TX in loop()
```

### MessageTarget Interface

`Samsung_AC` implements `MessageTarget`, which protocol implementations call back into for device lookups and data publishing. This decouples protocol logic from device management.

## Key Files Reference

| File | Purpose |
|------|---------|
| `__init__.py` | ESPHome config schema (CONFIG_SCHEMA, DEVICE_SCHEMA) and `to_code()` code generation |
| `protocol_nasa.h/cpp` | NASA packet encode/decode, message numbers, address types |
| `protocol_non_nasa.h/cpp` | Non-NASA packet encode/decode, command types (Cmd20, CmdC0, etc.) |
| `samsung_ac_device.h` | Device class + all ESPHome entity class definitions |
| `conversions.cpp` | Bidirectional mapping between internal enums and ESPHome climate enums |
| `protocol.h` | Shared enums (Mode, FanMode, SwingMode), ProtocolRequest struct, Protocol base |
| `example.yaml` | Reference configuration showing all supported options |

## Build & Test

### Running Unit Tests Locally

```bash
# Run all tests (NASA + Non-NASA)
cd test && bash test.sh

# Run only NASA protocol tests
cd test && bash test_nasa.sh

# Run only Non-NASA protocol tests
cd test && bash test_non_nasa.sh
```

Tests compile C++ directly with `g++` using mock ESPHome headers from `test/esphome/`. No ESP hardware or ESPHome installation required.

### CI Pipeline

GitHub Actions (`.github/workflows/build.yml`) runs three jobs on every push:

1. **Arduino framework build** — compiles with `esphome/build-action@v1.5.2` using `.github/conf_arduino.yml`
2. **ESP-IDF framework build** — same action with `.github/conf_esp_idf.yml`
3. **Unit tests** — installs `build-essential`, runs `test/test.sh`

### Building with ESPHome

Users include this as an external component in their ESPHome YAML:

```yaml
external_components:
  - source: github://lanwin/esphome_samsung_ac@main
    components: [samsung_ac]
```

## Development Conventions

### C++

- **Standard:** GNU++14
- **Style:** Follow existing code patterns — no `.clang-format` or `.editorconfig`
- **Naming:** `snake_case` for functions/variables, `PascalCase` for classes/enums, `UPPER_CASE` for constants
- **Namespace:** All component code is in `esphome::samsung_ac`
- **Headers:** Use `#pragma once`

### Python (`__init__.py`)

- **Formatter:** autopep8
- **Pattern:** Standard ESPHome component structure:
  - `CONFIG_SCHEMA` defines the YAML validation schema using `cv.` helpers
  - `async def to_code(config)` generates C++ from validated config
  - `cg.` (code generation) helpers create C++ variable declarations and method calls

### Addresses

- **NASA:** `"XX.XX.XX"` (class.channel.address), e.g., `"20.00.00"` = indoor unit 0
- **Non-NASA:** `"XX"` (single hex byte), e.g., `"00"` = first indoor unit, `"c8"` = outdoor unit
- Address format determines which protocol handler is used

### Temperature Values

- **NASA protocol:** Stored as integer × 10 (e.g., 28.5°C = 285)
- **Non-NASA protocol:** Integer degrees
- Conversions happen in `conversions.cpp` and custom climate filters

### Presets

Defined in `__init__.py` with numeric protocol values:

| Preset | Value |
|--------|-------|
| sleep | 1 |
| quiet | 2 |
| fast | 3 |
| longreach | 6 |
| windfree | 9 |

## Protocol Quick Reference

### NASA Protocol

**Packet structure:**
```
[0x32] [size:2] [src:3] [dst:3] [cmd:3] [msg_count:1] [messages...] [crc16:2] [0x34]
```

- Start byte: `0x32`, end byte: `0x34`
- CRC16-CCITT (polynomial 0x1021)
- Size range: 16–1500 bytes
- Optional `0x55` preamble (up to 100 bytes, skipped)

**Key message numbers (hex):**

| Message | ID | Type |
|---------|----|------|
| Power on/off | `0x4000` | Enum |
| Operation mode | `0x4001` | Enum |
| Fan mode | `0x4006` | Enum |
| Preset (alt mode) | `0x4060` | Enum |
| Vertical swing | `0x4011` | Enum |
| Horizontal swing | `0x407E` | Enum |
| Target temp | `0x4201` | Variable (×10) |
| Room temp | `0x4203` | Variable (×10) |
| Outdoor temp | `0x8204` | Variable (×10) |

**Address classes:** `0x20` = Indoor, `0x10` = Outdoor, `0x11` = HTU, `0x38` = MCU, `0x40` = RMC

**Modes:** Auto=0, Cool=1, Dry=2, Fan=3, Heat=4

**Fan speeds:** Auto=0, Low=1, Mid=2, High=3, Turbo=4

### Non-NASA Protocol

**Command types:**
- `Cmd20` — Indoor unit status (temp, fan, mode, power, wind direction)
- `CmdC0` — Outdoor unit status (4-way valve, compressor, temperatures)
- `Cmd54` — Control message ACK
- `CmdF0` — Outdoor diagnostics (error codes, inverter frequency)
- `CmdF1` — Electronic expansion valve positions
- `CmdF3` — Inverter data (frequency, capacity, current, voltage, power)

**Modes:** Heat=0x01, Cool=0x02, Dry=0x04, Fan=0x08, Auto=0x22

**Fan speeds:** Auto=0, Low=2, Medium=4, High=5

## Common Tasks

### Adding a New NASA Message Number

1. Add the entry to `enum MessageNumber` in `protocol_nasa.h`
2. Handle the message in `process_messageset()` in `protocol_nasa.cpp`
3. If it maps to a device property, add update logic in `Samsung_AC_Device`
4. If it needs an ESPHome entity, wire it through `__init__.py` schema + `to_code()`
5. Add a test case in `test/main_test_nasa.cpp`

### Adding a New Sensor

1. Define the sensor in `DEVICE_SCHEMA` in `__init__.py` (follow existing patterns like `room_humidity`)
2. Add `to_code()` logic to register the sensor with the device
3. Add the C++ member and update method in `samsung_ac_device.h/cpp`
4. Wire the protocol message to the new sensor in the relevant protocol file

### Adding a New Preset

1. Add the preset to the `PRESETS` dict in `__init__.py` with its protocol value
2. Add the mapping in `conversions.cpp` (both directions: preset ↔ climate preset)
3. Update `protocol_nasa.cpp` if needed for the preset's alt mode value

## Code Owners

- **matthias882** — Original ESPHome component
- **lanwin** — Core protocol and component development
- **Lokheizer** — Current fork maintainer
