# muse-gadget-sdk フォークの整理（2026-11-08）

> **状態: 保留（2026-11-08 ユーザー判断）。** 公式が勧めるのは SDK を直接改造するやり方なので、
> 「自分の部品は components/ の下（muse_diy_stopwatch/）、公式ファイルには必要な改造だけ」とする。
> 差し込み口（A の口）を作る進め方 3〜5 は行わない。置き場所の移動（進め方 2）だけ行った。

## 目的

公式 SDK（facebookincubator/muse-gadget-sdk）の大きな更新があっても、マージがスパゲティにならないようにする。
ユーザーの考え（2026-11-08）: 「SDK に足りない機能を足す改造」と「このプロジェクト（StopWatch・アプリ寄り）の改造」は別もの。

## 前提（調べたこと）

- ESP32 版 SDK は、ライブラリではなく「フォークして直接いじる前提のお手本ファーム」。
  esp32/AGENTS.md「Adding a command」も、noise_control.cpp の build_register_json() と app.c の on_ws_command() を
  直接書き換える手順。外からツールを登録する口はない（Linux 版は commands.d/ に JSON を置くだけで足せる）。
- 公式の更新とぶつかるのは「公式ファイルの中の書き換え」だけ。新しいフォルダ・ファイルはぶつからない。
- ESP-IDF は components/ の直下を自動で探す。下の階層にまとめるなら EXTRA_COMPONENT_DIRS を渡す
  （dev.ps1 のビルド引数で渡せば、公式ファイルは変えずに済む）。
- 公式の新しいコミットは、この時点ではない（upstream/main = 463bd2a）。

## 今の変更（463bd2a → dev/stopwatch 54bba9b＋未コミット）

- 新しいファイル 109 個（components/miniapp、components/sanotts、components/muse/msw_* など）。
- 公式ファイル 21 個に約 1,060 行の追加・124 行の削除。大きいのは muse_settings_ui.c（60 か所）、
  muse_chat_session.cpp（185 行）、board_m5stack_stopwatch.c（176 行）、app.c（130 行）、image_fetch.c（110 行）。

## 分類

A = SDK に足りない機能（どの機種にも役立つ。フォークに残す。機種名ではなく Kconfig で選ぶ。公式に提案できる）
B = museStopWatch 独自（StopWatch・アプリ寄り。muse_diy_stopwatch/ に移し、A の口からつなぐ）

| 場所 | 中身 | 分類 | 整理の方向 |
|---|---|---|---|
| noise_control.cpp | 登録 JSON の上限（MUSE_REGISTER_JSON_MAX） | A | 機種名ではなく Kconfig に。既定は公式の 8192 |
| noise_control.cpp, app.c | app.* と voice.say の登録と処理 | 口は A、中身は B | 「ツール登録の口」を 1 つ作り、app.* と voice.say は B 側で登録する |
| muse_pixel.c, muse_audio.c | 静的バッファを PSRAM へ（EXT_RAM_BSS_ATTR） | A | そのまま（PSRAM のない機種では内部 RAM に戻る） |
| tunnel_netif.c, noise_control.cpp, muse_glue.c, muse_input.c, app.c, board | タスクのスタックを PSRAM へ（msw_stack.h） | A | 名前を muse_ に、muse 側の共通ヘッダーへ |
| muse_link.c/.h, muse_wifi.h, muse_glue.c | 保存済み Wi-Fi に切り替える（muse_wifi_choose） | A | そのまま |
| image_fetch.c/.h | VM のファイルを丸ごと PSRAM に取る（image_fetch_vm_file） | A | そのまま |
| muse_chat_session.cpp | サイドチャット（session_id） | A | そのまま（kantanMuse と同じ機能） |
| muse_chat_session.cpp, muse_voice.c/.h | 返事を本体の声で読む（ローカル TTS を差し込む口） | 口は A、中身は B | 「返事の声」の口として整える。SanoTTS は B |
| muse_chat_session.cpp | 返事の文をミニアプリに渡す（msw_app_muse_reply） | 口は A | 「返事の文を受け取る口」に名前を一般化 |
| muse_input.c | コンソールの msw.* をこちらに渡す | 口は A | 「コンソール拡張の口」に一般化 |
| muse_input.c | アプリ中は黄・青ボタンをアプリへ | 口は A、中身は B | 「ボタンを横取りする口」に一般化 |
| muse_ui.c | 文字盤の左にランチャーのタイル | 口は A、中身は B | 「タイルを足す口」に一般化 |
| muse_ui.c, Kconfig, CMakeLists, fonts/ | Noto Sans JP フォント | A/B 境目 | Kconfig は残す。フォントのファイルは B に移せるか検討 |
| muse_settings_ui.c | 日本語の表示、QR 画面、サイドチャット画面、保存済み Wi-Fi の行 | ほぼ B | 一番大きい。「設定に行・ページを足す口」と「表示の文字を差し替える口」を A に作り、中身は B へ |
| boards/board_m5stack_stopwatch.c | タッチ、IMU、振動、表示、スタック | A | 機種サポートの改善として残す（公式に提案できる） |
| components/muse/msw_*（net, web, qr, side, text, mem, wifi_saved, side_ui） | Web 設定、AP、QR、サイドチャット画面、日本語 | B | muse_diy_stopwatch/ui/ へ移す |
| components/miniapp | ミニアプリの仕組み、ガイド | B | muse_diy_stopwatch/miniapp/ へ |
| components/sanotts | 読み上げ（他の人のコードとモデル） | B | muse_diy_stopwatch/sanotts/ へ（ライセンスを分けたまま） |

## 置き場所（ユーザー決定 2026-11-08）

```
esp32/components/muse_diy_stopwatch/     まとめ用のフォルダ（EXTRA_COMPONENT_DIRS で読み込む）
  miniapp/    ミニアプリの仕組み、guide.md
  sanotts/    読み上げ（upstream/ に他の人のコード、model/ は git 管理外）
  ui/         Web 設定、AP、QR、サイドチャット、日本語の文字など
```

## 進め方（案。順番に、1 段ごとにビルドと実機確認）

1. MUSE_REGISTER_JSON_MAX を Kconfig にする（StopWatch の設定で 256 KB）。
2. 新しいファイルを muse_diy_stopwatch/ に移す（中身は変えない）。dev.ps1 に EXTRA_COMPONENT_DIRS。
3. A の口を作る（ツール登録、返事の文、コンソール、ボタン、タイル、設定ページ、表示の文字）。
   公式ファイルには口の呼び出しだけを残し、weak の msw_* 呼び出しを口に置き換える。
4. 設定画面（muse_settings_ui.c）の書き換えを、口を使う形に減らす。
5. 公式ファイルの残りの差分を見直し、すべてに「museStopWatch:」の印があるか、A の説明になっているかを確かめる。

## 決めること

- A の口を公式に提案する（PR）か、フォークの中だけにするか。
- 設定画面をどこまで減らすか（日本語化をやめて公式どおりにする選択もある）。
- 公式の手順（tools/muse/board.sh）での組み立てを残すか（EXTRA_COMPONENT_DIRS が入らないので、そのままでは部品が見つからない）。
