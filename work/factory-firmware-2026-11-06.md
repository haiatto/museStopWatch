# 出荷時ファームウェアの調査（2026-11-06）

## 結論

- 出荷時ファームは M5Stack 公式の「UserDemo」。ハードウェア評価用のデモアプリ集で、
  汎用の実行環境（MicroPython や UIFlow のような環境）ではない。
- ソース公開: https://github.com/m5stack/M5StopWatch-UserDemo （MIT ライセンス）
- 自作ファームを書き込むと上書きされる。戻すにはこのリポジトリをビルドして書き込むか、
  書き込み前に取ったフラッシュのバックアップを戻す。

## 構成

- ESP-IDF v5.5.4 のプロジェクト（Arduino ではない）。
  PlatformIO 用の `platformio.ini` も同梱（pioarduino 55.03.312、framework = espidf、
  board = esp32s3box を流用、依存は scripts/fetch_components.py で取得）。
- アプリ管理フレームワーク mooncake（M5Stack の Forairaaaaa 氏作）の上に、
  以下のアプリを載せている:
  ランチャー、FluidBox（流体シミュレーション）、アラーム時計、時計の文字盤、
  ストップウォッチ、名札（Badge）、IMU 表示、FFT（音声スペクトル）、ラッキールーレット、設定。
- UI: LVGL 9.5 + smooth_ui_toolkit + M5GFX 0.2.19。
- `main/hal/` にハードウェア抽象化層がある（ディスプレイ、タッチ CST820、RTC RX8130、
  IMU BMI270/BMM150、電源 M5PM1、IO エキスパンダ M5IOE1、オーディオ、ボタン、ネットワーク等）。
  自作するときのピン配置・初期化手順の参考になる。
- パーティション: nvs / otadata / phy_init / ota_0 (約5MB) / ota_1 (約5MB) / storage (FAT 4MB) / coredump。

## Arduino 側の情報（M5Stack ボードパッケージ 3.3.10）

- ボード ID `m5stack_stopwatch`、ESP32-S3、Flash 16MB、PSRAM、USB CDC on boot 有効。
- variant: `variants/m5stack_stopwatch/pins_arduino.h`
- pioarduino の公式ボード一覧には StopWatch 用定義がない（2026-11-06 時点）。
