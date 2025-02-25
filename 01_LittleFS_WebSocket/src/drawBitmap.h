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
#ifndef DRAWBITMAP_H
#define DRAWBITMAP_H

// ===============================
//      必要なライブラリのインクルード
// ===============================
#include "LittleFS.h"  // ESP32 の LittleFS を使用するためのライブラリ
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // HUB75 LED パネル制御ライブラリ

//#define DEBUG  // デバッグモードを有効にする場合はコメントを解除

// ===============================
//      外部で初期化される LED マトリクスパネルのインスタンスとサイズ
// ===============================
extern MatrixPanel_I2S_DMA *matrix; // LED パネルのインスタンス
extern const int panelWidth;  // パネルの横幅（ピクセル単位）
extern const int panelHeight; // パネルの縦幅（ピクセル単位）

// ===============================
//      表示の更新間隔（ミリ秒単位）
// ===============================
//extern unsigned long switchInterval; // 画像の切り替え間隔（トグル表示用）
//extern unsigned long scrollInterval; // スクロール更新間隔（スクロール表示用）

// ===============================
//      表示状態管理フラグ
// ===============================
extern bool toggleState;     // トグル表示の状態（true: 表示1, false: 表示2）
extern bool flg_scrollEnd; // スクロールが終了したかどうかのフラグ（true: 終了）
extern bool toggleLangState; // 言語切り替えフラグ（true: 日本語, false: 英語）

// ===============================
//      時間管理用変数（更新タイミング管理）
// ===============================
extern unsigned long previousToggleMillis; // 最後にトグルした時間
extern unsigned long previousScrollMillis; // 最後にスクロールを更新した時間

// ===============================
//      BMP データ構造体定義
// ===============================

/**
 * @brief BMPデータのキャッシュ用構造体
 *
 * 画像データをメモリに保持し、再描画時に素早くアクセスできるようにする。
 */
struct BMPData {
    uint16_t *cache = nullptr; // ピクセルデータのキャッシュ（RGB565形式）
    int width = 0;  // 画像の横幅（ピクセル単位）
    int height = 0; // 画像の縦幅（ピクセル単位）
    int offsetX = 0; // 画像のオフセット（スクロールの際に使用）
};

/**
 * @brief BMP画像の切り替え用構造体（ファイルパス指定）
 *
 * LittleFS に保存された BMP ファイルのパスを管理し、画像のトグル表示を行う。
 */
struct ToggleBMPPart {
    String bmp1;   // 表示1（日本語など）
    String bmp2;   // 表示2（英語など）
    int startX;    // 描画開始 X 座標
    int startY;    // 描画開始 Y 座標

    // コンストラクタ
    ToggleBMPPart(const String &b1, const String &b2, int x, int y)
        : bmp1(b1), bmp2(b2), startX(x), startY(y) {}
};

/**
 * @brief BMP画像の切り替え用構造体（キャッシュデータを使用）
 *
 * - `bmpList` に **任意の数の画像データを格納可能**
 * - `startX`, `startY` は **描画位置**
 * - 使用する画像の枚数は **外部で管理（関数の引数で指定）**
 */
struct ToggleCacheBMPPart {
    std::vector<BMPData*> bmpList;  ///< 格納する BMP データのリスト（何枚でも可）
    int startX;                             ///< 描画開始 X 座標
    int startY;                             ///< 描画開始 Y 座標

    /**
     * @brief コンストラクタ（複数画像対応）
     *
     * 初期化時に、複数の BMP データをリストとして受け取ることができる。
     *
     * @param images 初期化時に格納する BMP データのリスト（可変長）
     * @param x 描画開始 X 座標
     * @param y 描画開始 Y 座標
     */
    ToggleCacheBMPPart(std::vector<BMPData *> images, int x, int y)
        : bmpList(images), startX(x), startY(y) {}
};

// ===============================
//      連結画像のキャッシュ（スクロール用）
// ===============================
extern uint16_t *concatenatedCache;
extern int concatenatedWidth;
extern int concatenatedHeight;

// ===============================
//      関数の宣言（詳細は drawBitmap.cpp に実装）
// ===============================

/**
 * @brief BMPファイルのヘッダー情報を解析する
 *
 * BMP ファイルのヘッダーを読み取り、画像の幅・高さ・ピクセルデータの開始位置を取得する。
 * また、BMP のデータが上下逆（Bottom-Up）かどうかも判定する。
 *
 * @param file BMPファイルの参照（LittleFS から開いたファイルオブジェクト）
 * @param imgWidth 読み取った画像の幅（ピクセル単位）
 * @param imgHeight 読み取った画像の高さ（ピクセル単位）
 * @param pixelDataOffset BMPファイル内でピクセルデータが始まる位置（バイト単位）
 * @param isTopDown 画像データの並びが「上から下」なら true、「下から上」なら false
 * @return 成功時 true / 失敗時 false
 */
bool parseBMPHeader(File &file, int &imgWidth, int &imgHeight, int &pixelDataOffset, bool &isTopDown);

/**
 * @brief BMP画像をメモリにキャッシュして、高速描画を可能にする
 *
 * 画像データを一度読み込み、メモリ上にキャッシュすることで、ファイルアクセス不要で即座に描画可能にする。
 *
 * @param bitmapFilePath BMPファイルのパス（LittleFS上に保存されている）
 * @param bmpData BMPデータのキャッシュ構造体（幅・高さ・ピクセルデータを格納）
 */
void cacheBMPData(const String &bitmapFilePath, BMPData &bmpData);

/**
 * @brief 一定時間ごとに言語（日本語 / 英語）を切り替える
 * @param interval 設定された時間間隔（ミリ秒単位）
 */
void toggleLanguage(unsigned long interval);

/**
 * @brief 指定された複数の BMP 画像を連結し、スクロール表示用のキャッシュを作成する
 *
 * この関数は、複数の BMP 画像を連結し、メモリ上にキャッシュを作成する。
 * これにより、画像スクロール時に高速描画が可能になる。
 *
 * @param imagePaths 連結する画像のパスリスト（複数の BMP ファイルを結合）
 * @param createdBMP 連結画像のキャッシュデータ（BMPData 構造体に格納）
 */
void cacheConcatenatedImages(const std::vector<String> &imagePaths, BMPData *createdBMP);

/**
 * @brief 指定された BMP ファイルを描画する（キャッシュを使わず毎回ファイルから読み込む）
 *
 * BMP ファイルを直接開き、ヘッダーを解析し、ピクセルデータを読み込んで描画する。
 * キャッシュを使わず、都度ファイルを読み込むため、頻繁に描画する場合はパフォーマンスが低下する。
 *
 * @param filename BMPファイルのパス（LittleFS上にある）
 * @param startX 描画開始X座標
 * @param startY 描画開始Y座標
 * @param targetCanvas 描画先のキャンバス（NULL の場合は直接 LED パネルへ描画）
 */
void drawBMP(const String &filename, int startX, int startY, GFXcanvas16 *targetCanvas = nullptr);

/**
 * @brief キャッシュから直接 BMP を描画（ファイルを再読み込みせずに高速表示）
 *
 * 事前にキャッシュされた BMP データを使用し、ピクセルデータを直接 LED パネルに描画する。
 * LittleFS へのファイルアクセスを省略することで、高速に描画できる。
 *
 * @param bmpData キャッシュされた BMP データ（幅・高さ・ピクセルデータを格納）
 * @param startX 描画開始X座標
 * @param startY 描画開始Y座標
 * @param targetCanvas 描画先のキャンバス（NULL の場合は直接 LED パネルへ描画）
 */
void drawBMPFromCache(const BMPData *bmpData, int startX, int startY, GFXcanvas16 *targetCanvas = nullptr);

/**
 * @brief 画像をスクロール表示する関数（非ブロッキング処理）
 *
 * キャッシュされた BMP 画像をスクロールさせながら描画する。
 * 一定時間ごとにスクロールを更新することで、滑らかなアニメーションを実現する。
 *
 * @param conCache スクロール表示する BMP データのキャッシュ（BMPData 構造体）
 * @param start_x 描画開始 X 座標（スクロール領域の左上の位置）
 * @param start_y 描画開始 Y 座標
 * @param area_width スクロールエリアの幅（描画する範囲）
 * @param area_height スクロールエリアの高さ
 * @param scrollInterval スクロールの更新間隔（ミリ秒単位）
 */
void updateScroll(BMPData *conCache, int start_x, int start_y, int area_width, int area_height, int scrollInterval);

/**
 * @brief 表示を一定間隔で切り替える関数（BMP のトグル表示）
 *
 * 指定された BMP 画像リストから、一定時間ごとに表示を切り替える。
 * 画像はあらかじめキャッシュしておくことで、高速に切り替えられる。
 *
 * @param parts トグル対象の BMP 画像リスト（`ToggleBMPPart` 構造体の配列）
 * @param interval 画像の切り替え間隔（ミリ秒単位）
 * @param width 描画領域の幅
 * @param height 描画領域の高さ
 * @param canvas 描画用キャンバス（切り替えをスムーズにするための中間バッファ）
 */
void toggleBMP(std::vector<ToggleBMPPart> &parts, unsigned long interval, int width, int height, GFXcanvas16 &canvas);

/**
 * @brief キャッシュデータを利用して画像を一括ローテーション表示
 *
 * 事前にキャッシュされた BMP データを利用し、指定された間隔で全ての `ToggleCacheBMPPart` を一括で切り替える。
 * - `numImages` に応じて画像をローテーション
 * - `numImages <= 0` の場合、処理を中止
 * - 画像が足りない場合は描画内容を変更せず、その部分は維持
 *
 * @param parts 切り替え対象の BMP データ（複数の `ToggleCacheBMPPart` を管理）
 * @param numImages 使用する BMP 画像の数（リスト内の最大値を超えない範囲で適用）
 * @param interval 画像の切り替え間隔（ミリ秒単位）
 */
void toggleCacheBMP(std::vector<ToggleCacheBMPPart> &parts, int numImages, unsigned long interval);

/**
 * @brief キャンバスから LED パネルにピクセルデータを転送する
 *
 * キャンバス (`GFXcanvas16`) に描画された画像を LED パネル (`matrix`) に反映する。
 *
 * @param canvas 描画用の中間バッファ（キャンバス）
 * @param width LED パネルの幅
 * @param height LED パネルの高さ
 */
void drawPixelfromCanvas(GFXcanvas16 &canvas, int width, int height);

#endif // DRAWBITMAP_H
