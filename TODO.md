# ブロック崩しTODO

設計上の決定は`SPEC.md`へ記録する。本ファイルには実機確認の結果、残る確認、
将来追加する可能性がある機能だけを記録する。

## 実装・実機確認済み

- [x] OLED、ロータリーエンコーダー、振動モジュールを同時接続して動作させる。
- [x] OLEDを3.3V、SDA=PC1、SCL=PC2、アドレス`0x3C`で表示する。
- [x] ロータリーをS1=PC5、S2=PD0、KEY=PC3で読み取る。
- [x] ロータリーでバーを動かし、KEY押下でボールを発射する。
- [x] 16列×3行のブロック、22×2のバー、3×3のボールを表示する。
- [x] 壁、バー、ブロックの衝突と反射、ブロック消去を処理する。
- [x] ブロック破壊時にレベル50で30ms振動する。
- [x] GAME OVERを表示し、レベル80で約1.5秒振動する。
- [x] 全ブロック破壊後にGAME CLEARと花火アニメーションを表示する。
- [x] OLEDの部分転送により、ロータリー操作中の入力停止時間を短縮する。
- [x] 約60度で発射し、移動中のバーに合わせて反射方向と角度を変える。
- [x] 移動中のバーによる反射が安定し、操作感に問題がないことを実機確認する。
- [x] [Issue #6: 右移動中のバーのすり抜けを通過範囲判定で修正する](https://github.com/vestige/UipBlockBreaker/issues/6)
- [x] [Issue #7: 右端の境界値変換によるボールのすり抜けを修正する](https://github.com/vestige/UipBlockBreaker/issues/7)
- [x] Devkit内外から`make`、`make size`、`make flash`を使用できる構成にする。
- [x] ゲーム本体を警告なしでビルドし、FLASHとRAMの使用量を確認する。
- [x] KEYを押し続けても、発射や再スタートが複数回発生しない。
- [x] GAME OVERとGAME CLEARの両方からKEYで正しく再スタートできる。
- [x] ボールがブロックを飛び越えず、バーと画面外へ抜けない。

## 残る実機確認

- [ ] [Issue #1: 10分以上の連続プレイで動作安定性を確認する](https://github.com/vestige/UipBlockBreaker/issues/1)

## 将来の任意機能

- [ ] [Issue #2: GAME CLEAR時の振動パターンを追加する](https://github.com/vestige/UipBlockBreaker/issues/2)
- [ ] [Issue #3: ゲーム画面に得点表示を追加する](https://github.com/vestige/UipBlockBreaker/issues/3)
- [ ] [Issue #4: ブロック減少に合わせてボールを加速する](https://github.com/vestige/UipBlockBreaker/issues/4)
- [ ] [Issue #5: バーの衝突位置による反射角を細かくする](https://github.com/vestige/UipBlockBreaker/issues/5)
