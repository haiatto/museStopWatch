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
- 実機: 未接続・未確認。フラッシュのバックアップはまだ取っていない。
