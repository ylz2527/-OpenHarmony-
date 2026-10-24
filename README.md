# -OpenHarmony-

基于 Hi3861（OpenHarmony LiteOS-M 轻量系统）的综合应用实验代码。

## 目录结构

| 目录 | 内容 |
|------|------|
| `experiment1/` | 综合应用实验一：GPIO/外设基础（`Experiment_Stage1.c` + `BUILD.gn`） |
| `experiment2/` | 综合应用实验二：传感器驱动（SHT20 温湿度 / AP3216C 光照 / MPU6050 / SSD1306 OLED / NFC） |
| `experiment3/` | 综合应用实验三：在实验二基础上增加 Wi-Fi 连接与 MQTT 上云（`oc_mqtt`） |

每个目录为一个完整的 OpenHarmony 组件：

```
experimentN/
├── BUILD.gn              # 组件构建脚本（static_library）
├── Experiment_StageN.c   # 实验主程序入口
├── include/              # 外设驱动头文件
├── src/                  # 外设驱动源文件
└── nfc/                  # NFC/NDEF 协议实现
```

- `experiment1/app BUILD` 为应用层 `app/BUILD.gn` 的备份，编译时需将其内容放入
  `applications/sample/wifi-iot/app/BUILD.gn`，并把工程目录放到
  `applications/sample/wifi-iot/app/9.0_Experiment_Stage1` 下参与整体编译。
- `experiment1/实验结果.mp4` 为实验一的运行演示视频。
- 历史压缩包（`综合应用实验*.zip`）为早期上传版本，内容与对应目录一致。
