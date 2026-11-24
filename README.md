# p1metertomqtt

P1 meter to MQTT. Reads a [HomeWizard P1 meter](https://www.homewizard.com/p1-meter/) (HWE-P1, on the
electricity meter's P1/HAN port) over its local API every interval, and publishes the meter's data to an MQTT
broker as it comes, with the time it was read added.

One of a small family of single-binary *tomqtt daemons (gqgmctomqtt, rtsptomqtt, ...) that share a common
structure and config convention.

## What it does

Every `interval` seconds, on the clock (30: at :00 and :30), it GETs `p1meter-url` (the meter's
`/api/v1/data`) and publishes the JSON object to `mqtt-topic`, with `ts` (unix seconds, when it was read) added
as the first member. Not retained. A line is logged when the meter stops answering, and when it answers again;
with `debug=true`, a line for each publish.

The local API must be enabled on the meter (HomeWizard Energy app: Settings, Meters, the P1 meter, Local API).

## Build

Single C source (`p1metertomqtt.c`) with headers in `include/`; libmosquitto and libcurl.

    make install-dev        # the build dependencies (libmosquitto-dev, libcurl4-openssl-dev)
    make                    # native build
    make armhf              # cross-compile for 32-bit ARM (armhf)
    make format             # clang-format the source
    make test               # build and run against the local .cfg, with debug
    make clean

## Install

**Host-specific config convention:** the Makefile installs `p1metertomqtt.<hostname>.cfg` if it exists,
otherwise falls back to the generic `p1metertomqtt.cfg`.

    make install            # binary -> /usr/local/bin/p1metertomqtt
                            # config -> /etc/default/p1metertomqtt
                            # unit   -> p1metertomqtt.service (enabled, started)
    make restart

## Configuration

Every setting is a config-file `key=value` **and** an equivalent `--key value` command-line flag.
`--config <file>` selects the file (default `p1metertomqtt.cfg`).

| key | default | meaning |
|---|---|---|
| `mqtt-server` | `mqtt://localhost` | broker URL |
| `mqtt-client` | `p1metertomqtt` | client id (prefix) |
| `mqtt-topic` | `sensors/p1meter` | topic to publish to |
| `p1meter-url` | `http://p1meter.local/api/v1/data` | the meter's data endpoint (it announces itself as `p1meter-XXXXXX.local`) |
| `interval` | `30` | seconds between reads (the meter's kWh counter makes energy over any period exact, whatever the interval) |
| `debug` | `false` | a line for each publish |

## Output (MQTT)

```
$ mosquitto_sub -v -t 'sensors/p1meter'
sensors/p1meter {"ts":1795541310,"wifi_ssid":"...","wifi_strength":100,"meter_model":"...","unique_id":"...",
  "total_power_import_kwh":27568.395,"total_power_import_t1_kwh":27568.395,"total_power_export_kwh":0.164,
  "total_power_export_t1_kwh":0.164,"active_power_w":413,"active_power_l1_w":303,"active_power_l2_w":91,
  "active_power_l3_w":18,"active_voltage_l1_v":233.3,"active_voltage_l2_v":233.6,"active_voltage_l3_v":234.5,
  "active_current_a":2.9,"active_current_l1_a":1.5,"active_current_l2_a":0.9,"active_current_l3_a":0.5,"external":[]}
```

The fields are the meter's (they vary by meter and firmware: tariffs, gas, `external` devices, ...).

## License

CC BY-NC-SA 4.0 — see [LICENSE](LICENSE).
