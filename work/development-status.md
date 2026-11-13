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

## 自分の署名鍵と OTA の取得先（2026-11-08）

- SDK の dev_signing_key.pem は共有のサンプル鍵（公式の最初のコミットから。sdkconfig.defaults に「出荷前に替える」）。
  ユーザー判断で自分の鍵に替えた: `.tools/keys/msw_signing_key.pem`（RSA-3072、espsecure で生成、git 管理外）。
- dev.ps1: 鍵があれば `.tools/keys/sdkconfig.signing` を作って SDKCONFIG_DEFAULTS に足し、既存の
  build/sdkconfig の CONFIG_SECURE_BOOT_SIGNING_KEY も書き換える。鍵がなければサンプル鍵のまま。
- ビルドした muse-gadget.bin は自分の鍵で検証 OK、サンプル鍵では失敗（espsecure verify-signature）。
  COM5 に USB で書き込み、ws=up。以後の OTA は自分の鍵で署名したものだけが入る（未試験）。
- stopwatch_guide.md（版 2026-11-08.2）に「ファームの更新」: 取得先は haiatto/museStopWatch の Releases の
  muse-gadget.bin だけ、ほかの URL は実行前にユーザーに確かめる。CLAUDE.md と README に派生版の手順。
- Releases にはまだ何も上げていない。

### 2026-11-08 追記: stopwatch.doc が Muse で「不明なエラー」

- Muse の報告: コマンド一覧には出る（17 個、版 2026-11-08.2）が、呼ぶと不明なエラー。
- 原因: 結果は `{ok, payload}` の形が必要（noise_control.cpp の wrap_result_json は ok・payload・error だけを送る）。
  msw_doc_command は `{version, content}` を直接返していたので、中身が捨てられていた。
- 直した: `{ok: true, payload: {version, content}}`。Muse が本文を読めたことをユーザーが確認（2026-11-08）。
- Muse の指摘: ガイドの voice.configure は一覧にない（StopWatch は CONFIG_HOMEHUB_VOICE オフ）。ガイドから外し、
  音量は本体の 設定 > Sound と書いた（版 2026-11-08.3）。COM5 に書き込み済み。
- Muse の報告では、ファームの版が 999.0.0 と出ている（esp_app_desc の version）。OTA の「新しい版だけ」の判定に関わるので、
  Releases に上げる前に版の付け方を決める必要がある。

### 2026-11-08 追記: Muse が作った dance（おどるアバター v6）で本体が落ちる

- app.log: `dance was running at the last restart (it crashed the device)`。Muse は dance を v6 まで、piano（かんたん鍵盤）も作った。
- シリアルで再現: 起動直後に LoadStoreError。msw_gfx_text ← msw_text_width ← アプリのティック（msw_api_tick）。
  EXCVADDR 0x4239ed8e = アプリの命令の領域の中（entry 0x4239ed58）。命令の領域は 1 バイトずつ読めない。
- 文字列がなぜ命令の領域にあるかは未確定。elf_loader は .got.loc があると .text から .got.loc までを命令の領域に置き、
  その間に .rodata が入ることがある（esp_elf.c の「The gap can contain .rodata」）。dance のソースを見て確かめる。
- 案: ソースを手に入れて ELF を調べ、build.py か実行環境で直す。あわせて、文字列を受け取る API で読めないアドレスなら
  落ちずに app.log に書くようにする。未着手。

## アプリのソースをパッケージに入れる（2026-11-08、ユーザーの案）

- sdk/build.py: ELF の後に "MSWS" + 長さ + `--- name ---` 区切りの app.json・.c・.h（256 KB まで）。
- 本体: check_package はぴったりの大きさか、この形を受け付ける。`msw_app_source()`（サービスタスクで読む）、
  Muse の `app.source`（id、offset でページ）、シリアル `msw.app.src <id>`。guide.md・stopwatch_guide.md（版 .4）・スキルに書いた。
- 実機: pyoko を入れ直し、`msw.app.src pyoko` で返ったソースが手元の main.c と同じ。dance（前に入れたもの）は no_source。
- dance の原因調べは、Muse に新しい build.py で作り直して入れてもらってから（app.source でソースが取れる）。

## Muse 向けのデバッグ支援（2026-11-08）

- クラッシュ報告（msw_app_crash.c）: `-Wl,--wrap=esp_panic_handler` で panic を先に受け、RTC_NOINIT に
  アプリ id・EXCCAUSE・EXCVADDR・理由・タスク名・バックトレース 12 段・直前のログ 4 行を残す。
  次の起動で app.log に `CRASH:` の行（原因の名前、ヒント、アプリの行は ELF アドレス、ファームの行は
  近い書き出し関数、ファームの ELF sha256）。実機: わざと NULL に書くアプリで StoreProhibited、
  `app crashtest, ELF 0x22b` → addr2line で boom/tick の行が出た。シリアルのスタックあふれ（下）も報告された。
- build.py: `-g` で組み、`build/<id>.elf`（シンボル付き、同じアドレス）を残す。`.so` は strip。
- ガード: 文字列・バッファを受ける関数（msw_log、msw_gfx_text、msw_text_width、msw_sprite_from_art、
  msw_pcm_write、msw_say、msw_store_put/get、msw_bubble）は `msw_api_readable/writable` を通す。
  読めないポインタは無視して 1 行、コード内のポインタはデータ側のコピーを読む。lvgl の関数は対象外。
- 止まったティック: esp_timer が 1 秒ごとに見て、3 秒戻らないと 1 行。300 ms を超えるティックも 1 行（3 回まで）。
  タスク WDT は無効なので、待つループは再起動しかない。
- `app.screenshot`（msw_app_shot.c）: フレーム描画ならフレーム＋オーバーレイ、それ以外は lv_snapshot
  （CONFIG_LV_USE_SNAPSHOT=y を追加）。esp_new_jpeg で 466x466 の JPEG（品質 70、約 6〜23 KB、150〜210 ms）。
  16 KB の PSRAM スタックの専用タスクで作る（シリアルのタスク 3.5 KB で lv_snapshot がスタックあふれ →
  クラッシュ報告で見つけた）。シリアル `msw.app.shot` は base64 の行で出す。
- **dance が落ちた原因**: elf_loader が `.text` の大きさを 4 バイトに切り上げて（esp_elf.c:334
  `ELF_ALIGN(text_size, 4)`）区間を決めるので、`.text` が奇数の長さで、すぐ後の `.rodata` が 1 バイト揃え
  （文字列だけ）だと、最初の文字列がコード（I バス）側に再配置される。バイト読みで LoadStoreError。
  managed_components は git 管理外なので直さず、build.py で `.rodata` に 4 バイト揃えの定数を足して
  （`-Wl,-u,msw_rodata_align`）揃え、組み立て時に配置を確かめる。古いアプリは、ガードが本当の .rodata の
  コピーに読み替える（実機: 前の dance が落ちずに「テストちゅう」を描いた）。
- ガイド: guide.md、stopwatch_guide.md（版 .5）、スキルに書いた。
- ランチャーのタイトルの右にスピーカーの状態（オン = 黄の音のマーク、オフ = 灰の消音マーク）。ユーザーの要望
  （アプリ選択の画面では黄ボタンが効くのに、状態が見えない）。0.4 秒ごとの poll で合わせる。実機での見え方は未確認。

### 2026-11-08 追記: アプリメニューで Muse と話す（アイデア 006）

- アプリメニューを開いている間は、青ボタンが Muse の「話す」（msw_muse_talk）。離したときは押したときの
  行き先に送る（Muse の入力タスクは、話し始めたら離しを自分で受ける）。
- メニューの中（BOX 320x272、全部箱の中: フレーム描画で箱の外を描き直さないため）に、
  本体の Muse を 96px と吹き出し。最初は手描きの顔だったが、ユーザーが「ダサイ」→ 本物の Muse に。
  - 縮小は公式の顔画面と同じやり方: lvgl のデコーダーが描くときに 16 行ずつ縮める
    （muse_pixel.c に `muse_pixel_scale_px` を追加、顔画面の muse_pixel_set_size には触らない）。
    絵のバッファは持たない。帯のバッファ（96x16x2 = 3 KB を描画ユニットの数）はメニュー中だけ。
  - 顔画面はアプリ中は描かない（frame_tick がランチャーのタイルで抜ける）ので、メニューが 40 ms ごとに
    muse_pixel_render を呼ぶ（lvgl のロックの中）。
  - 吹き出しは muse_state の字幕。状態の英語表示は「きいてるよ…」「かんがえちゅう…」に。
    何も言っていない間はヒント（青ボタンを おして はなしてね／つながっていません）。
- シリアル `>msw.app.menu`（黄ボタン 1 回押しの代わり）を追加。`msw.app.shot` は lvgl のトップレイヤー
  （メニュー）も重ねて撮るようにした。
- 確認: 撮影でメニュー・Muse・吹き出し、`face=listening/speaking` でポーズが変わること、開け閉め 3 回で
  落ちないこと。**未確認**: 実機で青ボタンを押して話す・返事の字幕・声（ユーザーに確認してもらう）。
- 気づき: Muse の LISTENING で muse_ui_show_face() が裏の顔画面のタイルを顔に動かす。アプリから戻ると
  ランチャーでなく顔画面になるかもしれない（未確認）。
- 注意: シリアルのコマンドは先頭に `>` がいる。`>` なしの文字は 1 文字ずつキー（m = MP3 テスト、
  p = 画面の吸い出し、a/s = メニュー、d/u = 話す）になる。

### 2026-11-08 追記: app.screenshot のメモリ不足

- ユーザー: アプリ中に Muse に画面を見るよう頼んだら「メモリ不足で撮れない」。
- 原因: アプリ中の PSRAM の空きは約 780 KB（アプリなし 1.2〜2.3 MB、Muse の会話中はさらに減る）。撮影は
  画面全体 434 KB ＋ JPEG 192 KB を一度に取り、メニューを開いていると、さらにトップレイヤー用の ARGB 868 KB を
  取っていた（後者は黙って失敗し、メニューが写っていなかった）。
- 直し: 16 行ずつ作る（フレームの行のコピー、または lvgl に帯だけ描かせる。lv_snapshot と同じ手順を帯の
  範囲で行い、トップレイヤーも同じ帯に重ねて描く）→ esp_new_jpeg のブロックモード。要るのは帯 15 KB、
  エンコーダーの入力 15 KB、JPEG 128 KB だけ。
- ブロックモードの注意: 466 行だと 29 ブロック目で JPEG_ERR_OK（完成）。それ以上送ると新しい画像が始まる
  （黒い画像になった）。最後の 2 行は捨てられ、下端に少しごみが出る（円の外側なので放置）。
- 実機: アプリなし（顔画面 lvgl、約 270 ms）、ぴょこの森、まるい水槽のメニュー中（約 180 ms、メニューも写る）。
  Muse と話している最中の撮影も、ユーザーの試験で成功した（ログで確認）。

### 2026-11-08 追記: app.screenshot の待ち時間と半分の大きさ

- 測定（アプリ中、青ボタンで「画面を見て」、PC の時刻つきでシリアルを記録）: 全体 58.5 秒。
  離してから ack まで 1.9 秒、ack から Muse が `app.screenshot` を呼ぶまで 28.5 秒、
  本体の撮影と 30 KB の送信 0.4 秒、画像を返してから返事が始まるまで 15.6 秒、読み上げ約 7 秒。
  ほとんどは Muse 側の時間。
- 返事の形は公式の `camera.capture` と同じ（`format: jpeg-base64`、`data_base64`）。公式も文字で返していて、
  道具の返事を画像として渡す仕組みは SDK にない。Muse は「文字として届いている」と言った。
  画像として渡せる入り口は、チャット送信（/chat/stream）の `{"type":"image",…}` だけ
  （Linux 版の `send_image`。ESP32 版は "Photo attachments to voice messages are not included."）。
- 直し（ユーザーの案）: 既定を半分の大きさにした。232 x 232（2x2 の平均、偶数にそろえる）、画質 40。
  `size: "full"` で 466 x 466、画質 70（前と同じ）。説明に「文字で届くので既定は小さい」
  「画面を見てと言われたらすぐ呼ぶ」を足した。stopwatch_guide.md は 2026-11-08.8。
- 実機（シリアル `>msw.app.shot` / `>msw.app.shot full`）: 顔画面 半分 4.6 KB・122 ms、全体 21.6 KB・293 ms。
  まるい水槽 半分 約 5 KB・710 ms、全体 約 21 KB・1.5 秒（アプリが描いている間、帯ごとに表示のロックを待つため。
  メニュー中はアプリが止まるので約 180 ms）。base64 は約 6.7 千字（全体は約 2.9 万字）。
  ユーザーの試験: 「短くなった感じがする」（体感。秒数は測っていない）。
- `/fs/write`（VM へのファイル書き込み）は 403 "route not allowed for this credential's scopes"。
  本体の資格では使えない（読むだけの `/fs/raw/` は使える）。試したコードは消した。

### 2026-11-08 追記: 返事を待つ間の Muse の状態（アイデア 008）

- ユーザーの指示: アバター画面の状態表示とアプリメニューのミニアバターで出す共通の処理として。
- 仕組み: muse_chat_session.cpp が `agent.status` の `activity_code` と `activity_text` を
  `muse_state_set_activity` に渡す（返事待ちの間だけ）。muse_state.c が日本語の1行にする
  （公式 12 個の訳は kantanMuse と同じ、書き直し）。muse_voice.c の hatch_reply が、返事が始まるまで
  それを字幕にする（今までの NOTE SENT - WAITING FOR MUSE の代わり）。アバター画面の字幕も
  アプリメニューの吹き出しも字幕を読むので、両方に出る。テキストチャット（`chat=`）には出さない。
- 実機のログ（12 回の会話）: 説明は「Updating avatar」「Searching SDK」「Installing app」「Transcribing voice note」
  など英語の短い句。1 回の返事待ちで 2〜6 回変わる。
  - 説明が「is working」「is responding」のときがある → 名前の繰り返しとして出さない。
  - 公式の一覧にない `making_something` が来る → 「作成中」と訳した。
  - `online`（終わって待機）は返事が始まった後に来るので、字幕には出ない。
- ユーザーの試験: アバター画面で、長い1行が電源ボタンのアイコンにかぶって見切れた。考え中の字幕は
  中央ぞろえの最大 18 文字ほどの箱（s_reply_lbl、ANSWER_HEARD）に入り、返事はページに切って入るが、
  この1行は切っていないので1行目が端まで伸びた。→ 日本語の名前だけを1行目、説明を2行目からにした
  （「作成中…」/「Generating animation frames」）。シリアル `>msw.caption <文字>`（`\n` で改行）を足し、
  撮った画面でアイコンにかからないことを確かめた。3 行を超える説明は切れる（ログの説明は 2〜4 語）。
- **未確認**: 直した後の実際の会話での見え方。


### 2026-11-08 追記: アプリの起動でソースを読まない

- ユーザーの指摘: 起動のたびに .mswapp を丸ごと（ソース込み）PSRAM に読んでいた。理由はパッケージ全体の CRC の
  確認だけだった。
- 直し（msw_app_core.c の load）: ヘッダーを読み、ELF の部分だけを読む（アイコンとソースはフラッシュのまま）。
  magic、大きさ、API の版、使う関数（check_symbols）は起動時も確かめる。CRC は入れるとき（install）だけ。
- 実機: dance（パッケージ 669 KB）が 65 ms で起動、aquarium・pyoko は 1〜2 ms。`>msw.app.src` も読める。

### 2026-11-08 追記: アプリの数と置き場（Muse が入れ替えて使う）

- ユーザーの方針: 置き場は 4 MB のままでよい。いっぱいになったら Muse がバックアップして入れ替える。
- 今の上限: 48 個（MSW_APP_MAX_APPS、ランチャーはスクロールする）、1 つ 1 MB、置き場は FAT で 4020 KB。
  配置表の末尾に約 2 MB の空きがある（広げるなら 6 MB まで。中のアプリは消える。今はしない）。
- 足したこと: `app.list` に `free_bytes`・`space_bytes`・`max_apps`・`max_app_bytes`。
  入れる前に確かめて、49 個目は `too_many_apps`、空きが足りなければ `no_space`（必要と空きの KB を書く）。
  前は 49 個目が入っても一覧に出なかった。guide.md に「ユーザーに聞いてから app.source で保存して app.remove」。
  シリアルの `>msw.app.list` も空きを出す。
- 実機: 9 個で 2468 KB 空き / 4020 KB。aquarium の入れ直し（更新）は通った。no_space・too_many_apps は未試験。

### 2026-11-08 追記: 会話の後も Muse の状態を出す、アプリ削除の確認

- ユーザーの指示: 会話が終わった後も、対象のチャットで Muse が作業している間は状態を見たい。
- 原因: 購読（/chat/subscribe）はこの時計のチャットだけだが、返事待ち（P_WAIT_REPLY）以外のイベントは捨てていた。
- 直し（muse_chat_session.cpp の msw_background_activity）: 会話の外の `agent.status` も
  `muse_state_set_activity` に渡し、時計が待機中（MUSE_MODE_IDLE）なら字幕にする（2 分の hint。
  `muse_state_set_hint_ms` を足した）。`online` / `idle` が来たら、自分が出した字幕のときだけ消す。
  アプリメニューの吹き出しも、開いたときに作業中なら出し、状態が消えたら案内に戻る。
- 実機のログ: 会話の後、約 7 分で 37 回の状態が届いた。説明は日本語（「ビルドを確認中」「Todoを更新中」
  「アプリを再追加中」など）。1 回「⁨Edit an⁩を実行しました」に文字の向きの記号（U+2068/2069）が
  入っていた → 状態の1行から U+200E/F・U+202A〜E・U+2066〜9 を取り除く。
- ランチャーの削除: 長押しの ✕ で、すぐ消していた → 「「名前」を本当に消しますか？ 元には戻せません」と
  「やめる」「けす」を出すようにした（ユーザーの指示）。
- ユーザーが画面で見た（会話の後の字幕、削除の確認。問題の報告なし）。
- **未確認**: online で字幕が消えるか（ログの間に online が来なかった。来なければ 2 分で消える）。

### 2026-11-09 追記: 009 アバターの差し替え

- ユーザーの決定: A（ELF は 64x64 のマス目と色表を描くだけ。拡大はファームのまま）。
  戻し方と選ぶ画面は、提案どおり（落ちる・重いと自動で公式へ。一覧は「公式」が一番上）。
- 作り:
  - API（msw_app.h）: `msw_avatar_define(&def)`、`def.render(pose, grid, palette)`（64x64、256 色）、
    `def.accent(mode)`（なくてよい）、`MSW_MODE_*`。app.json に `"kind": "avatar"` →
    `sdk/build.py` が magic "MSWV"、アイコンなしで作る。`msw_` は msw_avatar_define だけ（build.py と
    時計の check_symbols の両方で断る）。
  - 置き場: `<id>.mswava`（ランチャーと app.list には出ない）。16 個まで。app.install・app.remove・
    app.source はアプリと共通。ELF の読み込みは load_elf にまとめた（アプリと共通）。
  - 描画: avatar/muse_pixel.c の muse_pixel_render の頭で `msw_avatar_hook_render`（weak）を呼ぶ。
    拡大（muse_pixel_scale・scale_px）が使う色表をポインタにして、アバターの 256 色に向ける。
    ふち取りの暗い色（0.72 倍）はファームが毎フレーム作る。accent も `msw_avatar_hook_accent`。
    **Muse が書き換えた muse_pixel.c（components/muse/avatar/）を使うときは、このフックも要る**。
  - msw_avatar.c: 読み込み・入れ替え（s_lock の下で差し替え、古いものは後で解放）、選んだものは NVS
    （"msw" / "avatar"）。描いている途中を RTC_NOINIT に記録 → 落ちて再起動したら公式に戻して app.log に書く。
    3 秒の平均が 25 ms を超えたら公式に戻す（解放と NVS はサービスタスクで非同期: msw_app_service_async）。
  - 選ぶ画面: muse_ui.c の文字盤の下にタイル（文字盤から上にスワイプ。weak の msw_avatar_build）。
    「公式」＋名前の一覧、タップで切り替えて文字盤に戻る。
  - Muse のコマンド: `app.avatar_list`（current、draw_ms、avatars）、`app.avatar_use`（id / "official"）。
    シリアル: `>msw.app.avatars`、`>msw.app.avatar <id|official>`、試験用に `>msw.wake`（画面を起こす）。
  - 見本: apps/shizuku（しずく、オリジナル）。1 フレーム 3.5 ms（最初は 5.5 ms。ソフトの割り算と fmodf を減らした。
    浮動小数の割り算は __divsf3 でソフト）。
- 実機の試験（シリアル）:
  - しずくの入れ替え・上書き（表示中なら描き直す）、文字盤とアプリメニューの小さな Muse の両方に出る（スクリーンショット）。
  - 重いアバター（37 ms）: 起動時に NVS から読み、3 秒で公式に戻った。
  - 落ちるアバター: 再起動後「avatar avacrash crashed the device (in its render)」で公式から始まった。
  - msw_log を呼ぶアバターは build.py が断る。
- **未確認**: 上スワイプの一覧の見た目と操作（ユーザーに見てもらう）、Muse が作るところ。

### 2026-11-09 追記: 009 を公式のアバター作り（avatar.py）に揃える

- ユーザーの指示: 公式スキルとの差（指示の細かさ、全状態の確認、PC での検査、話す身ぶり）を 4 つとも埋める。
  Muse 側は「スキル」の代わりに、手順書＋VM の検査＋時計のプレビューで同じ流れにする。
- 公式の流れ（tools/muse/AVATAR_RECIPE.md、avatar_prompt.md、avatar.py）: PC が Muse に avatar_prompt.md を送る →
  返った muse_pixel.c を PC で -Wall 付きで組み、全アニメーションを ASan で動かし、状態ごとの GIF を作る →
  失敗は Muse に返して 2 回まで直させる → ファームを組んで書き込む。
- 足したこと:
  1. `app.avatar_doc`（miniapp/avatar_guide.md、埋め込み）: avatar_prompt.md を今の仕組みに合わせて書き直した手順書。
     誰を描くか（Muse 自身のアバター、既存キャラ・Jollybot は不可、先頭コメントに名前・形・色・性格）、手順
     （build.py → all.png → app.install → app.avatar_preview で 9 状態 → app.avatar_use → 直す）、API、見た目
     （部品で組む、Bayer、1 px 輪郭、色の役割表、状態ごとの色と 1-exp(-dt*7) の混ぜ方）、全状態の動き、
     muse_speaking.h の使い方、速さの決まり（ピクセルループは整数、割り算はソフト）。guide.md は短い案内だけにした。
  2. `app.avatar_preview`（id、state、frames、seconds、scale、quality）: アバターをもう 1 つ読み込み、指定の状態を
     25 fps で動かし（前に IDLE 0.5 秒、声の大きさは作り物）、コマを並べた JPEG と draw_ms を返す。画面とは別なので、
     眠っていても、選んでいないアバターでも見られる。既定は 4 コマ 2 倍で 256x256、画質 60（約 9〜18 KB）。
     最初は 3 倍・画質 75 で 30〜48 KB になり、Muse が文字で読むには大きすぎたので下げた。シリアルは
     `>msw.app.avatar_preview <id> <state> [frames]`。
  3. `sdk/build.py` の PC での試し実行: `"kind": "avatar"` のとき、VM の cc/gcc/clang で `sdk/avatar_host.c` と組み、
     アドレスと UB のサニタイザー付きで 9 状態を時計と同じポーズで動かす。書き過ぎ・0 割りなどで止まり、全部黒い状態も失敗。
     `build/preview/all.png`（1 行 1 状態）と状態ごとの PNG（Python だけで書く。PIL なし）。コンパイラがなければ飛ばす。
     `MSW_HOST` のとき msw_app.h は lvgl を読まない（使う型は lv_obj_t と lv_font_t だけ）。
  4. `muse_speaking.h`（公式の話す身ぶり、ヘッダーだけ、Apache）を tools/make_sdk.py で sdk/include にコピー。
- しずくを手順書どおりに作り直した（状態ごとの色の混ぜ方、ランダムなまばたきと視線、小さな手、muse_speaking の身ぶり、
  全状態の動き、ピクセルループは整数）。時計で 1 フレーム平均 1.8〜2.0 ms、最大 7.4 ms。9 状態ともプレビューで確認。
- **未確認**: build.py の PC での試し実行（この PC にも WSL にも C コンパイラがない。avatar_host.c と shizuku.c が
  MSW_HOST で通ることは Xtensa の gcc で確かめた）。Muse が手順書どおりに作れるか。上スワイプの一覧の見た目。

## 呼び名を「Museストップウォッチ」に（2026-11-09、ユーザー決定）

- Muse 向けの文書（stopwatch_guide.md 版 2026-11-09.3、guide.md、avatar_guide.md）と
  stopwatch.doc の説明から「時計」をなくし、「Museストップウォッチ」か「本体」にした。
  ガイドの冒頭に「時計の機能はない。時計とは呼ばない」と書いた。
- そのまま: Muse アプリの一覧に出る名前（main/app.c の noise_ctrl_init の "Muse DIY StopWatch"）。
  変えるとアプリ側の表示が変わるので、ユーザーに聞いてから。

## Muse からサイドチャットの部屋を足す（2026-11-10、ユーザーの案）

- Muse のコマンド `side.add`（id、name は任意）: 一覧になければ足す。あれば足さない（name があれば名前だけ変える）。
  話す先は切り替えない（会話の途中で切り替えると、その返事の接続が切れるため）。
  `side.list`: 一覧と今の話す先。処理は msw_side.c の msw_side_put、コマンドは msw_doc.c。
- 説明・エラーは日本語。ガイド版 2026-11-10.1。

## ミニアプリのマイク（2026-11-10、ユーザーの案）

- API: msw_mic_on(bool)、msw_mic_read(buf, max)、msw_mic_level()（0〜100、来ていないとき -1）。
  miniapp/msw_app_mic.c（PSRAM のリング 1 秒）。muse_voice.c の idle_capture と play_player から弱参照の
  msw_mic_tap で渡す。アプリが終わると切れる（msw_mic_app_end）。
- 確認済み（シリアル）: apps/mimi で毎秒約 16000 サンプル、静かな部屋で振幅 50〜70。画面写真で表示。
- 未確認: 話し声での反応、青ボタンの録音・再生（スピーカーがオフだった）、音を鳴らしながらの読み取り、
  画面が消えたときの -1。
- 半分サイズの画面写真で、画面下の文字が二重に写った（原寸では正しい）。shot の縮小を後で見る。

## ミニアプリの時刻・電池・歩数・PORT.A（2026-11-10、ユーザーの案）

- API: msw_time、msw_battery、msw_steps、msw_port_power、msw_i2c_begin/end/scan/write/read/write_read。
  miniapp/msw_app_port.c、ui/msw_time.c、ボードに RTC（RX8130CE 0x32、UTC）・Grove 5V（PMIC PWR_CFG bit3）・歩数。
  pmic_update をミューテックスで守った（電源 LED と 5V が同じレジスタ）。
- 確認済み（シリアルと画面写真）: 起動 4.5 秒で SNTP が合わせて RTC に保存。apps/sensors で時刻・電池 100% 4.18 V・
  USB 給電・歩数 0・PORT.A 何もなし、を表示。内部 RAM は heartbeat int=60K/34K で変わらず。
- 未確認: 再起動後に RTC から時刻が戻るか（ネットなしで）、歩いたときの歩数、PORT.A に実際の Grove センサー、5V の出力。
- 背面の拡張ピン（2.54 mm 7P・6P）は回路図でピンを確かめてから。今回は入れていない。

## ウェイクワードの検討（2026-11-10、ユーザーの質問。まだ作っていない）

- SDK に仕組みなし（register の is_wakeup_supported は false）。候補は WakeNet9（ESP-SR、S3 で 1 フレーム 32 ms に約 3 ms、
  内部 16 KB・PSRAM 約 320 KB、既製は英中、日本語は Espressif に依頼）と microWakeWord（モデル約 60 KB、作業領域 23 KB〜、
  既製は英語、自前で学習可）。ファームは 4 MB の枠に約 3.5 MB。
- 電池で画面が消えている間に聞くと休止（コーデック off・light sleep）ができない。設定案: オフ / 画面がついている間 /
  ＋USB 給電中はいつでも（初期値の案）/ いつでも。言い終わりの判定（無音で送る）が新しく要る。

## ウェイクワードの試作 1（2026-11-10、microWakeWord）

- 部品: components/muse_diy_stopwatch/wakeword（msw_wake.cc）。espressif/esp-tflite-micro 1.4.2、
  esphome/esp-micro-speech-features 1.2.3（ESPHome と同じ前処理）。モデルは esphome/micro-wake-word-models の v2
  （hey_jarvis、okay_nabu、vad。Apache-2.0、models/LICENSE・NOTICE）。alexa は商標なので入れていない。
- 音: muse_voice.c の idle_capture から弱参照 msw_wake_feed で渡す（待機中だけ。話している間・返事の間・休止中は来ない）。
  PSRAM のストリームバッファ 1 秒 → 専用タスク（PSRAM スタック、優先度 3）で 10 ms ごとに特徴量、3 スライスごとに推論。
- 今はシリアルだけ: `>msw.ww on|off|stat`。反応すると `@msw.ww detected <名前>` と 120 ms 振動。初期値はオフ。
- 実測: 推論 1 回 hey_jarvis 2.7 ms・okay_nabu 2.1 ms・vad 1.3 ms（30 ms ごと）。msw_cpu で msw_wake 25%（3 モデル）。
  取りこぼし 0。内部 RAM はほぼ増えない（前処理の 11 KB は heap_caps_malloc_extmem_enable(0) で PSRAM へ）。
  ファーム 3.87 MB（4 MB の枠、+約 0.33 MB）。
- 未確認: 実際に「Hey Jarvis」「Okay Nabu」と言って反応するか、誤反応の頻度、画面が消えた USB 給電中の動き。
- 次: 設定（オフ / 画面がついている間 / 画面がついている間＋USB 給電中。電池でいつでもは作らない。初期値オフ）、
  反応したら話し始め、VAD で言い終わりを判定して送る。日本語の言葉は学習が要る。

## ウェイクワードの試作 2（2026-11-10、設定と話し始め）

- 設定: 本体の Settings > Wake word（Off / Screen on / Screen on + USB）。NVS の msw 名前空間 `wake_mode`、初期値 Off。
  制御タスク msw_wake_ctl（PSRAM スタック）が 0.5 秒ごとに画面（muse_state_asleep）と USB（muse_state_on_battery）を見て入れ切りする。
  Off の間は通知を待つだけで動かない。最初に入るときにモデルを読む。
  ページは ui/msw_wake_ui.inc（muse_settings_ui.c が include、ウェイクワード部品へは弱参照）。Web 設定にはまだない。
- 反応したら: `muse_input_app_talk(true)`（黄ボタンを押したのと同じ）＋ 120 ms 振動。そこからは VAD だけ動かし、
  話し声の後 0.9 秒黙ったら離す。話し声が 6 秒来なければ・14 秒たったら離す。Muse メニューが開いている・ミニアプリがマイクを使っているときは始めない。
- 音: 待機中は idle_capture → msw_wake_feed。話している間は record() と run_call() → 弱参照 msw_wake_turn_feed
  （通話が待ち状態 READY のときはウェイクワードも聞く。Muse がしゃべっている間は聞かない）。
- okay_nabu は外した（フラッシュ節約）。ファーム 3.63 MB、残り 380 KB。
- シリアル: `>msw.ww off|screen|usb|test|stat`（off/screen/usb は設定を保存する。test は言葉を聞いたことにして話し始める）。
- 実測: `>msw.ww test` で PTT down → 録音 → 話さずに 6.0 秒で離した（no speech）。内部 RAM は int=59K のまま。
- 未確認: 「Hey Jarvis」と言って話し始め、言い終わりで送られるか。画面の表示。リアルタイム通話（PTT 通話）側での動き。
  USB 給電で画面が消えているときに起こして話せるか。
- 気になる点: 話さずに終わった回も録音を Muse に送ってしまう（音声メモのとき）。取り消す手段を後で考える。
- 次: 「Hey Muse」のモデルを学習させる（microWakeWord の学習手順。英語なので合成音声は Piper のままでよい）。

## ウェイクワード「Hey Muse」（2026-11-10、自前で学習）

- 学習は WSL（Ubuntu 24.04）の `~/msw-wakeword`、RTX 4070 SUPER。手順は microWakeWord の basic_training_notebook に合わせ、
  スクリプトは tools/wakeword/（setup.sh → data.py → gen.sh → features.py → train.sh、env.sh は学習環境の読み込み）。
  sudo なし。uv を ~/.local/bin に入れ、Python 3.10 の環境を 2 つ（.venv-piper: torch と piper、.venv: TensorFlow）。
- 詰まった所:
  - torch（CUDA 13）と tensorflow[and-cuda]（CUDA 12）は同じ環境に入れるとライブラリがぶつかる → 環境を分けた。
  - TensorFlow 2.21 は libcusolver.so.11 を自分で見つけられない → env.sh で nvidia/*/lib を LD_LIBRARY_PATH に足す。
  - datasets 4 は音声の読み込みに torchcodec（と FFmpeg）が要る → datasets<4。
  - AudioSet は tar から parquet に変わった（bal_train の 09 と 19 を使用）。piper-sample-generator はパッケージ形式に変わった
    （`python -m piper_sample_generator`、PYTHONPATH にリポジトリ、webrtcvad の代わりに webrtcvad-wheels）。
  - 学習には tensorboard が要る。
- データ: 合成音声 5000（"hey muse" と "hey, muse" 各 2500、LibriTTS-R の話者を混ぜた声）。背景は MIT RIR 270・AudioSet 1000・FMA 210。
  否定例は kahrendt/microwakeword の speech・dinner_party・no_speech（展開後 25 GB）。特徴量 4 GB。WSL の作業場所は全体で約 38 GB。
- 学習: 10000 ステップ、約 40 分。テスト（合成音声と dinner_party_eval）: しきい値 0.97 で聞き逃し 6.4%・誤反応 0 回/時、
  0.94 で 4.8%・0.19 回/時。実際の声でどうかは未確認。
- ファーム: models/hey_muse.tflite（62 KB、arena 30000 のうち 26124 使用）を hey_jarvis と入れ替え。しきい値 0.97。
  推論 1 回 2.1 ms（最大 12 ms）。ファーム 3.69 MB、残り 316 KB。
- 実測（本体、静かな部屋で 1 分）: 誤反応 0、取りこぼし 0。
- 注意: 背景音データの一部は非商用ライセンス（ノートブックの注意書き）。学習したモデルを公開ファームに入れてよいかは要確認。
- C ドライブの空きが約 5 GB。WSL のディスクは中で消しても縮まない。
- 未確認: 実際に「Hey Muse」と言って反応するか、話し始め・送信、誤反応の頻度（会話・テレビ）。

## 返事の TTS がしゃべらないことがある（2026-11-10）

- ユーザー: 「返答のttsが喋らないことがある」（ウェイクワードと関係なく起きる）。
- ログ: `@msw.tts error no memory: arena 121872 B in psram (largest free 120832)` → `muse reply: 0.00s of audio`。
  そのとき heartbeat は psram=710K（起動 2 時間後。起動直後は約 1.7 MB）。アプリは動いておらず、アバター zundacx は読み込み済み。
  空きの量はあっても、ひと続きの 119 KB が取れなかった（細切れ）。
- 直し: sanotts/msw_tts.c は、PSRAM の作業領域（arena、ids、pull_us）を起動時に取って持ち続ける（s_keep）。
  ベンチと読み上げが同時なら、後の方が自分で取る（s_keep_busy）。PSRAM を約 120 KB 常に使う。
- 確認: 書き込み後 `>msw.say` で 2 文しゃべった（ログ）。2 時間後に起きる状態はまだ再現・確認していない。
- PSRAM が減っていた件（同日調査）: `>msw.mem psram`（PSRAM の使用ブロックの一覧、8 KB 以上はアドレス付き）を足して比べた。
  起動 1 分後 used 3.01 MB・free 1.57 MB（最大 1.53 MB）→ 10 分後（会話 4 回）used 4.02 MB・free 551 KB（最大 483 KB）。
  増えたのは 786432 B（TTS のモデルを PSRAM に写したもの）、192512 B（TTS のリング 6 秒）、30720 B（変換フィルター）で、
  どれも最初の読み上げで 1 回だけ取って持ち続ける設計どおりのもの。リークではない。
  2 時間後の 710 KB もこの分でほぼ説明がつく。それより長い時間で少しずつ減るかは未確認（heartbeat の psram= で見られる）。
- 定常の空きは約 550 KB（TTS の作業領域を常に持つようにしたので、そこからさらに約 120 KB 少ない前提）。
- 小さい音声バッファ（2026-11-10、ユーザー指示）: `CONFIG_MUSE_HATCH_COMPACT_BUFFERS=y` を devices/sdkconfig.musestopwatch に足した。
  返事の MP3 512→128 KB、話しはじめのマイクのため置き 8→2 秒、デコード済みの音 2→1 秒。
  起動後・TTS 前の psram= が 1584K → 2189K（約 600 KB 増）。`>msw.mem psram`: free 2188356（最大 2149732）。
  TTS が 1 回しゃべった後の定常は約 1.15 MB の見込み（未測定）。長い返事・つながるまでの長い発話で音が切れないかは未確認。
- 日本語フォントをフラッシュへ（2026-11-10、ユーザー指示）: ファームは PSRAM から動く（XIP）ので、
  フォントの字の絵 830578 B もその分 PSRAM を使っていた。新しいパーティション `msw_font`（0xDF0000、1 MB、subtype 0x52）に置き、
  最初の日本語の描画で esp_partition_mmap する（components/muse/fonts/muse_font_part.c、内部スタックの短いタスクで map）。
  `tools/muse/split_cjk_noto_jp_font.py` が元の .c から `_part.c` と `.bin`（16 B の見出し MSWF・大きさ・FNV-1a）を作る。
  パーティションがない・中身が違うときは `@msw.font error ...` で、日本語は □（落ちない）。パーティション表と font は USB でのみ書ける。
  結果: .rodata 1.64 MB → 0.81 MB、起動後（TTS 前）の psram= 2189K → 2999K（`>msw.mem psram` free 3.02 MB）。
  確認: ユーザーが画面の日本語を見て「問題なさそう」。返事の字幕と TTS も動いた（ログ）。未確認: フラッシュ書き込み中（NVS 保存・アプリの入れ替え）の日本語の描画。

## アバター「ずんだもん Claude版」（2026-11-10、ユーザー依頼）

- apps/zundaclaude（id zundaclaude）。ユーザーの参考画像（ずんだもん立ち絵素材）の大きい絵を切り抜いて 64 マス・40 色に（work/zundamon/sprite2.py → soft.py → gen.py が zunda_sprite.h を作る）。
  目・口・ほお・首元の Claude 色のきらめき・効果は毎フレーム描く。全 9 状態を `>msw.app.avatar_preview` で確認、1 フレーム約 1 ms（最大 3.4 ms）。文字盤のスクリーンショットで表示を確認。
- 残り: 起動時のつぶれ（0.6 秒）で頭の上に横線が出る。プレビューの JPEG に色の帯が出ることがある（本体の画面には出ていない。本体側のプレビュー処理の疑い、未調査）。
- SKILL.md のオリジナル限定のきまりの例外（ユーザーの依頼）。**リポジトリには入れない**（ユーザー決定）。apps/zundaclaude/ と work/zundamon/ は .gitignore 済みで、この PC と本体にだけある。

## 「オリジナルだけ」のルールを削った（2026-11-11、ユーザー指示）

- ユーザー: 「museのルールで最初から書いてある制限ルール以外はカット。ossすら使えなくなってる」。
- 公式 SDK（facebookincubator、tools/muse/avatar_prompt.md など）には、既存のキャラや素材を禁じるルールはない
  （あるのは「自分のアバターを描く」「先頭コメントに名前・形・色・性格」だけ）。4 か所の独自ルールは前のセッションで足したもの。
- 削った: SKILL.md の手順 1、miniapp/guide.md の「ルール」の 1 項目、avatar_guide.md の「既存のキャラクターをまねしない…Jollybot」、
  stopwatch_guide.md の「絵・名前・音楽はオリジナルだけ」。stopwatch_guide の版は 2026-11-11.1（ファームの次のビルドで入る）。
- 追記: ユーザー「テトリスに限定された話だったはず」。元は ideas.md 001（2026-11-08 ユーザー）の
  「元のゲームの素材（絵・音・キャラ・名前）は使わない。遊び方の種類だけを参考にする。guide.md にも入れる」で、
  既存のゲームを元にしたゲームだけの話だった。前のセッションがキャラ・素材全般に広げていた。
  その範囲で guide.md の「ルール」と SKILL.md の手順 1 に戻した。avatar_guide.md と stopwatch_guide.md には戻さない。
- 追記 2（同日、ユーザー決定）: 「ミニアプリの内容に関する余計な制限は不要。それは作る時にエージェントが考えるべき」。
  guide.md と SKILL.md に戻した 1 項目も削った。内容（絵・キャラ・素材など）についてのルールは、ガイドにもスキルにも書かない。

## ミニアプリが約 500 KB を超えると起動しない（2026-11-11、Muse の報告）

- Muse の報告: Doom（555〜790 KB）が組み立てはできるが起動しない。「本体の読み込み上限が約 500 KB」と推測。
- 実際は固定の上限ではなく、読み込み中の PSRAM 不足。app.log に `cannot read the app (765796 bytes)`
  （ファイル全体のバッファが取れない）と `ELF relocation failed (-12)`（ENOMEM）があった。
- 読み込み中に、ファイル全体のコピー・コード側の領域・データ側の領域の 3 つを同時に持っていた。
  elf_loader（2.1.0）は `.text` の始まりから `.got.loc` の終わりまでをコード側に取る。build.py の ELF では
  `.got.loc` が `.rodata` の後ろなので、`.rodata` がコード側とデータ側の 2 回入る。
  Doom は WAD が `.rodata` なので、ほぼ 3 倍いった。badapple2（467 KB）は読み込むと PSRAM が約 1.2 MB 減っていた。
- 直したこと（案 2）: `load_elf` はファイル全体を読まず、`esp_elf_relocate_source` でファイルから少しずつ読む。
  `check_symbols` も、セクション表と .dynsym・.dynstr だけを読む。ENOMEM のときは `out_of_memory` と
  空き・いちばん大きいひと続きを返す。実機: badapple2 は 107 ms で読み込み（前は 54 ms）、アバターも読める。
- まだ: `.rodata` の二重（案 1: `.got.loc` を `.text` のすぐ後ろへ）。
- 続き（同日）: パッケージを FAT から出して、ひと塊ずつフラッシュに置くことにした（下の節）。読み込みは
  ファイルからではなく、マップしたパッケージから（`esp_elf_relocate`）。

## ソースを zlib で入れる（2026-11-11、ユーザー決定）

- 「ソースは圧縮するルールでよくて、容量大きくなるなら確認して入るなら入れる」「ソース付きで入れるかのオプション」。
- build.py: ソースは "MSWZ" + zlib の長さ + テキストの長さ + zlib。256 KB の上限をやめ、パッケージが 1 MB
  （PKG_MAX = MSW_APP_MAX_BYTES）に収まれば入れる。収まらなければソースなしで作り、知らせる。`--no-source` で入れない。
  ELF とアイコンだけで 1 MB を超えるときはエラーにする。
- 本体: "MSWZ" を ROM の tinfl で展開する（状態 11 KB は PSRAM）。"MSWS" も読む。`app.source` は
  パッケージのうち ELF の後ろだけを読む（前は全体を読んでいた）。
- 実機: mimi（テキスト 4637 B → 1766 B）を入れて `msw.app.src` で読み、main.c が元と同じだった。
  前の形の piano も読めた。2026-11-11 より前のファームは "MSWZ" を読めない（ソースなしと答える）。

## パッケージをひと塊ずつフラッシュに置く（2026-11-11、ユーザー決定）

- ユーザー: 「これからどんどん入れるので容量を無駄にしたくない」「一塊にする。その上で足りない時のデフラグも可能にしたい」。
  私が「今の作りを変えない」前提で小さい案（末尾の 1 MB にデータだけコピー、FAT とデータに分ける）から出し、
  「変えないとか言ってない」と言われた（メモリ design-from-goal）。
- FAT のファイルは 4 KB ずつ散らばり、ウェアレベリングで位置も入れ替わるので、マップしてもひと続きに見えない。
- パーティション（partitions_musestopwatch.csv）: apps（FAT、保存データだけ）0x9F0000 512 KB、
  msw_pkgs（0x53）0xA70000 0x490000（4668 KB 使える）、msw_font を 0xF00000 へ移した。
  フラッシュの残りは 0（ota_0/1 は各 4 MB、ファーム 2.88 MB）。
- msw_app_pkgs.c: セクター 0 は書式の印（"MSWP"）とジャーナル。各パッケージは 4 KB 境界から、32 バイトの
  見出し（magic, seq, size, dead）+ パッケージ。size は書き終えて確かめてから書く（コミット）。起動時に
  書きかけ・dead・CRC の合わないものを消し、同じ id が 2 つなら seq の新しい方を残す。印がなければ全体を
  一度消す（初回 7.2 秒）。詰め直しは下から 4 KB ずつ写し、ジャーナルのビットを落とす。写し先は 1 セクター
  以上下なので、途中で切れても写し元が残っていて、起動時に続きから終える。動いているアプリは動かさない。
- 入れる: `msw_app_install_begin/write/end`。大きさがわかるとき（シリアル、まとめて受け取ったもの）は
  ちょうど入るいちばん小さいすき間、なければ詰め直す。わからないとき（VM からの app.install）は
  いちばん大きいすき間に書き、あふれたら詰め直して、VM から最初から読み直す（image_fetch_vm_stream、
  sink が 1 を返す）。1 MB の上限はなくなった（app.list の max_app_bytes = 空き）。
- `msw_app_data(name, &size)`（API 2）: build.py が `data/` のファイルを "MSWD" に入れる（16 バイト境界）。
  動いているアプリのパッケージの中を指す。
- 移行: 書き込む前に、アプリ領域 4 MB を esptool で読み（全体の読み出しはデータ化けで 2 回失敗、512 KB ずつなら通った）、
  IDF の fatfsparse.py（--wl-layer enabled、IDF の Python に construct がある）でファイルを取り出した。
  パッケージ 15 個（CRC 全部 OK）と保存データ 2 つを work/app-backup-2026-11-11/（git 管理外）に置き、
  backups/apps-fat-COM5-20261111.bin も残した。全部入れ直した。保存データ（blocks・pyoko のハイスコア）は
  戻していない（書き込むコマンドがない）。
- 実機: 1.5 MB のデータを 94 ms で読めた（ハッシュ一致、読み込み後の PSRAM 空き 3.0 MB）。2 つ消してすき間を作り、
  すき間より大きい 2.3 MB を入れると詰め直しが動いた。詰め直しの途中でリセットしても、起動時に
  「finishing a package move」で終わり、移動したアプリのデータも正しかった。起動時の確認は 15 個 4.2 MB で 0.3 秒。
- シリアルのインストーラーは 1 行ごとに 0.2 秒待っていた（pyserial の read(4096) がタイムアウトまで待つ）。
  届いた分だけ読むようにして 35 倍速く（9.6 KB で 3.5 秒 → 0.1 秒）。2.3 MB の途中で返事が 10 秒来ないことが
  2 回あった（原因不明）。`msw.app.putpos` で位置を聞いて続きから送るようにした。
- まだ確かめていない: VM からの app.install（Muse から。大きさがわからない流れ、あふれたときの読み直し）。

## 2026-11-11（夜）: Muse の Doom（VM からの入れ直し）、遅れて来る返事のログ

- VM からの app.install が動いた: 22:43 に Muse が doom_probe.mswapp（559360 B）を入れた
  （`link.image ... done, 559360 bytes in parts` → `installed doom v1`）。Muse の話では読み込み・配置も通った。
- Muse の報告: 読み込み直後の空き約 850 KB で、画面用の約 430 KB も、作業メモリ 1 MB も取れなかった。その後
  アプリが止まらず（app.stop が効かない）、ユーザーがリセットした。
  - 止まらない理由: app.stop は旗を立てるだけ（msw_app_stop → s_stop）。アプリが app_main や
    コールバックから戻らないと、旗を見られない（watch() が「restart the device」とログに出す、設計どおり）。
    app_main は display_lock を持ったまま呼ぶので、戻らないと画面も止まる。
  - 空き: Doom を入れる前の 22:42 で heartbeat の psram= が 561K（起動から 5.5 時間）。リセット直後の
    `>msw.mem psram` は free 2990804（最大 2915464）。TTS が最初の読み上げで約 1 MB 取る分を引いても、
    約 1.4 MB の行き先がわからない（前の Doom の試しが残っていたのか、漏れなのか、未調査）。
- 遅れて来る返事（タスクの後の Muse の報告が本体に出ない）: work/logs-2026-11-11/events.log（ev_logger.py で記録）。
  返事待ちでないときの delta.* は捨てている。本体から始めた仕事の返事の chat_context は device: mcu-wearable、
  Muse アプリからの会話は device_id（端末の UUID）。message.assistant の行は別の番号（seq）で来るので、
  今のコードは 1 つの s_last_seq で見て捨てている（ログにも出ない）。公式は b4bee64（11/10）で、覚えた
  リクエストへの message.assistant（reply_to_message_id）を後から読むようにした。こちらは 463bd2a のまま。
- 記録は 23:03 に、本体の `sleep: Incorrect wakeup source (4) to disable.` の直後に COM5 が切れて止まった。

### 2026-11-11 追記: アプリのたびに PSRAM が 16 KB 減る件、止まらないアプリの強制停止

- ユーザーの指示: 「最初の二つを」（PSRAM の行き先と強制停止）。遅れる返事の件は後でじっくり。
- 測り方: scratchpad の memcycle.py（`>msw.app.run` → 4 秒 → `>msw.app.stop` → `>msw.mem psram`）。
  6 本を 1 回ずつ動かすと、止めるたびに used が 16416 B（1 ブロック）ずつ増えていた。
- 原因: アプリのタスクは xTaskCreatePinnedToCoreWithCaps（スタック 16 KB を PSRAM）で作り、終わりに
  vTaskDelete(NULL) を呼んでいた。WithCaps のタスクはこれだとスタックと TCB が解放されない
  （msw_stack.h の注意どおり）。vTaskDeleteWithCaps(NULL) に直した。ほかの WithCaps のタスクは正しかった。
  5.5 時間で 1.4 MB ＝ 約 87 回分。直した後は 6 回動かして止めても ±100 B。
- 強制停止: アプリのコードは画面のロック（esp_lv_adapter の再帰ミューテックス）を持ったまま動くので、
  タスクだけ消すとロックが残る。止める指示（app.stop、ランチャー、黄ボタン 1.5 秒長押し）から
  5 秒たってもコードが戻らないと、watch()（1 秒ごとの esp_timer）が msw_crash_stuck() で
  アプリのタスクを止めて場所（バックトレース）とログ 6 行を RTC に残し、esp_restart() する。
  次の起動で app.log に `STUCK:` の報告。app.stop の返事には、戻らない間 `note` が付く。
- 実機: 戻らないテストアプリ（app_main で回り続ける）で、`>msw.app.stop` の 5 秒後に再起動し、
  `STUCK:  #0 app hangtest, ELF 0x1a2` が addr2line でループの行（main.c:7、app_main の中）を指した。
  テストアプリは消した。**未確認**: 黄ボタンの長押しでの強制停止（ユーザーに押してもらう）、
  Muse の app.stop からの流れ、Doom で実際にどこで止まっていたか。

## 2026-11-12: 公式の「遅れて来た返事」を取り込み、Muse が自分から送るメッセージ、TTS を起動時に確保

- 公式 upstream/main（b4bee64 ほか）を muse-gadget-sdk に取り込んだ（`--no-commit` のまま、未コミット）。
  message.assistant と他のイベントの番号（seq）を分けた（s_last_row_seq / s_last_seq）。
- ユーザー決定: 20 秒たっても返事がないときの知らせ（「Museが作業中です…」が返事の代わりに出る）は使わない。
  muse_chat_session.cpp の `MSW_DETACH_NOTICE 0`。返事もメッセージもないまま上限に来て、Muse がまだ作業中なら、
  静かに会話を終えて遅れた返事を待つ（`no reply in the turn: waiting for it in the background`、pending_arm）。
  **未確認**: この経路と、遅れた返事の読み上げ（`MUSE HAS AN ANSWER`）の実機での動き。
- タイマーの試し（ユーザー、09:33 と 09:41、work/logs-2026-11-12/events.log）: 頼んだ返事はすぐ出た。
  タイマーが終わったときの Muse のメッセージは出なかった。届いたのは delta.message_start/done だけで、
  message.assistant の行はなく、reply_to_message_id は空。公式の仕組みは「本体の頼みへの返事」だけを拾うので、
  これは捨てられる。
- 返事待ちでないときのメッセージの見分け（chat_context、11/11 と 11/12 の記録）:
  - `device_id`（UUID）あり: Muse アプリでの会話（その UUID はアプリの node.disconnected の node_id と同じ）。
  - `device: mcu-wearable`（output_modality text）: 本体から頼んだ仕事が後で終わったときの報告。
  - どちらもなし: Muse が自分から送ったもの（タイマー）。
  ユーザーの希望: 「できるだけ見わけたい」。**未作成**。未確認: Web 版の Muse・別の端末からの会話、予定した仕事
  （tasks.updated）のメッセージの形、公式の遅れた返事との二重の読み上げ。
- PSRAM の流れ（同じ記録、heartbeat の psram=）: 待ち受け 3299K → 押して Muse につながり声を送る 2404K
  （−895K）→ TTS で読み上げ 1415K（−994K）→ 会話の後 1887K。2 回目の会話は −484K で、全部戻った。
  - TTS の分は モデルの写し 786432 B・リング 192000 B・フィルター 30720 B だけ（Open JTalk は arena を借り、
    一時的な確保は 1 文ごとに返す）。
  - 会話ごとに借りて返す 480 KB は録音のバッファ（muse_voice.c の s_rec、15 秒 × 16 kHz × 2 B）。
    Wi-Fi がないときや送るのに失敗したときに、声を残してあとで送るためのもの。
  - 残りの約 0.42 MB は最初の会話で Muse との会話の接続を開いたときに取られ、持ち続けている。中身は未確認
    （本体で最初の会話の前後に `>msw.mem psram` を比べる）。
- TTS を起動時に確保（ユーザー決定「まずはAで」）: モデルはフラッシュから直接読む（s_cfg.model_psram の既定を
  false。`>msw.tts.cfg model=psram` で今までどおり写せる）。リングとフィルターも起動時に取る（msw_tts_register で
  rs_init）。11/7 のベンチでは、フラッシュ置きは定常 xRT 0.63〜0.65（PSRAM 0.52〜0.54）。ビルドは通った。
  **未確認**（書き込み前）: 起動後の psram=（見込み約 3.1 MB）、スピーカーでの読み上げが途切れないか、
  会話・アプリ・アバターと重なったときの速さ。

## 2026-11-13: TTS を起動時に確保したファームの実機確認

- 書き込み（COM5）。起動 16 秒の psram= は 2655K。前の版の同じ時点は 2881K で、差の 226K は
  リング（192000 B）とフィルター（30720 B）の分。11/12 に書いた「待ち受け 3299K」は起動直後ではなく、
  10 分たってチャットが切れた後（`disconnect: idle`）の値だった。見込み「約 3.1 MB」は比べる時点の間違い。
- 「残りの約 0.42 MB」はチャットの接続の分: 11/12 の記録で、`disconnect: idle` のときにちょうど 418K 戻った。
  起動時の `>msw.mem psram` の 64K のかたまりは 11 個（65568 B が 1 個、65536 B が 10 個）。アドレスの並びとコードから:
  - チャットの接続（muse_chat_session.cpp の connect: ws・rx・tf・sr・svc・env）が 6 個 = 384K。
    暗号とセッションの分を足すと、idle で戻る 418K と合う。
  - Muse との制御の接続（main/noise_control.cpp: ws_buf 65535+32・rx_buf・tf・sr）が 4 個 = 256K。常につながっている。
    PSRAM のある板では SVC_FRAME_SCRATCH・WS_BUF_SIZE が 64K（プロトコルの 1 フレームの上限）。
  - 残りの 1 個（0x3c5629ac）は持ち主が未確認。
- 会話の試し（ユーザー、3 回の読み上げ）: 押している間 2183K（録音の 480K）、会話の後は 2645〜2655K に戻った。
  TTS で psram が減ることはなくなった（前の版は読み上げ中 1415K、会話の後 1887K）。
- フラッシュから読む速さ: 定常 xRT 0.57〜0.61、p90 0.61〜0.71、full 0.71〜0.80、最初の音まで 270〜276 ms。
  どれも 1 より小さい（作る方が速い）。arena peak 約 113 KB。
  **未確認**: ユーザーが聞いた音（途切れの有無）、アプリ・アバターと重なったときの速さ。
- 同じログで、t=191s にチャットが `receive failed` で切れ、すぐの再接続は `VM connect failed (HTTP 400)`、
  t=204s にはつながり直した。原因は未調査。

## 2026-11-13: device_id を足して返事の chat_context を見る実験

- 変更: 本体から送る声・文字のメッセージに `device_id`（identity_node_id、`homelink-13e96c`。公式の声の通話と同じ値）
  を付けた（muse_chat_session.cpp の MSW_SEND_DEVICE_ID、0 で外せる）。返事を待っている間の
  delta.message_start / message.assistant の chat_context も `@msw.ctx` で出す。
- 結果（ユーザーが声で話し、タイマーを頼んだ。scratchpad の devid_test.log）:
  - 本体への返事: `{"device":"mcu-wearable","device_id":"homelink-13e96c","output_modality":"text",...}`。
    本体の device_id がそのまま返ってきた。会話はいつもどおり終わった（turn done、1 message）。
  - タイマーが終わったときの Muse のメッセージ: chat_context に device も device_id もない（前と同じ）。
    metadata.source は "runtime"、reply_to_message_id は空。頼んだときの印は引き継がれない。
  - message.assistant の行は、どちらでも来なかった。
- わかったこと: 本体から始めた会話の返事は、device_id で本体のものと確実に見分けられる。Muse が自分から
  送るメッセージは、chat_context では「どの端末とも結び付いていない」ことしかわからない。
  **未確認**: 本体のコマンド呼び出しへの影響（今回の会話では見ていない）、Muse アプリから頼んだ仕事の後の報告の印、
  delta.message_done の残りの項目（ログは 600 文字で切っていて transcript などが見えていない）。
- 続き（同じログ、ユーザーがほかにも試した）:
  - Muse アプリでの会話の返事 4 つ（363〜446 s、待っていないとき）: chat_context に アプリの device_id（fec5cc64-…）。本体は出さない。
  - 137 s のタイマー（待っていないとき）: 何も付かない。本体は捨てた（表示されなかった、ユーザー確認）。
  - 472〜494 s: 本体から話した会話で返事が 2 つ。1 つ目は本体の device_id、2 つ目は何も付かない。ユーザーによると
    10 秒のタイマーを頼んでいたので、返事を待っている間に鳴ったタイマーの知らせと考えられる。返事を待っている間は、
    reply_to_message_id が空のメッセージを公式の bind_msg がその会話のものとして読み上げるので、タイマーも出た。
  - まとめ: 本体への返事＝本体の device_id、Muse アプリ＝アプリの device_id、Muse が自分から＝何もなし、で
    3 通りが分かれた。返事を待っている間に来た「何もなし」は、今は本体への返事と同じに扱われている。
- 公式の議論（2026-11-13 に GitHub を検索）:
  - PR #90（Linux SDK、閉じた）のレビューで、保守者が「/chat/stream の device_id は残して。後のデバイスのコマンドを
    その機器へ戻す道筋になる」と書いている。Linux SDK は送っていて、ESP32 は文字・声メモでは送っていなかった。
  - PR #268（b4bee64）の説明は「待っている間は、ほかの依頼を指さない delta.message_start でも拾う。ほかの会話の
    親なしメッセージも 30 分の間は出るかも」だが、取り込まれたコードは message.assistant の行だけを見る。
  - issue #19（閉じた）: 返事を待っている間に、ほかのメッセージへの返事が自分のものとして出る件（拒否した ID を覚える修正）。
  - Muse が自分から送るメッセージ（タイマー）を機器で出す話・chat_context での見分けの話は、見つからなかった。
  - 公式の新しいコミット 6424ae0（#334、返事の進み具合の表示）が muse_chat_session.cpp にある。未取り込み。

## 2026-11-13: [to_stopwatch] の印で、会話の後に届く結果を出す

- ユーザー決定: コマンドで知らせる案はターンが増えるので使わない。本体から頼まれたことの結果の頭に
  `[to_stopwatch]` を付けてもらい、本体は印を消して字幕と声で出す。
- ファーム（muse_chat_session.cpp）: msw_mark_pass / msw_mark_end（会話中の返事の印を、分かれて届いても消す）、
  msw_own_event（待っていないときのメッセージの文字をため、印があれば遅れた返事と同じく s_pending.replies に
  ためて再生）。印のないものは出さない。incoming_play は、親のない行だけなら待っている依頼を消さない。
- 文書: stopwatch_guide.md 版 2026-11-13.1 に「本体から頼まれたことの結果には文の一番最初に [to_stopwatch]、
  本体以外から頼まれたことには付けない」。
- 実機: ユーザーが試して「うまくいった」（タイマーの結果が会話の後に出た）。ログは取れていない（記録の 30 分が
  先に終わった）。**未確認**: 長いタスクの報告、Muse アプリから頼んだ分に付けないか、印が分かれて届く場合。

## 2026-11-13: voice.say をツールから外した。MP3 のバッファ

- ユーザー決定「ツールとしてはカットします。アプリのはそのままで」: voice.say / voice.stop を Muse のコマンド
  （main/noise_control.cpp の登録、main/app.c の処理）から外した。ミニアプリの msw_say（msw_tts_say を直接呼ぶ）と
  シリアルの >msw.say は残る。stopwatch_guide.md 版 2026-11-13.2、README も直した。ビルドは通った、書き込みはまだ。
- MP3 のバッファ（muse_chat_session.cpp）: 返事の音を Muse のサーバーからもらう公式の道筋（K_TTS）は、
  公式でも「好きな TTS API をつなぐ場所」として空いていて、だれも流していない。それでも起動時に取っている:
  s_turn.mp3 = MP3_BUF（CONFIG_MUSE_HATCH_COMPACT_BUFFERS=y なので 128 KB、`>msw.mem psram` の 131072 B の
  かたまり）、s_pcm / s_pcm16 約 2.3 KB ずつ、mp3dec_t は s_turn の中（外部 .bss）。minimp3 は他に
  `test_reply.mp3` のベンチ（muse_chat_session.cpp の最後）だけが使う。

## 2026-11-13: voice.play と sound.play（かんぷれの play_audio と同じ流し方）

- ユーザー決定: 「明示的に指示した時に、会話と同じ内容の音声を…渡して会話を返すコマンド」「このコマンドの後の会話は
  本体の tts はしない」「純粋に音を鳴らす為のコマンド…声の上に乗る感じ」「声の名前のテキスト指定が欲しい…画面のどこかに
  出したい」「メモリは mp3 の今あるやつだけで良い…同時はひとつしか読めない…ストリーミング的に読むのはかんぷれと同じ」。
- 作り（main/msw_play.c）:
  - VM の MP3 か HLS の index.m3u8 を image_fetch_vm_stream で読む。HLS は kantanMuse と同じ:
    一覧を約 1 秒ごとに読み直し、#EXT-X-ENDLIST と流し切りで終わり、30 秒新しいファイルがないと playlist_stalled。
  - バッファは muse_chat_session の MP3 バッファ 128 KB を分けて使う（一覧 8K、MP3 のリング 64K、デコードの窓 16K、
    16 kHz の PCM リング 32K）。自分のタスク（PSRAM のスタック 32K、再生の間だけ）で minimp3 → 16 kHz モノラル。
  - voice.play: 返事の声（muse_reply_voice）を包み、say の数に数える。ターンの返事は say_live() でこの音声になり、
    字幕は今までどおり再生位置に合わせて進む。本体の声では読まない。話している最中（P_WAIT_REPLY の声のターン）だけ、
    それ以外は no_turn。返事が届く前に失敗して音がないときは数を戻すので、本体の声で読む。読まれないまま 60 秒で捨てる。
  - sound.play: muse_audio_write で上に足す（msw_audio_overlay、volume 0〜100）。待機中は msw_app_sound.c の
    プレイヤーが無音を流してその上に乗せる。ボタンを押すと止まる。
  - 同時に 1 つ。後から呼んだ方に置き換わる。stop=true で止める。返事はすぐ（後の失敗はシリアルの `@msw.play` と
    `>msw.play` だけ）。
  - CV: 顔の下（BOTTOM_MID、-40）に「CV: 名前」を、voice.play の音声が流れている間だけ出す（muse_ui.c）。
- stopwatch_guide.md 版 2026-11-13.3（「声と音」）。ビルドは警告なし、COM5 に書き込み済み。起動後 psram=2634K、
  `>msw.play` は mode=0。voice.say 外しのビルドもこれに入っている。
- **未確認**（実機）: voice.play の声と字幕、CV の位置と見え方、HLS の途中から流れるか、sound.play が待機中と
  返事の最中に重なるか、ボタンで止まるか。話を聞いている間（P_LISTEN）はスピーカーに書かないので sound.play は止まる。

## 補足（2026-11-13）: 記録の中の「本体」について

- この記録やほかの work/ の記録、README、スキルにある「本体」は、11-06 の最初から私（Claude）が自分で使い始めた言葉。
  ユーザーが決めた呼び名ではない。ユーザーが決めたのは「Museストップウォッチ」だけ（2026-11-08）。
- 11-09 のコミット（d35cf34）から 2026-11-13 まで、CLAUDE.md に「言い換えは『本体』（ユーザー決定）」と誤って書いていた。
  Muse 向けのガイドにも「この名前か『本体』と呼んで」と入れていたので、Muse も「本体」と言うようになった。
  前の「時計」も同じ原因（私の言葉が Muse 向けの文書に入った）。
- 2026-11-13 に、ファーム（Muse 向けのガイド、ツールの説明、画面と Web の文）から「本体」を全部消した。
  古い記録は書き換えずに残している。読むときは「本体」を「Museストップウォッチ」と読み替え、新しく書くときに使わない。

### 2026-11-13 追記: voice.play が最初の 2 秒しか鳴らなかった

- ユーザーの実機テスト: ずんだもんの声が最初の一文しか出なかった。Muse は 1 本の MP3（24 kHz モノラル、7.2 秒、文面は返事と同じ）を
  作っていた。`>msw.play` は `voice mp3: done, 2.2s of audio`。
- 原因（msw_play.c の decode_some）: 返事が届くまで PCM（1 秒）がいっぱいで待つ間、デコードの窓 16 KB がいっぱいになると、
  「フレームがない＝MP3 でない」として窓を捨てていた。PCM がいっぱいのときは捨てないように直した。書き込み済み、実機は未確認。

### 2026-11-13 追記: voice.play の HLS を実機で見た（シリアル記録 work/logs/voice-play-2026-11-13.txt、git に入れない）

- 再生の途中にファイルが届くのは動いた: stream402 は声が出た時点で 5 本、残り 7 本を再生中に約 1.5 秒おきに読み、
  `#EXT-X-ENDLIST` まで 12 本、31.1 秒を作って 31.1 秒鳴らし切った。
- 2.5 秒で切れた回: 一覧の読み直しの途中で Muse のサーバーが接続を 2 本とも切った（つなぎ直しに約 13 秒）。
  読み込みに失敗すると再生をやめる作り。ユーザー:「サーバー側はミューズの今回だけの問題」なので、読み直しは足していない。
- 「最後が途切れる」: Museストップウォッチは作った分を全部鳴らしていた。返事の文（465 文字）に対して音が 31.1 秒と短く、
  Muse が途中までの音で ENDLIST を書いた可能性。ユーザー:「サーバリセットで間に合うようになってしまったのでまた起きたら調べる」。
- 記録に `@msw.say the voice task did not play it for 30 s: dropped` が 1 行（ミニアプリの読み上げ？）。出どころは未確認。

## 2026-11-13: 公開の準備（OTA の取得先、ライセンス）

- OTA: `CONFIG_HOMEHUB_OTA_URL_PREFIX`（SDK の main/Kconfig.projbuild に追加、ota.c の ota_start で確かめる）。
  StopWatch では `https://github.com/haiatto/museStopWatch/releases/download/` 以外を、Muse・BLE・Wi-Fi 設定のどの経路でも、
  force でも、ダウンロードの前に断る（`url not allowed`）。ユーザー:「公式は取り込むことはないから公式が取り込まれるルートがあったら塞いでおきたい」。
  COM5 に書き込み、起動と `OTA image validated` を確認。断る動きは実機では未確認。
  注意: Kconfig に項目を足したとき、`.\dev.ps1 build` だけでは sdkconfig に入らなかった（reconfigure が要った）。
- ライセンス: museStopWatch は MIT（ユーザー:「mitが競合しないならそれで」）。調べた範囲で GPL の混入・秘密情報はなし。
  LVGL の LICENCE.txt を sdk/include/lvgl に写す（make_sdk.py）。hey_muse の出どころと学習データを wakeword/models/README.md に書いた
  （背景音の一部が非商用。モデルに及ぶかは未確定）。README のライセンスの節を一覧にした。
- ファームの版は 999.0.0 のまま（最初の版の番号はユーザーの返事待ち）。

## 2026-11-13: PORT.A の入力（msw_pin_read）と、はじめる前の画面（API 3）

- ユーザー:「足す。あと、外へのioを使うアプリは起動時に何するかを出して開始ボタンを押してから始まるような対応しておきたい」
  （M5 の Dual Button ユニットを見せて、読めるかを聞いた流れ）。
- msw_pin_read(MSW_PIN_YELLOW=G10 / MSW_PIN_WHITE=G11): 入力＋内部プルアップだけ。出力はない。I2C は閉じる。
- msw_port_power・msw_i2c_begin・msw_pin_read を使うアプリは app.json の `port` が要る（build.py が止める）:
  `connect`（つなぐもの、120 バイトまで）と `about`（すること、240 バイトまで）。ユーザー:「どちらかというとアプリ側に説明を用意させて
  何を繋げられるのかみたいな説明を起動の画面で出したい」。パッケージのデータ `.port`（connect、改行、about）。
  ファームは ELF の import で見分け、app_main の前に画面いっぱいで両方と「やめる」「はじめる」を出す。
  開始は青ボタンを押しながら「はじめる」をタッチ（ユーザー:「開始ボタンも間違って押さないように、青ボタン押しながらタッチとかにしたいかも」）。
  片方だけなら始まらず、案内の行が赤くなる。「はじめる」は灰色で、青ボタンを押している間だけ緑（ユーザー:「はじめる、は灰色っぽくて
  青ボタン押したら今の色になるとかしたい」）。やめる・黄ボタン・app.stop で始めずに終わる。それまで PORT.A の関数は失敗する。
  「はじめる」の後に一瞬ランチャーが見えた（ユーザー:「始めるを押した後一瞬アプリ画面出る」）→ 起動の画面は、アプリの画面を読み込んでから消す。
- 書き込み・apps/sensors を入れて起動: ログに `uses PORT.A; waiting for "はじめる"` まで確認。
  起動の画面・青ボタンと「はじめる」・その後の切り替えは、ユーザーが実機で見て「もんだなさそう」（2026-11-13）。
  msw_pin_read は実機で未確認（Dual Button で試せる）。

## 2026-11-13: Doom が落ちる件と、app.install の 30 秒

- ユーザー:「なんかdoomのアプリ落ちるのだけどメモリ足りてない？」。Doom のソース（msw.app.src doom）から:
  zone は PSRAM のいちばん大きいひと続き − 192 KB（最大 1280 KB）。WAD は deflate で .rodata（220,198 B、elf_loader で 2 つ分）、
  起動時に malloc へ展開（523,678 B）、画面 430 KB。動作中の heartbeat は psram=110K、lv_draw_buf の 434 KB が取れない警告も出た。
  案: WAD を展開済みで data/ に置いて msw_app_data で読む（約 1 MB 減る。Muse に頼む形）。ファーム側は未着手。
- Muse の「パッケージのソースが壊れている」: シリアルの msw.app.src doom は最後まで読めた。read_source は展開用の
  raw+1 バイトが取れないときも broken_source と返す（誤った名前。未修正）。
- Muse の「呼び出しの返事待ちが 30 秒で時間切れ、デバイス側は続いていた」: app.install は _async で、パッケージを全部取り終えて
  から返事する。30 秒は Muse の呼び出し側の上限（Muse の言葉。公式の資料では未確認）。
  同じ id の入れ直しは古い版を残したまま別の空きに書くので、1.34 MB のひと続きが要る。大きさを知らずに書き始め、入りきらないと
  詰め直してからダウンロードを最初からやり直していた（入れた後のいちばん大きい空きは 1188 KB）。実際に詰め直しが起きたかはログで未確認。
- 直した（ユーザー:「1で」= 初めに大きさを知る）: VM の最初の返事のヘッダー（Content-Range の全体、200 なら Content-Length）で
  大きさを知り、最初の部分を書く前に、その大きさで書き込みを始め直す（足りなければここで詰め直す）。古い版を残す作りはそのまま。
  noise_control に noise_ctrl_req_open_head（返事のヘッダーを受け取る。noise_ctrl_req_open は今までどおり）、
  image_fetch_vm_stream_sized、msw_app_install_size / msw_app_install_json_size。ヘッダーがなければ今までどおり。
  ビルドは通った。書き込みと、Muse からの app.install での確認はまだ（VM のデーモンが Content-Range を返すかも未確認）。

## 2026-11-13: SDK とファームの組（clone の行をファームの版から作る）

- 発端: Muse の「道具のリポジトリはサーバー側と同じ版で、それでもボタンの関数は入っていません」。sdk/ の変更（msw_pin_read、API 3）を
  まだ push していないため。build.py はどのアプリにも SDK の今の API 版を書くので、SDK だけ上げると古いファームでは新しく作るアプリが
  全部 api_too_new になる、と話した。ユーザー:「ファームが指定するリビジョンがあれば良い感じ？」→ タグと組にする案に「ならそんな感じで」。
- miniapp/CMakeLists.txt: guide.md をビルドフォルダに写して埋め込む。version.txt が 999.0.0 でなければ、clone の行を
  `git clone --depth 1 --branch v<版> <URL>` にする（開発中は main のまま）。cmake -P で 999.0.0 はソースと同じ、0.1.0 は --branch v0.1.0 を確かめた。
  ビルドフォルダの guide.md もソースと同じ（開発版）。
- app.list に `firmware`（esp_app_get_description の版）。ガイドの手順 3 に「SDK の版」の説明（ファームが替わったら読み直して入れ直す）。
- リリースの手順に加わること: 外側のリポジトリに `v<版>` のタグを付けて push（SKILL.md にも書いた）。
- 初版について、ユーザー:「初版はまだ色々試してからにしたい」。版の番号は決めない。
- 書き込みはまだ（インストールの変更と同じファーム）。
