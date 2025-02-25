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
#include "CSVReader.h"

// CSVReader クラスのコンストラクタ
CSVReader::CSVReader(const char *path) : filePath(path) {}

/**
 * @brief CSV ファイルを開く
 *
 * 指定された CSV ファイルを LittleFS から開く。
 * ファイルが開けた場合は `true` を返し、開けなかった場合は `false` を返す。
 *
 * @return 成功時 `true` / 失敗時 `false`
 */
bool CSVReader::open() {
    // 1. LittleFS からファイルを開く
    File file = LittleFS.open(filePath, "r");

    // 2. ファイルが開けなかった場合はエラーを出力し `false` を返す
    if (!file) {
        Serial.printf("CSVファイル %s を開けませんでした。\n", filePath);
        return false;
    }

    // 3. 正常に開けた場合は `true` を返す
    return true;
}

/**
 * @brief CSV ファイルを閉じる
 *
 * ファイルが開いている場合に、適切にクローズする。
 */
void CSVReader::close() {
    File file = LittleFS.open(filePath, "r");

    // ファイルが開いていたら閉じる
    if (file) file.close();
}

/**
 * @brief 指定された列のインデックスを取得する
 *
 * CSV のヘッダー行を読み込み、指定された列名が何番目にあるかを取得する。
 *
 * @param label 検索する列名
 * @return 列のインデックス（0 から始まる）/ 見つからない場合は -1
 */
int CSVReader::getColumnIndex(const String &label) {
    // 1. CSV ファイルを開く
    File file = LittleFS.open(filePath, "r");
    if (!file.available()) {
        Serial.println("CSVが空です。");
        return -1;
    }

    // 2. ヘッダー行を読み取る（1行目）
    String headerLine = file.readStringUntil('\n');
    file.close();  // 読み終わったらすぐに閉じる
    headerLine.trim(); // 前後の空白を削除

    // 3. ヘッダーの各列を解析
    int columnIndex = 0;
    while (headerLine.length() > 0) {
        int separatorIndex = headerLine.indexOf(',');
        String currentLabel;

        if (separatorIndex == -1) {
            // 最後の列
            currentLabel = headerLine;
            headerLine = "";
        } else {
            // 通常の列
            currentLabel = headerLine.substring(0, separatorIndex);
            headerLine = headerLine.substring(separatorIndex + 1);
        }

        // 4. 指定されたラベルと一致する列が見つかったら、そのインデックスを返す
        if (currentLabel == label) {
            return columnIndex;
        }

        // 5. 次の列へ
        columnIndex++;
    }

    // 6. ラベルが見つからなかった場合はエラーメッセージを出力し、-1 を返す
    Serial.printf("ファイル %s でラベル %s が見つかりませんでした。\n", filePath, label.c_str());
    return -1;
}

/**
 * @brief 指定した ID に対応するファイルパスを取得する
 *
 * 指定された ID の行を検索し、指定された列（label）の値を取得する。
 *
 * @param id 検索する ID（文字列）
 * @param label 取得するデータの列名
 * @param file 読み込み対象の CSV ファイル
 * @return 見つかった場合は該当データ（ファイルパスなど）を返す / 見つからなかった場合は空文字列
 */
String CSVReader::getFilePath(const String &id, const String &label, File &file) {
    // 1. ID列と指定ラベル列のインデックスを取得
    int idColumnIndex = getColumnIndex("ID");
    int labelColumnIndex = getColumnIndex(label);

    // 2. 指定された列が見つからなかった場合はエラーを出力し、空文字列を返す
    if (idColumnIndex == -1 || labelColumnIndex == -1) {
        Serial.printf("指定された列が見つかりません (IDまたは%s)\n", label.c_str());
        return "";
    }

    // 3. CSV を 1 行ずつ読み込み、ID が一致する行を探す
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        String currentID;
        String currentLabelData;
        int currentColumn = 0;

        // 4. 各列のデータを取得
        while (line.length() > 0) {
            int separatorIndex = line.indexOf(',');
            String part;

            if (separatorIndex == -1) {
                part = line;
                line = "";
            } else {
                part = line.substring(0, separatorIndex);
                line = line.substring(separatorIndex + 1);
            }

            if (currentColumn == idColumnIndex) {
                currentID = part;
            } else if (currentColumn == labelColumnIndex) {
                currentLabelData = part;
            }
            currentColumn++;
        }

        // 5. ID が一致したら、指定された列のデータを返す
        if (currentID == id) {
            return currentLabelData;
        }
    }

    // 6. 該当する ID が見つからなかった場合
    Serial.printf("ID %s が見つかりませんでした。\n", id.c_str());
    return "";
}

/**
 * @brief 指定した ID に対応するファイルパスを取得する（整数 ID 対応版）
 *
 * `getFilePath()` を呼び出して、整数 ID を文字列に変換して検索する。
 *
 * @param IDNumber 検索する ID（整数）
 * @param label 取得するデータの列名
 * @return 見つかった場合は該当データ（ファイルパスなど）を返す / 見つからなかった場合は空文字列
 */
String CSVReader::getPath(int IDNumber, const String &label) {
    // 1. CSV ファイルを開く
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Serial.printf("CSVファイル %s を開けませんでした。\n", filePath);
        return "";
    }

    // 2. 整数 ID を文字列に変換して検索
    String idString = String(IDNumber);
    String path = getFilePath(idString, label, file);

    // 3. ファイルを閉じて結果を返す
    file.close();
    return path;
}

/**
 * @brief 指定した文字列がスペース区切りのデータ内に含まれているか判定
 *
 * 与えられた文字列（スペース区切りで複数の単語を含む可能性あり）から、特定の単語を検索する。
 * 単語単位で比較を行い、一致する単語が見つかった場合は true を返す。
 *
 * @param str 検索対象の文字列（スペース区切りの単語群）
 * @param target 検索する単語
 * @return 検索対象の文字列に指定した単語が含まれていれば true、含まれていなければ false
 */
bool containsWord(const String &str, const String &target) {
    // 1. 先頭からスペースを探しながら単語を取得
    int start = 0;
    int end = str.indexOf(' ');  // 最初のスペース位置

    while (end != -1) {  // スペースが見つかる間ループ
        if (str.substring(start, end) == target) {
            return true;  // 単語が見つかった
        }
        start = end + 1;  // 次の単語の開始位置を更新
        end = str.indexOf(' ', start);  // 次のスペースを探す
    }

    // 2. 最後の単語をチェック（スペースがない単独の単語 or 最後の単語）
    if (str.substring(start) == target) {
        return true;
    }

    // 3. どの単語とも一致しなかった場合は false を返す
    return false;
}
