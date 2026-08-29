# L298N Motor Driver Kernel Module (Raspberry Pi 5)

A Linux character-device kernel module for controlling an L298N dual H-bridge
motor driver from a Raspberry Pi 5, using the modern `gpiod` descriptor-based
GPIO API. Motor commands are exposed through `/dev/l298n` and can be driven
either directly from the shell or remotely via an MQTT bridge.

## Hardware

- Raspberry Pi 5 (RP1 southbridge, `gpiochip` label `pinctrl-rp1`)
- L298N dual H-bridge motor driver module
- Two DC motors

### GPIO Pin Mapping

| Signal | BCM GPIO | Purpose |
|---|---|---|
| ENA | 16 | Left motor enable |
| IN1 | 17 | Left motor direction |
| IN2 | 27 | Left motor direction |
| IN3 | 22 | Right motor direction |
| IN4 | 23 | Right motor direction |
| ENB | 13 | Right motor enable |

> **Note:** ENA was originally wired to GPIO12. That pin's output stage
> failed (it could not be pulled low in software and floated when touched),
> so it was reassigned to GPIO16. If you rebuild this project on different
> hardware, verify each pin is a clean, unused GPIO before wiring
> (`pinctrl get <pin>` should report `op` with the expected level, not an
> alternate function).

## Files

| File | Description |
|---|---|
| `l298n_driver.c` | Kernel module source. Registers a character device (`/dev/l298n`) and maps text commands to GPIO states via a manually registered `gpiod_lookup_table`. |
| `Makefile` | Standard out-of-tree kernel module build file. |
| `mqtt_bridge.py` | Subscribes to an MQTT topic and forwards commands to `/dev/l298n`. Includes a command whitelist, reconnect backoff, and safe shutdown (sends `STOP` on exit). |

## Supported Commands

Commands are plain ASCII strings written to `/dev/l298n`:

| Command | Effect |
|---|---|
| `COME` | Drive both motors forward |
| `STOP` | Stop both motors (all outputs low) |
| `OPEN_LID` | Stop both motors (reserved for lid-open event handling) |

Any other string is logged as an unknown command and ignored.

## Building the Kernel Module

Kernel headers matching your running kernel must be installed first:

```bash
sudo apt update
sudo apt install raspberrypi-kernel-headers
```

Then build:

```bash
make
```

This produces `l298n_driver.ko`.

## Loading / Unloading

```bash
sudo insmod l298n_driver.ko
sudo rmmod l298n_driver
```

Check kernel logs for status messages:

```bash
dmesg | tail -20
```

On load, the module:
1. Registers a character device and creates `/dev/l298n`.
2. Registers a virtual `platform_device` purely as an anchor for `gpiod_get()`.
3. Registers a `gpiod_lookup_table` mapping `ena`/`in1`/`in2`/`in3`/`in4`/`enb`
   to the GPIO offsets above on the `pinctrl-rp1` chip (this stands in for a
   device tree overlay, since no DT node is used).
4. Requests each GPIO as an output, initialized low.

On unload, all outputs are set low before the GPIOs and lookup table are
released.

## Sending Commands Manually

```bash
echo -n "COME" > /dev/l298n
echo -n "STOP" > /dev/l298n
```

## MQTT Bridge

`mqtt_bridge.py` listens on topic `bingo/command` (default broker:
`127.0.0.1:1883`, i.e. a broker running locally on the Pi) and forwards
whitelisted commands to `/dev/l298n`.

### Requirements

```bash
pip install paho-mqtt
```

An MQTT broker (e.g. Mosquitto) must be running on the Pi:

```bash
sudo apt install mosquitto mosquitto-clients
```

### Running

```bash
python3 mqtt_bridge.py
```

### Publishing a test command

```bash
mosquitto_pub -h 127.0.0.1 -t bingo/command -m "COME"
```

### Safety behavior

- Only `COME`, `STOP`, and `OPEN_LID` are forwarded to the driver; anything
  else is logged and dropped.
- On `SIGINT`/`SIGTERM` (e.g. `Ctrl+C`, `systemctl stop`), the bridge sends
  `STOP` to the driver before exiting, so the motors don't stay in whatever
  state they were last commanded to.
- Reconnects to the broker with exponential backoff (1s–30s) instead of
  retrying immediately in a tight loop.

## Debugging Tips

- `sudo cat /sys/kernel/debug/gpio` — shows live output level for every
  requested GPIO line.
- `pinctrl get <pin>` — shows the pin's current function (`op`/`ip`/alt
  function) and level; useful for confirming a pin hasn't been muxed away
  from plain GPIO by an overlay or is not stuck due to a hardware fault.
- `gpioinfo` — lists all GPIO lines and their current consumer, useful for
  checking a pin isn't already claimed by another driver before wiring it up.

## License

GPL (see `MODULE_LICENSE("GPL")` in `l298n_driver.c`).