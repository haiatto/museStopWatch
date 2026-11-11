---
name: msw-app
description: museStopWatch のミニアプリ（M5Stack StopWatch の PSRAM で動く ELF アプリ）を、このワークスペースで作る・ビルドする・実機に入れる・測る。apps/ のアプリを書く・直すとき、またはミニアプリの実行環境（components/muse_diy_stopwatch/miniapp）と SDK を変えるときに使う。
---

# museStopWatch のミニアプリ（このワークスペース）

Muse は同じ知識を、ファームに入っているガイド
（`muse-gadget-sdk/esp32/components/muse_diy_stopwatch/miniapp/guide.md`、`app.doc` で読める）から得る。
まずそれを読む（API の要約、うまくいく作り方、落とし穴、ルール）。
ガイドとこのスキルは揃えておく。ここでわかったことはガイドにも書く。

## どこに何があるか

- API のヘッダー: `muse-gadget-sdk/esp32/components/muse_diy_stopwatch/miniapp/include/msw_app.h`
- 実行環境: `components/muse_diy_stopwatch/miniapp/msw_app_*.c`（一覧は `msw_app_priv.h`）
- msw_* 以外にアプリが呼べる関数: `sdk/exports.list`
- アプリ: `apps/<名前>/app.json` と `*.c`（blocks、pyoko、aquarium がお手本）
- アバター（009）: `apps/shizuku`（`"kind": "avatar"`）。実行側は `msw_avatar.c`、描画のフックは
  `avatar/muse_pixel.c` の `msw_avatar_hook_render`。シリアルは `>msw.app.avatars`、`>msw.app.avatar <id|official>`、
  画面を起こすのは `>msw.wake`。状態ごとの絵は `>msw.app.avatar_preview <id> <state> [コマ数]`（Muse は
  `app.avatar_preview`。JPEG の base64 行）。Muse 向けの作り方は `miniapp/avatar_guide.md`（`app.avatar_doc`）で、
  公式の `tools/muse/avatar_prompt.md` を元にしている。`build.py` は PC の C コンパイラがあれば
  `sdk/avatar_host.c` で全状態をサニタイザー付きで動かし `build/preview/*.png` を作る（`MSW_HOST`、lvgl なし）。1 フレーム 5 ms 以内（`>msw.app.avatars` に平均）。浮動小数の割り算はソフト
  （`__divsf3`）なので、ループの中では逆数を掛ける。`fmodf` も重い。

## コマンド（ワークスペース直下、Git Bash か PowerShell）

```
.venv/Scripts/python.exe tools/gen_msw_exports.py      # msw_app.h か exports.list を変えたあと
.\dev.ps1 build                                        # ファーム（上の行のあとは必要）
.venv/Scripts/python.exe tools/make_sdk.py             # ファームのビルドのあと: sdk/include を作り直す
.venv/Scripts/python.exe sdk/build.py apps/<名前>      # -> apps/<名前>/build/<id>.mswapp
.venv/Scripts/python.exe tools/msw_app_install.py COM5 apps/<名前>/build/<id>.mswapp --run
.venv/Scripts/python.exe tools/msw_app_install.py COM5 --cmd "msw.app.log"
```

ほかのコンソールコマンド: `msw.app.list`（置き場の空きといちばん大きいすき間も出す。アプリ 48 個・置き場 4.5 MB まで、1 つの上限なし）、
`msw.app.compact`（置き場を詰め直す）、`msw.app.run <id>`、`msw.app.stop`、
`msw.app.rm <id>`、`msw.app.src <id>`（パッケージに入っているソース。Muse は `app.source`）、
`msw.app.shot`（今の画面の JPEG を base64 の行で。既定は Muse と同じ半分の大きさ、`msw.app.shot full` で 466 x 466。Muse は `app.screenshot`、`size: "full"` で全体）、`msw.caption <文字>`（字幕を出す。`\n` で改行。アバター画面やメニューの吹き出しの見え方の確認に）、`msw.imu`（IMU の生の値と画面の向きに直した値）、
`msw.tp <秒>`（タッチ IC のレジスタを表示）。

## 手順

1. アプリを書く。
2. ビルドする。「the firmware does not give: X」と出たら、別の関数を使うか、
   X を `sdk/exports.list` に足して作り直し、ファームもビルドし直す
   （lvgl の inline 関数は書き出せないので、それが中で呼ぶ関数を書き出す）。
3. COM ポートを確かめ（`.\dev.ps1 ports`）、ユーザーに伝えてから入れて起動する。
4. 10 秒ほど動かして `msw.app.stop` し、`msw.app.log` を読む
   （1 秒あたりのティック数、ティックの時間、表示の時間、片付けたもれ）。
   落ちると本体が再起動し、次の起動時の app.log に `CRASH:` の報告が出る（原因、ヒント、
   バックトレース。アプリの行は `ELF 0x...` なので `addr2line -pfiaC -e apps/<名前>/build/<id>.elf 0x...`）。
   app_main やコールバックから戻らないアプリは、`msw.app.stop`（または黄ボタンの長押し）から 5 秒で
   本体が自分で再起動して止め、app.log に `STUCK:` の報告（止めたときの場所のバックトレース）が出る。
   コードは画面のロックを持ったまま動くので、タスクだけを消すことはできない。
5. 画面の見え方と音をユーザーに聞く。ログがきれいでも、実機で確かめたことにはならない。

## 実測（2026-11-08）

- 全画面の表示 26.7 ms（1 ピクセル 0.12 µs、QSPI 40 MHz）。PSRAM への描画は
  1 ピクセルあたり約 25 ns。重ねて全部描き直すと約 18〜20 fps。
- アプリの読み込み 1〜4 ms。アプリと画面バッファがあるときの PSRAM の空き 約 2 MB。

## わかったこと

- elf_loader は `.text` の大きさを 4 バイトに切り上げて区間を決める。`.text` が奇数の長さで、
  後ろの `.rodata` が文字列だけ（1 バイト揃え）だと、最初の文字列がコード（I バス）側に再配置され、
  バイトで読むと LoadStoreError（2026-11-08、dance）。build.py が `.rodata` を 4 バイトに揃え、
  配置を確かめる。古いアプリは msw_api_readable が .rodata のコピーに読み替える（lvgl に渡すと落ちる）。
- 大きな仕事（lv_snapshot、JPEG）は呼び出し元のスタックで動かさない。シリアルのタスクは 3.5 KB で、
  lv_snapshot がスタックをあふれさせた。PSRAM スタックの専用タスク（msw_task_create）で動かす。

- パッケージにはソースが入る（2026-11-08、ユーザーの案）。2026-11-11 から ELF の後ろは
  「4 文字のタグ + 長さ」の並び（msw_app_priv.h）: "MSWD"（`data/` のファイル）、"MSWZ"（ソースを zlib で。
  本体は ROM の tinfl で展開）、"MSWS"（前の形の無圧縮ソース。読める）。`--no-source` でソースを入れない。
  Muse が作ったアプリで落ちたら、`msw.app.src <id>` でソースを取って、手元で組み立てて調べる。
- パッケージの置き場（2026-11-11、ユーザー決定）: FAT をやめ、`msw_pkgs` パーティション（4.5 MB）に
  1 つずつひと続きに置く（msw_app_pkgs.c）。パーティション全体を一度マップし、ELF はそこから PSRAM に
  読み込み、`data/` のファイル（`msw_app_data`）はフラッシュのまま読む。すき間は入れるときに詰め直す
  （動いているアプリは動かさない）。詰め直しは 4 KB ずつ写してジャーナルに印を付けるので、途中で
  リセットしても次の起動で続きから終わる（実機で確かめた）。保存データ（msw_store_put）だけ小さい FAT（512 KB、/apps）。
  起動の確認（CRC 込み）は 4.2 MB で 0.3 秒。パーティション表を変えたので、OTA では入らない（USB）。
- シリアルの `msw.app.put/data/end` は、受け取りながらフラッシュに書く（大きさの上限なし）。
  tools/msw_app_install.py は返事が来ないとき `msw.app.putpos` で続きの位置を聞いて送り直す
  （2026-11-11、2.3 MB の途中で返事が 10 秒来ないことが 2 回あった。原因は不明）。

- 実行環境（ファーム側）の変数は内蔵 RAM に置かない（CLAUDE.md のルール）。2026-11-08、
  音のリング `s_pcm`（16 KB）とアプリ一覧のコピー 3 つ（3 KB ずつ）が内蔵にあり、
  内蔵 RAM の空きが 74K → 39K に減っていた。`EXT_RAM_BSS_ATTR` で PSRAM に移した。
  調べ方: `esp_idf_size --archives <map>` の DIRAM .bss の列、`--archive-details libminiapp.a`。

- タッチは 1 本指（ルール、ユーザー 2026-11-08）。CST820 は 2 本まで報告する
  （0x02 に本数、0x03 から 1 本 6 バイト。`msw.tp <秒>` で生のレジスタを見られる）。
  msw_touches も試したが、自己容量式なので、矢印を押したままジャンプをタップすると
  2 → 0 → 1 となって、押したままの指を見失うことが多かった（手元のログ
  work/logs-2026-11-07/tpdump3.log、git には入れていない）。msw_touches は削除した。
  アプリは 2 本同時のタッチを前提にしない。2 つ目の操作は青ボタンに割り当てる。
  上スライドでジャンプも試したが、押している指はずれるので誤動作し、やめた。
- ボタン: 黄 = 左上（アプリの外ではスピーカー）、青 = 右上（アプリの外では話す）。
  アプリ中は、黄で実行環境のアプリメニュー（つづける / おわる、msw_app_menu.c）が開き、
  ティックが止まる。青はアプリのもの（MSW_BUTTON_BLUE）で、Muse には送らない。
  ただしメニューが開いている間は、青は Muse の「話す」になる（006、2026-11-08）。
  メニューには本体と同じ Muse（muse_pixel を msw_app_menu.c のデコーダーで 96px に縮小、
  絵のバッファは持たない）と吹き出し（muse_state の字幕）が出る。
  返事を待つ間は、Muse の状態（agent.status を日本語にした1行、`muse_state_activity`、アイデア 008）が字幕に入る。
  会話の後も Muse がこのチャットで作業していれば、待機中の字幕とメニューの吹き出しに出る（online で消える）。
  ランチャーの長押し → ✕ は確認（やめる・けす）を出してから消す。
  シリアルで `>msw.app.menu` が黄ボタン 1 回押しの代わり（開く・閉じる）。
  `>msw.app.shot` は lvgl のトップレイヤー（メニュー）も重ねて撮る。
- 撮影（msw_app_shot.c）は 16 行ずつ作って esp_new_jpeg のブロックモードへ（画面全体のバッファを持たない。
  要るのは帯 15 KB×2 と JPEG 128 KB）。ブロックモードは 466 行で 29 ブロック目に JPEG_ERR_OK を返して
  画像が完成する。その後に送ると新しい画像が始まる（黒い画像になった、2026-11-08）。下の 2 行は捨てられる。
  既定は半分の大きさ（232 x 232、2x2 の平均、画質 40）。`app.screenshot` の結果は Muse に文字（base64）として
  届くので、小さくして読む量を減らす（2026-11-08。Muse が「文字として届いている」と言った）。`size: "full"` で 466 x 466。
  ユーザーのルール（2026-11-08）: これを標準とする。ほかのやり方にするアプリ
  （msw_menu_own）は、終わり方をユーザーに案内する。
- 画面のパッド: 円の左右の中ほどは、親指で画面が隠れすぎた。斜め下の左右、
  円周に沿う所に置くのを試している（2026-11-08、apps/blocks）。
- ぴょこの森の移動（試作、2026-11-08）: 左下の縁にスライド式のパッド
  （触れた所が中心、40 px で最高速、1.5 乗の曲線）。10 時の位置に「傾き」ボタン
  （押している間だけ傾きで移動。押した瞬間の傾きが水平、0.15 g（約 9°）で最高速、
  1.8 乗の曲線、デッドゾーンなし）。ジャンプは青ボタン。右上の縁に青い↑の印と、
  タイトルに操作の案内を出す。ユーザーの感想: デッドゾーンは不要で、傾きはそのまま
  反映し、非線形の曲線がよい。
- マイク（2026-11-10、apps/mimi）: msw_mic_on / msw_mic_read / msw_mic_level。音声タスクが 20 ms ごとに読む音を
  msw_app_mic.c のリング（PSRAM 1 秒）に分ける。待機中のほか、音を鳴らしている間も読む（muse_voice.c の play_player）。
  Muse と話している間・返事の間・休止中は来ない（level -1）。実測: 毎秒 16000 サンプル、静かな部屋で振幅 50〜70（level 0）。
  半分サイズの画面写真（shot）で、画面下の文字が二重に写ることがあった。原寸（shot full）では正しい。
- 時刻・電池・歩数・PORT.A（2026-11-10、apps/sensors）: msw_time / msw_battery / msw_steps / msw_port_power /
  msw_i2c_*。時刻は ui/msw_time.c（起動時に RTC から、Wi-Fi の IP 取得で SNTP、合わせたら RTC へ UTC で保存、JST-9）。
  RTC・5V・歩数はボードの msw_board_rtc_get/set・msw_board_port_power・msw_board_imu_steps。PORT.A の I2C はアプリ用の
  別バス（i2c_port -1、SDA G10・SCL G11、内蔵プルアップ）。歩数計は BMI270 の機能ページ 6 で有効にする（Bosch の SensorAPI どおり）。
