
[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp)用のZMKファームウェア

* _centralがついているuf2をトラックボールがついている方に、_peripheralを反対側に書き込んでください
* キーマップはkeymap-editorおよびzmk-studioで編集できます

## Bluetooth 診断ブランチ（ZMK v0.3）

左 Central / 右 Peripheral 専用。UF2、reset・再登録・往復試験、診断キーと通常 Status LED、ソース調査、新しい ZMK の互換性評価は [DIAGNOSTICS.md](DIAGNOSTICS.md) を参照してください。

左 LED は通常の起動時バッテリー表示・接続表示へ復元済みです。Profile 番号の診断点滅は削除し、Bluetooth 切替設定は維持しています。
