# Block Breaker

UIAPduino、SSD1306 OLED、ロータリーエンコーダー、振動モジュールを使う
ブロック崩しです。3部品の統合動作を実機確認し、ゲーム本体まで実装済みです。
ゲーム全体の実機確認と操作感の調整はこれから行います。

設計は[仕様](SPEC.md)、進捗と未決定事項は[TODO](TODO.md)を参照してください。

## 必要な開発環境

このプロジェクトは、XP祭り2026ワークショップ向けの
[UIAP Devkit統合配布物](https://github.com/amapyon/xpfes2026)を使って作成しました。
ビルドと書き込みには、Devkitに含まれる`ch32fun`、RISC-Vツールチェーン、
`minichlink`が必要です。このリポジトリ単独ではビルド環境を同梱していません。

本リポジトリはUIAP Devkitを利用した独立作品であり、`xpfes2026`の公式演習や
公式サンプルではありません。また、元のDevkitはWindows版も提供していますが、
このプロジェクトが現在対応するのはApple Silicon搭載Macだけです。

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

最初に、上記のUIAP Devkitを取得してセットアップしてください。

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
