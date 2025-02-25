/*
 * Custom License (自由利用ライセンス)
 *
 * Copyright (c) 2025 RChikamura
 *
 * このソフトウェアは自由に利用・改変・再配布できますが、
 * 著作権は RChikamura に帰属します。
 * 再配布時は、本ライセンス文を保持してください。
 *
 * 問い合わせには可能な範囲で対応しますが、全てのサポートを保証するものではありません。
 * 本ソフトウェアは「現状のまま」提供され、いかなる保証も行いません。
 * 利用に伴う損害について、作者は一切責任を負いません。
 */
// ===============================
//        必要なライブラリのインクルード
// ===============================
#include <Arduino.h>       // Arduino フレームワークの基本ライブラリ
#include <WiFi.h>          // ESP32 の WiFi 通信を制御するライブラリ
#include <WebServer.h>     // ESP32 で Web サーバーを動作させるためのライブラリ
#include "LittleFS.h"      // 小型ファイルシステム（LittleFS）のライブラリ
#include "CSVReader.h"     // CSV データを読み取るカスタムクラス
#include "drawBitmap.h"    // BMP 画像描画関連のカスタムライブラリ

//#define DEBUG  // デバッグモードを有効にする場合はコメントを解除

// ===============================
//         LEDパネルの設定
// ===============================

/**
 * @brief LED マトリクスパネルのインスタンス
 *
 * `MatrixPanel_I2S_DMA` は ESP32 用の LED マトリクス制御ライブラリ。
 * `matrix` は LED パネル全体を制御するためのオブジェクト。
 */
MatrixPanel_I2S_DMA *matrix;

/**
 * @brief LEDマトリクスパネルの設定
 *
 * LEDマトリクスディスプレイの解像度、接続パネル数、輝度を設定する。
 * - `PANEL_RES_X` はパネル1枚あたりの横解像度（ピクセル数）
 * - `PANEL_RES_Y` はパネル1枚あたりの縦解像度（ピクセル数）
 * - `PANEL_CHAIN` はチェイン接続されたパネルの総数（横方向に連結）
 * - `PANEL_BRIGHTNESS` は表示輝度（0～255の範囲で設定可能）
 *
 * 例えば、`PANEL_RES_X = 64`, `PANEL_RES_Y = 32`, `PANEL_CHAIN = 2` の場合:
 * - 64×32ピクセルのパネルを **横に2枚** 連結し、全体で **128×32ピクセル** のディスプレイとして使用する
 * - `PANEL_BRIGHTNESS = 128` なら、最大輝度（255）の約50%で動作
 *
 * @note 設定を変更した場合、マトリクス制御ライブラリの初期化処理も適切に調整する必要がある。
 */
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 2    // チェイン接続されたパネルの総数
#define PANEL_BRIGHTNESS 128 // 輝度の設定（0～255）
const int panelWidth = PANEL_RES_X * PANEL_CHAIN;  // 全体の横幅
const int panelHeight = PANEL_RES_Y;               // 全体の縦幅

// ===============================
//          CSVファイル設定
// ===============================

/**
 * @brief CSVファイルのパス（LittleFS 上）
 *
 * 列車の表示データは CSV に保存されており、それを取得して描画する。
 */
const char *fullListPath = "/list/list_full.csv";  // 全画面データ
const char *typeListPath = "/list/list_type.csv";  // 種別データ
const char *destListPath = "/list/list_dest.csv";    // 行先データ
const char *nextListPath = "/list/list_next.csv";  // 次駅データ

/**
 * @brief CSVファイルのデータを管理する `CSVReader` インスタンス
 *
 * 各 CSV を扱うために `CSVReader` クラスのインスタンスを作成。
 * それぞれのファイルパスを指定し、列車データを取得できるようにする。
 */
CSVReader fullReader(fullListPath);
CSVReader typeReader(typeListPath);
CSVReader destReader(destListPath);
CSVReader nextReader(nextListPath);

// ===============================
//          表示モード設定
// ===============================

/**
 * @brief 列車表示モード（`mode` によって異なる描画を行う）
 *
 * 0: 全画面表示
 * 1: 種別 + 行先 (俗に言う始発表示)
 * 2: 種別 + 行先 + 次駅
 * 3: 停車駅スクロール
 */
unsigned short mode = 0;

// ===============================
//         列車データ設定
// ===============================

/**
 * @brief 表示する列車データの ID（CSV 内の行番号）
 *
 * 各データは CSV 内で定義されており、番号を指定すると該当する情報を取得できる。
 */
unsigned short num_full = 1;  // 全画面表示用のデータ
unsigned short num_type = 1;  // 種別データ
unsigned short num_dest  = 1;  // 行先データ
unsigned short num_dep  = 7;  // 始発駅データ
unsigned short num_next = 1;  // 次駅データ

// ===============================
//      中間描画用キャンバス
// ===============================

/**
 * @brief LED パネル描画用のバッファ（キャンバス）
 *
 * `GFXcanvas16` を使用して、LED パネルに直接描画するのではなく、
 * 一度キャンバスに描画してから表示することで、スムーズな更新を実現する。
 */
GFXcanvas16 canvas(PANEL_RES_X * PANEL_CHAIN, PANEL_RES_Y);

// ===============================
//         Webサーバー設定
// ===============================

/**
 * @brief Web サーバーのインスタンス
 *
 * ESP32 上で Web サーバーを実行し、HTTP リクエストを処理する。
 * 例えば、WiFi 経由で LED パネルの設定を変更できるようにする。
 */
WebServer server(80);  // ポート 80 で Web サーバーを開始

// ===============================
//       マルチタスク設定
// ===============================

/**
 * @brief ESP32 のマルチタスク用ハンドル
 *
 * ESP32 はデュアルコアのため、複数の処理を並列で実行できる。
 * `TaskPanel` は LED パネルの描画を処理するタスク。
 * `TaskServer` は Web サーバーのリクエスト処理を行うタスク。
 */
TaskHandle_t TaskPanel;  // LED パネル制御タスク
TaskHandle_t TaskServer; // Web サーバータスク

// ===============================
//          WiFi 設定
// ===============================

/**
 * @brief ESP32 の WiFi 設定
 *
 * ESP32 を WiFi に接続するための SSID（ネットワーク名）とパスワードを指定する。
 * `ssid` : 接続する WiFi の SSID
 * `password` : WiFi のパスワード
 */
const char* ssid = "Your_SSID";     // WiFi の SSID（ネットワーク名）
const char* password = "PASSWORD";  // WiFi のパスワード

// ===============================
//      関数の定義と実行
// ===============================

/**
 * @brief LittleFS に保存されたファイル一覧を取得（デバッグ用）
 *
 * `LittleFS` は ESP32 の小型ファイルシステム（SPIFFS の代替）として動作する。
 * この関数を呼び出すと、ESP32 内に保存されているファイル一覧をシリアルモニタに出力する。
 */
void listLittleFSFiles() {
    Serial.println("LittleFS内のファイル一覧:");

    // 1. ルートディレクトリを開く
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("ルートディレクトリが開けませんでした！");
        return;
    }

    // 2. ファイルを順番に取得して一覧表示
    File file = root.openNextFile();
    while (file) {
        Serial.printf("ファイル名: %s, サイズ: %d バイト\n", file.name(), file.size());
        file = root.openNextFile();
    }

    Serial.println("ファイル一覧の取得が完了しました！");
}

/**
 * @brief LED マトリクスパネルの初期化
 *
 * ESP32 の HUB75 LED マトリクスを初期化し、描画を行う準備をする。
 * - パネルの解像度 (`PANEL_RES_X` / `PANEL_RES_Y`) と連結数 (`PANEL_CHAIN`) を設定
 * - `MatrixPanel_I2S_DMA` のオブジェクトを作成し、パネルを制御
 * - 輝度を設定し、初期状態で画面をクリアする
 */
void initPanel() {
    // 1. パネル設定のロード（HUB75 の構成を定義）
    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X,   // 1つのパネルの横幅
        PANEL_RES_Y,   // 1つのパネルの縦幅
        PANEL_CHAIN    // 連結するパネルの数
    );

    // 2. LED マトリクスパネルのオブジェクトを作成
    matrix = new MatrixPanel_I2S_DMA(mxconfig);

    // 3. パネルの初期化を実行
    matrix->begin();

    // 4. パネルの輝度（明るさ）を設定（0～255 の範囲）
    matrix->setBrightness8(PANEL_BRIGHTNESS);

    // 5. 初期状態でパネルをクリア（全画面を黒にする）
    matrix->clearScreen();

    // 6. 画像用バッファ（今後の描画用）
    uint32_t* buffer = nullptr; // 画像のデータを保持するバッファ
    int imgWidth = 0;           // 読み込んだ画像の幅
    int imgHeight = 0;          // 読み込んだ画像の高さ
}

/**
 * @brief CSV から BMP のパスを取得し、指定座標に描画する関数
 *
 * CSV に保存された画像データのパスを取得し、LED マトリクスに表示する。
 * - CSV からパスを検索（`getPath()` を使用）
 * - 取得したパスをもとに `drawBMP()` を呼び出して描画
 * - パスが取得できなかった場合はエラーメッセージを出力
 *
 * @param reader  使用する CSV のインスタンス（例: `typeReader`, `destReader` など）
 * @param IDNumber 取得したいパーツの ID（CSV 内の行番号）
 * @param label    取得したいデータの列名（CSV のヘッダーと一致する文字列）
 * @param startX   画像の左上の X 座標（描画開始位置）
 * @param startY   画像の左上の Y 座標（描画開始位置）
 */
void drawImageFromReader(CSVReader &reader, int IDNumber, const String &label, int startX, int startY) {
    // 1. CSV から画像のパスを取得（`getPath()` を呼び出す）
    String imagePath = reader.getPath(IDNumber, label);

    // 2. パスが取得できたか確認（最低 1 文字の長さがあるかチェック）
    if (imagePath.length() > 0) {
        // 3. 画像を LED パネルに描画
        drawBMP(imagePath, startX, startY);

        // 4. デバッグモードの場合、取得したパスと座標をシリアルに出力
        #ifdef DEBUG
            Serial.println(imagePath + String(startX) + String(startY));
            Serial.printf("画像を描画しました: %s\n", imagePath.c_str());
        #endif
    } else {
        // 5. 画像が見つからなかった場合のエラーメッセージを出力
        Serial.printf("画像が見つかりませんでした: 行=%d, ラベル=%s\n", IDNumber, label.c_str());
    }
}

/**
 * @brief 指定範囲の停車駅リストを `imagePaths` に追加し、停車駅数をカウントする
 *
 * 指定した範囲の駅 ID について、種別 `className` に該当する駅を `imagePaths` に追加する。
 * - `start` と `end` の大小を判定し、自動でリストを上る or 下る方向を決定
 * - 停車駅数が12を超えた場合は処理を中断し、制限フラグを返す
 * - `cnt` を引数にすることで、異なる路線の処理を分けてもカウントを引き継げる
 *
 * @param imagePaths 停車駅画像リスト（更新対象）
 * @param nextReader 次駅表示の CSV インスタンス
 * @param typeReader 種別表示の CSV インスタンス
 * @param numType 種別の ID（該当するクラスを検索するために使用）
 * @param start 検索開始駅 ID
 * @param end 検索終了駅 ID
 * @param cnt 現在の停車駅数（外部で管理し、継続的にカウント可能）
 * @return 停車駅が12駅を超えた場合は `true`（表示制限フラグ）、そうでなければ `false`
 */
bool addStationList(std::vector<String> &imagePaths, CSVReader &nextReader, CSVReader &typeReader, int numType, int start, int end, unsigned char &cnt) {
    bool overLimit = false; // 停車駅が 12 駅を超えたか
    int Startid = (start<end)? (start + 1) : (start - 1);
    int step = (start < end) ? +1 : -1; // 自動でリストの進む方向を判定

    for (int i = Startid; i != end; i += step) {
        if (cnt >= 12) {  // 12駅を超えたら中断
            overLimit = true;
            break;
        }
        if (containsWord(nextReader.getPath(i, "type"), typeReader.getPath(numType, "className"))) {
            imagePaths.emplace_back("/img/Scroll/touten.bmp"); // 「、」
            imagePaths.emplace_back(nextReader.getPath(i, "Scroll")); // 駅名
            cnt++;
        }
    }

    return overLimit;
}

/**
 * @brief 全画面描画 (Mode 0)
 *
 * 指定された ID の BMP 画像を 1 枚表示する。
 * - CSV から画像パスを取得し、座標 (0,0) に描画するだけのシンプルな処理
 * - 主にフルスクリーン用の BMP 画像を表示するのに使われる
 *
 * @param fullReader 全画面表示用 CSV のインスタンス
 * @param numFull 表示する全画面 BMP の ID
 */
void drawMode0(CSVReader &fullReader, int numFull) {
    drawImageFromReader(fullReader, numFull, "path", 0, 0);
}

/**
 * @brief 種別 + 行先(俗に言う始発表示)の描画 (Mode 1)
 *
 * 指定された ID の BMP 画像を 2 枚表示する。
 * - 種別（例: 急行・快速など）と行先（例: 新宿・池袋など）を組み合わせて表示
 * - 画像の配置は、種別を `(0,0)`, 行先を `(48,0)` に描画
 * - 次駅IDから路線を判別し、行先表示の一部として利用することがある
 *
 * @param typeReader 種別用 CSV のインスタンス
 * @param destReader 行先用 CSV のインスタンス
 * @param numType 表示する種別の ID
 * @param numDest 表示する行先の ID
 * @param numNext 次駅の ID(表示はしないが、番号から路線を判別する)
 */
void drawMode1(CSVReader &typeReader, CSVReader &destReader, int numType, int numDest, int numNext) {
    // 1. 直前の表示データを記録し、変更があった場合のみ更新する
    static int last_type = -1, last_dest = -1, last_next = -1;

    if(numType != last_type) { // 種別に変更があったとき
        drawImageFromReader(typeReader, numType, "large", 0, 0); // 種別を描画
        last_type = numType;
    }

    if(numDest >= 900 || numNext == 0 || numNext >= 900) {
        // 行先が無効範囲 (900番台) または次駅が無効範囲 (無表示 or 900番台)、もしくは路線名が非表示にされているとき
        if(numDest != last_dest) { // 行先に変更があったとき
            drawImageFromReader(destReader, numDest, "large", 48, 0);  // 行先を描画
            last_dest = numDest;
        }
    } else {
        // 2. トグル表示のためのキャッシュ準備
        static BMPData bmpCacheLine, bmpCacheDest;
        static std::vector<BMPData*> partDest;
        static std::vector<ToggleCacheBMPPart> parts;
        bool flg_change = false;

        if(numDest != last_dest) { // 行先に変更があったとき
            cacheBMPData(destReader.getPath(numDest, "large"), bmpCacheDest);
            last_dest = numDest;
            flg_change = true;
        }
        if(numNext != last_next) { // 次駅に変更があったとき
            if(numNext < 100) {
                cacheBMPData(destReader.getPath(901, "large"), bmpCacheLine); // 夢の森線
            } else {
                cacheBMPData(destReader.getPath(902, "large"), bmpCacheLine); // 花霞線
            }
            last_next = numNext;
            flg_change = true;
        }

        // 3. パーツ構造体を更新（変更があった場合のみ）
        if (flg_change) {
            partDest.clear();
            partDest.emplace_back(&bmpCacheLine); // 路線
            partDest.emplace_back(&bmpCacheDest); // 行先
            parts.clear();
            parts.emplace_back(ToggleCacheBMPPart(partDest, 48, 0));
            flg_change = false;
        }

        // 4. 設定したパーツを `toggleCacheBMP()` で一定間隔ごとに切り替え表示
        toggleCacheBMP(parts, 2, 3000);
    }
}

/**
 * @brief 種別 + 行先 + 次駅の描画 (Mode 2)
 *
 * 指定された ID の BMP 画像を 3 枚表示し、言語 (日本語 / 英語) のトグル処理を行う。
 * - 日本語版 / 英語版の 2 つの画像を交互に切り替える
 * - toggleBMP() を使用して 3000ms ごとに表示を更新
 * - CSV のパスをキャッシュし、ID の変更があった場合のみ再取得することで処理を最適化
 *
 * @param typeReader 種別用 CSV のインスタンス
 * @param destReader 行先用 CSV のインスタンス
 * @param nextReader 次駅用 CSV のインスタンス
 * @param numType 表示する種別の ID
 * @param numDest 表示する行先の ID
 * @param numNext 表示する次駅の ID
 */
void drawMode2(CSVReader &typeReader, CSVReader &destReader, CSVReader &nextReader, int numType, int numDest, int numNext) {
    // 1. 直前の表示データを記録し、変更があった場合のみ更新する
    static int last_numType = -1, last_numDest = -1, last_numNext = -1; // 前回のID
    static String type_jp, type_en, dest_jp, dest_en, next_jp, next_en; // 各パーツの画像パス
    static BMPData bmpCacheTypeJP, bmpCacheTypeEN; // 種別（日本語 / 英語）
    static BMPData bmpCacheDestJP, bmpCacheDestEN; // 行先（日本語 / 英語）
    static BMPData bmpCacheNextJP, bmpCacheNextEN; // 次駅（日本語 / 英語）
    static BMPData bmpCacheLine; // 路線名
    static std::vector<BMPData*> partType, partDest, partNext; // トグル表示する画像群のベクター
    static std::vector<ToggleCacheBMPPart> parts; // トグル表示用の構造体
    bool flg_change = false; // 1つでもパスが変わった場合に true にする
    bool flg_line = false; // 路線名を表示するか

    // 2. ID に変更があった場合のみ、新しい画像パスを取得
    if (numType != last_numType) {
        cacheBMPData(typeReader.getPath(numType, "JP"), bmpCacheTypeJP);
        cacheBMPData(typeReader.getPath(numType, "EN"), bmpCacheTypeEN);
        last_numType = numType; // ID を更新
        flg_change = true;
    }

    if (numDest != last_numDest) {
        //dest_jp = destReader.getPath(numDest, "JP"); // 日本語版
        //dest_en = destReader.getPath(numDest, "EN"); // 英語版
		cacheBMPData(destReader.getPath(numDest, "JP"), bmpCacheDestJP);
		cacheBMPData(destReader.getPath(numDest, "EN"), bmpCacheDestEN);
        last_numDest = numDest; // ID を更新
        flg_change = true;
    }

    if (numNext != last_numNext) {
        cacheBMPData(nextReader.getPath(numNext, "JP"), bmpCacheNextJP);
        cacheBMPData(nextReader.getPath(numNext, "EN"), bmpCacheNextEN);
        if(numDest < 900 && numNext != 0 && numNext < 900){
            // 行き先が無効範囲(900番台)か次駅が無効範囲(無表示または900番台)ではなく、かつ路線名表示が有効化されているとき
            if(numNext < 100){
                cacheBMPData(destReader.getPath(901, "JP"), bmpCacheLine); // 夢の森線
            } else {
                cacheBMPData(destReader.getPath(902, "JP"), bmpCacheLine); // 花霞線
            }
            flg_line = true;
        } else {
            flg_line = false;
        }
        last_numNext = numNext; // ID を更新
        flg_change = true;
    }

    // 3. パーツ構造体を更新（変更があった場合のみ）
    if (flg_change) {
        // 既存リストをクリア
        partType.clear();
        partDest.clear();
        partNext.clear();
        if(flg_line){
            partType.emplace_back(&bmpCacheTypeJP); // 種別JP
            partDest.emplace_back(&bmpCacheLine); // 路線
            partNext.emplace_back(&bmpCacheNextJP); // 次駅JP
        }
        partType.emplace_back(&bmpCacheTypeJP); // 種別JP(2回目)
        partDest.emplace_back(&bmpCacheDestJP); // 行先JP
        partNext.emplace_back(&bmpCacheNextJP); // 次駅JP(2回目)

        partType.emplace_back(&bmpCacheTypeEN); // 種別EN
        partDest.emplace_back(&bmpCacheDestEN); // 行先EN
        partNext.emplace_back(&bmpCacheNextEN); // 次駅EN

        parts.clear(); // 既存リストをクリア
        parts.emplace_back(ToggleCacheBMPPart(partType, 0, 0));   // 種別
        parts.emplace_back(ToggleCacheBMPPart(partDest, 48, 0));   // 行先
        parts.emplace_back(ToggleCacheBMPPart(partNext, 48, 16)); // 次駅
        flg_change = false; // フラグをリセット
    }

    // 4. 設定したパーツを toggleBMP() で一定間隔ごとに切り替え表示
	toggleCacheBMP(parts, parts[0].bmpList.size(), 3000);
}

/**
 * @brief 種別 + 行先 + 停車駅スクロールの描画 (Mode 3)
 * 日本語 / 英語のトグル処理あり。
 * 停車駅リストをスクロールさせながら表示する。
 * - 停車駅の数が 2 つ未満（始発と終点が隣接）の場合は Mode 2 にフォールバック
 * - cacheConcatenatedImages() を使用し、停車駅リストを 1 枚の画像に連結
 * - updateScroll() を使ってスクロールアニメーションを実行
 *
 * @param typeReader 種別表示の CSV インスタンス
 * @param destReader 行先表示の CSV インスタンス
 * @param nextReader 次駅表示の CSV インスタンス（停車駅リストもここに格納）
 * @param numType 表示する種別の ID
 * @param numDest 表示する行先の ID (停車駅リストの生成にも使用)
 * @param numDep 列車の始発駅の ID (停車駅リストの生成に使用)
 */
void drawMode3(CSVReader &typeReader, CSVReader &destReader, CSVReader &nextReader, int numType, int numDest, int numDep) {
    // 1. 直前の表示データを記録し、変更があった場合のみ更新する
    static int last_numType = -1, last_numDest = -1, last_numDep = -1;
    static BMPData stationScroll;
    static BMPData bmpCacheTypeJP, bmpCacheTypeEN; // 種別（日本語 / 英語）
    static BMPData bmpCacheDestJP, bmpCacheDestEN; // 行先（日本語 / 英語）
    static BMPData bmpCacheLine; // 路線名
    static std::vector<BMPData*> partType, partDest; // トグル表示する画像群のベクター
    static std::vector<ToggleCacheBMPPart> parts;
    static std::vector<String> imagePaths;
    bool flg_change = false; // 種別・行先の画像が変更されたか
    bool scr_change = false; // 停車駅リストが変更されたか
    bool flg_line = false; // 路線名を表示するか

    // 2. 停車駅の数が 2 未満、または行き先に駅名以外(900番台)が設定されているなら Mode 2 にフォールバック
    if (abs(numDest - numDep) < 2 || numDest >= 900 || numDest == 0) {
        mode = 2;
        drawMode2(typeReader, destReader, nextReader, numType, numDest, numDest);
        num_next = numDest;
        return;
    } else {
        // 3. 種別の画像パスを取得し、キャッシュを作成
        if (numType != last_numType) {
            cacheBMPData(typeReader.getPath(numType, "JP"), bmpCacheTypeJP);
            cacheBMPData(typeReader.getPath(numType, "EN"), bmpCacheTypeEN);
            last_numType = numType;
            flg_change = true;
            scr_change = true; // 停車駅リストの更新フラグ
        }

        // 4. 行先の画像パスを取得し、キャッシュを作成
        if (numDest != last_numDest) {
            cacheBMPData(destReader.getPath(numDest, "JP"), bmpCacheDestJP);
            cacheBMPData(destReader.getPath(numDest, "EN"), bmpCacheDestEN);
            last_numDest = numDest;
            flg_change = true;
            scr_change = true;
        }

        // 5. 始発駅が変わった場合も停車駅リストを更新
        if (numDep != last_numDep) {
            if(numDest < 900 && numDep != 0 && numDep < 900){
            // 行き先が無効範囲(900番台)か次駅が無効範囲(無表示または900番台)ではない
                if(numDep < 100){
                    cacheBMPData(destReader.getPath(901, "JP"), bmpCacheLine); // 夢の森線
                } else {
                    cacheBMPData(destReader.getPath(902, "JP"), bmpCacheLine); // 花霞線
                }
                flg_line = true;
            } else {
                flg_line = false;
        }
            last_numDep = numDep;
            flg_change = true;
            scr_change = true;
        }

        // 6. 停車駅リストを更新
        if (scr_change || stationScroll.cache == nullptr) {
            imagePaths.clear(); // 既存リストをクリア
            imagePaths.emplace_back("/img/Scroll/ScrollStart.bmp"); // 「この電車の停車駅は」

            unsigned char cnt = 0; // 停車駅数をカウント
            bool overLimit = false; // 停車駅が 12 駅を超えたか

            // 直通の有無で分岐
            if(numDep < 100 && numDest > 100){ // 夢の森線→花霞線
                overLimit = addStationList(imagePaths, nextReader, typeReader, numType, numDep, 10, cnt); // ID=10(夢の森線夢見ヶ丘)まで
                imagePaths.emplace_back("/img/Scroll/touten.bmp"); // 「、」
                imagePaths.emplace_back(nextReader.getPath(10, "Scroll")); // 夢見ヶ丘
                overLimit = addStationList(imagePaths, nextReader, typeReader, numType, 110, numDest, cnt); // ID=110(花霞線夢見ヶ丘)から
            } else if(numDep > 100 && numDest < 100){ // 花霞線→夢の森線
                overLimit = addStationList(imagePaths, nextReader, typeReader, numType, numDep, 110, cnt); // ID=110(花霞線夢見ヶ丘)まで
                imagePaths.emplace_back("/img/Scroll/touten.bmp"); // 「、」
                imagePaths.emplace_back(nextReader.getPath(10, "Scroll")); // 夢見ヶ丘
                overLimit = addStationList(imagePaths, nextReader, typeReader, numType, 10, numDest, cnt); // ID=10(夢の森線夢見ヶ丘)から
            } else { // 線内完結
                overLimit = addStationList(imagePaths, nextReader, typeReader, numType, numDep, numDest, cnt);
            }

            // 7. 停車駅の終端画像を追加
            if (overLimit) {
                imagePaths.emplace_back("/img/Scroll/ScrollEnd2.bmp"); // 「の順に停まります」
            } else {
                imagePaths.emplace_back("/img/Scroll/ScrollEnd.bmp"); // 「駅に停まります」
            }

            // 8. 停車駅の連結画像キャッシュを作成
            cacheConcatenatedImages(imagePaths, &stationScroll);
            scr_change = false;
        }

        // 9. トグル画像の構造体を更新
        if (flg_change) {
            // 既存リストをクリア
            partType.clear();
            partDest.clear();
            if(flg_line){
                partType.emplace_back(&bmpCacheTypeJP); // 種別JP
                partDest.emplace_back(&bmpCacheLine); // 路線
            }
            partType.emplace_back(&bmpCacheTypeJP); // 種別JP(2回目)
            partDest.emplace_back(&bmpCacheDestJP); // 行先JP

            partType.emplace_back(&bmpCacheTypeEN); // 種別EN
            partDest.emplace_back(&bmpCacheDestEN); // 行先EN

            parts.clear(); // 既存リストをクリア
            parts.emplace_back(ToggleCacheBMPPart(partType, 0, 0));   // 種別
            parts.emplace_back(ToggleCacheBMPPart(partDest, 48, 0));   // 行先
            flg_change = false; // フラグをリセット
        }

        // 10. 画像トグル
        toggleCacheBMP(parts, parts[0].bmpList.size(), 3000);

        // 11. スクロール処理の更新
        updateScroll(&stationScroll, 48, 16, 80, 16, 30);
    }
}

/**
 * @brief パネル制御タスク
 *
 * ESP32 の **コア 1** に割り当てられ、LED パネルの描画を担当する。
 * - `mode` の変更を監視し、適切な描画関数を呼び出す。
 * - `Mode 2`, `Mode 3` ではトグル / スクロール処理をリアルタイムで更新する。
 * - `drawModeX()` の呼び出しは、前回の状態と比較して変更があった場合のみ実行する。
 *
 * @param pvParameters タスク用の引数（未使用）
 */
void panelTask(void *pvParameters) {
    static int last_mode = -1;
    static int last_full = -1, last_type = -1, last_dest = -1, last_dep = -1, last_next = -1;

    while (true) {
        // 1. モード変更 or 列車情報の更新があれば再描画
        if (mode != last_mode || num_full != last_full || num_type != last_type ||
            num_dest != last_dest || num_dep != last_dep || num_next != last_next) {

            #ifdef DEBUG
                Serial.printf("mode: %d\tnum_full: %d\tnum_type: %d\tnum_dest: %d\tnum_next: %d\n",
                              mode, num_full, num_type, num_dest, num_next);
            #endif

            vTaskDelay(100 / portTICK_PERIOD_MS); // 安定性のため 100 ミリ秒の遅延を追加

            // 2. モードに応じて適切な描画関数を呼び出す
            if (mode == 0) {
                drawMode0(fullReader, num_full);  // 全画面表示
            }

            // 3. 直前の状態を保存（次回比較用）
            last_mode = mode;
            last_full = num_full;
            last_type = num_type;
            last_dest = num_dest;
            last_dep = num_dep;
            last_next = num_next;
        }

        // 4. Mode 1, 2, 3 は RTC を使用したトグル / スクロール処理のため、常に実行
        if (mode == 1) {
            drawMode1(typeReader, destReader, num_type, num_dest, num_next);  // 種別 + 行先 (俗に言う始発表示)
        } else if (mode == 2) {
            drawMode2(typeReader, destReader, nextReader, num_type, num_dest, num_next);
        } else if (mode == 3) {
            drawMode3(typeReader, destReader, nextReader, num_type, num_dest, num_dep);
        }
    }
}

/**
 * @brief Webページから数値を取得し、グローバル変数に代入
 *
 * クライアントから受け取ったパラメータ (`name`) の値を `target` に設定する。
 * - 例えば、`http://192.168.x.x/send?mode=2` のようなリクエストを処理できる。
 * - 数値が指定されていない場合は `400 Bad Request` を返す。
 *
 * @param target 取得した数値を代入する変数
 * @param name 取得する URL パラメータの名前
 */
void web2gnum(unsigned short *target, String name) {
    if (server.hasArg(name)) {
        unsigned short webnum = server.arg(name).toInt(); // 受け取った値を整数に変換
        *target = webnum; // 変数に代入

        server.send(200, "text/plain", String(name) + (": ") + (*target)); // 成功レスポンス
        #ifdef DEBUG
            Serial.println(name + ": " + *target);
        #endif
    } else {
        server.send(400, "text/plain", "number not specified"); // 失敗レスポンス
    }
}

/**
 * @brief Web サーバーの現在の状態を JSON 形式で返す
 *
 * クライアントが `/status` にアクセスすると、ESP32 の変数状態を JSON で返す。
 * - 例: `{ "mode": 2, "full": 0, "type": 1, "dest": 5, "dep": 3, "next": 6 }`
 */
void sendStatus() {
    String json = "{";
    json += "\"mode\":" + String(mode) + ",";
    json += "\"full\":" + String(num_full) + ",";
    json += "\"type\":" + String(num_type) + ",";
    json += "\"dest\":" + String(num_dest) + ",";
    json += "\"dep\":" + String(num_dep) + ",";
    json += "\"next\":" + String(num_next);
    json += "}";

    server.send(200, "application/json", json); // JSON をクライアントへ送信

    #ifdef DEBUG
        Serial.println("Sent JSON status: " + json);
    #endif
}

/**
 * @brief Web サーバータスク
 *
 * ESP32 の **コア 0** に割り当てられ、HTTP リクエストを処理する。
 * - WiFi に接続し、ESP32 の IP アドレスをシリアル出力
 * - `index.html` をクライアントへ送信
 * - `/send` エンドポイントでパラメータを受け取り、変数を更新
 *
 * @param pvParameters タスク用の引数（未使用）
 */
void serverTask(void *pvParameters) {
    #ifdef DEBUG
        Serial.println("デバッグモード：WiFi接続を開始します…");
    #endif

    WiFi.begin(ssid, password);

    // 1. WiFi 接続処理（接続完了まで 500ms ごとにチェック）
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        #ifdef DEBUG
            Serial.print(".");  // 接続待機中はドットを表示
        #endif
    }

    // 2. WiFi 接続成功時の情報をシリアル出力
    Serial.println("\nWiFi接続成功！");
    Serial.print("ESP32のIPアドレス: ");
    Serial.println(WiFi.localIP());

    // 3. Web サーバーの設定
    // 3.1 `index.html` を提供
    server.on("/", HTTP_GET, []() {
        File file = LittleFS.open("/index_csv.html", "r");
        if (!file) {
            server.send(404, "text/plain", "File not found");
            #ifdef DEBUG
                Serial.println("index.htmlが見つかりませんでした！");
            #endif
            return;
        }
        server.streamFile(file, "text/html");
        file.close();

        #ifdef DEBUG
            Serial.println("index.htmlをクライアントに送信しました。");
        #endif
    });

    // 3.2 `/send` で変数を更新
    server.on("/send", HTTP_GET, []() {
        web2gnum(&mode, "mode");
        web2gnum(&num_full, "full");
        web2gnum(&num_type, "type");
        web2gnum(&num_dest, "dest");
        web2gnum(&num_dep, "dep");
        web2gnum(&num_next, "next");
    });

    // 3.3 `/status` で現在の変数状態を取得 (JSON)
    server.on("/status", HTTP_GET, sendStatus);

    // 3.4 `/test` でCSV表示用の `test_csv.html` を提供
    server.on("/test", HTTP_GET, []() {
        File file = LittleFS.open("/test_csv.html", "r");
        if (!file) {
            server.send(404, "text/plain", "File not found");
            #ifdef DEBUG
                Serial.println("test_csv.htmlが見つかりませんでした！");
            #endif
            return;
        }
        server.streamFile(file, "text/html");
        file.close();

        #ifdef DEBUG
            Serial.println("test_csv.htmlをクライアントに送信しました。");
        #endif
    });

    // 3.5 CSV ファイル提供エンドポイント
    server.on("/list/list_full.csv", HTTP_GET, []() {
        File file = LittleFS.open("/list/list_full.csv", "r");
        if (!file) {
            server.send(404, "text/plain", "CSV File Not Found");
            return;
        }
        server.streamFile(file, "text/csv");
        file.close();
    });

    server.on("/list/list_type.csv", HTTP_GET, []() {
        File file = LittleFS.open("/list/list_type.csv", "r");
        if (!file) {
            server.send(404, "text/plain", "CSV File Not Found");
            return;
        }
        server.streamFile(file, "text/csv");
        file.close();
    });

    server.on("/list/list_dest.csv", HTTP_GET, []() {
        File file = LittleFS.open("/list/list_dest.csv", "r");
        if (!file) {
            server.send(404, "text/plain", "CSV File Not Found");
            return;
        }
        server.streamFile(file, "text/csv");
        file.close();
    });

    server.on("/list/list_next.csv", HTTP_GET, []() {
        File file = LittleFS.open("/list/list_next.csv", "r");
        if (!file) {
            server.send(404, "text/plain", "CSV File Not Found");
            return;
        }
        server.streamFile(file, "text/csv");
        file.close();
    });

    // 4. Web サーバー開始
    server.begin();
    #ifdef DEBUG
        Serial.println("Webサーバーが開始されました。");
    #endif

    // 5. HTTP リクエストを常時監視（ESP32 の loop を使わずにタスク内で処理）
    while (true) {
        server.handleClient();
        vTaskDelay(10 / portTICK_PERIOD_MS); // 安定性のため 10 ミリ秒の遅延
    }
}

/**
 * @brief ESP32 の初期化処理
 *
 * - LittleFS の初期化
 * - GPIO の設定
 * - LED パネルの初期化
 * - タスクの作成（パネル制御 / Web サーバー）
 */
void setup() {
    Serial.begin(115200);

    // 1. LittleFS の初期化
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFSの初期化に失敗しました。");
    }

    // 2. ノイズ防止のため GPIO32 を LOW に設定
    pinMode(32, OUTPUT);
    digitalWrite(32, LOW);

    // 3. LED パネルの初期化
    initPanel();

    // 4. タスクの作成とコア割り当て
    // 4.1 パネル描画処理（コア 1）
    xTaskCreatePinnedToCore(panelTask, "Panel_Task", 4096, NULL, 1, &TaskPanel, 1);

    // 4.2 HTTP 処理（コア 0）
    xTaskCreatePinnedToCore(serverTask, "Server_Task", 4096, NULL, 1, &TaskServer, 0);

    #ifdef DEBUG
        Serial.println("Initialized");

        // 5. LittleFS のストレージ情報を取得
        size_t total = LittleFS.totalBytes();
        size_t used = LittleFS.usedBytes();
        Serial.printf("Total: %d bytes, Used: %d bytes, Free: %d bytes\n", total, used, total - used);

        // 6. 保存されているファイル一覧を表示
        listLittleFSFiles();
        delay(5000);
    #endif
}

/**
 * @brief メインループ
 *
 * - ESP32 のタスク処理をタスクごとに分けているため、ここでは何もしない。
 */
void loop() {
    // すべての処理はタスクで実行されるため、loop() には処理を入れない。
}
