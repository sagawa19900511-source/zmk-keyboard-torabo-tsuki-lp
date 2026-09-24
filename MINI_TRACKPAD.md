# 右 mini trackpad: 公式最小構成 / ZMK v0.3

この文書は正常動作を確認したスクロール専用版の記録です。`feature/mini-trackpad-layer-mouse-v0.3-20260924` のレイヤー切替・1/2 倍設定・慣性無効化については [MINI_TRACKPAD_LAYER.md](MINI_TRACKPAD_LAYER.md) を参照してください。

基準は通常 Status LED 復元済みの `diagnose/bluetooth-v0.3-20260922`、commit `97dc1f6aae605e5cd727180d2977e3b6587192b5`。
作業ブランチは `feature/mini-trackpad-v0.3-20260923`。基準ブランチと master は変更しない。
[公式手順](https://github.com/sekigon-gonnoc/torabo-tsuki-lp/blob/master/mini-trackpad-option.md)の3つの snippet を既存構成に追加する。

## ファイル変更

ファームウェア設定の変更は `build.yaml` の snippet 2行のみ。

| 対象 | snippet（変更後の全文） | 理由 |
|---|---|---|
| 左 Central | `studio-rpc-usb-uart split-central input-trackball input-listener input-split-listener` | 左トラックボールを保持し、右からの split input を受け取る |
| 右 Peripheral | `studio-rpc-usb-uart input-trackpad-mini input-split` | IQS7211E mini trackpad を有効にして入力を左へ送る |
| settings_reset | 追加なし | 既存 BMP Boost 用 reset を保持 |

`README.md` に本手順へのリンク、`DIAGNOSTICS.md` に追加前の記録であることを追記。新規文書は本ファイルのみ。
Actions workflow と3つの artifact-name は変更しないため、UF2 名には既存の `diag_v03` が残る。取得時は必ず **mini trackpad ブランチの成功 run** を選ぶ。

## 再利用する snippet / overlay / conf（編集なし）

| 既存ファイル | 有効になる内容 |
|---|---|
| `snippets/input-trackpad-mini/snippet.yml` | 同ディレクトリの overlay と conf を右ビルドへ追加 |
| `snippets/input-trackpad-mini/input-trackpad-mini.overlay` | I2C0、400 kHz、IQS7211E `0x56`、IRQ P0.20、電源 P0.08。`init-symbol = "mini_trackpad_iqs7211e_init"`、217 byte 初期化、`scroller-mode` |
| `snippets/input-trackpad-mini/input-trackpad-mini.conf` | `CONFIG_IQS7211E=y`、`CONFIG_IQS7211E_SCROLLER_INERTIA=y` |
| `snippets/input-split/snippet.yml` / `input-split.overlay` | 右の `pointing_device` を `zmk,input-split` の ID 0 に接続 |
| `snippets/input-split-listener/snippet.yml` / `input-split-listener.overlay` | 左に同じ ID 0 の受信口と有効な input listener を追加。既存の XY 反転 processor をそのまま使用 |

`scroller-mode` は指の移動を wheel / horizontal wheel イベントにする。新 listener の XY 反転は X/Y 用であり、wheel をカーソル移動へ変換しない。
既存の `CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y` を保持し、公式デフォルトの高分解能スクロール構成を使う。高分解能の利用や体感はホスト OS・アプリ側にも依存する。

## 維持する設定

- 左 PAW3222 トラックボール、既存 AML（Layer 4 / 5）、速度・向き・scroll processor。
- Mac=`BT_SEL 0`（MO(1)+A）、Fold6=`BT_SEL 1`（MO(1)+S）、`BT_CLR`（MO(1)+Y）、`BT_CLR_ALL`（MO(1)+U）。F+G combo と OUT_USB / OUT_BLE の削除状態。
- 左 Central / 右 Peripheral、Central の `CONFIG_BT_MAX_CONN=6` / `CONFIG_BT_MAX_PAIRED=6`、既存 Bluetooth 設定。
- 左右の通常 Status LED、起動時バッテリー表示。独自診断 LED は復活させない。
- ZMK v0.3、`config/west.yml` の全 pin。IQS7211E は既存 `436d3c42172abf812ec104521f29384fc02fc50e` を使用。
- BLE HID、USB HID、ZMK Studio。ホストへは左 Central が入力を送る。右を Mac/Fold6 に別途ペアリングしない。

左右 shield の overlay / conf、keymap、CMakeLists.txt、初期化データ、west.yml は変更しない。トラックパッド用のスクロール⇔マウス切替・レイヤー割当・速度調整は追加しない。

## UF2 と実機確認

GitHub Actions の `Build Bluetooth diagnostic firmware (ZMK v0.3)` を本ブランチで実行し、成功 run の `torabo-tsuki-lp-bluetooth-diagnostic-v0.3` を取得する。

| UF2 | 書き込み先 |
|---|---|
| `torabo_tsuki_lp_diag_v03_left_central.uf2` | トラックボール付き左 |
| `torabo_tsuki_lp_diag_v03_right_peripheral.uf2` | mini trackpad 付き右 |
| `torabo_tsuki_lp_diag_v03_settings_reset.uf2` | 必要な場合のみ左右それぞれ。その後、各側の通常 UF2 を書く |

1. [公式手順](https://github.com/sekigon-gonnoc/torabo-tsuki-lp/blob/master/mini-trackpad-option.md)に沿って右へ取り付ける。電源を切って作業し、FPC をアンテナに重ねず、ケースで挟まない。
2. **同じ run の左・右の通常 UF2 を両方に書き込む。** 追加のみなら settings reset は不要。既存ペアリングを保持して試す。
3. 左右を再起動し、USB を外して BLE で試す。Mac を MO(1)+A（Profile 0）で選ぶ。
4. 長い Web ページなどで、左右の文字入力、左トラックボールのカーソル移動、右 mini trackpad の上下スクロールを確認する。右パッドはスクロール用なので、指移動でのカーソル移動は期待しない。
5. MO(1)+S（Profile 1）で Fold6 に切り替え、同じ3項目を確認する。`SEL1 → SEL0 → SEL1` と往復して確認する。
6. スクロールしない場合は、端末・アプリ名、右キー入力の可否、左トラックボールの可否、右パッドが反応したかを記録する。右キーも効かない場合は split 接続、キーだけ効く場合はパッド認識・配線・入力経路の切り分けに役立つ。

`BT_CLR` / `BT_CLR_ALL` / settings reset は登録を消すため、通常の往復確認では使わない。必要になった場合の再登録手順は [DIAGNOSTICS.md](DIAGNOSTICS.md) を参照。

ビルドおよび生成設定の検証と、実機のセンサー認識・Mac/Fold6 上のスクロール確認は別。実機の動作は上記手順で確認する。

## ローカル検証結果（2026-09-23）

ZMK v0.3 と既存 pin を使用し、左 Central・右 Peripheral・settings_reset の3ビルド成功。
生成された設定・Devicetree・ELF を通常 LED 復元版と比較し、以下を確認した。

- 左の有効設定の差分は split input 有効化とその初期化優先度・対応 Devicetree フラグのみ。
- 左の既存 Devicetree ノードは全て不変（keymap、PAW3222、AML を含む）。
- 左の HID report descriptor はバイト単位で同一。BLE の mouse HID / 高分解能スクロール定義を保持。
- 左右の通常 Status LED 設定は全て不変。Central の接続・登録枠 6/6 と右 1/1 を保持。
- 右の IQS7211E、217 byte 初期化データ、scroller-mode、慣性スクロール、ID 0 の split 入力、および左の受信 listener を確認。
- settings_reset の有効設定と UF2 は基準版と完全一致。
