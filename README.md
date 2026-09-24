
[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp)用のZMKファームウェア

* _centralがついているuf2をトラックボールがついている方に、_peripheralを反対側に書き込んでください
* キーマップはkeymap-editorおよびzmk-studioで編集できます

## 右 mini trackpad 追加版（ZMK v0.3）

`feature/mini-trackpad-layer-mouse-v0.3-20260924` は右パッドを通常スクロール／既存 Mouse Layer 4 中カーソルに切り替える拡張版です。両モードの倍率は初期値 1/2、固定 scroller-mode 専用の慣性は無効です。[設定・検証・書き込み手順](MINI_TRACKPAD_LAYER.md)を参照してください。

`feature/mini-trackpad-v0.3-20260923` は、通常 LED 復元済みの安定設定を基に、公式 snippet で右ミニトラックパッドのスクロールを有効化します。左右両方の通常 UF2 を更新してください。変更内容・確認手順は [MINI_TRACKPAD.md](MINI_TRACKPAD.md) を参照してください。

## Bluetooth 診断ブランチ（ZMK v0.3）

左 Central / 右 Peripheral 専用。UF2、reset・再登録・往復試験、診断キーと通常 Status LED、ソース調査、新しい ZMK の互換性評価は [DIAGNOSTICS.md](DIAGNOSTICS.md) を参照してください。

左 LED は通常の起動時バッテリー表示・接続表示へ復元済みです。Profile 番号の診断点滅は削除し、Bluetooth 切替設定は維持しています。
