# SanoTTS（sanoTTS-jp）を StopWatch に入れてベンチ — 2026-11-07

## 結論

- **採用する設定**: モデルを PSRAM にコピー、アリーナ（推論の作業領域）は PSRAM、TTS タスクは **コア 0・優先度 4**。
  - この設定で定常 xRT **約 0.52〜0.54**（再生の約 2 倍の速さで作れる）。画面（lvgl）の CPU 時間は減らない。
- **コア 1 で優先度 6 にすると最速（xRT 0.456）だが、画面が止まる**（lvgl の CPU が 28% → 1.2%）。使わない。
- **コア 1 で優先度 4 だと遅い（xRT 0.75〜0.77）**。コア 1 は画面描画（lvgl 優先度 5、常に約 28%）が使っている。
- モデルをフラッシュから直接読む（mmap）より、PSRAM にコピーした方が速い（コア 0: 0.64 → 0.52）。
  代わりに PSRAM を 768KB 使う（初回の読み上げ時にコピー、以後置きっぱなし）。
- **アリーナを内部 RAM に置く構成（公式の最速構成）は今は無理**。必要 121,872B に対し、内部 RAM の最大の塊は約 30KB。
  それでも PSRAM 置き + 64KB D-cache で、公式の CoreS3 の値（xRT 0.473、内部 DRAM 置き）に近い。
- 内部 RAM の増加は静的に約 4.9KB（Open JTalk の数字読み表 2.7KB、erf 表 1KB ほか）。実行時の確保は全部 PSRAM。
  そのあとアバター・音声の静的バッファを PSRAM に移して内部の空きは 74KB（最大の塊 48KB）。それでもアリーナ（122KB）は入らない。
  移した後も xRT 0.530〜0.535（コア 0・優先度 4）で変わらない。

## 条件

- M5Stack StopWatch（ESP32-S3R8、CPU 240MHz、Octal PSRAM 80MHz、D-cache 64KB/64B 行、QIO フラッシュ 80MHz）。
- ESP-IDF 6.0.3。命令と読み取り専用データは PSRAM（`SPIRAM_FETCH_INSTRUCTIONS` / `SPIRAM_RODATA`）。
- sanoTTS-jp f05d7ae（2026-11-02）の C ソースをそのままコピー（`tools/sync_sanotts.py`）。
  漢字対応（SAAN_KANJI）、W8A8 + PIE（上流の ESP32-S3 既定）、Open JTalk の一時ヒープは PSRAM。
- モデル: v1.2.0 `voice-v4-int8.bin` 654,032B（SHA256 a1eb6b08…）。
  辞書: `k1-dict-44000-2mb.bin` 977,456B（kantanMuse と同じもの）。
- Muse・Wi-Fi 接続中、画面は待ち受け表示。会話はしていない。音は出さない（`>msw.tts` は合成だけ）。
- 文: 「これは音声合成と日本語の吹き出しを同時に動かす確認です。」（漢字経路、147 ids、15 形態素、音声 3.529 秒）。
  kantanMuse の cache64 実験と同じ文。出力のチェックサムは各回で一致。
- 各設定 3〜4 回。xRT は上流 `esp32/main/main.c` と同じ定義（最初の pull を除いた満チャンク pull の中央値 ÷ 92.9ms）。

## 結果

| モデル | コア | 優先度 | 定常 xRT | p90 | 全体 xRT | G2P | 初回 pull |
|---|---|---|---|---|---|---|---|
| flash (mmap) | 1 | 4 | 0.854〜0.875 | 0.93〜1.01 | 0.94〜0.97 | 72ms | 350〜396ms |
| PSRAM | 1 | 4 | 0.762〜0.767 | 0.81〜0.84 | 0.80〜0.82 | 63〜77ms | 320ms |
| PSRAM | 0 | 4 | 0.519〜0.544 | 0.57〜0.61 | 0.58〜0.60 | 50ms | 220〜233ms |
| flash (mmap) | 0 | 4 | 0.627〜0.650 | 0.69〜0.71 | 0.70 | 50ms | 278ms |
| PSRAM | 0 | 2 | 0.546〜0.570 | 0.61 | 0.61 | 52ms | 234ms |
| PSRAM | 0 | 6 | 0.512〜0.524 | 0.55 | 0.56 | 48ms | 220ms |
| PSRAM | any | 4 | 0.537〜0.541 | 0.58 | 0.58 | 50ms | 226ms |
| PSRAM | 1 | 6 | 0.456〜0.462 | 0.50 | 0.52 | 45ms | 202ms |

合成中の CPU（`>msw.cpu 6`、1 コアに対する %）:

| TTS の置き場所 | lvgl（画面） | swdraw | lcd_send | IDLE0 | IDLE1 |
|---|---|---|---|---|---|
| 待ち受け（TTS なし） | 27.9 | 4.4 | 3.8 | 69.3 | 61.9 |
| コア 0・優先度 4 | 35.0 | 6.8 | 4.7 | 1.3 | — |
| コア 1・優先度 6 | **1.2** | 0.5 | 0.3 | 94.2 | 0.9 |
| コア 1・優先度 4 | 28.2 | 4.9 | 3.7 | 89.3 | 1.3 |

- 待ち受けでも lvgl がコア 1 の約 28% を使い続けている（画面に変化がなくても描き直している可能性。未調査、アイデア 002）。
- メモリ: TTS タスクのスタック 12,288B（PSRAM）のうち使用は約 7.7KB。アリーナ 121,872B のうちピーク 113,808B。
  PSRAM の空きは約 2.5MB（モデルのコピー後 1.7MB）。

## 読み上げ（スピーカー）

- `>msw.say <文>`（シリアル）または Muse のコマンド `voice.say`（text）で読み上げる。`voice.stop` / `>msw.say.stop` で止める。
- 文末（。！？!? 改行）で区切って 1 文ずつ合成し、作りながら再生する。1 文が長すぎる（350 ids 超・形態素数超過）と
  「、」の位置（なければ真ん中）で分けて読む。モデルが受け付けない文字は 1 文字ずつ外して読む。
- モデルは 22,050Hz、本体の再生は 16kHz（返事と同じ）。窓付き sinc（24 タップ、7.2kHz 低域通過）で変換し、
  PSRAM のリング（6 秒分）に入れる。0.3 秒たまったら声のタスク（`muse_voice.c`）が待ち受け中に再生する。
  ボタンを押すと止まり、押した操作はそのまま普段の会話になる。
- 夜間のため **音は出していない**（`>msw.tts.cfg quiet=1` で同じ長さの無音を再生して経路だけ確認）。
  確認できたこと: 5 文・約 19.6 秒の文章を最後まで再生、長い文は自動で 2 つに分割、
  「Muse」「StopWatch」「2026年11月7日」も合成できた（外した文字 0）、サンプル数の比は 0.7256（320/441 どおり）、
  最初の音まで約 0.45 秒（G2P + 初期化 + 最初の pull + 0.3 秒分）。
- **未確認**: 実際の音質・音量・聞き取りやすさ（ユーザーに聞いてもらう）。Muse から `voice.say` を呼んだ動作。
  通話中（StopWatch の既定は押して話す通話）には待ち受けにならないので読み上げが始まらず、30 秒で捨てる。

## 分かった注意点

- **`esp_partition_mmap()` は PSRAM スタックのタスクから呼べない**（キャッシュを凍結し、IDF が呼び出し元の
  スタックが内部 RAM かを assert する: `esp_cache_utils.c`）。最初の版はここで落ちた。
  モデル・辞書の mmap は内部スタックの短いタスクで 1 回だけ行う。`esp_ota_get_state_partition` などの OTA 状態の
  読み出しも内部で mmap する。PSRAM に移したタスク（lvgl、muse_input、muse_keep、noise_ctrl、tun_tx、muse_serial、
  httpd、muse_vm、muse_chat、muse_voice）はどれも mmap も OTA 状態の読み出しもしない（コードで確認）。
  OTA は `ota` / `ota_verify` / `bug_report` / `main` の内部スタックのタスクで行われる。
- 重いタスクは画面描画と別のコアで、画面より低い優先度にする。

## 作ったもの

- `components/sanotts/`（muse-gadget-sdk/esp32 内）
  - `upstream/`: sanoTTS-jp のソースのコピー（変更しない。`tools/sync_sanotts.py` で更新、`upstream/SOURCE.txt` に版）。
  - `model/`: モデルと辞書（v1.2.0、kantanMuse から）。**モデルは MIT ではない**（`upstream/LICENSE-MODEL.md`）。
  - `msw_tts.c`: 読み上げ・ベンチ・シリアルコマンド。
- `partitions_musestopwatch.csv`: 既存の表の後ろに `sano_model`（0x830000、768KB）と `sano_dict`（0x8F0000、1MB）。
  `idf.py flash`（`.\dev.ps1 flash`）が一緒に書く。アプリ領域・NVS は変えていない。
- `muse_voice.c/h`: ほかの音源を待ち受け中に再生する口（`muse_voice_set_player`）。
- `main/noise_control.cpp`・`main/app.c`: Muse 向けコマンド `voice.say`・`voice.stop`（TTS が入っているときだけ登録）。
- `msw_mem.c`: `>msw.cpu [秒]`（タスクごとの CPU、`CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`）。
- ログ: `work/logs-2026-11-07/`。

## ベンチのやり方

```
.venv\Scripts\python.exe tools\msw_serial.py COM5 --cmd ">msw.tts.cfg model=psram core=0 prio=4 rep=3" --cmd ">msw.tts これは音声合成と日本語の吹き出しを同時に動かす確認です。" --until "mem:" --secs 90 --grep "@msw"
.venv\Scripts\python.exe tools\msw_serial.py COM5 --cmd ">msw.cpu 5" --until "@msw.cpu done" --secs 20
```

設定は再起動で既定（model=psram core=0 prio=4 quiet=0）に戻る。
