# 01_LittleFS_WebSocket

## **概要**
このバージョンは、ESP32 を用いた **HUB75 LEDパネル** の行先表示システムです。**LittleFS** を使用したファイル管理と、**WebSocket** によるリアルタイム通信をサポートしています。

---

## **特徴**
- **ESP32-WROOM-32E-N16 + オリジナル基板**（回路図は `schematics` フォルダ内）
- **Wi-Fi制御対応**（WebSocket通信で、スマホやPCからリアルタイムで表示を変更）
- **LittleFSストレージ対応**（ESP32の内部ストレージに画像や設定を保存可能）
- **動的ファイル管理**（CSVファイルを編集するだけで行き先リストをカスタマイズ可能）
- **HUB75 LEDパネル対応**（ESP32専用DMAライブラリを使用した滑らかな表示）

---

## **必要なハードウェア**
- **ESP32** (推奨ボード: [ESP32-WROOM-32E-N16](https://akizukidenshi.com/catalog/g/g115675/))  
※フラッシュサイズが16MB未満のモデル(DevKitCなど)では、エラーが出る可能性が高い。
- **専用回路基板**（schematicsフォルダにKiCadデータあり）
- **HUB75 LEDマトリクスパネル** (連結後のサイズ 128x32 推奨)
- **電源 (5V, 5A以上推奨)** (例: [5V10A スイッチング電源](https://akizukidenshi.com/catalog/g/g109086/))
- **UARTモジュール(3.3V対応)** 推奨品: [AE-TTL-232R](https://akizukidenshi.com/catalog/g/g109951/)  
※基板のフル機能を利用するには改造が必要になる([改造の概要](./docs/UART_jump.md))。

---

## **セットアップ手順**

### **1. PlatformIO のセットアップ**
1. [VS Code](https://code.visualstudio.com/) をインストール
2. [PlatformIO](https://platformio.org/install/ide?install=vscode) 拡張機能をインストール
3. `01_LittleFS_WebSocket` フォルダを **PlatformIO プロジェクトとして開く**

### **2. 必要なライブラリのインストール**
PlatformIO の **Libraries** タブを開き、以下のライブラリを検索してインストールしてください。

| ライブラリ名 | バージョン | 検索ワード | 公式リンク |
|-------------|----------|-----------|------------|
| ESP32 HUB75 LED MATRIX PANEL DMA Display | `3.0.12` | `ESP32 HUB75 LED MATRIX` | [GitHub](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA) |
| Adafruit GFX Library | `1.11.11` | `Adafruit GFX` | [GitHub](https://github.com/adafruit/Adafruit-GFX-Library) |
| ESPAsyncWebServer-esphome | `3.3.0` | `ESPAsyncWebServer` | [GitHub](https://github.com/esphome/ESPAsyncWebServer) |

---

## **フォルダ構成**
```
01_LittleFS_WebSocket/
├── src/                 # ソースコード
│   ├── CSVReader.cpp    # CSV処理の実装
│   ├── drawBitmap.cpp   # 画像描画の実装
│   ├── main.cpp         # メインプログラム
├── data/                # LittleFS 用のデータ
│   ├── list/            # CSVファイル (行先リスト)
│   ├── img/             # 画像データ (BMP形式)
│   ├── index_CSV.html   # 操作パネル (HTML形式)
├── schematics/          # 回路図・基板データ（KiCad）
├── platformio.ini       # PlatformIO の設定
└── README.md            # このファイル
```

---

## **ビルドと書き込み**
1. **UARTモジュール**を基板と接続する。このとき、未改造モジュールを使用するならスライドスイッチをDL側に切り替える
2. **PlatformIO** の **Upload** ボタンをクリック（下側の→マーク）
3. **Upload Filesystem Image** で `data/` 内のファイルを ESP32 の **LittleFS** に書き込む
4. スライドスイッチをAuto側に切り替える。
5. **Monitor** でESP32のデバッグ情報を確認 (`monitor_speed = 115200`)。このとき、IPアドレスを控えておく

## **表示切替**
1. http://(ESP32のIPアドレス)/ にアクセスする
2. 画面を操作し、好みの表示内容にする
3. 表示更新ボタンをクリックする  
※スクロール表示の場合、スクロール生成にしばらく時間がかかるため、切り替え直後に長時間フリーズします。
