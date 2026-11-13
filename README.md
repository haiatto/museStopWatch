# museStopWatch

M5Stack StopWatch（ESP32-S3、1.75 インチ円形 AMOLED）で Muse を動かすための開発ワークスペースです。
ファームは公式の [Muse Gadgets SDK](https://github.com/facebookincubator/muse-gadget-sdk) をフォークして改造しています
（フォーク: [haiatto/muse-gadget-sdk の dev/stopwatch ブランチ](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch)）。

Muse アプリの一覧での名前は「Muse DIY StopWatch」、呼び名は「Museストップウォッチ」です。
ストップウォッチや時計の機能はありません。

## できること（公式に足したもの）

**日本語で話す**
- Muse の返事を、ファームの中で動く [sanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp) で読み上げます。字幕は日本語フォント（Noto Sans JP）。
- 返事を待っている間は、今の状態（考えている、ツールを使っている など）を字幕に出します。
- 青ボタンで話した声を Muse が正しく聞き取れるよう、日本語の音声認識の手順を Muse 向けのガイドに入れています。
- 会話が終わった後に届く知らせ（タイマーや調べものの結果）は、頭に `[to_stopwatch]` の印があれば、いつ届いても字幕と声で出します。

**Muse の声と音**
- `voice.play`: Muse が作った音声（MP3、長いものは HLS で少しずつ）で返事を流します。声の名前は画面に「CV: 名前」と出ます。
- `sound.play`: アラームや効果音を、声や返事の上に重ねて鳴らします。ボタンを押すと止まります。

**ミニアプリ**
- Muse が C で小さなアプリ（ゲーム、道具、おもちゃ）を作り、その場で入れて動かせます（`app.install`、`app.run` など）。
- 文字盤から右にスワイプするとランチャー。アプリ中は黄ボタンでアプリメニュー（つづける / おわる）。
  メニューを開いている間は、青ボタンで Muse と話せます（小さな Muse と吹き出しが出ます）。
- アプリから使えるもの: 画面（LVGL）、1 本指のタッチ、ボタン、音、マイク、傾きセンサー、振動、時刻、電池、歩数、PORT.A（Grove）。
- Muse が自分で確かめるための口: 画面の写真（`app.screenshot`）、ログと落ちたときの報告（`app.log`）、
  入っているアプリのソース（`app.source`）。
- アプリはフラッシュのアプリ置き場（約 4.5 MB）に入り、電源を切っても残ります。

**アバター**
- 文字盤の Muse の見た目を、ミニアプリと同じ作り方で差し替えられます。Muse が作って入れ、文字盤から上にスワイプして選びます。

**ウェイクワード（試作）**
- 「Hey Muse」と言うと話し始めます（設定 > Wake word。初期値はオフ）。言葉のモデルは自分で学習したものです。

**設定と Wi-Fi**
- スマホのブラウザから Wi-Fi と Muse を設定する Web 設定。Wi-Fi がないところでは AP モード。どちらも設定 > QR から開けます。
- 保存済み Wi-Fi の切り替え・削除。
- サイドチャット: 話す先の会話を切り替えます（設定 > Muse > Side chat）。Muse から会話を一覧に足すこともできます（`side.add`）。
- 設定画面の説明を日本語に。読み上げの声の大きさ（Voice level）。
- 時刻は RTC で保ち、Wi-Fi につながるとネットワークで合わせます（日本時間）。

**Muse 向けの説明**
- `stopwatch.doc`（総合ガイド）、`app.doc`（ミニアプリの作り方）、`app.avatar_doc`（アバターの作り方）、`app.api`（API）。

**ファームの更新**
- Muse に頼むと、このリポジトリの Releases から OTA で更新します。自分の鍵で署名したものだけを、Releases の URL からだけ受け付けます。

## このリポジトリの構成

| 場所 | 中身 |
|---|---|
| [muse-gadget-sdk/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch) | ファーム（公式 SDK のフォーク、サブモジュール） |
| [sdk/](sdk/) | ミニアプリの SDK（`build.py`、API ヘッダー、使える関数の一覧）。Muse が VM で使う |
| [apps/](apps/) | お手本のミニアプリ（おちものブロック、ぴょこの森、まるい水槽、みみ、センサー）とアバター（しずく） |
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
| [miniapp/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp) | ミニアプリの実行環境（PSRAM で ELF を動かす）、ランチャー、アプリメニュー、アプリ置き場、アバター、`app.*` コマンド、Muse 向けガイド [guide.md](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp/guide.md)、API [msw_app.h](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp/include/msw_app.h) |
| [sanotts/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/sanotts) | 日本語読み上げ。`upstream/` は sanoTTS-jp のコピー（変更しない）、`model/` は git 管理外 |
| [wakeword/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/wakeword) | ウェイクワード（microWakeWord、「Hey Muse」のモデル） |
| [docs/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/docs) | Muse 向けの総合ガイドと、日本語の音声認識の手順 |
| [ui/](https://github.com/haiatto/muse-gadget-sdk/tree/dev/stopwatch/esp32/components/muse_diy_stopwatch/ui) | AP モードと Web 設定、QR 画面、サイドチャット画面、保存済み Wi-Fi の行、ウェイクワードの設定、時刻、日本語の文字、Muse 向けガイドの配信、メモリ監視とコンソール |

ほかに足したファイル:
[main/msw_play.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/msw_play.c)（`voice.play` と `sound.play`）、
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
| [main/app.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/app.c) | `app.*`・`voice.play`・`sound.play` の処理（部品がないときは何もしない）、VM 認証タスクのスタック |
| [main/noise_control.cpp](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/noise_control.cpp) | `app.*`・`stopwatch.doc`・`side.*`・`voice.play`・`sound.play` の登録、登録 JSON の上限（StopWatch は 256 KB）、PSRAM のスタック |
| [main/image_fetch.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/image_fetch.c) | VM のファイルを丸ごと PSRAM に取る（`app.install` 用） |
| [main/muse_glue.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/muse_glue.c), [muse_link.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_link.c) | 保存済み Wi-Fi に切り替える（`muse_wifi_choose`） |
| [main/ota.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/ota.c), [main/Kconfig.projbuild](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/Kconfig.projbuild) | OTA の取得先の制限（`CONFIG_HOMEHUB_OTA_URL_PREFIX`） |
| [main/tunnel_netif.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/main/tunnel_netif.c) | タスクのスタックを PSRAM に |
| [components/muse/boards/board_m5stack_stopwatch.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/boards/board_m5stack_stopwatch.c) | タッチ（1 本指、レジスタの表示）、IMU、振動、表示まわりの調整 |
| [components/muse/muse_chat_session.cpp](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_chat_session.cpp) | サイドチャット、返事を sanoTTS-jp で読む、`[to_stopwatch]` の知らせ、返事の文をミニアプリに渡す |
| [components/muse/muse_voice.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_voice.c) | sanoTTS-jp の読み上げを音声タスクで鳴らす |
| [components/muse/muse_settings_ui.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_settings_ui.c) | 日本語の表示、QR 画面・サイドチャット画面・保存済み Wi-Fi の行（`ui/` の `.inc` を取り込む） |
| [components/muse/muse_ui.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_ui.c) | 文字盤の左にランチャーのタイル、日本語フォント |
| [components/muse/muse_input.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_input.c) | アプリ中のボタン（黄 = アプリメニュー、青 = アプリ）、`msw.*` コンソール |
| [components/muse/muse_audio.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse/muse_audio.c), [avatar/muse_pixel.c](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/avatar/muse_pixel.c) | 静的なバッファを PSRAM に、`sound.play` の音を重ねる口 |
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

## OTA と派生版

- Muse に `device.ota` を頼むと、[Releases](https://github.com/haiatto/museStopWatch/releases) の
  `muse-gadget.bin` でファームを更新します（取得先は Muse 向けの総合ガイド
  [stopwatch_guide.md](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/docs/stopwatch_guide.md) に書いてあります）。
- ファームは自分の鍵で署名しています（鍵は git に入れていません）。ファームは、今動いているファームと同じ鍵で
  署名したものしか OTA で受け付けません。鍵がなければ、SDK の共有のサンプル鍵で署名されます。
- 派生版を作るときは:
  1. 自分の鍵を作る（`espsecure generate-signing-key --version 2 --scheme rsa3072 .tools/keys/msw_signing_key.pem`）。
     最初の 1 回は USB で書き込む。
  2. `stopwatch_guide.md` の取得先と、`miniapp/guide.md` の SDK の clone 先を、自分のリポジトリに書き換える。
     ファームは `devices/sdkconfig.musestopwatch` の `CONFIG_HOMEHUB_OTA_URL_PREFIX` 以外の URL の OTA を断るので、
     ここも自分の Releases の URL にする。
  3. ガイドの 2 行目の版を上げる（Muse が読み直すように）。

## ミニアプリ

- Muse 向けの説明: `app.doc`（中身は [guide.md](https://github.com/haiatto/muse-gadget-sdk/blob/dev/stopwatch/esp32/components/muse_diy_stopwatch/miniapp/guide.md)）と `app.api`
- 組み立て: `python3 sdk/build.py apps/<名前>`（コンパイラは xtensa-esp-elf GCC 15.2）
- 画面のタッチは 1 本指。アプリの終わり方は黄ボタンのアプリメニュー

## 作業の記録

- エージェント向けルール: [CLAUDE.md](CLAUDE.md)
- 現在の状況: [work/development-status.md](work/development-status.md)
- アイデア: [work/ideas.md](work/ideas.md)
- フォークの整理の検討（保留）: [work/sdk-fork-cleanup-2026-11-08.md](work/sdk-fork-cleanup-2026-11-08.md)

## ライセンス

このリポジトリ（museStopWatch）で書いたもの（`apps/`、`tools/`、`sdk/` の自作部分、`dev.ps1` など）は
[MIT License](LICENSE) です。ほかの人のものは、それぞれのライセンスに従います。

| もの | ライセンス | 場所 |
|---|---|---|
| フォークした Muse SDK（ファーム。私たちの変更を含む） | Apache License 2.0 | `muse-gadget-sdk/LICENSE`。SDK が持つ部品はその README の一覧 |
| LVGL のヘッダー（ミニアプリ用 SDK に写したもの） | MIT | `sdk/include/lvgl/LICENCE.txt` |
| `sdk/include/muse_speaking.h` | Apache License 2.0（Meta） | ファイルの先頭 |
| sanoTTS-jp のコード | MIT | `…/sanotts/upstream/LICENSE` |
| Open JTalk（sanoTTS-jp が使う部分） | 修正 BSD | `…/sanotts/upstream/openjtalk/COPYING`、`…/sanotts/model/NOTICE-openjtalk.txt` |
| sanoTTS-jp の辞書（NAIST-JDIC から） | BSD 型 | `…/sanotts/model/NOTICE-dictionary.txt` |
| sanoTTS-jp のモデル（声の重み） | sanoTTS-jp Model License 1.0 | `…/sanotts/upstream/LICENSE-MODEL.md` |
| ウェイクワードの VAD（ESPHome micro_wake_word） | Apache License 2.0 | `…/wakeword/models/LICENSE`、`NOTICE` |
| ウェイクワード「Hey Muse」（私たちが学習） | 下の注意 | `…/wakeword/models/README.md` |
| Noto Sans JP（日本語フォント） | SIL Open Font License 1.1 | `muse-gadget-sdk/esp32/components/muse/fonts/README.md` |

`…` は `muse-gadget-sdk/esp32/components/muse_diy_stopwatch`。

- **sanoTTS-jp のモデルと辞書は、このリポジトリに入れていません。** ビルドする人が
  [sanoTTS-jp の Releases](https://github.com/ayutaz/sanoTTS-jp/releases) から自分で取ってきます。
  モデルには、帰属表示と、作った声の使い道の制限（人を批判・攻撃することに使わない、など）があります。
  使う前に `LICENSE-MODEL.md` を読んでください。モデルの入ったファイル一式を配るときは、その §3 の表示と制限を付けます。
- **「Hey Muse」のモデル**は、学習の背景音に非商用ライセンスのデータを一部使っています（microWakeWord の学習手順の注意書き）。
  その条件が学習したモデルに及ぶかは、はっきりしていません。商用で使う前にデータのライセンスを確かめるか、
  自分で学習し直してください。
- Releases の `muse-gadget.bin` はアプリの部分だけです。モデル・辞書・フォントは入っていません。
