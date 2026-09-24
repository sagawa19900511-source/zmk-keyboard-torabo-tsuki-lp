# 右 mini trackpad: 独立 Layer 6 / 2D 慣性比較版

基準は実機で正常動作した `35d385eb6015ad359bf4705febfb1d067c503618`（`feature/mini-trackpad-layer-mouse-v0.3-20260924`）。基準ブランチを維持し、`feature/mini-trackpad-independent-inertia-v0.3-20260925` で実装する。ZMK は引き続き v0.3。

## 動作と専用キー

| 状態 | 左トラックボール | 右 mini trackpad |
|---|---|---|
| 通常 | 従来のカーソル | 従来の方向・1/2 倍の縦横スクロール＋慣性 |
| 左 AML 4 中 | 従来のマウス操作 | スクロールのまま。AML 4 ではカーソル化しない |
| Layer 5 中 | 従来の 1/40 スクロール | 通常と同じスクロール |
| 専用 Layer 6 保持中 | 元のレイヤー状態に従う | 旧カーソルの上下左右を反転、2/3 倍。慣性なし |
| Layer 6 解除後 | 従来どおり | 次の入力からスクロール。古い慣性は復活しない |

Layer 6 は全66位置を `&trans` にして追加。既存キー操作は下位レイヤーへ通り、左ボール用の Layer 4 / 5 を作り替えない。右 listener の `mouse_layer.layers` を4から6へ変更した。右入力に `zip_temp_layer` は追加しない。

専用キーはユーザー指定の右上段 **P**。Layer 0 の `&kp P` だけを `&lt 6 P` に変更した（Lレイアウトの格納位置22、0始まり）。短く押すとP、200 ms以上長押ししている間だけLayer 6になる。既存の標準 `lt` は tap-preferred、内部のhold側は `&mo`、tap側は `&kp`。QやENTER等の既存hold-tap設定は変更しない。Layer 4/5ではP位置が透明なので、左AML中でもこの操作が使える。Layer 2/3で同じ位置にあるPLUS/ESCAPEは維持する。Layer 6を離すと即座に元の入力モードへ戻る。

長押し確定前の約200 msはスクロールモードのままなので、カーソル操作はPを保持してから開始する。長押しでPを連続入力する操作はこのキーでは行わない。

## 方向と倍率

通常スクロールの既存 processor 列・順序・引数は保持し、その末尾に慣性観測を追加した。直接入力の値は慣性 processor で変更・抑制しない。

旧カーソル列にはすでに X/Y 両反転が入っていた。今回そこへ同じ反転を残すだけでは方向が直らないため、**カーソル列だけ既存の両反転を除去**し、旧出力に対して X/Y を両方逆にした。スクロール列の両反転は残している。2本指の HWHEEL/WHEEL を生 X/Y に揃える符号正規化も維持した。

`snippets/input-split-listener/mini-trackpad-settings.h` で個別に変更できる。

```c
#define MINI_TRACKPAD_SCROLL_MULTIPLIER 1
#define MINI_TRACKPAD_SCROLL_DIVISOR 2
#define MINI_TRACKPAD_CURSOR_MULTIPLIER 2
#define MINI_TRACKPAD_CURSOR_DIVISOR 3
#define MINI_TRACKPAD_MOUSE_LAYER 6
```

`1/2`、`2/3`、`3/4`、`1/1` は分子／分母をそのまま指定する。左ボールにはこれらの倍率を使わない。

## Keymap Editor / Studio の違いと7レイヤー

ユーザー提示の画面はGitHubへソースを保存するKeymap Editorで、キーボード本体内の設定を書き換えるZMK Studioとは異なる。Keymap Editorのみを使用していた場合、その編集は既存ソースに含まれており、Studio保存値の競合を理由にresetする必要はない。今後Keymap Editorで編集する際は今回の新ブランチを選び、Layer 6とPのLayer-Tapを維持する。古いブランチから生成したUF2には今回の追加が含まれない。

固定 v0.3 では、Studio のレイヤー容量は `ZMK_KEYMAP_LAYERS_LEN` で決まり、Studio 有効時は Devicetree の keymap 子ノード数を数える。別の `CONFIG_ZMK_KEYMAP_MAX_LAYERS` 等を追加する方式ではない。Layer 0〜6 の7ノードを有効な状態で定義し、既存の `CONFIG_ZMK_STUDIO=y` / settings storage / layer reordering を維持する。Layer 名は既存の最大20文字以内の `Mini Trackpad Mouse`。

`keymap_init()` は全7層のソース既定値と順序を初期化し、その後 settings の個別キー・名前・順序を重ねる。旧6層の `keymap/layer_order` は実際の保存長（最大6 byte）だけ読み込むため、新しい末尾 ID 6 を書き換えない。旧 `keymap/l/<layer>/<position>` は該当するキーだけを上書きする。Layer 0〜5 の ID・キー数・座標・物理レイアウトは保持するため、旧保存済み配置を丸ごと初期化する必要はない。

ただし、**選んだ専用キー位置に Studio の保存値がある場合、その値はソースの新しい `&lt 6 P` より優先される**。実機の保存内容は接続して読み取っていないため、保存値がないとは断言しない。この場合は更新後の Studio で **その1キーだけ**を Layer-Tap → hold: `Mini Trackpad Mouse`（ID 6）、tap: `P`に変更して保存する。他のキー、物理レイアウト、順序には触らず、Reset Keymap / settings_reset を使わない。ソース追加の Layer 6 が存在するか Studio で確認する。旧ファームのセッションを使い回さず再接続する。

レイヤー順を Studio で変えていても `&lt` のlayerパラメーターと input listener は安定した layer ID を使用する。ただし表示上の行番号だけで選ばず、新しいレイヤー名で確認する。

## 慣性の実装と停止

候補 `mjmjm0101/zmk-input-processor-scroll-inertia` の commit `f7dadefee453d555fe066d13a3de3bb60739b45e` を調査した。ZMK v0.3 対応・2D 対応だが、同方向の新入力中も coast を維持・合成する設計、layer OFF を主に扱う gate、直接 HID 出力で XY 移動量をゼロにする箇所があり、今回の厳密な入力キャンセルと左ボール隔離には追加改変が必要だった。外部依存としては採用せず、限定用途のローカル processor を追加した。

- `src/mini_scroll_inertia.c`: 左 Central だけで有効。右パッド listener の **1/2 scaler 後**の値から縦横の速度を計測する。時間間隔を使って8 ms当たりの速度へ換算し、減衰して追加イベントを出す。直接入力は変更しない。
- `src/mini_scroll_inertia_core.h`: 時間・速度・減衰・世代番号の処理を独立し、ホストでテスト可能にした。
- 出力は専用の仮想入力デバイス → `mini_coast_listener` → ZMK 標準の mouse report 経路へ流す。独自処理から共有 HID の XY やボタンを直接書き換えない。
- すべての新しい右入力（同方向、逆方向、ゼロ移動、ボタンを含む）は継続中の慣性を止め、古い出力を無効化する。新しい入力が続けば、その新しい操作に基づく慣性を後で生成できる。
- Layer 6 への入口・出口で状態と予約処理をキャンセルする。カーソル列先頭でもキャンセルし、待ち時間を挟むだけの停止にはしない。Layer 4 / 5 の変更では右慣性を止めない。
- 予約済みの合成入力には世代番号を持たせ、別 listener の gate で確認する。キャンセル以前のイベントがキューに残っても、Layer 6 解除後まで持ち越して出力しない。
- 入力スレッド／遅延処理／レイヤー通知間の状態は spinlock で保護。キューへの送信は非ブロッキングで、混雑時は慣性を停止する。実入力を無視して惰性を優先しない。

### 制約

現行 IQS7211E の非 scroller モードは明示的な touch-down / touch-up を送らない。そのため「40 ms 新しい入力が来なければ指を離したとみなす」方式。**停止した指を置くだけではキャンセルできず、最初の移動・ゼロ移動通知・ボタン通知で止まる**。ゆっくりした断続操作では無入力区間を release と判断する可能性がある。必要なら release 値を増やす。真のタッチ開始で止めるには driver / split にタッチ状態を追加する必要があり、今回は禁止された変更を行わない。

高分解能設定・HID descriptor は保持。慣性も直接入力と同じ scaler 後の単位を使い、合成入力に1/2を二重適用しない。ただし慣性は過去の速度を平滑化した推定値なので、指の速度を完全に再現する保証ではない。実機での感触・OS／アプリのスクロール解釈は比較確認する。

## ON/OFF と強さ

同じ `mini-trackpad-settings.h` にまとめる。

| 設定 | 初期値 | 意味 |
|---|---:|---|
| `MINI_TRACKPAD_INERTIA_ENABLED` | 1 | 0で無効、1で有効 |
| `..._RELEASE_MS` | 40 | 新入力が途切れてから慣性を開始する時間 |
| `..._DECAY_PERMILLE` | 950 | 1 tickごとの速度残量。大きいほど長く滑る（1000未満） |
| `..._BLEND_PERMILLE` | 500 | 速度推定で前回値を残す比率。大きいほど平滑 |
| `..._START_MILLI` | 1000 | 開始に必要な速度（1/1000スクロール単位/tick） |
| `..._STOP_MILLI` | 250 | 停止する速度 |
| `..._MIN_SAMPLES` | 3 | 慣性を許すまでの有効速度サンプル数 |
| `..._LIMIT_MILLI` | 64000 | 速度上限 |
| `..._MAX_MS` | 1500 | 慣性の最長時間 |
| `..._TICK_MS` | 8 | 更新間隔。減衰率の意味も変わるため通常は維持 |

`...` は `MINI_TRACKPAD_INERTIA`。減衰を控えめにするなら例として 950→930、長くしたければ 970。過剰な速さを足す別 gain は設けず、scroll scaler と速度単位を一致させる。倍率を変更した場合も自動的に慣性へ反映する。

A/B 比較用 `mini-trackpad-inertia-off` snippet は同じ processor の `enabled=0` を上書きする。スカラーやキー、方向、カーソルモードは ON 版と同じ。

## UF2・実機確認

| UF2 | 用途 |
|---|---|
| `torabo_tsuki_lp_diag_v03_left_central.uf2` | 左、慣性 ON |
| `torabo_tsuki_lp_diag_v03_right_peripheral.uf2` | 右、共通 |
| `torabo_tsuki_lp_diag_v03_settings_reset.uf2` | 復旧用。通常の更新では使わない |
| `torabo_tsuki_lp_diag_v03_left_central_inertia_off.uf2` | 左、慣性 OFF 比較用 |

初回は同じ成功ビルドの左右通常ファームを使用する。その後の ON/OFF 比較は左UF2だけを入れ替える。既存ペアリングや Studio 設定を消さないため、reset は使わない。

1. PのタップでPが入力され、200 msの長押しでLayer 6になることを確認。Studioを別途使ったことがある場合だけ再接続し、保存済みの別割当があればPの1キーだけ `&lt 6 P` にする。
2. Mac/Profile 0、USBを外して右の縦横スクロール・方向・1/2量を確認。AML 4 中も変わらないことを確認。
3. フリック後の慣性、縦・横・斜め、同方向／逆方向の新入力での停止を確認。
4. 慣性中にPを200 ms以上長押しし、スクロールが止まり、右カーソルが修正後の方向・2/3量で動くことを確認。Layer 6に入ってからすぐ離した場合も古い慣性が復活しないことを確認。Pの短押しは通常文字入力のため、Layer 6には入らない。
5. 専用キーを離し、次の移動がスクロールになることを確認。左ボールと AML 4、Layer 5 の挙動・キー・通常 LED は従来どおりか確認。
6. Fold6/Profile 1でも同じ項目を確認し、SEL1→SEL0→SEL1を往復。次に左OFF版で同じ条件を比較する。

## 変更範囲と検証

変更: 右専用 listener overlay・倍率設定header、Layer 6追加とPの `&lt 6 P` 割当、ローカル慣性processor/core/binding、CMake・ローカルbinding検索root、OFF snippet、比較用build entry、テストと説明書。

変更ファイル一覧（16ファイル）:

| ファイル | 理由 |
|---|---|
| `config/keymap.keymap` | PをLayer-Tap化、透明なLayer 6を追加 |
| `snippets/input-split-listener/input-split-listener.overlay` | 右のLayer 6分岐、カーソル方向、慣性と独立listener |
| `snippets/input-split-listener/mini-trackpad-settings.h` | 個別倍率・慣性ON/OFF・減衰等の集約 |
| `src/mini_scroll_inertia.c` | Central側イベント連携・キャンセル・出力 |
| `src/mini_scroll_inertia_core.h` | 2D速度推定・減衰・キュー世代検査 |
| `dts/bindings/input_processors/torabo,mini-scroll-inertia.yaml` | ローカルprocessorのDevicetree定義 |
| `CMakeLists.txt` | 左Centralだけで慣性コードをビルド |
| `zephyr/module.yml` | ローカルDevicetree定義を検索対象へ追加 |
| `snippets/mini-trackpad-inertia-off/snippet.yml` | 比較版のsnippet登録 |
| `snippets/mini-trackpad-inertia-off/mini-trackpad-inertia-off.overlay` | 比較版では慣性だけ無効化 |
| `build.yaml` | 左OFF版を追加し4UF2を生成 |
| `tests/mini_trackpad/test_inertia.c` | 慣性・停止・境界値のテスト |
| `tests/mini_trackpad/test_processors.py` | 実際のZMK処理と生成設定の変換テスト |
| `MINI_TRACKPAD_INDEPENDENT.md` | 本説明・比較と調整手順 |
| `README.md` / `MINI_TRACKPAD_LAYER.md` | 新版説明へのリンク |

不変: `config/west.yml` の全依存pin、IQS7211E driver／初期化データ／非 scroller overlay/conf、左右shield、左ボールlistenerとAML・Layer 5、PAW3222、BMP Boost、Bluetooth・profile・BT_SEL、splitのIDと通信設定、通常LED。Layer 4 / 5 のキー配置も変更しない。

テスト: `tests/mini_trackpad/test_inertia.c` はホストCで速度・減衰・入力キャンセル・キュー世代・上限を検証。`test_processors.py` は生成DTの processor列と固定ZMKのC実装で Layer 4/5 独立・6切替・方向・1/2対2/3・2本指とボタンを検証する。実機保存設定や実機操作感を検証したものではない。

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address tests/mini_trackpad/test_inertia.c -o /tmp/mini-inertia-test
/tmp/mini-inertia-test
# ZMK v0.3 の west workspace と左ビルドのパスを指定（gcc と Python dtlib が必要）
python3 tests/mini_trackpad/test_processors.py --workspace /path/to/west-workspace --build /path/to/central-build
```

参照: [調査した候補module](https://github.com/mjmjm0101/zmk-input-processor-scroll-inertia/tree/f7dadefee453d555fe066d13a3de3bb60739b45e)、[ZMK v0.3 keymap / settings](https://github.com/zmkfirmware/zmk/blob/v0.3/app/src/keymap.c)、[layer容量定義](https://github.com/zmkfirmware/zmk/blob/v0.3/app/include/zmk/keymap.h)。

現段階のローカル検証: left_central・right_peripheral・settings_reset・left_central_inertia_off の4ビルド成功。ELF の layer order 配列は7 byteで、Studio用 layer ID 0〜6 の容量を確認済み。右UF2とresetUF2は基準版とバイト一致。Pの短押し/長押し割当を含む最終ビルド・54項目の入力変換・慣性状態テストも成功。生成されたキーマップで、既存6層の変更がLayer 0のP 1キーだけであることと、200 msのLayer-Tap設定を検証した。
