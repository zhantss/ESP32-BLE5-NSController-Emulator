# ESP32-BLE5-NSController-Emulator

## Project Overview

This project is an open-source implementation that emulates a **Nintendo Switch Pro2 controller** on **ESP32 series devices** and connects to **Nintendo Switch2 (NS2)**. By analyzing public BLE communication data between an official controller and NS2 console, this project implements a compatible protocol stack that allows ESP32 to emulate controller behavior.All protocol implementations are based on independently observed and documented BLE traffic from legally obtained consumer hardware. No proprietary SDK, leaked documents, or encrypted keys were used.

The project adopts a **modular design**, supporting custom Transport Layer and Protocol Layer, making it easy to connect different host devices (such as PC, mobile phones, game consoles, etc.) to control the ESP32 microcontroller. The Transport Layer handles byte stream transmission and reception, while the Protocol Layer parses and encapsulates application-layer data. Decoupling these two layers allows the project to flexibly adapt to various input sources and communication methods.

## Features

- **Complete Nintendo Switch Pro2 controller emulation**: Supports all buttons and joystick operations
- **NS2 BLE protocol stack**: Emulates the observable behavior of NS2 controller communication, including pairing, authentication, and data formatting
- **Modular architecture**:
  - **Transport Layer**: Supports UART, USB Serial/JTAG, BLE HID, and other physical transmission methods
  - **Protocol Layer**: Supports custom binary protocols, currently compatible with some features of the [EasyCon](https://github.com/EasyConNS/EasyCon) controller
- **TODO Configurable controller types**: Uses NVS storage for configuration, supports dynamic switching between different controller types
- **Debug-friendly**: Provides detailed log output and remote debugging interfaces

## Hardware Support

### Tested ESP32 Models
- **ESP32-C61** ✅ Fully tested, stable connection
- **ESP32-C6**  ✅ Basic tests passed, stable connection
- **ESP32-S3**  ❌ Slow progress due to closed-source NimBLE stack
- **ESP32-C3**  ❌ Slow progress due to closed-source NimBLE stack

### Bluetooth Stack Requirements
This project is developed based on the **ESP-IDF** framework. Theoretically, any ESP-IDF firmware using the Apache NimBLE open-source stack can be supported. However, since NS2's controller communication protocol exceeds the BLE specification for minimum connection interval (standard minimum is 7.5ms, NS2 requires 5ms), modifications to the NimBLE stack are required.

## Quick Start

### Environment Requirements
- **ESP-IDF version**: Must use **v5.5.2+**
- **Test environment**: Currently released firmware tested with **v5.5.3**
- **Python dependencies**: ESP-IDF standard toolchain

### Get the Code
```bash
git clone https://github.com/your-repo/ESP32-BLE5-NSController-Emulator.git
cd ESP32-BLE5-NSController-Emulator
```

### Apply Patch (Critical Step)
Since NS2 protocol requires 5ms connection interval while standard BLE specification only allows a minimum of 7.5ms, we need to modify the underlying parameters of the NimBLE stack:

1. **Backup original file**:
   ```bash
   cp $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c61/libble_app.a $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c61/libble_app.a.backup
   cp $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c6/libble_app.a $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c6/libble_app.a.backup
   ```

2. **Apply patch**:
   ```bash
   # Ensure the IDF environment is configured
   cd patch
   python patch_nimble_lib.py --target esp32c6
   # or
   python patch_nimble_lib.py --target esp32c61
   ```
   *Note: The patch script currently only supports ESP32-C6/C61. Other chips require manual adaptation or future official support.*  

### Build and Flash
```bash
idf.py set-target esp32c61  # Select target based on your hardware
idf.py build
idf.py -p PORT flash monitor
```

### Configuration and Pairing
1. After first boot, the device will enter BLE advertising mode
2. Enter controller pairing mode on the NS2 console
3. Pairing will complete automatically, showing controller icon in the interface (button colors differ from default Pro2 as an Easter egg)
4. After successful pairing, subsequent device boots will automatically enter wake-up advertising mode, waking sleeping NS2 and reconnecting automatically

## Development Guide

### Project Structure
```
├── main/
│   ├── src/              # Main program source files
│   │   ├── controller/   # Controller implementation
│   │   ├── protocol/     # Protocol layer implementation
│   │   ├── transport/    # Transport layer implementation
│   │   └── buffer/       # Zero-copy buffers
│   └── include/          # Header files
└── patch/                # Stack patch files
    └── patch_nimble_lib.py   # NimBLE Protocol Stack Patch Script
```

### Adding New Transport Layer
1. Create new header file in `main/include/transport/`
2. Implement all functions in the `transport_ops_t` interface
3. Register the new transport layer in `transport.c`
4. Enable the new transport layer via Kconfig configuration

### Adding New Protocol Layer
1. Create new protocol header file in `main/include/protocol/`
2. Implement all functions in the `protocol_ops_t` interface
3. Register the new protocol handler in `protocol_router.c`
4. Configure protocol routing rules

### Debugging and Logging
Enable detailed logging:
```bash
idf.py menuconfig
# Go to Component config -> MCU Debug -> Enable MCU debug logs
```

## Patch Information

### Patch Background
The Nintendo Switch 2 requires a BLE connection interval of 5ms, which falls below the minimum standard limit of 7.5ms specified in official BLE specifications. This project is exclusively for academic research and educational use, investigating the performance boundaries of BLE 5.0 and interoperability with devices that use non-standard connection parameters. To achieve this connection interval on ESP32-C6/C61 chips, fixed constant parameters within the NimBLE BLE stack of the ESP-IDF framework must be modified.

### How the Patch Works (Apache 2.0 Compliant)
- This project does **not** distribute any modified precompiled binaries or proprietary closed-source code. It only provides a tool that operates on your local official ESP-IDF development framework. The patch workflow is as follows:
Legitimate Source Analysis: We studied the underlying implementation of BLE connection parameters using the Apache Mynewt NimBLE source code released under Apache 2.0, alongside the corresponding precompiled binaries shipped with ESP-IDF.
- Independent Open-Source Development: Based on the analysis above, we developed a standalone open-source tool (MIT licensed) that safely edits specific bytes in local binary files to unlock permission for 5ms BLE connection intervals. This tool contains no content from the original libble_app.a library. Note that this does **not** enable actual operation at a true 5ms interval.
- Local-Only Modification: When executed, the tool automatically backs up the original library file and applies parameter adjustments solely on your local device.

### Current Status
- **ESP32-C61** ✅ Patch tested successfully, stable connection
- **ESP32-C6**  ✅ Patch tested successfully, stable connection
- **Other models** ⚠️ Theoretically all models using Apache NimBLE open-source stack are supported, but require compiling corresponding patch files for different models

## Known Issues

## Reconnection Issue
The firmware supports automatic reconnection. However, when connecting the controller on the Change Grip/Order interface of NS2, HID reports become stuck. The root cause is still under investigation.  
It is not recommended to connect the controller via this interface afterwards. Only enter this interface for initial pairing. After subsequent power-on, the MCU will broadcast automatically and reconnect seamlessly.  

### ESP32-S3 Support Issues
Due to ESP32-S3 using a closed-source Bluetooth stack, we cannot directly modify connection parameters. I've submitted related ISSUEs to Espressif, requesting open interfaces or technical support. Current progress is slow and requires community push.

### Compatibility Issues
1. **NS2 System Updates**: Nintendo may change protocols through system updates, causing existing implementations to fail
2. **Regional Version Differences**: Different regional NS2 consoles may have subtle differences
3. **Multiple Controllers Simultaneous Connection**: Not yet tested scenarios with multiple emulated controllers connected simultaneously

### Performance Limitations
1. **Data Throughput**: BLE 5.0 throughput may not satisfy all gaming scenarios
2. **Latency Fluctuation**: Latency fluctuations may occur in wireless interference environments
3. **Power Consumption Balance**: Low connection intervals increase power consumption, requiring fine power management

## Contribution Guidelines

We welcome contributions in various forms:

### Reporting Issues
1. Submit detailed issue reports in GitHub Issues
2. Include hardware model, ESP-IDF version, reproduction steps, etc.
3. Provide log files and packet capture data if possible

### Submitting Code
1. Fork this repository and create a feature branch
2. Follow existing code style and architecture design
3. Add necessary tests and documentation
4. Submit Pull Request with description of changes

### Testing Assistance
1. Test existing functionality on different hardware models
2. Verify effectiveness of patch files
3. Test compatibility with different NS2 console versions

### Documentation Improvements
1. Improve usage documentation and development guides
2. Add more example code and configuration instructions
3. Translate documentation to other languages

## License

This project uses the **MIT License**. See the [LICENSE](LICENSE) file for details.

## Legal and Compliance Statement
This project is an independent, open-source software (OSS) endeavor. It is **NOT** affiliated with, endorsed, or sponsored by Espressif Systems, Nintendo Co., Ltd., or the Apache Software Foundation.

* **Compliance with Apache 2.0**: Our modification tool (located in `/patch/`) is an independent work. It only modifies the user's local copy of the `libble_app.a` binary, which is a component of ESP-IDF. We have taken care to ensure this process does not violate the terms of the Apache License 2.0, under which `libble_app.a` is distributed [3†L25-L27].
* **Fair Use for Interoperability**: The primary purpose of this project is to achieve software interoperability for educational and research purposes. Modifying local software to achieve interoperability with a lawfully acquired device (Nintendo Switch 2) is a legitimate use of one's own property.
* **Non-Infringement**: This project does not contain, and its patch tool does not generate, any code that is directly copied from Nintendo's proprietary software. All communication protocols are independently re-implemented based on publicly observable data.

### Disclaimer
This project is for learning and research purposes only. Using this project may violate Nintendo's Terms of Service. Please ensure you use it within legal boundaries. The author is not responsible for any legal issues arising from the use of this project.

## Acknowledgements

- Thanks to [ndeadly/switch2_controller_research](https://github.com/ndeadly/switch2_controller_research) project, which provided extensive protocol analysis foundational work.  
- Thanks to [EasyConNS/EasyCon](https://github.com/EasyConNS/EasyCon) project, which provides out-of-the-box automation tools

## Changelog

### v0.1.0 (2026-04)
- Initial version release
- Support ESP32-C61 as NS2 Pro2 controller emulation
- Implement basic transport and protocol layer framework

### v0.1.1 (2026-04)
- Reduced the advertising restart interval after disconnection
- Optimized startup logic of HID report tasks
---

**Note**: This project is in early development stage. APIs and functionality may undergo significant changes. Please check changelog and documentation regularly.