# museStopWatch: エージェント作業ルール

## 目的

- M5Stack StopWatch（SKU C152）で Muse を動かす（2026-11-06 ユーザー決定）。
- 参考は `C:\MyProj\kantanMuse`。ただし「Link 式」ではなく「Muse 式」で接続する。
  SDK をそのまま使うのではなく、kantanMuse で作ったものの一部を取り込む。
- Muse SDK の扱いは kantanMuse と同じ: 最新 SDK を導入し、必要なら改造し、
  その後も公式の最新をマージしていく。
- kantanMuse から参考にするもの:
  - ボイス再生の仕組み（MP3 デコード・再生など）
  - 説明を返すためのツールの doc 部分
  - Web 設定、Wi-Fi の複数接続などの利便性の部分（特に重視）
- 参考にしないもの: かんぷれ（楽器）としてのツール部分。
- 導入するもの:
  - システムメニューのような設定画面
  - ミニアバター＋吹き出し（Muse がしゃべった言葉をそのまま出す）は**保留**（2026-11-07）。
    まずは「公式＋α」で進め、返答は公式の字幕表示を使う
  - ボイス再生コマンド
  - SanoTTS-jp のローカル実行と、Muse 側から使える TTS 再生コマンド
  - Muse に渡す doc に、日本語音声認識の案（kantanMuse で採用したもの）を入れてセットアップする
- 呼び名は「Museストップウォッチ」（2026-11-08 ユーザー決定）。
  時計の機能はないので「時計」とは呼ばない（返事・Muse 向けの文書とも）。
  製品名としては M5Stack StopWatch。
- 音声の前に定型文は送らない（kantanMuse の voice preface はやらない）。音声だけを送る。

## 対象ハードウェア

- M5Stack StopWatch Dev Kit（SKU C152）
  - ESP32-S3R8（PSRAM 8MB）、Flash 16MB
  - 1.75インチ 円形 AMOLED タッチ、マイク・スピーカー、IMU、RTC、振動、ボタン、450mAh バッテリー
- Arduino のボード名は `M5StopWatch`（M5Stack Board Manager 3.3.7 以上）。
  M5Unified 0.2.15 以上、M5GFX 0.2.21 以上が必要。
- 公式資料: https://docs.m5stack.com/en/products/sku/C152
  工場出荷デモ（ESP-IDF）: GitHub の M5StopWatch-UserDemo
- ピン配置・パーティションなどは推測せず、公式資料で確認してから使う。
- 内蔵 RAM は足りない（2026-11-08 ユーザー決定）。自分で足す変数・バッファ・タスクのスタックは
  PSRAM に置く（`EXT_RAM_BSS_ATTR`、`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`、`msw_task_create`）。
  内蔵に置くのは、特別な理由があるもの（DMA、割り込み、フラッシュ書き込み中に使うものなど）と小さなものだけ。
  音などは最低限の大きさなら内蔵でもよい。足した後は heartbeat の `int=` か `>msw.mem` で確かめる。

## 開発環境の方針

- Windows / PowerShell。参考は `C:\MyProj\kantanMuse`（あくまで参考。そのまま流用しない）。
- ツールはプロジェクト内に閉じて導入する（`.venv/`、`.tools/`）。
  グローバルの Python / PlatformIO / Windows 設定は変更しない。
- ビルドは PlatformIO + ESP-IDF（pioarduino 55.03.312、出荷時デモと同じ構成）。
  PlatformIO 本体は `.venv/`、ツールチェーンは `.tools/platformio/` に置く。
- `upstream/M5StopWatch-UserDemo/` は公式の出荷時デモの参照用 clone（git 管理外）。
  HAL・ピン配置の参考と、ツールチェーンの動作確認・出荷時ファームへの復帰用。

## コマンド（ワークスペース直下の PowerShell で実行）

- ビルド: `.\dev.ps1 build`（既定は upstream のデモ。`-Project <フォルダ>` で切替）
- ポート一覧: `.\dev.ps1 ports`
- 書き込み: `.\dev.ps1 upload -Port COMx`
- シリアル監視: `.\dev.ps1 monitor -Port COMx`
- フラッシュ全体のバックアップ（読むだけ）: `.\dev.ps1 backup -Port COMx`

## 実機への書き込み

- 最初の書き込み前に、フラッシュ全体のバックアップを `backups/` に保存する。
- 書き込み・再起動の前に、ユーザーに一言伝える。COM ポートは毎回確認する。
- ユーザーの指示なしにフラッシュ消去・NVS リセット・eFuse 変更をしない。
- ビルド成功は実機テストではない。画面表示や音はユーザーに確認してもらい、
  自分で見た・聞いたとは書かない。

## OTA と署名

- OTA の署名鍵は自分のもの（`.tools/keys/msw_signing_key.pem`、git 管理外。2026-11-08 ユーザー決定）。
  鍵があれば `dev.ps1` がそれで署名し、なければ SDK の共有のサンプル鍵（`esp32/dev_signing_key.pem`）になる。
  鍵の中身を出力・コミットしない。鍵を替えた後の最初の書き込みは USB で行う。
- Muse に OTA させるファームの取得先は、配布元のリリースだけ
  （`stopwatch_guide.md` の「ファームの更新」。今は haiatto/museStopWatch の Releases）。
- 派生版（フォーク）を作るときは、自分の鍵を作り、`stopwatch_guide.md` の取得先と、
  `guide.md` の SDK の clone 先を自分のリポジトリに書き換え、ガイドの版を上げる。

## Git

- 公開リポジトリ: https://github.com/haiatto/museStopWatch （2026-11-06 ユーザー指示で公開）
- コミット・push はユーザーの指示があったときだけ行う。force push はしない。
- Wi-Fi パスワード、トークン、バックアップの中身を出力・コミットしない。

## スキル

- `.claude/skills/html-plan/`: 実装計画を対話式の HTML 1枚にまとめる（`/html-plan <作るもの>`）。
  入手元は SOURCE.txt。計画ページは `work/plans/` に保存し、pack には node を使う。
- `.claude/skills/msw-app/`: ミニアプリ（`apps/`、`components/muse_diy_stopwatch/miniapp`）の作成・ビルド・実機での確認。
  Muse 向けの同じ内容は `components/muse_diy_stopwatch/miniapp/guide.md`（`app.doc`）。片方で学んだことはもう片方にも書く。

## 作業記録

- 現在の状況・実測値・未確認事項は `work/development-status.md` に書く。
- アイデアは `work/ideas.md` に番号（001〜）付きで記録する。番号は振り直さない。
- まとまった調査や試行は `work/<内容>-YYYY-MM-DD.md` に残す。
- サブエージェントはユーザーが頼んだときだけ使う。
- 「ユーザー決定」「ユーザー指示」と書くのは、ユーザーの言葉を引用できることだけ。自分の提案や付け足しは、
  報告に「よい」と返されても決定として書かない。要るならユーザーにはっきり聞く。
- Muse 向けの文書（ガイド、ツールの説明）に、ユーザーが決めていない呼び名・言い換えを持ち込まない。
  Muse は文書の言葉をまねて話す（「時計」「本体」は自分の言葉が Muse に移った例。2026-11-13）。
- 古い記録（work/、README、スキル）に残る「本体」は私の言葉で、ユーザーの決定ではない。新しく書くときに使わない
  （work/development-status.md の最後の補足）。
