# 開発状況

## 2026-11-06

- プロジェクトフォルダを作成。エージェント用の基本設定（CLAUDE.md、.claude/settings.json）を用意。
- 未決定: 開発の目的・機能、ビルド手段（PlatformIO / arduino-cli / ESP-IDF）。
- ビルド方式を PlatformIO + ESP-IDF に決定。.venv に PlatformIO 6.2.0 を導入済み。
  tools/pio.py と dev.ps1 を用意。出荷時デモを upstream/ に clone（81099c7）。
  `.\dev.ps1 build` でデモのビルドに成功（ESP-IDF 5.5.5、約4分20秒、初回はツール導入込み）。
  RAM 17.5%（57,364 / 327,680 B、静的）、Flash 76.2%（3,942,885 / 5,177,344 B、app 枠）。
  出力: upstream/M5StopWatch-UserDemo/.pio/build/m5stack-stopwatch/firmware.bin（実機には未書き込み）
- dev.ps1 修正: PowerShell 5.1 でネイティブツールの stderr が停止扱いになるため
  ErrorActionPreference を Continue にし、終了コードで判定する。
- 実機: 下の「公式 StopWatch ファームの書き込み」を参照。

## 公式 StopWatch ファームの書き込み — 2026-11-06

- 書き込み前に出荷時フラッシュ全体を保存: backups/factory-COM5-c49e7e13e96c-20261106.bin
  （16MB、SHA256 7b5859fa2dd03272214786c6958b41b1dca55b6866e25d8dd202d898105cfb9b）。
  デバイス: ESP32-S3 (QFN56) rev0.2、PSRAM 8MB、Flash 16MB、MAC c4:9e:7e:13:e9:6c、COM5。
- SDK upstream 463bd2a ＋ overlay devices/sdkconfig.musestopwatch（CONFIG_MUSE_CJK_FONT=y）を
  ESP-IDF 6.0.3 でビルドし USB で書き込み（全領域ハッシュ照合 OK）。アプリ 3,411,968 B / ota_0 4MB。
  SDK トークンはビルドに入れていない（未設定）。
- ユーザー確認: 起動して画面が出る。設定画面から Wi-Fi を画面キーボードで入力できる。
- 未解決: 再起動後、USB シリアル（COM5）からログが読めない（0 バイト）。原因未調査。
  ペアリング・会話は未確認。

## 2026-11-07: Settings に QR メニュー（AP Mode QR / Web QR）

- 公式ファーム＋α の最初の α。SDK フォーク（dev/stopwatch）に追加、上流ファイルの変更は最小限:
  - 新規 `components/muse/msw_net.c/.h`: AP モード（APSTA、SSID `museSW-XXXX`、WPA2、
    パスワードは初回に乱数生成して NVS `msw/ap_pass`）と小さな Web サーバー（esp_http_server、`/` に状態ページ、日本語）。
  - 新規 `components/muse/msw_settings_qr.inc`: QR 一覧ページと QR 表示ページ（210px の QR を中央、下に日本語の説明）。
    説明文は Montserrat 16 ＋ CJK フォント（muse_font_cjk_16）のフォールバックで表示。
  - `muse_settings_ui.c`: include、ホームの Wi-Fi の上に「QR」行、drop/go_back/tick のフック（各1〜3行）。
  - `CMakeLists.txt`: msw_net.c と esp_http_server。overlay に `CONFIG_LV_USE_QRCODE=y`。
- ビルド OK（アプリ 0x341000 B、ota_0 の 19% 空き）。COM5 に書き込み、ハッシュ照合 OK。NVS は消していない。
- 未確認（ユーザーの実機確認待ち）: QR の表示・読み取り、AP への接続、Web ページの表示、
  AP 中の Muse 接続への影響、戻る操作。

## 2026-11-07: 日本語フォントを Noto Sans JP に、Web 設定（Wi-Fi・Muse）

- 日本語フォント: Noto Sans JP Medium 16px・4bpp（JIS X 0208 全部、7,064 字）を採用（ユーザー決定）。
  `tools/muse/gen_cjk_noto_jp_font.sh` で生成（notofonts/noto-cjk の SubsetOTF、lv_font_conv 1.5.3）。
  Kconfig `MUSE_CJK_FONT_NOTO_JP` を追加し overlay で有効化。字幕と QR 画面の説明文の両方に効く。
  比較画像: work/font-compare/。アプリは 3,149,824 B（Unifont 版より約 260KB 減、ota_0 の 25% 空き）。
- Web 設定ページ（`components/muse/msw_web.html`、本体に埋め込み）:
  - 状態、Wi-Fi（保存済み一覧と削除、スキャン、追加して接続）、Muse（サーバー、VM ID、デバイストークン、
    接続テスト、SDK トークン＝保存で再起動）。
  - 命令は公式 BLE 設定ページと同じもの（`muse_ble_command`）を Web から渡す。状態は `muse_ble_status_json`。
  - アクセス鍵: Web QR の URL に `?k=` 付きの鍵（12文字、NVS `msw/web_key`）。開くと Cookie に保存して
    URL から消す。鍵のないブラウザには「本体の Web QR から開いて」とだけ表示。
    変更系 API は Cookie と `X-MSW` ヘッダーの両方が必要（他サイトからの送信を防ぐ）。
- ユーザー確認済み（前の版）: AP Mode QR・Web QR・ブラウザでの表示が一通り動いた。
- 書き込み済み（COM5、ハッシュ照合 OK）。未確認: 新フォントの見た目、Web 設定の各操作。

## 2026-11-07: Wi-Fi の切り替え・削除、設定画面の日本語・多言語表示

- 保存済みネットワークをタップでつなぎ替え（保存済みパスワードを使う）。`muse_wifi_choose()` を追加し、
  main/muse_glue.c の keeper タスクが NVS からパスワードを読んで選択扱いにする（KEEP_CHOOSE）。
  つなぎ方は公式のまま: 前回つないだものを優先し、だめなら電波の強い順。
- 削除: 2回タップで削除をやめ、左スワイプか長押しで行に赤い × → 確認ダイアログ（削除／キャンセル）。
  右スワイプで × を閉じる（× が出ていないときは従来どおり戻る）。`msw_wifi_saved.inc`。
- 設定画面の文字: 上流は非 ASCII を全部 ASCII の代わりの文字に置き換えていた（SSID が化ける原因）。
  `msw_text.c` で、フォント（＋CJK フォールバック）にある字はそのまま表示し、ない字だけ置き換える。
- Battery ページを日本語化（項目名・状態・説明）。Wi-Fi ページの説明も日本語。
- Web: 保存済みをタップでつなぎ替え、空のパスワードで保存済みを上書きしないよう修正。
- 書き込み済み（COM5、ハッシュ照合 OK）。未確認: スワイプと長押しの反応、確認ダイアログ、つなぎ替え、日本語 SSID の表示。

## 2026-11-07: Web サーバーの自動起動、ページを開いている間は眠らない

- 起動時に小さなタスクが Wi-Fi 接続（または AP）を待って Web サーバーを起動（`msw_web_autostart`）。
  書き込み後の再起動から約 5 秒で、PC から 192.168.11.2 に応答あり（QR 画面は開いていない）。
- 設定ページからの問い合わせごとに `muse_state_poke()`（自動スリープまでの時間を延ばすだけで、
  消えた画面は起こさない）。ページは見えている間だけ 2 秒ごとに問い合わせ、5 分操作がなければ止まる。
- メモリ（概算・未実測）: サーバー待機中で内部 RAM 約 10KB（タスクのスタック 6KB ほか）、lwIP ソケット 2 個。
  待機中は CPU を使わない。
- 「つながらない」件の原因はスマホのテザリングで Wi-Fi がオフだったこと（ファームの問題ではない）。

## 2026-11-07: サイドチャット、設定画面の説明文を日本語に

- サイドチャット（SDK の用語は side chat、ID の項目は `session_id`。`chat_id` は API にない）:
  - `muse_chat_session.cpp`: 音声メモの先頭、文字の送信、返事の受信に `session_id` を付ける
    （`muse_hatch_set_session_id()`、変わったら受信をつなぎ直す）。空ならメインの会話。
  - `msw_side.c/.h`: 本体が覚える一覧（8 件まで、NVS `msw/side_list`・`side_cur`・`side_seq`）。
    追加は ID を入れればその会話、空欄なら UUID v4 を作る。起動時に前回の会話を選ぶ。
    Muse 側には一覧を返す API がない（SDK の `open_side_chat` は ID の有無を調べるだけ）。
  - 本体: Muse > Test connection の下に「Side chat」。ページで Main chat／各サイドチャットをタップで選択、
    Add side chat（Session ID 入力、空欄で新規）、左スワイプか長押しで × → 確認して一覧から削除。
    確認ダイアログと × は Wi-Fi の保存済みと共通（`msw_side_ui.inc`）。
  - Web: 「サイドチャット」欄（一覧・選択・削除・ID 入力で追加、空欄で新規）。命令は
    `side.main` / `side.add=<ID>` / `side.select=<ID>` / `side.delete=<ID>`。
  - 説明に「サイドチャットで Muse にエージェントのセッション ID を聞くと教えてくれます」を入れた（ユーザー指示）。
    → 2026-11-07 訂正: Muse は「エージェントのセッション ID」と「このサイドチャット自体の ID」の 2 つを返すことがあり、
      入れるべきなのはサイドチャット自体の ID だった（ユーザー確認）。本体・Web の説明文をそのように直した。
      Muse の返事の呼び方は一定しない: サイドチャットの ID を「chat_id」、エージェント実行の ID を「session_id」と
      呼ぶこともある（SDK の API ではサイドチャットの ID の項目名が session_id）。
      説明文はユーザー指示で「そのサイドチャットで Muse に『このチャットの ID を教えて』と聞くと教えてくれます」に統一、
      「エージェント実行の ID ではなく、チャットの ID です」を添える。
- 設定画面の英語の説明文を日本語に（Wi-Fi の状態、Muse、Bluetooth、Sound、Sleep、Power、文字入力の例文）。
  項目名は英語のまま。文字入力欄も CJK フォールバックのフォントに。
- 事故: 翻訳スクリプトの書き込み失敗で muse_settings_ui.c が空になった。Claude Code の編集履歴
  （file-history の直前版）から戻し、同じ変更をやり直した。Bash の heredoc 経由だと `\n` が崩れることがあるので、
  エスケープを含む編集は Edit ツールで行う。
- ビルド成功（アプリ 0x301000、25% 空き）、COM5 に書き込み（ハッシュ照合 OK）。
  未確認: 本体の Side chat 画面の操作・ID 入力、サイドチャットで実際に別の会話になるか、Web のサイドチャット欄、日本語の説明文の見た目。

## 2026-11-07: Muse に送れない件（内部 RAM 不足）、新しいサイドチャットの 404、フラッシュの書き込み保護

- 症状: メインでもサイドチャットでも「CAN'T REACH MUSE」。ログでは接続処理が 7ms で失敗。
  - 原因 1（内部 RAM）: Link 経由の VM 検索（`main/app.c` の `app_hatch_vm_credentials`）が毎回 8KB の
    内部 RAM スタックのタスクを作っていた。Web サーバー常時起動（スタック 6KB）で内部 RAM の最大の塊が
    7KB になり、タスクが作れず即失敗していた。
  - 対策: VM 検索を常駐タスク 1 本に（スタック 8KB は PSRAM、初回に確保）。Web サーバーのスタックも
    PSRAM（`httpd_config_t.task_caps`）。この構成（`SPIRAM_FETCH_INSTRUCTIONS` と `SPIRAM_RODATA`）では
    フラッシュ書き込み中もキャッシュを止めない（IDF `spi_flash_os_func_app.c` の `SPI_FLASH_CACHE_NO_DISABLE`、
    上流 `main/helper_task.h` の注記）ので、PSRAM スタックから NVS に書いても安全。
  - 結果: 起動 6 秒前後で VM に接続、内部 RAM の空き 8KB → 18〜24KB（heartbeat の int=）。
- 原因 2（サイドチャット）: まだ一度も話していないサイドチャットには `/chat/subscribe` が 404 を返す
  （SDK linux/AGENTS.md: 新しいサイドチャットは最初のメッセージで作られる）。本体は受信を先につなぐので
  切断→再接続を繰り返していた。
  - 対策（`muse_chat_session.cpp`）: session_id があって subscribe が 404 なら、受信なしで接続を保ち
    （`s_sub_wait`）、最初の `/chat/stream` の受け付け（ack）後に subscribe する。SDK の send-user-msg --wait と同じ順。
    返事が subscribe より先に終わると取りこぼすが、返事は数秒かかるので実用上は問題ない想定（未確認）。
  - サイドチャット切り替えは VM 情報を捨てず（`CMD_SESSION`）、接続だけ張り直す。
- 内部 RAM の内訳（調査中、heap task tracking で 1 回だけ実測。tracking 自体の余分を含む）:
  - 静的: DIRAM 約 177KB（IRAM の命令 109KB、.bss 40KB、.data 26KB）。残り約 165KB はほぼ実行時に使われている。
  - 内部 RAM にスタックがあるタスク: swdraw×2、lvgl、wifi、btController、nimble_host、noise_ctrl、
    muse_input、muse_keep、muse_serial、tcpip、sys_evt、esp_timer など。
  - タスクごとの確保量は PSRAM と内部が混ざった一覧しか取れていない（wifi 約 96KB など）。
  - 内部だけに分ける診断は `msw_mem.c` の `>msw.mem` で完了（下の「内部 RAM を空ける」の節）。
- 事故: 診断版を書いたあと本体が画面真っ暗・ログなしになった。
  - フラッシュの状態レジスタが 0x20021c（BP0〜BP2 が 1）で書き込み保護になっており、以後の書き込みが
    反映されていなかった（esptool の MD5 照合で発覚、読み出すと前の版のまま）。
    ユーザーの了解を得て `write-flash-status --bytes 2 --non-volatile 0x0200` で解除（QE は維持）。保護が付いた原因は不明。
    フラッシュの製造元 ID は 0x20（XMC）、デバイス 0x4018、16MB。
  - さらにソフトからのリセット後に ROM の書き込み待ちモードで止まることがあった（緑ランプ点滅）。
    電池で動き続けるので USB の抜き挿しでは直らない。電源ボタン 2 回押しで切り、1 回押しで入れ直すと復帰。
    公式: 電源オフは電源ボタンを素早く 2 回。USB 接続で約 2 秒長押しすると書き込み待ちモード。
  - 書き込み直後はシリアルを開くのが早すぎるとログが 0 バイトになる。5 秒ほど待ってから開く。
- SanoTTS（ayutaz/sanoTTS-jp）の公式ベンチ（2026-11-07 に GitHub の README で確認、kantan の記録と同じ）:
  CoreS3 で定常 xRT 0.473、arena 139,264B を内部 DRAM に静的確保、D-cache 64KB/64B 行。
  arena を PSRAM に置くと遅い（上流 main.c の注記）。この本体は D-cache 64KB/64B 行・Octal PSRAM 80MHz・
  CPU 240MHz で、キャッシュは公式と同じ。最速構成には内部 RAM 約 140KB が要る。
  → 組み込みとベンチの結果は下の「SanoTTS の組み込みとベンチ」の節と `work/sanotts-bench-2026-11-07.md`。
- 実機確認（ログ）: 既存のサイドチャット・新規のサイドチャット（最初の一言から）とも、ack の後に subscribe して
  返事を受け取り表示できた（返事まで約 12〜13 秒）。新規では subscribe が 404 → 送信 → ack → subscribe の順。
  一度だけ新規の最初の一言で返事が来なかったことがある（原因未特定、再現せず）。
  受信イベントを全部ログに出す一時的なトレースは外した。

## 2026-11-07（夜）: 内部 RAM を空ける（タスクのスタックを PSRAM へ）

- 調べ方: `>msw.mem`（シリアル。`msw_mem.c`）で内部 RAM の合計・タスクごとのスタックの場所と残り・
  内部ヒープの 1KB 以上の塊を出す。シリアルの送受信は `tools/msw_serial.py`（DTR/RTS を触らない＝リセットしない）。
- 移す前（`work/ram-baseline-2026-11-07.log`）: 内部の空き 17.9KB、最大の塊 4KB。大きいもの: noise_ctrl 12KB（使用 約 4.5KB）、
  main 8.7KB、swdraw 8KB×2（使用 約 1.8KB ずつ）、lvgl 8KB、wifi 6.6KB、muse_keep 6KB、ほか 4KB 級が数本。
  1.7KB 前後の塊が約 17 個（Wi-Fi のバッファと思われる）。64KB の塊は `SPIRAM_MALLOC_RESERVE_INTERNAL` の予約。
- 移したもの（`components/muse/msw_stack.h` の `msw_task_create`、XIP 構成のときだけ PSRAM、それ以外は内部のまま）:
  lvgl、muse_input、muse_keep、noise_ctrl、tun_tx、muse_serial、httpd、muse_vm、muse_chat、muse_voice。
  swdraw のスタックは 8KB → 5KB（`CONFIG_LV_DRAW_THREAD_STACK_SIZE=5120`、残り約 3.2KB）。
- 移した後（`work/ram-psram-stacks-2026-11-07.log`）: 内部の空き **65.4KB、最大の塊 38.9KB**。heartbeat `int=63K/38K`。
  Muse への接続・サイドチャットの返事は従来どおり（ログで確認）。
- 内部に残るもの: wifi、tcpip、sys_evt、esp_timer、BT、main、swdraw、lcd_send（IDF・ドライバ側）。
- **注意**: PSRAM スタックのタスクから `esp_partition_mmap()`（と OTA 状態の読み出し）は呼べない（IDF が assert）。
  移したタスクはどれも呼ばないことをコードで確認した。詳しくは `work/sanotts-bench-2026-11-07.md`。

## 2026-11-07（夜）: SanoTTS の組み込みとベンチ、読み上げ、Muse のコマンド `voice.say`

- 詳細は `work/sanotts-bench-2026-11-07.md`。
- sanoTTS-jp（f05d7ae）のソースを `components/sanotts/upstream/` にそのままコピー（`tools/sync_sanotts.py`）。
  モデルと辞書は新しいパーティション（`partitions_musestopwatch.csv` の `sano_model` / `sano_dict`、0x830000〜）。
  アプリ領域（4MB×2、使用 3.2MB）と NVS は変えていない。`.\dev.ps1 flash` がモデル・辞書も書く（約 5 秒増える）。
- ベンチ（音なし）: 採用設定（モデルを PSRAM にコピー、アリーナ PSRAM、コア 0・優先度 4）で定常 xRT 約 0.52〜0.54。
  コア 1 は画面描画が使っていて、優先度を上げると画面が止まる（lvgl の CPU 28% → 1.2%）。
  内部 RAM にアリーナ（約 120KB）を置く公式の最速構成は、最大の塊が約 30KB なので不可。
- 読み上げ: `>msw.say <文>`（シリアル）、Muse から `voice.say`（text）・`voice.stop`。16kHz に変換して声のタスクが
  待ち受け中に再生、ボタンで止まる。スピーカーが OFF の設定なら `speaker_off` を返す。
- 夜間のため音は出していない（`>msw.tts.cfg quiet=1` で無音再生して経路だけ確認）。
- 内部 RAM: TTS で静的に約 4.9KB 増えた。heartbeat は `int=58K/29K`（TTS を入れる前は 63K/38K）。
- **未確認（ユーザーに確認してもらうこと）**:
  - `>msw.say こんにちは。` などで実際の声・音量・音質。
  - Muse に「`voice.say` で〇〇と読み上げて」と頼んだときに読まれるか（コマンド一覧に出ているか）。
  - 通話中（押して話す通話）には読み上げが始まらない（待ち受けでないため。30 秒で捨てる）。
- 続けて静的バッファを PSRAM へ（`EXT_RAM_BSS_ATTR`）: アバターの描画用 `s_fb`・`s_mask`（4KB×2）と `s_sqrt`（2KB、
  `avatar/muse_pixel.c`）、音声の作業バッファ `s_in_stereo`・`s_in_mono`・`s_out_stereo` とチャープ音の `buf`
  （`muse_audio.c`。I2S ドライバは自分の DMA バッファにコピーするので内部でなくてよい）。
  静的 DIRAM 182,021B → 166,477B。heartbeat **`int=74K/48K`**。画面の CPU（lvgl 27%）も TTS の速さ（xRT 0.53）も変わらず。
  - **未確認**: アバターの表示が前と同じか、マイクと返事の音声が普段どおりか（ユーザーに見て・話してもらう）。
- 内部 RAM の残りの候補（未着手）: IRAM の命令 108KB のうち BT コントローラ 20KB（バイナリ提供で移せない）、
  spi_flash 11KB・ハード支援 14KB など（低レベルの設定変更で危険度が高いので保留）、
  Open JTalk の数字読み表 2.7KB（上流のファイルは変えない方針なので保留）。

## 2026-11-07（朝）: Muse の返事を Sano で読み上げる、voice.say を優先

- 返事（チャットの各メッセージ）を Sano で読み上げるようにした。これまでは公式どおり無音で字幕のペースを作るだけだった。
  `muse_voice.h` の `muse_reply_voice_t`（`msw_tts.c` が登録）を `muse_chat_session.cpp` の `start_tts` / `decode` が使う。
  スピーカー OFF や使用中のときは従来どおり無音で字幕だけ。URL・Markdown 記号・絵文字は読む前に除く。
- 会話中に Muse が `voice.say` を呼んだら、その say を読み、そのターンの返事は読まない（読み途中なら止めて say に替える）。
  `voice.say` の説明（Muse 向け）も「返事は自動で読まれる。違うことを言いたいときだけ使う」に変更。
- 実機ログ（`work/logs-2026-11-07/talk-check6.log`）: 返事のメッセージ完了から約 0.5 秒で音が出始め、2 文を読み切ってターン終了。
  会話中の xRT は約 0.59（待ち受け時のベンチ 0.53 より少し遅い）。ユーザーが「うまくいってそう」と確認。
- 「NO REPLY FROM MUSE」が続いた件: 約 5 時間動かし続けた後に発生し、再起動後は返事が届いた。
  購読（/chat/subscribe）が切れたのに気付かなかった可能性があるが、未確認。
  切り分け用に、届いた通知を 1 行ずつ出すログ（`event ... phase=`）と、ほかのメッセージへの返事として捨てたときの警告を入れたまま。

## 2026-11-08: ミニアプリ（アイデア 001）の実装と、テストアプリ 3 本

- 作ったもの
  - ランタイム `components/miniapp/`（`msw_app_priv.h` にファイル一覧）。アプリは ELF（共有オブジェクト）で、
    `espressif/elf_loader` 2.1.0 が PSRAM に読み込む。アプリが呼べるのは `msw_app.h`（API 版 1）と
    `sdk/exports.list` の関数だけ（elf_loader 自身の libc / IDF の表はオフ: `devices/sdkconfig.musestopwatch`）。
    malloc は追跡して、終了時に画面・タイマー・アニメ・音・IMU とまとめて片付ける。
  - 保存先: パーティション `apps`（FAT、0x9F0000 から 4MB、`/apps`）。フラッシュに触るのは内部 RAM スタックのタスクだけ。
  - ランチャー: 顔から右へスワイプ（設定と反対側）。丸いアイコン 3 列、タップで起動、長押しで情報と赤い × で削除。
  - Muse のコマンド: `app.doc`（最初に読む資料 = `components/miniapp/guide.md`）、`app.api`（ヘッダー）、
    `app.install`（VM のファイルを取ってきて入れる。非同期）、`app.list` / `run` / `stop` / `remove` / `log`。
  - 実機でのアプリ操作（Muse なし）: `tools/msw_app_install.py COM5 <.mswapp> [--run]`、`--cmd "msw.app.log"` など。
  - SDK: `sdk/build.py`（ビルド＋「ファームにない関数」のリンク検査＋パッケージ）、`tools/make_sdk.py`（ヘッダー一式）、
    `tools/gen_msw_exports.py`（関数表）。手順は `.claude/skills/msw-app/SKILL.md`。
  - 青ボタン: アプリ中はアプリへ（1.5 秒長押しで終了）。黄ボタンは今までどおり Muse と話す。
  - Muse の返事の文字をアプリに渡すフック（`muse_chat_session.cpp`）。ミニアバター・吹き出しの API は作ったが、どのアプリもまだ使っていない。
- テストアプリ（`apps/`、絵・曲・名前はすべてオリジナル）
  - `blocks`「おちものブロック」: 落ちものパズル。画面の十字キーと A/B、曲と効果音、ハイスコア保存。
  - `pyoko`「ぴょこの森」: 横スクロールのジャンプアクション。8x8 のテキスト絵を 3 倍で描く。種・クリスタル・トゲまる。
  - `aquarium`「まるい水槽」: 傾けると水面が重力に合わせて傾き、振ると波立つ。魚 16 匹のボイド、タップでエサ、泡。
- 実機で確認できたこと（ログ: `work/logs-2026-11-07/run-*`、`build-miniapp*`）
  - シリアルで 3 本とも入る（CRC 一致）。読み込み 1〜4ms、起動・停止・切り替えでクラッシュなし。終了時に残りのメモリを解放。
  - IMU: 表向きに置いて `az=+1.00`（z の向きは正しい）。ジャイロは起動直後の 1 回目が 0,0,0（要確認）。
  - 速さ（`app.log` の終了時の行）: 全画面の転送が 26.7ms（0.12µs/画素、QSPI 40MHz）。描画は PSRAM への書き込みが律速。
    blocks 30fps、pyoko 18fps（描画 20ms + 転送 27ms）、aquarium 9 → 19.5fps（2x2 ブロックと逐次計算に変更）。
  - 再起動後、直前に動いていたアプリを記録する仕組みが働いた（「aquarium was running at the last restart」）。
- **未確認（ユーザーに見て・聞いてもらう）**: ランチャーの見た目とアイコン、各アプリの画面・音・タッチ操作、
  水槽の傾きの向き（x/y の符号。逆なら `msw_app_api.c` の `IMU_SX`/`IMU_SY` を変える）、振動、
  Muse からの `app.doc` / `app.install`（VM 側に SDK とコンパイラが要る）。
- わかった問題・残り
  - 全画面を毎回描き直すゲームは 20fps 前後が上限。転送の裏で次を描く二重バッファ（任意で使える形）や、
    パネルのクロック 40→80MHz は未検討。
  - Muse の VM が SDK を使うには `sdk/`（`sdk/include` を含む）を GitHub に置く必要がある（未コミット）。
  - 長押しで消す・Muse の返事フック・アバター API は実機で未試験。


### 2026-11-08 追記: タッチは1点だけ

- ユーザー報告: ぴょこの森でボタンが同時に1つしか反応しない（走りながらジャンプできない）。
- 原因: CST820 の読み出し（board の tp_read）は1点だけ。msw_touch も1点。
- 対応（ぴょこの森）: 矢印から上へスライドでジャンプ（走ったまま）。矢印を離して 0.4 秒以内に
  ジャンプを押すと、その方向を押している間は保つ。タイトルに説明を表示。guide.md / SKILL.md に追記。
- 実機に入れ直した（COM5）。操作感はユーザー確認待ち。

### 2026-11-08 追記: 2点タッチ

- CST820 データシート V1.2 に「真实两点操作」とある。公式デモのドライバは1点だけ読む。
- 実測（`msw.tp 90` でレジスタ 0x00〜0x0F をダンプ、work/logs-2026-11-07/tpdump2.log）:
  0x02 = 指の数（最大 2）。指ごとに 6 バイト（0x03〜 / 0x09〜）: イベント|X上位, X下位, ID|Y上位, Y下位。
  イベント上位 2 ビット 0x00 押した / 0x80 接触中 / 0x40 離した。2 本とも座標が正しく取れた。
- 対応: board の tp_read が 2 点を読む（LVGL へは 1 点目）。API `msw_touches(xs, ys, max)` を追加（362 シンボル）。
  ぴょこの森は 2 本指で「走る＋ジャンプ」、1 本指の上スライドも残した。ファーム書き込み済み、操作はユーザー確認待ち。

### 2026-11-08 追記: アプリメニュー・ボタン配置・アイコン

- ランチャー画面でも、本体ボタン横のアイコン（マイク・スピーカー・電源）を隠す（設定画面と同じ。muse_ui.c update_control_hints）。
- 黄ボタン（左上、ふだんは話す）: アプリ中はアプリメニュー（アプリ名・「つづける」「おわる」）を開き、アプリを一時停止する
  （msw_app_menu.c、lv_layer_top。フレーム描画のアプリは閉じたあとに最後のフレームを送り直す）。
  API: msw_on_pause, msw_menu_own(true)（黄をアプリが使う。その場合は終了方法を案内するルール）、MSW_BUTTON_YELLOW。364 シンボル。
- 画面ボタンを左右の縁・上下の真ん中へ（ブロック: 十字 左縁 / A・B 右縁、ぴょこ: ←→ 左縁 / ジャンプ 右縁）。
- ファーム書き込み・2 本インストール済み。起動ログは正常。メニュー・配置の見え方はユーザー確認待ち。

### 2026-11-08 追記: ボタンの役割の修正と配置の試作 2

- 色の取り違えを修正: 黄＝左上（ふだんはスピーカー）、青＝右上（ふだんは話す）。
  アプリ中: 黄＝アプリメニュー（つづける / おわる）、青＝アプリのボタン（Muse には送らない）。
  黄の 1.5 秒長押しで終了は msw_menu_own(true) のアプリだけ。ゲーム独自のポーズ（旧・青ボタン）は削除。
- Muse を使うかはアプリの任意（計画どおり）。話す操作はアプリが用意する: msw_muse_talk(down) を追加（365 シンボル）。
- 画面ボタン: 左右の縁（上下の中央）は指で画面が隠れる → 左下・右下の円周に沿った位置で試作
  （中心から約 190 px、左 115〜155°、右 25〜65°）。ブロックとぴょこに反映。
- ファーム書き込み・3 本インストール済み。見え方・押しやすさはユーザー確認待ち。
- ぴょこの森の「上スライドでジャンプ」と方向の記憶は削除（暴発するとユーザー報告）。2 本指の同時押しだけにした。
- 2 本指で横移動が止まる件（work/logs-2026-11-07/tpdump3.log、ぴょこで → 押しっぱなし＋ジャンプ）:
  アプリ側に排他処理はない。タッチ IC が「2 本 → 0 本 → ジャンプの指だけ 1 本」と報告し、押しっぱなしの → の指を
  見失う（その後も戻らない）ことがある。成功時は「2 本 → 1 本（→ の指が残る）」。自己容量式の 2 点検出の限界と思われる。

### 2026-11-08 追記: タッチは 1 本指に戻す（ルール化）

- 矢印を押したままジャンプをタップすると、タッチ IC が 2→0→1 と報告し、押したままの指を見失うことがある
  （work/logs-2026-11-07/tpdump3.log）。自己容量式の限界と判断。
- ユーザー決定: タッチは 1 本指に限定し、「画面ボタンを押したまま別の画面ボタン」という作りをしないことをルールにする。
- 対応: `msw_touches` と board の 2 点保存を削除（364 シンボル）。tp_read は押されている最初の指だけを LVGL に渡す。
  `msw.tp` のレジスタダンプは残した。guide.md・SKILL.md・msw_app.h にルールを書いた。
- ぴょこの森: ジャンプは青ボタン（右上）。画面のジャンプボタンは削除し、右上の縁に青い↑の印、
  タイトルに「ジャンプ: 右上の青ボタン」。青ボタンでもスタート・再開できる。
- ファーム書き込み・ぴょこの森の再インストール済み（build-onetouch1 / flash-onetouch1）。操作はユーザー確認待ち。

### 2026-11-08 追記: ぴょこの森を傾きで移動（試作）

- ユーザー案: 画面のボタンを押している間だけ、傾きで左右に移動する。
- ←→ の 2 ボタンを、左下の縁の「傾き」ボタン 1 つ（(99,367) 半径 40）に置き換えた。
  押した瞬間の傾きを水平とみなす。横方向の重力 -ax を平滑化し、0.06 g 未満は止まる、0.30 g で最高速。→ 「もっと反映」で 0.025 g / 0.15 g（約 1.5° / 9°）、平滑化 0.6、傾き時の加速 1.6 倍に変更。
  押している間、ボタンの下に傾きの量を示す黄色い点を出す。ジャンプは青ボタンのまま。
- アプリだけ再インストール済み。操作感はユーザー確認待ち（しきい値は調整候補）。
- 「そのまま反応」: デッドゾーン 0、押している間は速さ = 4.2 × 傾き（0.15 g で最高速）を毎フレームそのまま入れる（加速なし）。
  向きは傾きが最高速の 5% を超えたときだけ変える。離すと今までどおり減速。
- 非線形: 速さ = 4.2 × (傾き / 0.15 g)^1.8（TILT_CURVE）。最高速の傾きは同じ約 9°、半分の傾きで約 29% の速さ。
- スライド＋傾きの 2 方式: 左下 (99,367) 半径 40 はスライド（触れた点が中心、40 px で最高速、1.5 乗）。
  その右上 (182,328) 半径 28 に「傾き」ボタン（押している間だけ傾きで移動、上の設定のまま）。
  触れた瞬間に近い方のボタンがその指を持ち、離すまで続く（スライド中にパッドの外へ出てもよい）。
- 傾きボタンを 10 時の位置 (69,138) 半径 28 に移動（ユーザー指示）。

### 2026-11-08 追記: スペースから本体 API を呼べるかの試験（/api/ping）

- 前提（ユーザー調べ）: スペースは自己完結かコネクタ経由。ガジェットのツールは Muse の CLI からだけ。VM のファイルは読むだけ。
  → スペースのページ（ブラウザの JS）から本体の IP を直接呼ぶ案を試す。
- msw_net.c に `GET /api/ping`（鍵なし・秘密なし、{"ok","device","uptime_ms"}）と `OPTIONS /api/ping` を追加。
  CORS（Allow-Origin *）と Allow-Private-Network を付ける。呼ばれるとシリアルに `@web.ping get|preflight origin=...`。
- 書き込み済み（build-ping1 / flash-ping1）。PC の curl で 200 と CORS ヘッダーを確認（本体 IP は ARP で 192.168.11.2、DHCP なので変わりうる）。
- Muse は今この API も IP も知らない（doc にもツールにもない）。試験では依頼文で IP と URL を渡す。
- 未確認: スペースのページから届くか（https→http の混在、ローカル網アクセス制限、スペースの CSP）。
- 結果（15:3x）: スペースから本体に届いた。シリアル `@web.ping get origin=hatch-spaces://<id>.metaaivm.com`。
  プリフライトなしの単純な GET。Origin は独自スキーム `hatch-spaces://`（Muse アプリ内の WebView と思われる）。
  ページ側の表示はスマホ Safari で成功、PC Chrome は失敗。いったんあきらめ（ideas.md 005）。
- ユーザー確認: スマホの Safari では表示できた（hatch-spaces の呼び出しはこれ）。PC の Chrome ではだめ。Chrome からの呼び出しはシリアルに出ていない（送る前に止められた）。
  候補: https→http の混在コンテンツ、または Chrome のローカルネットワークアクセス制限。Chrome のエラー文は未入手。
- `/api/ping` は本体に残す（2026-11-08 ユーザー判断。鍵なし・機種名と起動時間だけを返す）。

### 2026-11-08 追記: GitHub に公開（Muse が SDK を取れるように）

- museStopWatch main 1c99507 と muse-gadget-sdk dev/stopwatch 54bba9b を push（ユーザー指示）。
- 入れないもの: work/logs-*/ と work/*.log（.gitignore に追加）、SanoTTS のモデル .bin（MIT ではない。model/README.md に入手先）。
- 確認: `git clone --depth 1` した物の sdk/build.py で apps/blocks を組み立て、手元の .mswapp と同じバイト列になった（Windows のコンパイラで）。
- 未確認: Muse の VM（Linux）でのコンパイラ導入と組み立て、app.install までの通し。

### 2026-11-08 追記: 自分の部品を components/muse_diy_stopwatch/ にまとめた

- ユーザー決定: 公式が勧めるのは SDK の直接改造なので、「自分の部品は components/ の下、公式ファイルには必要な改造だけ」。
  差し込み口を作る整理（work/sdk-fork-cleanup-2026-11-08.md）は保留。
- 移動（中身は変えない、git mv）: components/miniapp → muse_diy_stopwatch/miniapp、components/sanotts → muse_diy_stopwatch/sanotts、
  components/muse/msw_*（net, text, side, mem, web.html, 3 つの .inc）→ 新しい部品 muse_diy_stopwatch/ui。
  msw_stack.h は muse に残した（どの機種にも役立つ）。
- ui は muse を REQUIRES、muse は ui を PRIV_REQUIRES（お互いに必要。ui は WHOLE_ARCHIVE）。miniapp も ui を使う。
- dev.ps1 が `-DEXTRA_COMPONENT_DIRS=components/muse_diy_stopwatch` を渡す。tools、CLAUDE.md、スキル、フォーク内のコメントのパスを更新。
- ビルド（build-diydir1）: ファームの大きさは移動前と同じ 0x341000。書き込み後、msw.app.list と /api/ping が返ることを確認。
- README.md に、構成、フォークで足したもの、公式ファイルの改造箇所の概要とリンクを書いた。

### 2026-11-08 追記: アプリメニューで左上が黒くなる不具合

- 症状（ユーザー、写真）: フレーム描画のアプリで黄ボタンのメニューを開くと、画面の左上 (0,0) から枠の右下までが黒い。
- 原因: lv_obj_create した枠は一度 (0,0) に置かれてから中央へ動く。両方の場所が描き直し対象になり、
  LVGL が 1 つの長方形にまとめ、アプリの黒い lvgl 画面で塗った。
- 対応（msw_app_menu.c）: 枠を作る間は lv_display_enable_invalidation(false)、lv_obj_update_layout のあと
  枠の場所だけ lv_obj_invalidate。書き込み済み（build-menufix1 / flash-menufix1）。見た目はユーザー確認待ち。

## 表示名 Muse DIY StopWatch（2026-11-08）

- Muse アプリの一覧の名前は link.register の display_name。kantanMuse は muse_link.cpp:358 で
  "KANTAN Play" を固定で渡していた。
- app.c の noise_ctrl_init() に、StopWatch のときだけ "Muse DIY StopWatch" を渡す（museStopWatch: の印）。
  BLE の名前は MuseGadget-XXXXXX のまま（公式 README: アプリは "MuseGadget" で始まる機器を探す）。
- ビルド・COM5 に書き込み済み。link.register acked。アプリの一覧で「Muse DIY StopWatch」になったとユーザーが確認（2026-11-08）。未コミット。

## 日本語音声認識ガイド（2026-11-08）

- ユーザーの voice_asr_flow.md を esp32/components/muse_diy_stopwatch/docs/muse_japanese_voice_asr_guide.md に置き、
  フォークに push（b62c09d）。
- guide.md（app.doc）の先頭に「まだ案内していなければ…」とリンクを書いた。ビルド・COM5 に書き込み済み。
- app.doc は Muse がミニアプリを作るときに読むものなので、それ以外の会話では案内されないかもしれない。未確認。
- 親リポジトリのサブモジュールの指す先は、まだ更新していない。

## 全体ガイド stopwatch.doc（2026-11-08）

- ユーザーの方針: ターンが増えると遅くなるので、全体ガイドである程度網羅する。読み直しは更新されたときだけ。
  kantanMuse の kanplay.doc と同じく、ツール説明に書ききれない概要やツールの詳細もここに書く。
- 中身: `components/muse_diy_stopwatch/docs/stopwatch_guide.md`（本体に埋め込み）。2 行目の「版: 2026-11-08.1」を
  ツール説明に入れ、「この版をまだ読んでいなければ会話の最初に 1 回」とした。**中身を変えたら版を上げる。**
- 処理: `ui/msw_doc.c`（msw_doc_version / msw_doc_command）。公式ファイルは noise_control.cpp（登録、先頭に置く）と
  app.c（呼び出し）に数行。ui に idf_component.yml（cjson）を足した。
- 音声認識ガイドの案内は app.doc の先頭から stopwatch.doc に移した。app.doc の先頭は stopwatch.doc への案内。
- ビルド・COM5 に書き込み済み。起動後 ws=up（登録は通った）。heartbeat int=39K/10K min=7K（前の記録より内部 RAM が少ない。
  今回の変更との関係は未調査）。
- 未確認: Muse が会話の最初に stopwatch.doc を読むか、版が同じとき読み直さないか。未コミット。

### 2026-11-08 追記: 内部 RAM が 73K → 39K に減った理由

- 減ったのはミニアプリを入れた版から（logs-2026-11-07 の boot-bss 74K/48K → boot-miniapp1 39K/11K）。stopwatch.doc ではない。
- `esp_idf_size --archives`: libminiapp.a が内部 RAM の .bss に 27,226 B。内訳: msw_app_sound.c の `s_pcm`（16 KB、
  音の再生リング）、`s_sine`（512 B）、アプリ一覧のコピー `list`（msw_app_core.c:851、msw_app_launcher.c:194）と
  `apps`（msw_app_cmd.c:93）が 3,264 B ずつ。
- ほかに msw_apps タスクのスタック 6 KB が内部（FAT への書き込みをするので、そのままにしている）。
- 直し方の案: 上の 5 つに EXT_RAM_BSS_ATTR（muse_audio.c と同じ）。約 26.5 KB 戻る見込み。未着手。
- `>msw.mem` の結果: work/logs-2026-11-07/mem-now.log（free 40,131、largest 10,752）。
- → 直した（同日）: s_pcm と list ×2・apps を EXT_RAM_BSS_ATTR で PSRAM へ。s_sine（512 B）は小さいので内蔵のまま。
  書き込み後: `>msw.mem` free 66,343・largest 49,152、heartbeat int=64K/48K（前 39K/10K）。ws=up。
  アプリの音が前と同じかは未確認（ユーザーに聞く）。ルールを CLAUDE.md と msw-app スキルに書いた。未コミット。

## 音量（2026-11-08）

- 音量 0〜100 → dB は全機種共通（esp_codec_dev の標準: 0 = -50 dB、100 = 0 dB）。機種ごとは hw_gain だけ。
  StopWatch は出荷時デモと同じ（pa 5.0 V / dac 3.3 V → 音量 100 で +3.6 dB）だった。
- ユーザー: 最大でも小さい → board_m5stack_stopwatch.c の hw_gain に pa_gain -6（音量 100 で +9.6 dB、約 3 倍）。
- SanoTTS: msw_tts.c に倍率 s_cfg.gain（既定 TTS_GAIN 1.0、`>msw.tts.cfg gain=` で変えられる）と、
  各文のピーク・割れたサンプル数のログ（`@msw.say made ... peak 0.352, clipped 0`）を足した。
  テスト文のピーク 0.352 → 音量 100 では codec で約 1.06 倍（割れはじめ）。倍率を上げると音量 100 で割れる。
- 不具合を直した: msw_console_add の口が 1 つだけで、ミニアプリが後から登録して `msw.say` / `msw.tts*` の
  シリアルコマンドが効かなくなっていた（ミニアプリを入れた版から）。4 つまで登録できるようにした（ui/msw_mem.c）。
- COM5 に書き込み済み。テストの声を 1 回鳴らした。聞こえ方・割れはユーザーの確認待ち。未コミット。

## device.ota について（調べたこと、2026-11-08）

- StopWatch は OTA が有効（sdkconfig.muse の CONFIG_HOMEHUB_OTA_ENABLED=y）。`device.ota`（url = HTTPS の .bin、force）。
- アプリ領域（ota_0/ota_1、4 MB ずつ）だけを書き換える。読み上げのモデル・辞書のパーティションは変わらない。
- 署名つきの .bin だけ入る（CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT、鍵は公式リポジトリの dev_signing_key.pem）。
  この鍵は公開されている共有の開発用の鍵なので、誰でも署名できる。守りは「Muse にしか device.ota を頼めない」ことだけ。
- 版が今より新しくないと入らない（force=true で無視）。起動して問題がなければ確定、だめなら前の版に戻る（rollback 有効）。
- GitHub の Releases の添付ファイル（HTTPS）なら URL として使えるはず。未試験。

## 読み上げの音量（Voice level、2026-11-08）

- 設定 > Sound の Volume の下に「Voice level」（50〜300%、10% きざみ、既定 100%）。NVS `msw/tts_level`。
  離すと「この大きさで読みます。」と鳴らす。muse_settings_ui.c に weak 宣言とスライダー（museStopWatch の印）。
- msw_tts.c: 声の倍率 = s_cfg.gain × Voice level。本体の音量と codec の増幅（音量 100 で +9.6 dB）から
  割れない上限を出し、その 0.6 倍より上を tanh でやわらかく抑える（ソフトリミッター）。
  ログ: `@msw.say made ... peak X (limit Y), limited N`。
- COM5 に書き込み済み。ユーザーが実機で「よい感じ」と確認（2026-11-08）。未コミット。
- OTA の署名: このビルドも SDK の esp32/dev_signing_key.pem で署名している（CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES）。
  本体は動いているファームに入っている公開鍵で新しい .bin を確かめるので、自分のビルドは OTA で入る。
  鍵を自分のものに替えるなら、最初の 1 回は USB で書く必要がある。

### 2026-11-08 追記: ミニアプリの音が鳴らなくなる不具合

- 原因: msw_tts.c が話すたびに muse_voice_set_player(&s_player) で自分のプレーヤーを入れ直し、
  ミニアプリの音を重ねるラッパー（msw_app_sound.c、起動時に msw_sound_init で差し込む）が外れていた。
  一度でも読み上げる（Muse の返事、voice.say、Voice level の見本）と、再起動までアプリの音が出なかった。
- 直した: 読み上げのプレーヤーは起動時（msw_tts_register のコンストラクタ）に 1 回だけ入れる。
- COM5 に書き込み、`msw.say` で読み上げた後に pyoko を起動。音が鳴ることをユーザーが確認（2026-11-08）。未コミット。
