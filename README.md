# Block Breaker

UIAPduino、SSD1306 OLED、ロータリーエンコーダー、振動モジュールを使う
ブロック崩しです。現在は3部品の統合動作まで実機確認済みで、次にゲーム本体を実装します。

設計は[仕様](SPEC.md)、進捗と未決定事項は[TODO](TODO.md)を参照してください。

## 配線

| 部品 | 端子 | UIAPduino |
|---|---|---|
| OLED | VDD / GND | 3.3V / GND |
| OLED | SDA / SCL | D3 / PC1、D4 / PC2 |
| ロータリー | S1 / S2 / KEY | D7 / PC5、D10 / PD0、D5 / PC3 |
| ロータリー | 電源 | 5V / GND |
| 振動 | IN | D6 / PC4 |
| 振動 | 電源 | 5V / GND |

配線を変更する前にUSBケーブルを外してください。

## ビルドと書き込み

`start-uiap.command`を先に実行しなくても、このフォルダから直接実行できます。

```text
cd /Users/vestige/Spike/uip/uiap-devkit-macarm64/workspace/exercises/block_breaker
make
make size
make flash
```

Makefileは現在位置からDevkitルート、macOS用ランタイム、RISC-Vツールチェーンを
自動検出します。このフォルダをDevkit外へ単独で移動した場合は動作しません。

発熱、異臭、停止しない振動、USB切断があれば直ちにUSBを外してください。
