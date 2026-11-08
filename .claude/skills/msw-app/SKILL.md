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

## コマンド（ワークスペース直下、Git Bash か PowerShell）

```
.venv/Scripts/python.exe tools/gen_msw_exports.py      # msw_app.h か exports.list を変えたあと
.\dev.ps1 build                                        # ファーム（上の行のあとは必要）
.venv/Scripts/python.exe tools/make_sdk.py             # ファームのビルドのあと: sdk/include を作り直す
.venv/Scripts/python.exe sdk/build.py apps/<名前>      # -> apps/<名前>/build/<id>.mswapp
.venv/Scripts/python.exe tools/msw_app_install.py COM5 apps/<名前>/build/<id>.mswapp --run
.venv/Scripts/python.exe tools/msw_app_install.py COM5 --cmd "msw.app.log"
```

ほかのコンソールコマンド: `msw.app.list`、`msw.app.run <id>`、`msw.app.stop`、
`msw.app.rm <id>`、`msw.imu`（IMU の生の値と画面の向きに直した値）、
`msw.tp <秒>`（タッチ IC のレジスタを表示）。

## 手順

1. アプリを書く。絵・名前・音楽はオリジナルだけを使う（既存の作品の素材やキャラは使わない）。
2. ビルドする。「the firmware does not give: X」と出たら、別の関数を使うか、
   X を `sdk/exports.list` に足して作り直し、ファームもビルドし直す
   （lvgl の inline 関数は書き出せないので、それが中で呼ぶ関数を書き出す）。
3. COM ポートを確かめ（`.\dev.ps1 ports`）、ユーザーに伝えてから入れて起動する。
4. 10 秒ほど動かして `msw.app.stop` し、`msw.app.log` を読む
   （1 秒あたりのティック数、ティックの時間、表示の時間、片付けたもれ）。
   落ちると本体が再起動し、次の起動時のログにアプリ名が出る。
5. 画面の見え方と音をユーザーに聞く。ログがきれいでも、実機で確かめたことにはならない。

## 実測（2026-11-08）

- 全画面の表示 26.7 ms（1 ピクセル 0.12 µs、QSPI 40 MHz）。PSRAM への描画は
  1 ピクセルあたり約 25 ns。重ねて全部描き直すと約 18〜20 fps。
- アプリの読み込み 1〜4 ms。アプリと画面バッファがあるときの PSRAM の空き 約 2 MB。

## わかったこと

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
