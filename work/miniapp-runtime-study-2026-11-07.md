# ミニアプリの実行方式の比較（アイデア 001）— 2026-11-07

## 前提（ユーザーの条件）

- 方式の候補は「スクリプト言語」か「動的ライブラリとしてのバイナリ」（ideas.md 001）。Lua は使わない。
- アプリは複数入れたり消したりでき、ランチャーで選んで起動できる。
- 描画だけでなく、本体の機能（ボタン・タッチ・音・振動・IMU・保存など）を使える。
- Muse は重要だが必須ではない。Muse なしでもアプリは動く。
  ミニアバター＋吹き出しが画面に常駐できると便利（必須ではない）。

## 本体の制約（実測・確認済み）

- ESP32-S3R8、CPU 240MHz × 2、PSRAM 8MB（Octal 80MHz）、フラッシュ 16MB。
- 命令と読み取り専用データは PSRAM から実行している（`SPIRAM_FETCH_INSTRUCTIONS` / `RODATA`）。
  → 読み込んだコードを PSRAM に置いて実行する形と相性がよい。
- 内部 RAM の空き約 74KB（最大の塊 48KB）。アプリのヒープ・コードは PSRAM に置くしかない。
- PSRAM の空き: 起動直後 約 2.5MB、Sano のモデルを写した後 約 1.6MB。
- フラッシュの空き: 0x9F0000〜0x1000000（約 6MB）。アプリの保存場所にできる。
- PSRAM スタックのタスクからフラッシュの書き込み・mmap はできない（2026-11-07 に実際に落ちた）。
- ESP-IDF 6.0.3。

## 候補

### A. 動的ライブラリとしてのバイナリ（ELF）— espressif/elf_loader

- Espressif 公式の部品（esp-iot-solution の `elf_loader`、registry では `espressif/elf_loader` 2.x）。
  ESP32-S3 対応。**ELF を PSRAM に読み込んで実行できる**（`ELF_LOADER_LOAD_PSRAM`）。
- 本体側の関数は `ESP_ELFSYM_EXPORT(name)` で表に登録し、アプリはその関数だけを呼べる。
  libc の表も用意されている。表に出さない関数はアプリから呼べない（＝API の範囲を本体が決める）。
- アプリの作り方: CMake の `project_elf()` で `xxx.app.elf` を作る。
  → **Muse の VM に Xtensa の GCC（ESP-IDF のツールチェーン）と、本体の API のヘッダが要る。**
- 長所: 速い（出荷時デモの流体シミュレーションのような重い処理も書ける）。実行環境が小さい。
  lvgl や本体の API をそのまま出せるので、描画以外もすべて同じ C の API で書ける。
- 短所: アプリのバグで本体ごと落ちうる（メモリ保護がない）。
  対策: 落ちた後の起動ではアプリを自動起動しない、ウォッチドッグ、アプリ用タスクに分ける。
  C++ の例外・RTTI は使えない。`.so` の静的コンストラクタは動かない。
- 未確認: IDF 6.0.3 で動くか。Muse の VM にツールチェーンを入れられるか（容量・ネットワーク）。

### B. スクリプト言語 — MicroPython

- LLM が最も書き慣れている言語の一つ。マイコン向けの資料・例が多い。
- 組み込み方は 2 通り:
  1. 公式の esp32 ポート（MicroPython 自体がファームになる形）→ 今のファームと同居できないので不可。
  2. embed ポート（C ファイル一式を生成して自分のアプリに入れる）。公式の想定は「独自の
     バインディングで組み込む」用途。esp32 ポートの機能（machine、ファイルシステム等）は付かないので、
     本体の API は自分で書く。コミュニティの IDF 部品（micropython-helper 1.22）は古い。
- 公式 esp32 ポートが対応する IDF は 5.x まで。**IDF 6.0.3 での実績は見つからない**（embed なら IDF に
  依存しにくいが未確認）。
- 更新状況（2026-11-07 に GitHub で確認）: v1.26.0（8月）、v1.27.0（12月）、v1.28.0（4月）、v1.29.0（8月24日）と
  年 3 回ほどのリリースが続いている。esp32 ポートの推奨 IDF は v5.5.5（v5.3〜v5.5.x 対応）、README に IDF 6 の記述はない。
  embed ポート（`ports/embed`）は今も本体リポジトリにあり、`mpconfigport.h` を用意して `embed.mk` で
  .c/.h 一式を生成する形。例は `examples/embedding`。
- 長所: 例外で止まるので本体は落ちにくい（サンドボックスに近い）。転送するのはソースだけで、
  Muse の VM にコンパイラが要らない。
- 短所: 遅い（インタプリタ）。重い描画・シミュレーションは苦しい。GC ヒープ（数百 KB）を PSRAM に取る。
  本体の API のバインディングを全部書く必要がある。
- 補足: MicroPython には「動的ネイティブモジュール」（.mpy に機械語を入れる、`xtensawin` 対応）があり、
  重い部分だけバイナリにできる。ただし呼べるのは `mp_fun_table` の関数だけ。

### C. スクリプト言語 — JavaScript（Moddable XS の mods）

- マイコン用の JS エンジン。「mods」はファームを変えずに後から入れるスクリプトで、
  専用のフラッシュ領域に置き、`Compartment` で閉じ込められる。
- ただし Moddable SDK 全体を土台にする作りで、今のファーム（Muse SDK）に組み込むのは大きな作業。
  公式資料では mod の領域は 1 つ（256KB）で、複数インストールの扱いは書かれていない。
  → 候補としては弱い。

### D. バイナリ（サンドボックス付き）— WebAssembly（WAMR）

- Espressif の registry に `wasm-micro-runtime` があり、ESP32-S3 対応。AOT（事前コンパイル）で速くなる。
- アプリは閉じ込められる（本体は落ちない）。ただし AOT の Xtensa での実績・速度は資料が少ない。
  Muse の VM に wasi-sdk（と AOT コンパイラ）が要る。API は wasm 用に包み直す必要がある。

## 比較（◎ よい / ○ / △ / × 悪い）

| 観点 | A. ELF | B. MicroPython | C. JS (XS) | D. WASM |
|---|---|---|---|---|
| 速さ（重い描画・計算） | ◎ ネイティブ | △ | △ | ○（AOT）|
| 本体の安全（アプリが落ちても本体は無事） | △ 対策が要る | ◎ | ◎ | ◎ |
| 描画以外も含む API の出しやすさ | ◎ C の関数をそのまま | △ 全部包む | △ | △ 全部包む |
| 組み込みの手間（IDF 6.0.3 で） | ○ 公式部品 | △ embed、実績なし | × | ○ |
| Muse の VM に要るもの | Xtensa GCC | なし | なし（事前コンパイルなら xsc）| wasi-sdk |
| Muse が書きやすい言語 | ○ C | ◎ Python | ◎ JS | ○ C |
| 実行環境の大きさ | ◎ 小 | △ 約 300〜400KB | △ | ○ |
| 複数アプリの入れ替え | ◎ ファイル単位 | ◎ | △ | ◎ |

## ランチャーと常駐アバター（どの方式でも同じ部分）

- 本体側（「OS」）が持つもの: アプリの一覧と保存（フラッシュの空き領域）、ランチャー画面、
  アプリの起動・終了、アプリ用タスク、API の表、エラーの記録。
- アプリは lvgl の自分の画面（または全画面）を持つ。ミニアバター＋吹き出しは lvgl の最上層
  （`lv_layer_top`）に置けば、どのアプリの上にも出せる。Muse がいないときは出さない、
  または通知だけにする。
- Muse から: アプリの作成・インストール・削除・起動をコマンドで。Muse がいなくても、
  ランチャーから入っているアプリを起動できる。

## 今の考え（推奨）

- **A（ELF）を主にする**のがこの本体に合っていると考える。理由:
  - PSRAM からの実行がもともとの構成で、公式部品がそれに対応している。
  - 描画以外の機能も、本体の C の API を表に出すだけで使える。
  - 出荷時デモ級の重いアプリが書ける。
- 安全面は、アプリ用タスク・ウォッチドッグ・「落ちたら次の起動で自動起動しない」で補う。
- B（MicroPython）は「手軽なアプリ用」として後から足せる（ideas.md の「両対応」）。
  ただし IDF 6.0.3 での組み込みは要確認。

## 次に確かめること（小さな試作）

1. `espressif/elf_loader` を IDF 6.0.3 のこのファームに入れてビルドできるか。
2. 「画面に四角を描いて、ボタンで終わる」だけの ELF を作り、フラッシュから PSRAM に読み込んで動かす。
   読み込み時間・使うメモリを測る。
3. Muse の VM で Xtensa の GCC が動くか（ユーザーから Muse に `uname -a` やディスク容量を聞いてもらう、
   またはツールチェーンの取得を試してもらう）。

## 実験: ミニアプリ 1 本の ELF ビルド時間（2026-11-07、この Windows PC）

- 使ったもの: ファームと同じ GCC（`xtensa-esp32s3-elf-gcc` esp-15.2.0_20251204）。
  インクルードは lvgl・`sdkconfig.h`・IDF の `esp_common/include` の 3 か所だけ（IDF のビルドシステムは使わない）。
- アプリ: ポモドーロ（lvgl のアーク・ラベル・ボタン）＋休憩中の粒子シミュレーション（96 粒、IMU）、約 200 行。
  本体 API のヘッダは仮のもの（`msw_app.h`、スクラッチ）。
- オプション: `-mlongcalls -O2 -ffunction-sections -fdata-sections -fPIC`、リンクは `-nostdlib -shared -e app_main`。
- 結果（5 回ずつ）: **コンパイル 0.16 秒、リンク 0.04 秒**（前処理だけで 0.10 秒、展開後 7,883 行。ほぼ lvgl のヘッダ）。
  初回（キャッシュが冷えた状態）は 1.6 秒ほど。
- できた ELF: コード 5,685B、データ 164B、BSS 1,964B（ファイル 9.4KB）。
- アプリが本体に求めた関数: lvgl 27 個、仮 API（msw_*）11 個、libc/libm 3 個（snprintf、sqrtf、__divsf3）。
  → 本体はこれらを `ESP_ELFSYM_EXPORT` で表に出す必要がある。
- 未確認: 本体で実際に読み込んで動かすこと（elf_loader をまだ入れていない）。Muse の VM（Linux）での時間。

## 出典

- elf_loader: https://github.com/espressif/esp-iot-solution/tree/master/components/elf_loader
- MicroPython embed ポート: https://github.com/micropython/micropython/pull/9529
- MicroPython 動的ネイティブモジュール: https://docs.micropython.org/en/latest/develop/natmod.html
- micropython-helper（IDF 部品）: https://components.espressif.com/components/mr9you/micropython-helper
- Moddable mods: https://github.com/Moddable-OpenSource/moddable/blob/public/documentation/xs/mods.md
- WAMR（IDF 部品）: https://components.espressif.com/components/espressif/wasm-micro-runtime/versions/2.4.0~1/examples/esp-idf
