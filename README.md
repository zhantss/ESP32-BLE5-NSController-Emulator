# ESP32-BLE5-NSController-Emulator

[English](README.en.md)

## 项目概述

本项目是一个在 **ESP32 系列设备** 上模拟 **Nintendo Switch Pro2 手柄**，并连接 **Nintendo Switch2 (NS2)** 的开源项目。通过分析官方控制器与NS2主机之间的公开BLE通信数据，本项目实现了一个兼容的协议栈，使ESP32能够模拟控制器行为。所有协议实现均基于对合法获得的消费级硬件上BLE通信流量的独立观察与记录。未使用任何专有SDK、泄露文件或加密密钥。

项目采用 **模块化设计**，支持自定义传输层（Transport Layer）和协议层（Protocol Layer），便于连接不同的上位机（如 PC、手机、游戏主机等）来控制 ESP32 单片机。传输层负责字节流的收发，协议层负责解析和封装应用层数据，二者解耦使得项目可以灵活适配各种输入源和通信方式。

## 功能特性

- **完整模拟 Nintendo Switch Pro2 手柄**：支持所有按钮、摇杆操作
- **NS2 BLE 协议栈实现**：模拟 NS2 手柄通信的可观测行为，包含配对、身份验证以及数据格式处理。  
- **模块化架构**：
  - **传输层**：支持 UART、USB Serial/JTAG、BLE HID 等多种物理传输方式
  - **协议层**：支持 自定义二进制协议，当前已兼容部分手柄功能 [EasyCon](https://github.com/EasyConNS/EasyCon)
- **TODO 可配置的控制器类型**：通过 NVS 存储配置，支持动态切换不同手柄类型
- **调试友好**：提供详细的日志输出和远程调试接口

## 硬件支持

### 已测试的 ESP32 型号
- **ESP32-C61** ✅ 已通过完整测试，连接稳定
- **ESP32-C6**  ✅ 已通过基本测试，连接稳定
- **ESP32-S3**  ⚠️ 由乐鑫官方在主分支上提供了补丁文件，已通过基本测试，连接稳定

### 蓝牙协议栈要求
本项目基于 **ESP-IDF** 框架开发。理论上，任何使用 Apache NimBLE 开源堆栈的 ESP-IDF 固件都可以支持。但由于 NS2 控制器通讯协议突破了 BLE 最低连接间隔的规范（标准最低为 7.5ms，NS2 要求 5ms），需要对 NimBLE 协议栈进行修改。

## 快速开始

### 环境要求
- **ESP-IDF 版本**：必须使用 **v5.5.2+** 版本
- **测试环境**：当前发布的固件使用 **v5.5.3** 测试通过
- **Python 依赖**：ESP-IDF 标准工具链

### 获取代码
```bash
git clone https://github.com/your-repo/ESP32-BLE5-NSController-Emulator.git
cd ESP32-BLE5-NSController-Emulator
```

### 应用补丁（关键步骤）
由于 NS2 协议要求 5ms 的连接间隔，而标准 BLE 规范最低只允许 7.5ms，因此需要修改 NimBLE 协议栈的底层参数：

1. **备份原始文件**：
   ```bash
   cp $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c61/libble_app.a $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c61/libble_app.a.backup
   cp $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c6/libble_app.a $IDF_PATH/components/bt/controller/lib_esp32c6/esp32c6-bt-lib/esp32c6/libble_app.a.backup
   ```

2. **应用补丁**：
   ```bash
   # idf 环境必须已配置
   cd patch
   python patch_nimble_lib.py --target esp32c6
   # or
   python patch_nimble_lib.py --target esp32c61
   ```
   *注意：目前patch脚本仅提供 ESP32-C6/C61 支持，其他型号需要自行尝试或等待后续支持*  

### 编译和烧录
```bash
idf.py set-target esp32c61  # 根据你的硬件选择目标
idf.py build
idf.py -p PORT flash monitor
```

### 配置和配对
1. 首次启动后，设备将进入 BLE 广播模式
2. 在 NS2 主机上进入手柄配对模式
3. 会自动完成配对，界面中出现手柄图标，其中按键颜色与默认Pro2有区别(彩蛋)
4. 配对成功后，之后设备启动会自动进入唤醒广播模式，会将休眠中的NS2唤醒并自动重连

## 开发指南

### 项目结构
```
├── main/
│   ├── src/              # 主程序源文件
│   │   ├── controller/   # 控制器实现
│   │   ├── protocol/     # 协议层实现
│   │   ├── transport/    # 传输层实现
│   │   └── buffer/       # 零拷贝缓冲区
│   └── include/          # 头文件
└── patch/                # 协议栈补丁文件
    └── patch_nimble_lib.py   # NimBLE 协议栈补丁脚本
```

### 添加新的传输层
1. 在 `main/include/transport/` 下创建新的头文件
2. 实现 `transport_ops_t` 接口中的所有函数
3. 在 `transport.c` 中注册新的传输层
4. 通过 Kconfig 配置启用新的传输层

### 添加新的协议层
1. 在 `main/include/protocol/` 下创建新的协议头文件
2. 实现 `protocol_ops_t` 接口中的所有函数
3. 在 `protocol_router.c` 中注册新的协议处理器
4. 配置协议路由规则

### 调试和日志
启用详细日志：
```bash
idf.py menuconfig
# 进入 Component config -> MCU Debug -> Enable MCU debug logs
```

## 补丁说明

### 补丁来源
NS2要求蓝牙连接间隔为 5ms，该数值低于BLE标准规范规定的 7.5ms 最低限值。本项目仅用于科研与教学研究，探究蓝牙 5.0 性能极限以及与非标设备的互通兼容性,以及与采用非标准连接参数设备之间的互兼容性。若要在 ESP32-C6/C61 芯片平台实现该连接间隔，需修改 ESP-IDF 框架中 NimBLE 蓝牙协议栈的固定参数常量。

### 补丁工作原理（符合 Apache 2.0 开源协议）
本项目**不**分发任何修改后的预编译二进制文件及私有闭源代码，仅提供一款作用于你本地官方 ESP-IDF 开发框架的工具。补丁运行逻辑如下：

- 合法源码分析：我们基于 Apache 2.0 协议开源的Apache Mynewt NimBLE源码，研究蓝牙连接参数底层实现逻辑，同时分析 ESP-IDF 内置分发的对应编译二进制文件。
- 独立开源开发：基于上述分析，自研独立开源工具（MIT 开源协议），可安全修改本地二进制文件对应字节，实现 5ms 蓝牙连接间隔连接指示许可。该工具不包含任何原版libble_app.a库文件内容。请注意，这**并非**真的使用5ms间隔工作。
- 本地单机生效：运行工具后，程序会自动备份原始库文件，并仅在你的本地设备完成参数修改。

### ⚠️ 用户责任须知
请确保本工具使用行为严格遵守 Apache 2.0 开源协议与乐鑫官方使用条款。本项目仅限教学研究用途，开发者不对因使用本工具产生的一切后果承担相关责任。

### 当前状态
- **ESP32-C61** ✅ 补丁已测试成功，连接稳定
- **ESP32-C6**  ✅ 补丁已测试成功，连接稳定
- **ESP32-S3**  ⚠️ 由乐鑫官方在主分支上提供了补丁文件，已通过基本测试，连接稳定
- **其他型号**  ⚠️ 请等待官方稳定版本，如使用Apache NimBLE开源堆栈的版本可以使用补丁脚本尝试。其他堆栈请查看下方ESP32-S3支持问题。

## 已知问题

### 重连问题
固件会自动重连，但目前在NS2中开启“更改握法/顺序”界面连上后HID报告会出现阻塞，暂未查明原因。  
不建议使用该界面功能连接手柄，只需要在第一次配对时进入此界面，后续单片机上电后会自动广播并重连。  

### ESP32-S3 支持问题
~~由于 ESP32-S3 使用了闭源的蓝牙协议栈，目前无法直接修改连接参数。我已经向乐鑫提交了相关 ISSUE，请求开放相关接口或提供技术支持。当前进展缓慢，需要社区共同推动。~~  
现已通过esp-idf主分支以及乐鑫官方人员提供的补丁文件支持，仓库已发布S3固件，如需自行开发请等待乐鑫将功能合入稳定版本或查看ISSUE下载补丁文件自行尝试。感谢乐鑫官方人员以及社区贡献者们的支持。  
ISSUE Link:  
[ESP32-S3 Bluedroid Controller Support for 5ms Connection Interval](https://github.com/espressif/esp-idf/issues/18467)  

### 兼容性问题
1. **NS2 系统更新**：任天堂可能通过系统更新更改协议，导致现有实现失效
2. **不同地区版本**：不同地区的 NS2 主机可能存在细微差异
3. **多手柄同时连接**：尚未测试多个模拟手柄同时连接的场景

### 性能限制
1. **数据吞吐量**：BLE 5.0 的吞吐量可能无法满足所有游戏场景
2. **延迟波动**：在无线干扰环境下可能出现延迟波动
3. **功耗平衡**：低连接间隔会增加功耗，需要精细的电源管理

## 贡献指南

我们欢迎各种形式的贡献：

### 报告问题
1. 在 GitHub Issues 中提交详细的问题报告
2. 包括硬件型号、ESP-IDF 版本、复现步骤等信息
3. 如果可能，提供日志文件和抓包数据

### 提交代码
1. Fork 本仓库并创建特性分支
2. 遵循现有的代码风格和架构设计
3. 添加必要的测试和文档
4. 提交 Pull Request 并描述变更内容

### 测试帮助
1. 在不同硬件型号上测试现有功能
2. 验证补丁文件的有效性
3. 测试与不同 NS2 主机版本的兼容性

### 文档改进
1. 完善使用文档和开发指南
2. 添加更多示例代码和配置说明
3. 翻译文档到其他语言

## 许可证

本项目采用 **MIT 许可证**。详细信息请查看 [LICENSE](LICENSE) 文件。

## 法律与合规声明
本项目为独立开源软件项目，不隶属于、不被乐鑫科技、任天堂株式会社、Apache 软件基金会认可或赞助。  
* **遵守 Apache 2.0 协议**：位于/patch/目录下的修改工具为独立创作成果，仅对用户本地 ESP-IDF 组件libble_app.a二进制文件进行修改。我方已严格确保该操作流程符合libble_app.a所遵循的 Apache 2.0 开源许可协议条款。
- **适配兼容合理使用**：本项目核心目的为出于教学、科研需求实现软件互联互通。对本地软件进行修改，以适配合法购入的任天堂 Switch 2 设备，属于对自身合法财产的正当使用范畴。
- **不构成侵权**：本项目及配套补丁工具不包含、也不会生成任何抄袭任天堂私有专有软件的代码。所有通信协议均依据公开可观测数据独立重构实现。

### 免责声明
本项目仅用于学习和研究目的。使用本项目可能违反任天堂的服务条款，请确保你在合法范围内使用。作者不对任何因使用本项目而产生的法律问题负责。

## 致谢

- 感谢 [esp-idf](https://github.com/espressif/esp-idf) 提供的技术支持
- 感谢 [ndeadly/switch2_controller_research](https://github.com/ndeadly/switch2_controller_research) 项目，提供了大量协议分析的基础工作。
- 感谢 [EasyConNS/EasyCon](https://github.com/EasyConNS/EasyCon) 项目，提供了开箱即用的自动化工具

## 更新日志

### v0.1.3 (2026-04)
- S3 初步支持
- 暂时防止NS2弹出固件更新提醒框
- 修复了部分bug

### v0.1.2 (2026-04)
- 修复了在部分芯片上通讯口有多余数据导致通讯失效的问题(增加丢包机制)

### v0.1.1 (2026-04)
- 减少了断开后重启广播的时间间隔
- 优化了HID报告任务的启动逻辑

### v0.1.0 (2026-04)
- 初始版本发布
- 支持 ESP32-C61 作为 NS2 Pro2 手柄模拟
- 实现基本的传输层和协议层框架

---

**注意**：本项目处于早期开发阶段，API 和功能可能发生重大变更。请定期查看更新日志和文档。