# torabo-tsuki LP XS: Bluetooth 診断 / ZMK v0.3

対象: 左トラックボール付き Central / 右 Peripheral、Mac=Profile 0、Galaxy Z Fold6=Profile 1。
元の master: `d2113dab4f9a1b684ab4454b5342ea60c2016e3b`。
診断ブランチ: `diagnose/bluetooth-v0.3-20260922`。master へマージしない。

原因は未確定。この変更は操作・profile 選択・ホスト接続・mouse HID を切り分けるためのもの。
USB での正常動作だけで、無線・電源条件を含むハードウェア要因すべてを否定することはできない。

## UF2

Actions の **Build Bluetooth diagnostic firmware (ZMK v0.3)** を診断ブランチで実行する。
成功 run の **torabo-tsuki-lp-bluetooth-diagnostic-v0.3** artifact をダウンロードする。

| UF2 | 書き込み先 |
|---|---|
| `torabo_tsuki_lp_diag_v03_left_central.uf2` | トラックボール付き左 |
| `torabo_tsuki_lp_diag_v03_right_peripheral.uf2` | 右 |
| `torabo_tsuki_lp_diag_v03_settings_reset.uf2` | 最初に左右それぞれ。同じ BMP Boost 用ファイルを使う |

matrix は誤書き込み防止のため3種類だけ。左右は同じ run の組み合わせを使う。
reset UF2 は通常ファームではない。コピー後に一度起動させ、再びブートローダーに入って通常 UF2 を書き込む。

## 診断キー

既存の `&mo 1` キーを保持し、下記のキーを単独で押す。位置名はベースレイヤーのキー名。

| 操作 | 位置 | binding |
|---|---|---|
| Mac 選択 | MO(1) + **A** | `&bt BT_SEL 0` |
| Fold6 選択 | MO(1) + **S** | `&bt BT_SEL 1` |
| 選択 profile の登録消去 | MO(1) + **Y** | `&bt BT_CLR` |
| 全ホスト profile の登録消去 | MO(1) + **U** | `&bt BT_CLR_ALL` |

SEL0/SEL1 は元の Q/W 位置から A/S 位置へ移動した。元の W 位置は Auto Mouse Layer 4/5 の右クリックに隠される可能性がある。
A/S/Y/U と MO(1) の位置は Layer 4/5 で透明なので、AML のレイヤー番号やマウス操作を変えずに切替できる。
診断時は他のレイヤーキーを同時に保持しない。**Q/W ではなく A/S** を使う。

F+G の BT_CLR combo、SEL2〜4、BT_NXT、OUT_USB/OUT_BLE を削除。
旧 OUT/BT_NXT の位置は Layer 3 で `&none` にし、意図しない下位レイヤーへの通過を防いだ。
消去キーは登録を消す。往復試験中は Y/U を押さない。

## 左 LED

左の既存 Status LED を診断表示に置換した。右の LED は従来通り。

- **1回**点滅、約2.5秒休止、反復: Profile 0。
- **2回**点滅、約2.5秒休止、反復: Profile 1。
- 各点灯が短い（約100ms）: 選択 profile の BLE 接続あり。
- 各点灯が長い（約400ms）: 接続なし。未登録と登録済み・未接続の区別はしない。
- 切替直後は途中の点滅群が残る場合がある。次の完全な点滅群を読む。
- Studio 等で Profile 2〜4 を選んだ場合は3〜5回。profile 容量は減らしていない。

点滅回数は選択状態、点灯時間は BLE 接続状態。**HID 通知購読、カーソル動作、USB/BLE 出力先の成功表示ではない**。
左の従来の起動時バッテリー表示は診断中は出ない。
非同期 work で表示し、接続・HID・保存状態を書き換えない。
`BT_DIAG profile=... connected=... open=...` ログフックも追加したが、標準の3ビルドでは USB ログ出力を有効にしていない。まず LED を使用する。

## 実機の診断手順

settings reset は bond、出力優先、Studio 編集・保存レイアウト等を消す。必要な Studio 設定は事前に保存する。
ソースの既定 physical layout は元から L Layout。XS の配線・transform を推測で変更していない。実機で別の layout を保存していた場合、reset 後に初期化される点も確認する。

1. **Settings reset**
   - Mac/Fold6 の Bluetooth 設定で既存の `torabo-tsuki` 登録を削除する。
   - 左右それぞれに reset UF2 を書き込み、一度起動する。片側の reset 完了後は電源を切っておき、もう片側も実施する。
   - `BT_CLR_ALL` はホスト profile の消去。split/Studio 等を含む settings reset の代用ではない。
2. **通常ファーム書き込み**
   - 左=left_central、右=right_peripheral。両側を再起動して近くに置き、split 接続を待つ。
   - **USB を Mac/Fold6 から外し、電池で試験**。給電が必要なら PC にデータ接続しない。
   - 最初の試験では Studio の設定を復元・編集せず、ソースの診断配置を使う。
3. **Mac を Profile 0 に登録**
   - Fold6 の Bluetooth を一時 OFF。MO(1)+A、LED 1回を確認して Mac から登録する。
   - 左右の文字、ボール移動、クリックを確認する。
4. **Fold6 を Profile 1 に登録**
   - Mac の Bluetooth は ON のまま。Fold6 を ON にして MO(1)+S、LED 2回を確認して登録する。
   - 左右文字、移動、クリックを確認。Fold6 の OS/One UI バージョンと機器詳細に表示される入力機能の項目も記録する（名称・有無は OS による）。
5. **SEL1 → SEL0 → SEL1**
   - 両端末の Bluetooth を ON のまま S → A → S。各回 MO(1) を保持して押し、離す。
   - 各回数秒待ち、LED 回数、短/長、実際に入力された端末を記録する。
   - 最初はボールを止めて2秒以上待ってから。続いてボール操作直後の AML 中でも試す。
   - 最後の選択から少なくとも2秒待って電源を切る（profile 保存の debounce）。
6. **各端末で文字とトラックボールを確認**
   - 各往復で左右文字、移動、クリック、Layer 5 のスクロールを別々に確認する。
   - Mac に戻らない場合、Fold6 Bluetooth OFF → A を試して記録する。
   - その後だけキーボード再起動で変化するか確認する。再現直後の reset/CLR は証拠を消してしまう。

| 操作/条件 | LED 回数 | 短/長 | 左文字 | 右文字 | 移動/クリック/scroll | 入力先 |
|---|---|---|---|---|---|---|
| 初回 Mac SEL0 | | | | | | |
| 初回 Fold6 SEL1 | | | | | | |
| Mac SEL0 | | | | | | |
| Fold6 SEL1 | | | | | | |
| Fold6 OFF → Mac | | | | | | |
| ボール直後の往復 | | | | | | |

USB 接続試験は別に扱う。v0.3 は reset 後、USB/BLE の両方が利用可能なら USB を優先する。
OUT キーを削除しても自動選択はなくならない。USB を挿すと「profile は変わったが入力は USB」という状態になり得る。
OS の「接続済み」表示が複数あること自体は異常ではない。BT_SEL は非選択ホストを強制切断する命令ではない。

## 変更前にコードで確認した内容

ZMK v0.3: `edf5c0814fd3ea202e43aad2d68fd32e882a518c`。

| 対象 | 確認箇所・結果 |
|---|---|
| BT_SEL | `app/src/behaviors/behavior_bt.c` → `ble.c:zmk_ble_prof_select()`。0始まりの profile 選択、広告更新、profile-changed イベントを実行。0/1 は有効。 |
| BT_CLR / ALL | `ble.c:clear_profile_bond()` はホストを unpair し profile アドレスを消す。ALL は全ホストに適用後0を選択。全 settings 消去ではない。 |
| OUT_USB / OUT_BLE | `behavior_outputs.c` → `endpoints.c`、`endpoints/preferred` に保存。profile 選択とは独立。利用可能な endpoint にフォールバックするので USB 優先だけで USB 未接続時の不具合を断定できない。 |
| profile storage | `ble.c` の `ble/profiles/N`、`ble/active_profile`、`ble/peripheral_addresses/N`。通常 UF2 の上書きでは Zephyr の bond とこれらの保存値が残る。 |
| mouse HID | `app/include/zmk/hid.h` の `CONFIG_ZMK_POINTING` 節に mouse report descriptor。USB/BLE で同じ定義を使用。キーボードとは別 report ID。 |
| BLE HID | `app/src/hog.c`: report map、mouse report/CCC、`send_mouse_report_callback()`、`bt_gatt_notify_cb()`。`endpoints.c` → `zmk_hog_send_mouse_report()`。文字成功だけでは mouse CCC 購読成功を示さない。 |
| pointing | shield の旧 `ZMK_MOUSE=y` → `app/src/pointing/Kconfig` で POINTING が既定で有効。変更前の実ビルドでも `ZMK_POINTING=y` を確認。診断左 conf で明示化。 |
| 容量 | `app/include/zmk/ble.h` でホスト数=`BT_MAX_PAIRED - ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS`。今回6−1=5 profile。 |
| 接続数 | `app/src/split/bluetooth/Kconfig.defaults` で Central は MAX_CONN/MAX_PAIRED=6/6。同 `Kconfig` で Peripheral は1/1。右半分＋2ホスト=3接続以内で不足ではない。接続上限と bond 保存数は別概念。 |
| Studio | 保存キー配置がソースに優先し得る。reset 後、Studio 復元・変更前に診断する。 |
| split | 左だけ split-central snippet。PAW3222 → 左 input-listener → mouse HID。右はキー転送。現構成では input-split snippet を使っていない。 |
| 省電力 | `src/board.c` は BLE central role の split 接続を対象にする。USB と電池で条件が異なるが、原因と断定せず維持。 |

一次ソース:
[BT behavior](https://github.com/zmkfirmware/zmk/blob/edf5c0814fd3ea202e43aad2d68fd32e882a518c/app/src/behaviors/behavior_bt.c) /
[BLE/storage](https://github.com/zmkfirmware/zmk/blob/edf5c0814fd3ea202e43aad2d68fd32e882a518c/app/src/ble.c) /
[endpoints](https://github.com/zmkfirmware/zmk/blob/edf5c0814fd3ea202e43aad2d68fd32e882a518c/app/src/endpoints.c) /
[HID descriptor](https://github.com/zmkfirmware/zmk/blob/edf5c0814fd3ea202e43aad2d68fd32e882a518c/app/include/zmk/hid.h) /
[BLE HOG](https://github.com/zmkfirmware/zmk/blob/edf5c0814fd3ea202e43aad2d68fd32e882a518c/app/src/hog.c) /
[split defaults](https://github.com/zmkfirmware/zmk/blob/edf5c0814fd3ea202e43aad2d68fd32e882a518c/app/src/split/bluetooth/Kconfig.defaults)。

## 観測結果の読み方

| 観測 | 次に確認する対象 |
|---|---|
| A/S で LED 回数が変わらない | MO(1)、位置/Studio 保存内容、レイヤー優先、キーイベント。profile 選択命令が届いているか。 |
| 1回になるが長い点灯のまま | Mac 側の旧 bond/登録、広告、再接続/認証。Fold6 OFF 条件と比較。 |
| 1回・短い点灯でも入力しない | USB を外したか、endpoint、HID 購読/通知。BLE 接続成功と HID 成功を分ける。 |
| Fold6 の文字のみOK | HID 再列挙、mouse CCC、descriptor/notify。キーボード report と分けて調べる。 |
| 両側 reset＋端末登録削除で改善 | bond、GATT/HID キャッシュ、Studio 配置等が候補。ただし一括消去なので単一原因の証明にはならない。 |
| 右の文字だけNG | split 接続、距離、省電力。ホスト接続とは分ける。 |

HID descriptor、smooth scrolling、BLE experimental、TX power、省電力を推測で変えていない。
Fold6 のマウスのみ再現するなら、baseline を保存し別ブランチで smooth scrolling のみ OFF 等の一変数比較を行う。descriptor が変わる場合は端末側も再登録する。
USB ログを追加する場合は Studio と別 CDC UART、および USB endpoint による試験条件変化を考慮する。現在の3ビルドには USB logging を含めない。

## 新しい ZMK の互換性 / リスク（2026-09-22 調査）

公式 remote のタグは v0.3.0/v0.3 までを確認。確認時点で v0.4 タグは見つからなかった。
比較対象は main `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0`（2026-09-14）。安定リリースとは呼ばない。
v0.3 は Zephyr 3.5系、比較対象 main は4.1系。以下はソース比較であり、新版の実機検証や改善保証ではない。診断用ビルドは v0.3 のまま。

| 項目 | 比較・リスク・移行時の確認 |
|---|---|
| Bluetooth profile 切替 | behavior_bt.c は同一。ble.c の選択/保存ロジックも同じで、差分は広告 API/appearance 定義等。Zephyr BLE stack が変わるため接続/切替/保存を再試験。更新のみで今回直る証拠はない。 |
| pointing device | hid.h と hog.c は比較対象と同一。ただし Zephyr 4.1 では input callback が `(event, user_data)`、`INPUT_CALLBACK_DEFINE` が3引数になる。現行 `src/board.c:262` は2引数なので、そのまま新版にするとコンパイル互換性がない。更新用ブランチでは callback 引数とマクロを移植する。マウス CCC/AML/scroll の実機確認も必要。 |
| endpoint / 保存値 | 新版は `ZMK_TRANSPORT_NONE`、endpoint API 改名、`endpoints/preferred` → `endpoints/preferred2` の移行を追加。旧値を削除して新値を保存するため、v0.3 へ戻す際に元の出力優先がそのまま復元される保証はない。更新/ロールバック試験では保存内容を記録し reset・再登録で条件を揃える。profile 保存と endpoint 保存は別。 |
| split input | 新版は Peripheral 切断時に追跡中入力キーを解放する処理と ZMK_INPUT_SPLIT_MAX_TRACKED_KEYS を追加。現在の左ローカル一球では使わない。左右キー通信/再接続は両側同版で検証。将来の両球には影響する。 |
| PAW3222 | 現在の torabo-tsuki branch は `0e1835c...`。標準 input API に加え Nordic SPIM/PSEL/GPIO を直接操作し MOSI/MISO 同ピンを扱う。Zephyr/nrfx 更新時にコンパイル、SPI 排他、電源・復帰の回帰リスク。汎用 PAW32xx に自動置換しない。 |
| BMP Boost | 現行 v0.2 `7dc3f9e...` は HWMv1（Kconfig.board、board.yml なし）。新 Zephyr は HWMv2 が必要。upstream master `2f5567523b6f0bc39575d48ed746ed9d635edf8b` は board.yml/Kconfig.bmp_boost を持つので移行候補。board ID、flash/bootloader offset、UF2、GPIO、電池計測を検証。ZMK revision だけ更新する方法は不可。 |
| ZMK Studio | v0.3 の左は Studio/RPC UART/RPC BLE が有効。新版では UART RX priority 等を追加。protocol、保存レイアウト、USB CDC、BLE 通信、復帰を検証。診断中は Studio 編集をしない。既存 LOCKING=n は維持。 |
| USB trigger / battery / LED | 外部モジュールも4.1対応の組み合わせを検証。特に CDC デバイス取得/USB イベント、電池センサー binding。診断 LED 自体も新版で再ビルドする。 |
| Actions | 新版用の別ブランチで reusable workflow と west の ZMK を対応させる。対応ビルドイメージも選ぶ。manifest だけ main にして workflow を v0.3 のまま混在させない。 |

一次ソース:
[公式 Zephyr 4.1 移行](https://zmk.dev/blog/2025/12/09/zephyr-4-1) /
[比較対象 west.yml](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/west.yml) /
[split central](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/src/split/bluetooth/central.c) /
[新版 endpoints](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/src/endpoints.c) /
[Zephyr 4.1 input API](https://github.com/zmkfirmware/zephyr/blob/v4.1.0%2Bzmk-fixes/include/zephyr/input/input.h) /
[BMP Boost HWMv2](https://github.com/sekigon-gonnoc/zmk-component-bmp-boost/blob/2f5567523b6f0bc39575d48ed746ed9d635edf8b/boards/arm/bmp_boost/board.yml) /
[PAW3222](https://github.com/sekigon-gonnoc/zmk-driver-paw3222/blob/0e1835c57f88d215ce401b6d0901f361629621fc/src/paw3222.c)。

## 変更ファイル

| ファイル | 理由 |
|---|---|
| config/keymap.keymap | BT キーを4つに整理、AML と重ならない位置へ移動、F+G combo/OUT/巡回切替削除。 |
| boards/shields/torabo_tsuki_lp/torabo_tsuki_lp_left.conf | 既定6/6と pointing を明示、左 LED 二重制御防止。 |
| src/bt_diagnostics.c | profile と接続状態の非同期 LED 表示、ログフック。 |
| CMakeLists.txt | 左 Central にだけ診断コードを組み込む。 |
| build.yaml | 左 Central、右 Peripheral、reset の3種に限定・明確な名前。 |
| .github/workflows/build.yml | v0.3 reusable workflow 維持、診断名/artifact 名、読み取り権限を明示。 |
| config/west.yml | ZMK v0.3 維持。外部モジュールは未変更 baseline で解決した同じ SHA に固定し無関係な更新を防ぐ。 |
| README.md | この手順へのリンク。 |
| DIAGNOSTICS.md | 手順、根拠、互換性と変更一覧。 |

左 overlay の XY 反転、AML Layer 4/2000ms、Layer 5 の scroll mapping と1/40倍率、smooth scrolling、PAW3222 配線/ドライバー、split 省電力コードを維持。
右 conf/overlay、layout/transform も変更していない。

## 検証範囲

未変更 master の左 Central を実ビルドし 6/6、pointing/BLE/smooth scrolling/Studio を確認。
診断版3 target もローカルでコンパイル・リンク・UF2 生成に成功した。
GitHub Actions の成功 run と出力は最終報告のリンクを参照。
実機への書き込み、LED の実際の点滅、Mac/Fold6 の接続・HID 動作は上記手順での実測が必要。ビルド成功は実機試験の代わりにはならない。

ローカル生成物も追加照合済み:
- 左 `.config`: 6/6、Central/BLE/POINTING/PAW3222/INPUT_LISTENER/Studio/smooth scrolling 有効。
- 右 `.config`: 1/1、Central 無効。reset: SETTINGS_RESET_ON_START 有効、BLE 無効。
- 生成 devicetree: 全レイヤー66 binding、ベース/Layer 2/4/5 は変更前と同一。4つの BT キーと MO(1) は AML 4/5 を透過。
- ELF 内の HID report descriptor のバイト列は変更前 baseline と同一。
- 3 UF2 のブロック形式・個数・nRF52840 family ID を確認。
- 左 UF2 523,776 bytes、右360,960 bytes、reset 96,256 bytes（ローカルビルド）。

既存ソース由来の warning（NRF_STORE_REBOOT_TYPE_GPREGRET の非推奨、temp-layer の format/unused、reset keymap の初期化警告等）は存在する。診断 LED コードのコンパイルエラーはない。
