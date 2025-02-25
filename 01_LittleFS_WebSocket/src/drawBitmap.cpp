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
#include "drawBitmap.h"

// -------------------------------
// グローバル変数定義
// -------------------------------

// スクロールの終了フラグ（スクロール処理が完了したかどうか）
bool flg_scrollEnd = false;

// 時間管理用変数（トグルとスクロールの更新タイミング管理）
//unsigned long previousToggleMillis = 0;  // トグルの更新時間
//unsigned long previousScrollMillis = 0;  // スクロールの更新時間

// 表示状態管理（トグル用フラグ）
bool toggleState = true;        // 初期表示を bmp1 に設定
bool toggleLangState = true;    // true: 日本語, false: 英語（言語切り替え用）

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
bool parseBMPHeader(File &file, int &imgWidth, int &imgHeight, int &pixelDataOffset, bool &isTopDown) {
    uint8_t header[54];  // BMP ヘッダーは 54 バイト
    if (file.read(header, 54) != 54) {  // ヘッダー部分を 54 バイト読み取る
        Serial.println("BMPヘッダーの読み込みに失敗しました。");
        return false;
    }

    // BMP ヘッダー情報を解析
    imgWidth = *(int *)&header[18];            // 画像の横幅（ピクセル）
    imgHeight = abs(*(int *)&header[22]);      // 画像の縦幅（ピクセル、絶対値を取る）
    pixelDataOffset = *(int *)&header[10];     // ピクセルデータの開始位置（バイト）
    isTopDown = (*(int *)&header[22] < 0);     // 画像が「上から描画される」なら true（通常は false）

    return true;  // ヘッダー解析成功
}

/**
 * @brief BMP画像をメモリにキャッシュして、高速描画を可能にする
 *
 * 画像データを一度読み込み、メモリ上にキャッシュすることで、ファイルアクセス不要で即座に描画可能にする。
 *
 * @param bitmapFilePath BMPファイルのパス（LittleFS上に保存されている）
 * @param bmpData BMPデータのキャッシュ構造体（幅・高さ・ピクセルデータを格納）
 */
void cacheBMPData(const String &bitmapFilePath, BMPData &bmpData) {
    // 1. 既存のキャッシュがある場合は解放（メモリリーク防止）
    if (bmpData.cache) {
        free(bmpData.cache);
        bmpData.cache = nullptr;
    }

    // 2. BMPファイルを開く（LittleFS から読み込む）
    File file = LittleFS.open(bitmapFilePath, "r");
    if (!file) {
        Serial.printf("BMPファイル %s を開けませんでした。\n", bitmapFilePath.c_str());
        return;
    }

    // 3. BMP ヘッダー情報を取得
    int imgWidth, imgHeight, pixelDataOffset;
    bool isTopDown;
    if (!parseBMPHeader(file, imgWidth, imgHeight, pixelDataOffset, isTopDown)) {
        Serial.println("BMPヘッダー解析失敗！");
        file.close();
        return;
    }

    // 4. ピクセルデータを格納するメモリを確保（RGB565 形式で保存）
    bmpData.cache = (uint16_t *)malloc(imgWidth * imgHeight * sizeof(uint16_t));
    if (!bmpData.cache) {
        Serial.println("メモリ確保に失敗しました。");
        file.close();
        return;
    }

    // 5. 画像の幅と高さを記録
    bmpData.width = imgWidth;
    bmpData.height = imgHeight;

    // 6. ピクセルデータの開始位置へ移動
    file.seek(pixelDataOffset, SeekSet);

    // 7. 1 行あたりのデータサイズ（パディングを考慮）
    int rowSize = (imgWidth * 3 + 3) & ~3; // 1 行のバイト数（24bit RGB なので 3 バイト × 幅）

    // 8. ピクセルデータのバッファ
    uint8_t rowBuffer[rowSize];

    // 9. BMP画像のピクセルデータを 1 行ずつ読み込む
    for (int y = 0; y < imgHeight; y++) {
        int rowIndex = isTopDown ? y : (imgHeight - 1 - y); // BMPが上下逆なら修正
        file.read(rowBuffer, rowSize);

        for (int x = 0; x < imgWidth; x++) {
            uint8_t b = rowBuffer[x * 3];   // 青（Blue）
            uint8_t g = rowBuffer[x * 3 + 1]; // 緑（Green）
            uint8_t r = rowBuffer[x * 3 + 2]; // 赤（Red）

            // BMPは RGB888 (8bit x 3) 形式なので、RGB565 (16bit) に変換して保存
            bmpData.cache[rowIndex * imgWidth + x] = matrix->color565(r, g, b);
        }
    }

    // 10. ファイルを閉じる（メモリ解放）
    file.close();
    Serial.printf("BMPファイル %s をキャッシュしました。\n", bitmapFilePath.c_str());
}

/**
 * @brief 一定時間ごとに言語（日本語 / 英語）を切り替える
 * @param interval 設定された時間間隔（ミリ秒単位）
 */
void toggleLanguage(unsigned long interval) {
    unsigned long currentMillis = millis();

    // 設定時間が経過したらトグルを切り替え
    if (currentMillis - previousToggleMillis >= interval) {
        previousToggleMillis = currentMillis;
        toggleLangState = !toggleLangState;
    }
}

/**
 * @brief 指定された複数の BMP 画像を連結し、スクロール表示用のキャッシュを作成する
 *
 * この関数は、複数の BMP 画像を連結し、メモリ上にキャッシュを作成する。
 * これにより、画像スクロール時に高速描画が可能になる。
 *
 * @param imagePaths 連結する画像のパスリスト（複数の BMP ファイルを結合）
 * @param createdBMP 連結画像のキャッシュデータ（BMPData 構造体に格納）
 */
void cacheConcatenatedImages(const std::vector<String> &imagePaths, BMPData *createdBMP) {
    // 1. 既存のキャッシュを解放（メモリリーク防止）
    if (createdBMP->cache) {
        free(createdBMP->cache);
        createdBMP->cache = nullptr;
    }

    // 2. 画像の総幅と高さを初期化
    createdBMP->width = 0;
    createdBMP->height = 0;

    // 各画像のピクセルデータを一時保存するリスト
    std::vector<uint16_t *> individualCaches; // 各画像のキャッシュ
    std::vector<int> imageWidths; // 各画像の幅

    // 3. 画像リスト内の各 BMP ファイルを順番に処理
    for (const auto &path : imagePaths) {
        // 3.1 BMPファイルを開く（LittleFS から）
        File file = LittleFS.open(path, "r");
        if (!file) {
            Serial.printf("BMPファイル %s を開けませんでした。\n", path.c_str());
            continue; // ファイルが開けなかったらスキップ
        }

        // 3.2 BMPヘッダー情報を取得
        int imgWidth, imgHeight, pixelDataOffset;
        bool isTopDown;
        if (!parseBMPHeader(file, imgWidth, imgHeight, pixelDataOffset, isTopDown)) {
            file.close();
            continue; // ヘッダーが読めなかったらスキップ
        }

        // 3.3 最初の画像の高さを記録し、以降の画像と一致しているか確認
        if (createdBMP->height == 0) {
            createdBMP->height = imgHeight;
        } else if (createdBMP->height != imgHeight) {
            Serial.println("画像の高さが一致しません。処理を中断します。");
            for (auto &cache : individualCaches) {
                free(cache); // メモリ解放
            }
            return;
        }

        // 3.4 BMPピクセルデータを格納するメモリ領域を確保
        uint16_t *tempCache = (uint16_t *)malloc(imgWidth * imgHeight * sizeof(uint16_t));
        if (!tempCache) {
            Serial.println("メモリ確保に失敗しました。");
            for (auto &cache : individualCaches) {
                free(cache);
            }
            return;
        }

        // 3.5 BMPピクセルデータの開始位置へ移動
        file.seek(pixelDataOffset, SeekSet);
        int rowSize = (imgWidth * 3 + 3) & ~3; // 各行のバイト数（パディング含む）
        uint8_t rowBuffer[rowSize];

        // 3.6 BMP画像のピクセルデータを 1 行ずつ読み込み
        for (int y = 0; y < imgHeight; y++) {
            int rowIndex = isTopDown ? y : (imgHeight - 1 - y); // BMPが上下逆なら修正
            file.read(rowBuffer, rowSize);

            for (int x = 0; x < imgWidth; x++) {
                uint8_t b = rowBuffer[x * 3];   // 青の値
                uint8_t g = rowBuffer[x * 3 + 1]; // 緑の値
                uint8_t r = rowBuffer[x * 3 + 2]; // 赤の値
                tempCache[rowIndex * imgWidth + x] = matrix->color565(r, g, b); // RGB565へ変換
            }
        }

        // 3.7 ファイルを閉じて、一時キャッシュに追加
        file.close();
        individualCaches.push_back(tempCache);
        imageWidths.push_back(imgWidth);
        createdBMP->width += imgWidth; // 連結後の総幅を更新
    }

    // 4. 連結画像のメモリを確保
    createdBMP->cache = (uint16_t *)malloc(createdBMP->width * createdBMP->height * sizeof(uint16_t));
    if (!createdBMP->cache) {
        Serial.println("連結キャッシュのメモリ確保に失敗しました。");
        for (auto &cache : individualCaches) {
            free(cache);
        }
        return;
    }

    // 5. 画像を横に連結（個々の画像データを1つのバッファにまとめる）
    int offsetX = 0;
    for (size_t i = 0; i < individualCaches.size(); i++) {
        uint16_t *tempCache = individualCaches[i];
        int imgWidth = imageWidths[i];

        for (int y = 0; y < createdBMP->height; y++) {
            // 各行を連結バッファにコピー（画像の一部を横に配置）
            memcpy(&createdBMP->cache[y * createdBMP->width + offsetX],
                   &tempCache[y * imgWidth],
                   imgWidth * sizeof(uint16_t));
        }

        offsetX += imgWidth; // 次の画像の開始位置を調整
        free(tempCache); // 一時キャッシュを解放
    }

    Serial.println("画像の連結キャッシュが完成しました！");
}

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
void drawBMP(const String &filename, int startX, int startY, GFXcanvas16 *targetCanvas) {
    // 1. BMPファイルを開く
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.printf("BMPファイル %s を開けませんでした。\n", filename.c_str());
        return;
    }

    // 2. BMP ヘッダー情報を取得
    int imgWidth, imgHeight, pixelDataOffset;
    bool isTopDown;
    if (!parseBMPHeader(file, imgWidth, imgHeight, pixelDataOffset, isTopDown)) {
        file.close();
        return;
    }

    // 3. ピクセルデータの開始位置に移動
    file.seek(pixelDataOffset, SeekSet);

    // 4. 1 行のサイズ（パディング含む）
    int rowSize = (imgWidth * 3 + 3) & ~3; // BMP の 1 行あたりのデータサイズ（24bit カラー + パディング）
    uint8_t rowBuffer[rowSize]; // 行データを一時的に格納するバッファ

    // 5. 画像データを 1 行ずつ読み込んで描画
    for (int y = 0; y < imgHeight; y++) {
        int rowIndex = isTopDown ? y : (imgHeight - 1 - y); // BMP の並び順に応じて Y 座標を調整
        file.read(rowBuffer, rowSize); // 1 行分のデータを読み込む

        // 5.1 1 ピクセルずつ処理して描画
        for (int x = 0; x < imgWidth; x++) {
            uint8_t b = rowBuffer[x * 3];   // 青 (Blue)
            uint8_t g = rowBuffer[x * 3 + 1]; // 緑 (Green)
            uint8_t r = rowBuffer[x * 3 + 2]; // 赤 (Red)

            uint16_t color = matrix->color565(r, g, b); // RGB888 を RGB565 に変換

            if (targetCanvas) {
                // 5.2 キャンバスが指定されている場合はキャンバスに描画
                targetCanvas->drawPixel(startX + x, startY + rowIndex, color);
            } else {
                // 5.3 キャンバスがない場合は LED パネルに直接描画
                int drawX = startX + x;
                int drawY = startY + rowIndex;
                if (drawX >= 0 && drawX < panelWidth && drawY >= 0 && drawY < panelHeight) {
                    matrix->drawPixel(drawX, drawY, color);
                }
            }
        }
    }

    // 6. ファイルを閉じる
    file.close();
}

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
void drawBMPFromCache(const BMPData *bmpData, int startX, int startY, GFXcanvas16 *targetCanvas) {
    // 1. キャッシュデータが存在しない場合は処理を中断
    if (!bmpData || !bmpData->cache) {
        Serial.println("キャッシュデータが存在しません！");
        return;
    }

    // 2. 描画開始前にデバッグログを出力
    #ifdef DEBUG
        Serial.printf("Drawing BMP at (%d, %d) with size (%d x %d)\n",
                      startX, startY, bmpData->width, bmpData->height);
    #endif

    // 3. キャッシュからピクセルデータを読み取り、パネルに描画
    for (int y = 0; y < bmpData->height; y++) {
        for (int x = 0; x < bmpData->width; x++) {
            uint16_t color = bmpData->cache[y * bmpData->width + x]; // キャッシュからピクセルカラーを取得
            if (targetCanvas) {
                // 3.1 キャンバスが指定されている場合はキャンバスに描画
                targetCanvas->drawPixel(startX + x, startY + y, color);
            } else {
                // 3.2 キャンバスがない場合は LED パネルに直接描画
                int drawX = startX + x;
                int drawY = startY + y;
                if (drawX >= 0 && drawX < panelWidth && drawY >= 0 && drawY < panelHeight) {
                    matrix->drawPixel(drawX, drawY, color);
                }
            }
        }
    }

}

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
void updateScroll(BMPData *conCache, int start_x, int start_y, int area_width, int area_height, int scrollInterval) {
    static unsigned long previousScrollMillis = 0;  // スクロールの更新時間
    // 1. キャッシュデータが存在するか確認
    if (!conCache->cache) {
        Serial.println("連結キャッシュが存在しません！");
        return;
    }

    // 2. スクロール更新処理（一定時間ごとに実行）
    unsigned long currentMillis = millis();
    if (currentMillis - previousScrollMillis >= scrollInterval) {
        previousScrollMillis = currentMillis;

        // 3. スクロール範囲内のピクセルを更新
        for (int y = 0; y < area_height; y++) {
            int cacheY = (y + start_y) % conCache->height; // 縦方向のスクロール位置を計算

            for (int x = 0; x < area_width; x++) {
                int cacheX = (x + conCache->offsetX) % conCache->width; // 横方向のスクロール位置を計算
                uint16_t color = conCache->cache[cacheY * conCache->width + cacheX]; // キャッシュからピクセルデータ取得

                int drawX = start_x + x;
                int drawY = start_y + y;

                // 4. 描画範囲がディスプレイ内であることを確認して描画
                if (drawX >= 0 && drawX < panelWidth && drawY >= 0 && drawY < panelHeight) {
                    matrix->drawPixel(drawX, drawY, color);
                }
            }
        }

        // 5. スクロール位置を更新（1 ピクセルずつ右に移動）
        conCache->offsetX++;

        // 6. スクロールが 1 周したらリセット
        if (conCache->offsetX >= conCache->width) {
            conCache->offsetX = 0;
            flg_scrollEnd = true; // スクロール終了フラグをセット
        } else {
            flg_scrollEnd = false;
        }
    }
}

/**
 * @brief 表示を一定間隔で切り替える関数（BMP のトグル表示）
 *
 * 指定された BMP 画像リストから、一定時間ごとに表示を切り替える。
 *
 * @param parts トグル対象の BMP 画像リスト（`ToggleBMPPart` 構造体の配列）
 * @param interval 画像の切り替え間隔（ミリ秒単位）
 * @param width 描画領域の幅
 * @param height 描画領域の高さ
 * @param canvas 描画用キャンバス（切り替えをスムーズにするための中間バッファ）
 */
void toggleBMP(std::vector<ToggleBMPPart> &parts, unsigned long interval, int width, int height, GFXcanvas16 &canvas) {
    // 1. 現在の時間を取得
    unsigned long currentMillis = millis();

    // 2. 指定間隔が経過しているかチェック
    if (currentMillis - previousToggleMillis >= interval) {
        previousToggleMillis = currentMillis; // 最後の切り替え時間を更新
        toggleState = !toggleState; // トグル状態を変更

        // 3. 各 BMP パーツの画像を描画
        for (const auto &part : parts) {
            String bmpPath = toggleState ? part.bmp1 : part.bmp2; // トグル状態に応じた画像を選択

            // 3.1 画像をキャンバスに描画
            drawBMP(bmpPath, part.startX, part.startY, &canvas);
        }

        // 4. キャンバスに描画した画像を LED パネルへ転送
        drawPixelfromCanvas(canvas, width, height);
    }
}

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
void toggleCacheBMP(std::vector<ToggleCacheBMPPart> &parts, int numImages, unsigned long interval) {
    static unsigned long previousToggleMillis = 0;
    static int currentImageIndex = 0; // 全体で統一する画像インデックス

    // 1. `numImages` が無効、またはpartsが空なら処理を中止
    if (numImages <= 0 || parts.empty()) {
        #ifdef DEBUG
            Serial.printf("toggleCacheBMP skipped: Invalid numImages (%d) or empty parts.\n", numImages);
        #endif
        return;
    }

    // 2. 現在の時間を取得
    unsigned long currentMillis = millis();

    #ifdef DEBUG
        Serial.printf("toggleCacheBMP called! parts size: %d, numImages: %d, currentImageIndex: %d\n",
                      parts.size(), numImages, currentImageIndex);
    #endif

    // 3. 指定間隔が経過したかチェック
    if (currentMillis - previousToggleMillis >= interval) {
        previousToggleMillis = currentMillis; // 最後の切り替え時間を更新

        // 4. 各 BMP パーツの描画
        for (size_t i = 0; i < parts.size(); i++) {
            ToggleCacheBMPPart &part = parts[i];

            // 画像リストが空ならスキップ
            if (part.bmpList.empty()) {
                #ifdef DEBUG
                    Serial.printf("Part %d has no images!\n", (int)i);
                #endif
                continue;
            }

            // 画像リスト内で `numImages` の範囲内に収める
            if (numImages > part.bmpList.size()) {
                #ifdef DEBUG
                    Serial.printf("Part %d: Not enough images (Requested: %d, Available: %d). Keeping previous display.\n",
                                  (int)i, numImages, (int)part.bmpList.size());
                #endif
                continue; // 画像が足りない場合はこの部分の描画を変更しない
            }

            // 画像を選択
            const BMPData *image = part.bmpList[currentImageIndex % numImages];

            // 5. デバッグメッセージ
            #ifdef DEBUG
                if (image == nullptr) {
                    Serial.printf("Part %d: image is null! Keeping previous display.\n", (int)i);
                } else {
                    Serial.printf("Part %d: Displaying image %d at (%d, %d)\n",
                                  (int)i, currentImageIndex % numImages, part.startX, part.startY);
                }
            #endif

            // 6. 画像を描画
            if (image != nullptr) {
                drawBMPFromCache(image, part.startX, part.startY);
            }
        }

        // 7. 次の画像インデックスに更新（全体で統一）
        currentImageIndex = (currentImageIndex + 1) % numImages;
    }
}

/**
 * @brief キャンバスから LED パネルにピクセルデータを転送する
 *
 * キャンバス (`GFXcanvas16`) に描画された画像を LED パネル (`matrix`) に反映する。
 *
 * @param canvas 描画用の中間バッファ（キャンバス）
 * @param width LED パネルの幅
 * @param height LED パネルの高さ
 */
void drawPixelfromCanvas(GFXcanvas16 &canvas, int width, int height) {
    // 1. キャンバス内のすべてのピクセルを LED パネルへ転送
    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            uint16_t color = canvas.getPixel(x, y); // 1 ピクセルの色を取得
            matrix->drawPixel(x, y, color);        // LED パネルに描画
        }
    }
}
