# 右 mini trackpad のレイヤー切替と感度調整

この文書は Layer 4 連動版の記録です。独立 Layer 6・慣性比較版は [MINI_TRACKPAD_INDEPENDENT.md](MINI_TRACKPAD_INDEPENDENT.md) を参照してください。

基準: 実機で正常動作した `feature/mini-trackpad-v0.3-20260923`、`c3289806cf1269c645535474e709c29dc66b9295`。
今回のブランチ: `feature/mini-trackpad-layer-mouse-v0.3-20260924`。基準ブランチと master は変更しない。

## 動作

| 条件 | 右 mini trackpad |
|---|---|
| Mouse Layer 4 が無効 | 縦・横スクロール、指移動に対して従来の約 1/2 |
| Mouse Layer 4 が有効 | X/Y カーソル移動、標準 XY 出力の 1/2 |
| Layer 4 が無効になった次の入力 | 待ち時間なくスクロールへ復帰 |

`config/keymap.keymap` の `layer_4` にマウスボタンと `&mo 5` があり、ベースに `&lt 4 Q` / `&lt 4 ENTER` があることを確認して **既存 Layer 4** を使用した。キー配置は変更していない。

左トラックボールの AML は既存の `zip_temp_layer 4 2000` のまま。左ボールで自動的に Layer 4 が有効になった間も右パッドはカーソルになる。手動の Layer キーを離しても AML が有効なら、Layer 4 が実際に解除されるまでカーソルのまま。右パッド自身は AML を起動・延長しない。

左トラックボールの通常カーソル動作・感度・AML は変更していない。既存の Layer 5 中の左ボールスクロール（1/40）も保持する。Layer 4 と 5 が同時に有効な場合、右パッドは Layer 4 を優先してカーソルになる。

## 調査結果と切替方法

既存の IQS7211E driver (`436d3c42172abf812ec104521f29384fc02fc50e`) の `scroller-mode` は、センサーのハードウェア固定設定ではなくドライバー内の処理分岐。`iqs7211e_process_scroller_motion()` は指の動きを1軸にロックし、wheel または horizontal wheel に変換する。横スクロールは指定された領域に制限される。この後で wheel を X/Y に読み替えても失われた軸の情報は戻らない。

そのため右 overlay の `scroller-mode` を外して、ドライバー既存の非 scroller 分岐を使用する。1本指は X/Y、2本指は HWHEEL=X / WHEEL=-Y を出すので、左 Central の **右入力専用** `pointing_device_split_listener` で標準 processor を使って揃える。

1. 2本指の WHEEL 符号を `zip_scroll_transform` で戻し、専用の code-mapper ノード（標準 `zmk,input-processor-code-mapper`）で HWHEEL/WHEEL → X/Y とする。1本指 X/Y はそのまま通る。
2. 既存の右側 XY 反転を適用する。通常スクロールの符号は旧 scroller の -X/-Y に合わせる。
3. 通常は `zip_xy_to_scroll_mapper` と `zip_scroll_scaler 1 2`、Layer 4 中は `zip_xy_scaler 1 2` を適用する。

Layer override に `process-next` を付けないため、カーソルとスクロールの両方に同じ入力が流れることはない。ZMK v0.3 の `filter_with_input_config()` が入力ごとに `zmk_keymap_layer_active(4)` を評価するため、独自のモード保存・遅延・右側へのレイヤー同期は不要。Bluetooth / split の通信設定はそのまま。

タップのボタンイベントはこの処理をそのまま通過する。2本指移動も Layer 4 中はカーソルへ変換するため、カーソルモード中に2本指だけスクロールになる混在を避けている。

### 高分解能と慣性

高分解能スクロールの設定と HID descriptor は保持。横方向は旧 scroller の領域制限・軸ロックがなくなり、パッド全域で縦横・斜め方向のスクロール入力が可能になる。OS・アプリによって横スクロールや高分解能の扱いは異なる。

**慣性スクロールは今回無効。** 固定 driver の慣性は scroller 分岐だけで生成される。非 scroller 分岐には XY の慣性生成がなく、標準 mapper/scaler は慣性を生成しない。`CONFIG_IQS7211E_SCROLLER_INERTIA=n` と明示する。指を離した後の惰性は従来より短くなるため、「約 1/2」は指を動かしている間の入力値を指し、従来の惰性まで含む移動距離の半分を保証するものではない。

慣性の維持には別途ドライバー等の拡張が必要になる。今回は既存 driver の改変・split レイヤー同期を追加せず、標準のレイヤー処理で実現する最小構成を採用した。

## 倍率の変更場所

[`snippets/input-split-listener/mini-trackpad-settings.h`](snippets/input-split-listener/mini-trackpad-settings.h) の4つの値だけを編集する。

```c
#define MINI_TRACKPAD_SCROLL_MULTIPLIER 1
#define MINI_TRACKPAD_SCROLL_DIVISOR 2
#define MINI_TRACKPAD_CURSOR_MULTIPLIER 1
#define MINI_TRACKPAD_CURSOR_DIVISOR 2
```

| 倍率 | MULTIPLIER | DIVISOR |
|---|---:|---:|
| 1/2 | 1 | 2 |
| 1/3 | 1 | 3 |
| 2/3 | 2 | 3 |

スクロールとカーソルは独立して調整できる。分数の端数は標準 scaler が listener・モード・軸ごとに保持するため、細かい連続入力を毎回切り捨てない。左 listener の `zip_scroll_scaler 1 40` や PAW3222 にはこの倍率を適用しない。
この倍率だけの変更後は左 Central を再ビルド・更新する。初回の本拡張への移行は下記のとおり左右両方を更新する。

## 変更したファイル

| ファイル | 変更理由 |
|---|---|
| `snippets/input-trackpad-mini/input-trackpad-mini.overlay` | `scroller-mode` を外し、失われていた X/Y を左へ送る |
| `snippets/input-trackpad-mini/input-trackpad-mini.conf` | 固定 scroller 専用の慣性を明示的に無効化 |
| `snippets/input-split-listener/input-split-listener.overlay` | 右入力専用のモード切替・軸正規化・標準 scaler を設定 |
| `snippets/input-split-listener/mini-trackpad-settings.h` | 倍率設定を集約、既存 Mouse Layer 4 を指定 |
| `MINI_TRACKPAD_LAYER.md` | 本説明・検証・実機確認手順 |
| `README.md` / `MINI_TRACKPAD.md` | 拡張版の案内、スクロール専用版との区別 |

`build.yaml`、`config/west.yml`、IQS7211E driver 本体、初期化データ、左右 shield overlay/conf、keymap、PAW3222、BMP Boost、通常 LED、CMake、Actions、`input-split` の ID / 通信経路には変更なし。

## 検証結果

2026-09-24、固定 ZMK v0.3 で `left_central` / `right_peripheral` / `settings_reset` のローカルビルドが全て成功。
生成物を基準版と比較して以下を確認した。

- 左の Kconfig は全項目一致。右の差分は IQS7211E 慣性関連のみ。
- 既存の左 Devicetree ノードは右パッド listener の processors 以外の全プロパティが一致。左ボール・AML・キー配列は不変。
- 右 Devicetree の差分は `scroller-mode` 削除のみ。split・I2C・GPIO・初期化データの参照は不変。
- BLE/USB 共通 HID descriptor の内容はバイト単位で一致。Central 6/6・Peripheral 1/1 の接続／登録枠、通常 LED 設定も保持。
- settings_reset は設定・UF2 の両方が基準版と完全一致。
- 生成 Devicetree の processor 列と固定 ZMK C ソースの scaler / mapper / transform / layer filter をホストで実行し、94項目を検証。縦横、正負、1本指・2本指イベント、1/2・1/3・2/3 倍、Layer 4 出入り、分数の端数、ボタン維持を確認。

これらはビルド・入力処理の検証。実機の指操作・BLE での体感・OS ごとの方向は以下で確認する。

## 書き込みと実機確認

本ブランチの成功した Actions run の artifact `torabo-tsuki-lp-bluetooth-diagnostic-v0.3` を取得する。UF2 名は既存名を保持するので、古い run と混ぜない。

1. **左 `torabo_tsuki_lp_diag_v03_left_central.uf2` と右 `torabo_tsuki_lp_diag_v03_right_peripheral.uf2` の両方を同じ run から更新する。** 片側だけだと出力形式と変換が合わない。settings_reset は同梱するが、通常の更新では不要。
2. USB を外し、MO(1)+A で Mac（Profile 0）を選ぶ。左ボールを止めて AML の Layer 4 が解除された後、右パッドで上下・左右・斜めスクロールを確認する。
3. 既存 `&lt 4` の Q または ENTER を保持して Mouse Layer に入り、右パッドで X/Y カーソル移動を確認する。ホールド判定後に試す。Layer 4 が解除されたら、次の移動からスクロールへ戻ることを確認する。
4. 左ボールによる AML 中も右パッドがカーソルになること、右パッドだけの操作では AML を延長しないことを確認する。1本指に加え、2本指・タップ・キー側クリックも確認する。
5. 左ボールの感度と既存 Layer 5 スクロール、左右キー入力、起動時バッテリー LED が従来どおりか確認する。
6. MO(1)+S で Fold6（Profile 1）へ移り、同じ項目を確認し、SEL1 → SEL0 → SEL1 と往復する。BT_CLR / BT_CLR_ALL は試験中に使わない。

元に戻す場合は、安定した `c328980` 版の左右 UF2 をセットで書き戻す。通常は settings reset 不要。

参照: [ZMK input processors](https://zmk.dev/docs/keymaps/input-processors)、[固定 IQS7211E ソース](https://github.com/sekigon-gonnoc/zmk-driver-iqs7211e/blob/436d3c42172abf812ec104521f29384fc02fc50e/src/iqs7211e.c)、[ZMK v0.3 listener](https://github.com/zmkfirmware/zmk/blob/v0.3/app/src/pointing/input_listener.c)。実装判断は最新ドキュメントだけでなく、上記固定版の実コードを基準にした。
