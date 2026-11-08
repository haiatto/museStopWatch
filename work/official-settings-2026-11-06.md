# 公式ファームの設定方法の一覧（2026-11-06 調査）

対象: muse-gadget-sdk upstream 463bd2a、StopWatch（タッチ機種）、`components/muse/muse_settings_ui.c`。
今の StopWatch ビルドで無効のもの: ライブ通話（CONFIG_MUSE_REALTIME_VOICE）、Phone camera（CONFIG_MUSE_VIDEO）。

## 本体の設定画面（顔の画面から左スワイプ。右スワイプで戻る）

| ページ | 項目 | 内容 |
|---|---|---|
| SETTINGS（ホーム） | Wi-Fi / Muse / Bluetooth / Sound / Sleep / Battery / Power off | 右に現在値。最下段にバージョン等 |
| WI-FI | Wi-Fi スイッチ、状態、Scan for networks、Other network...、MAC | 画面キーボードで入力。最大8件保存し、範囲内で一番強いものに接続。保存済みを2回タップで削除 |
| MUSE | 接続状態、Reset pairing、Server、VM ID、Device token、**SDK token**、Test connection | すべて画面キーボードで入力可。SDK token は空欄なら現状維持、新しい値で再起動。ここで入れた値がファーム内蔵のものより優先。Reset pairing は Wi-Fi とペアリングを消して再起動 |
| BLUETOOTH | Phone setup スイッチ、状態、Forget paired phones | オンにすると近くのスマホから見える。`tools/muse/ble_setup.html` を Chrome で開いて接続し、本体に出る6桁コードで認証 |
| SOUND | Speaker、Volume、Mic gain、**Mic level メーター**、Brightness | メーターが緑（-30〜-15 dBFS）に届き、橙にならない程度が目安。（ライブ通話を有効にすると Voice notes スイッチも出る） |
| AUTO-SLEEP | Never / 30秒 / 1分 / 2分 / 5分 / 10分、Sleep now | 画面タップかボタンで復帰 |
| BATTERY | 残量、使用量、満充電の持ち、画面オフ時間、スリープ時間、起床回数、CPU 使用率、Start over | USB を抜いてからの計測 |
| POWER | Power off / Cancel | |

- 表示は英語のみ（このページ群は翻訳対象外。言語設定は英語とベトナム語のみで、ボタンメニューとペアリングカードだけ翻訳される）。
- 返答の日本語表示は CONFIG_MUSE_CJK_FONT（museStopWatch の overlay で有効化済み）。

## 本体以外の設定経路

1. **Muse アプリのペアリング**（BLE）: Developer mode → Add Device → 本体で承認。Wi-Fi もここで渡せる。
2. **Web Bluetooth の設定ページ** `esp32/tools/muse/ble_setup.html`（PC かAndroid の Chrome。iOS は Bluefy）:
   Wi-Fi（参加・全削除）、Muse（Server / VM ID / Device token / 保存 / 接続テスト）、
   音量・スピーカー・マイク感度・明るさ・自動スリープ、ログ表示。
3. **Web フラッシャー**（`web-flasher/`、USB）: 書き込み、SDK トークン設定、Device settings
   （明るさ・画面スリープ・音量・スピーカー・マイク感度）。英語と日本語の表示あり。
4. **USB シリアル**: `>sdk_token=mgst_…` などのコンソールコマンド。
5. **ビルド時**: `idf.py menuconfig`（ESP32 Device SDK > Muse Gadgets SDK token など）。

## kantanMuse の利便性機能との対応

| kantanMuse | 公式 | 移植の要否 |
|---|---|---|
| SoftAP + QR の Web 設定（スマホのブラウザだけ） | なし（BLE の設定ページ、USB の Web フラッシャーはある） | 利便性として検討 |
| Web からトークン入力 | 本体画面・BLE ページ・Web フラッシャー・シリアルで入力可 | 必須ではない |
| Wi-Fi 複数保存と切替 | 8件保存、強い方へ自動接続、本体で追加・削除 | ほぼ公式で足りる |
| マイク感度＋試し録音・再生 | 感度スライダーとレベルメーター（録音再生なし） | 試し録音は追加候補 |
| 日本語のメニュー | なし | 作る（言語に日本語を追加、設定ページを翻訳対象にする、メニュー用日本語フォント） |
