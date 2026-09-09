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

対応環境はApple Silicon搭載Macです。WindowsおよびIntel Macでは動作確認しておらず、
現在の同梱ツールも対象外です。

このリポジトリがUIAP Devkit内のどこかにあれば、Makefileが上位フォルダを探索して
Devkitを自動検出します。`workspace/exercises`へ固定する必要はありません。

```text
make
make size
make flash
```

Devkit外へcloneした場合は、`local.mk.example`を`local.mk`へコピーし、
`UIAP_DEVKIT_ROOT`へ自分のDevkitルートを設定します。`local.mk`はGitへ登録されません。

発熱、異臭、停止しない振動、USB切断があれば直ちにUSBを外してください。
