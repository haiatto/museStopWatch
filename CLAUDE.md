# museStopWatch: エージェント作業ルール

## 目的

- M5Stack StopWatch（SKU C152）向けのファームウェアを開発する。
- 具体的な機能目標は未確定。決まったら `work/development-status.md` とこの節を更新する。

## 対象ハードウェア

- M5Stack StopWatch Dev Kit（SKU C152）
  - ESP32-S3R8（PSRAM 8MB）、Flash 16MB
  - 1.75インチ 円形 AMOLED タッチ、マイク・スピーカー、IMU、RTC、振動、ボタン、450mAh バッテリー
- Arduino のボード名は `M5StopWatch`（M5Stack Board Manager 3.3.7 以上）。
  M5Unified 0.2.15 以上、M5GFX 0.2.21 以上が必要。
- 公式資料: https://docs.m5stack.com/en/products/sku/C152
  工場出荷デモ（ESP-IDF）: GitHub の M5StopWatch-UserDemo
- ピン配置・パーティションなどは推測せず、公式資料で確認してから使う。

## 開発環境の方針

- Windows / PowerShell。参考は `C:\MyProj\kantanMuse`（あくまで参考。そのまま流用しない）。
- ツールはプロジェクト内に閉じて導入する（`.venv/`、`.tools/`）。
  グローバルの Python / PlatformIO / Windows 設定は変更しない。
- ビルド手段は未確定。決まったらここに「コマンド」節を追加する。

## 実機への書き込み

- 最初の書き込み前に、フラッシュ全体のバックアップを `backups/` に保存する。
- 書き込み・再起動の前に、ユーザーに一言伝える。COM ポートは毎回確認する。
- ユーザーの指示なしにフラッシュ消去・NVS リセット・eFuse 変更をしない。
- ビルド成功は実機テストではない。画面表示や音はユーザーに確認してもらい、
  自分で見た・聞いたとは書かない。

## Git

- 公開リポジトリ: https://github.com/haiatto/museStopWatch （2026-11-06 ユーザー指示で公開）
- コミット・push はユーザーの指示があったときだけ行う。force push はしない。
- Wi-Fi パスワード、トークン、バックアップの中身を出力・コミットしない。

## 作業記録

- 現在の状況・実測値・未確認事項は `work/development-status.md` に書く。
- まとまった調査や試行は `work/<内容>-YYYY-MM-DD.md` に残す。
- サブエージェントはユーザーが頼んだときだけ使う。
