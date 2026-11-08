# 実験: アプリ側に M5GFX を入れた ELF（2026-11-08、この Windows PC）

- 目的: ミニアプリが M5GFX を自分で持って、ゲームの描画に使えるかを確かめる。
- 使ったもの: 公式デモの M5GFX 0.2.19（`upstream/M5StopWatch-UserDemo/components/M5GFX`）と、デモのビルド設定（IDF 5.5.5、GCC 14.2）。
  ファーム（IDF 6.0.3）とは版が違う。形を見るための実験。
- アプリ: `LGFX_Sprite`（メモリ上の 466×466 画面）に円・線・文字（Font4）を描き、`msw_present()`（仮）で本体に渡す。
  パネルのドライバ（`M5GFX` / `LGFX_Device`）は使わない。画面は本体が持つ。
- コンパイル対象: アプリと M5GFX の 8 ファイル（LGFXBase、LGFX_Sprite、lgfx_fonts、pixelcopy、common_function、
  SpriteBuffer、Panel_Device、platforms/esp32/common）。`-fPIC -fno-exceptions -fno-rtti -O2`、`-shared --gc-sections`。
- 結果:
  - そのまま: コード 508KB、本体に求める関数 220 個（日本語・中国語などの全フォント、PNG/JPEG/QOI、QR、SPI・I2C・GPIO のドライバ）。
  - `-fvisibility=hidden`（外に見せるのは `app_main` だけ）: **コード 56KB、データ 1.6KB、求める関数 20 個**。
    求めたもの: malloc/free/heap_caps、new/delete、memcpy 等、vsnprintf、ceilf、esp_timer_get_time、vTaskDelay、
    `__cxa_pure_virtual`、`__atomic_fetch_add_4`、レジスタ `GPIO`・`GPIO_PIN_MUX_REG`（呼ばれないコードからの参照）。
  - `__dso_handle` は自分で定義した（静的オブジェクトの後片付け用。elf_loader は静的コンストラクタを動かさない）。
  - コンパイル 12.5 秒（M5GFX を毎回コンパイルした場合）。M5GFX をライブラリとして一度作っておけば、アプリ側はリンクだけで済む。
- 未確認: 本体で読み込んで動かすこと。IDF 6.0.3・GCC 15 での再現。日本語フォントを入れたときの大きさ。
