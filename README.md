# museStopWatch

M5Stack StopWatch（ESP32-S3、1.75 インチ円形 AMOLED）で Muse を動かすための開発ワークスペースです。
ファームは公式の [Muse Gadgets SDK](https://github.com/facebookincubator/muse-gadget-sdk) をフォークして改造しています
（フォーク: [haiatto/muse-gadget-sdk の dev/stopwatch ブランチ](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch)）。

できること（公式に足したもの）:

- **ミニアプリ**: Muse が C で小さなアプリ（ゲームなど）を作って本体に入れ、その場で動かせる
- **本体での日本語読み上げ**: [sanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp) を本体で動かす（`voice.say`）
- **Web 設定・AP モード・QR**: スマホのブラウザから Wi-Fi と Muse の設定。保存済み Wi-Fi の切り替え
- **サイドチャット**、日本語の表示（Noto Sans JP）

## このリポジトリの構成

| 場所 | 中身 |
|---|---|
| [muse-gadget-sdk/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch) | ファーム（公式 SDK のフォーク、サブモジュール） |
| [sdk/](sdk/) | ミニアプリの SDK（`build.py`、API ヘッダー、使える関数の一覧）。Muse が VM で使う |
| [apps/](apps/) | お手本のミニアプリ（おちものブロック、ぴょこの森、まるい水槽） |
| [tools/](tools/) | 関数の一覧の生成、SDK の作成、USB シリアルでのアプリの転送など |
| [dev.ps1](dev.ps1) | ビルド・書き込み・シリアル監視（Windows / PowerShell） |
| [work/](work/) | 状況メモ、調査、アイデア |
| [.claude/skills/](.claude/skills/) | Claude Code 用のスキル（ミニアプリ作成、計画ページ） |

## フォークで足したもの

自分の部品は、公式の構成どおり `esp32/components/` の下に、まとめ用のフォルダ
[`muse_diy_stopwatch/`](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch)
を作って置いています（ビルド時に `EXTRA_COMPONENT_DIRS` で読み込む）。

| 部品 | 中身 |
|---|---|
| [miniapp/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp) | ミニアプリの実行環境（PSRAM で ELF を動かす）、ランチャー、アプリメニュー、`app.*` コマンド、Muse 向けガイド [guide.md](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp/guide.md)、API [msw_app.h](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp/include/msw_app.h) |
| [sanotts/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/sanotts) | 日本語読み上げ。`upstream/` は sanoTTS-jp のコピー（変更しない）、`model/` は git 管理外 |
| [ui/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/ui) | AP モードと Web 設定、QR 画面、サイドチャット画面、保存済み Wi-Fi の行、日本語の文字、メモリ監視とコンソール |

ほかに足したファイル:
[devices/sdkconfig.musestopwatch](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/devices/sdkconfig.musestopwatch)（このプロジェクトの設定）、
[partitions_musestopwatch.csv](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/partitions_musestopwatch.csv)（パーティション）、
日本語フォント [muse_font_cjk_noto_jp_16.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/fonts/muse_font_cjk_noto_jp_16.c)
（生成: [gen_cjk_noto_jp_font.sh](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/tools/muse/gen_cjk_noto_jp_font.sh)）、
IMU の設定 [msw_bmi270_config.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/boards/msw_bmi270_config.c)、
タスクのスタックを PSRAM に置く [msw_stack.h](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/msw_stack.h)。

## 公式ファイルの改造箇所（概要）

公式のファイルに入れた変更には、すべて `museStopWatch:` の印を付けています。
全体の差分: [公式 main との比較](https://github.com/facebookincubator/muse-gadget-sdk/compare/main...haiatto:muse-gadget-sdk:dev/stopwatch)

| ファイル | 改造の内容 |
|---|---|
| [main/app.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/app.c) | `app.*` と `voice.say` の処理（部品がないときは何もしない）、VM 認証タスクのスタック |
| [main/noise_control.cpp](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/noise_control.cpp) | `app.*` と `voice.say` の登録、登録 JSON の上限（StopWatch は 256 KB）、PSRAM のスタック |
| [main/image_fetch.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/image_fetch.c) | VM のファイルを丸ごと PSRAM に取る（`app.install` 用） |
| [main/muse_glue.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/muse_glue.c), [muse_link.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_link.c) | 保存済み Wi-Fi に切り替える（`muse_wifi_choose`） |
| [main/tunnel_netif.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/tunnel_netif.c) | タスクのスタックを PSRAM に |
| [components/muse/boards/board_m5stack_stopwatch.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/boards/board_m5stack_stopwatch.c) | タッチ（1 本指、レジスタの表示）、IMU、振動、表示まわりの調整 |
| [components/muse/muse_chat_session.cpp](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_chat_session.cpp) | サイドチャット、返事を本体の声で読む、返事の文をミニアプリに渡す |
| [components/muse/muse_voice.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_voice.c) | 本体の読み上げを音声タスクで鳴らす |
| [components/muse/muse_settings_ui.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_settings_ui.c) | 日本語の表示、QR 画面・サイドチャット画面・保存済み Wi-Fi の行（`ui/` の `.inc` を取り込む） |
| [components/muse/muse_ui.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_ui.c) | 文字盤の左にランチャーのタイル、日本語フォント |
| [components/muse/muse_input.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_input.c) | アプリ中のボタン（黄 = アプリメニュー、青 = アプリ）、`msw.*` コンソール |
| [components/muse/muse_audio.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_audio.c), [avatar/muse_pixel.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/avatar/muse_pixel.c) | 静的なバッファを PSRAM に |
| [components/muse/CMakeLists.txt](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/CMakeLists.txt), [Kconfig](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/Kconfig) | `ui` 部品とのつなぎ、日本語フォントの設定 |

## ビルドと書き込み（Windows）

ツールはワークスペースの中（`.venv/`、`.tools/`）に入れています。ワークスペース直下の PowerShell で:

```powershell
.\dev.ps1 build              # ファーム
.\dev.ps1 ports              # COM ポートの一覧
.\dev.ps1 flash -Port COM5   # 書き込み
.\dev.ps1 monitor -Port COM5 # シリアル監視
```

- `dev.ps1` は `EXTRA_COMPONENT_DIRS=components/muse_diy_stopwatch` を渡します。公式の `tools/muse/board.sh` で
  そのまま組み立てると、`muse_diy_stopwatch/` の部品が見つかりません。
- 読み上げのモデルと辞書は git に入れていません。入手先は
  [sanotts/model/README.md](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/sanotts/model/README.md) にあります。

## ミニアプリ

- Muse 向けの説明: 本体の `app.doc`（中身は [guide.md](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp/guide.md)）と `app.api`
- 組み立て: `python3 sdk/build.py apps/<名前>`（コンパイラは xtensa-esp-elf GCC 15.2）
- ルール: 既存の作品の素材やキャラは使わない。画面のタッチは 1 本指。終わり方は黄ボタンのアプリメニュー

## 作業の記録

- エージェント向けルール: [CLAUDE.md](CLAUDE.md)
- 現在の状況: [work/development-status.md](work/development-status.md)
- アイデア: [work/ideas.md](work/ideas.md)
- フォークの整理の検討（保留）: [work/sdk-fork-cleanup-2026-11-08.md](work/sdk-fork-cleanup-2026-11-08.md)

## ライセンス

フォークした SDK は Apache License 2.0、sanoTTS-jp のコードは MIT（モデルは別のライセンス）、
Noto Sans JP は SIL Open Font License 1.1 です。それぞれのフォルダのライセンスの記載に従います。
