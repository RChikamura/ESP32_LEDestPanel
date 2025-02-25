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
#ifndef CSVREADER_H
#define CSVREADER_H

// ===============================
//      必要なライブラリのインクルード
// ===============================
#include <Arduino.h>  // Arduino 環境の基本ライブラリ
#include "LittleFS.h" // ESP32 の LittleFS（小型ファイルシステム）を使用

// ===============================
//      CSVReader クラスの定義
// ===============================
/**
 * @brief CSV ファイルを扱うクラス
 *
 * LittleFS 上の CSV ファイルを読み取り、データを検索する機能を提供する。
 * - 指定したラベル（列名）の列インデックスを取得
 * - 指定した ID に対応する値を取得
 * - LittleFS 上のファイル操作を簡単に行う
 */
class CSVReader {
public:
    /**
     * @brief CSVReader クラスのコンストラクタ
     *
     * CSV ファイルのパスを指定してインスタンスを生成する。
     * 実際にファイルを開くのは `open()` を呼び出した時。
     *
     * @param path 読み込む CSV ファイルのパス（LittleFS に保存されている）
     */
    CSVReader(const char *path);

    /**
     * @brief 指定した行番号と列ラベルからデータを取得
     *
     * ID（数値）に対応するデータを取得する。
     * 内部で `open()` を呼び出してファイルを開閉する。
     *
     * @param rowNumber 検索対象の ID（数値）
     * @param label 取得したいデータの列名
     * @return 取得したデータ（該当なしの場合は空文字列）
     */
    String getPath(int rowNumber, const String &label);

private:
    const char *filePath; // CSV ファイルのパス（LittleFS に保存）

    /**
     * @brief CSV ファイルを開く
     *
     * LittleFS を使用して CSV ファイルを開く。
     * ファイルが開けた場合は `true` を返し、開けなかった場合は `false` を返す。
     *
     * @return 成功時 `true` / 失敗時 `false`
     */
    bool open();

    /**
     * @brief CSV ファイルを閉じる
     *
     * ファイルが開いている場合に適切にクローズする。
     */
    void close();

    /**
     * @brief 指定したラベル（列名）の列インデックスを取得
     *
     * CSV のヘッダー行を解析し、指定した列名が何番目にあるかを取得する。
     *
     * @param label 検索する列名
     * @return 列のインデックス（0 から始まる）/ 見つからない場合は -1
     */
    int getColumnIndex(const String &label);

    /**
     * @brief 指定した ID に対応するデータを取得（内部処理用）
     *
     * CSV を 1 行ずつ解析し、指定した ID に対応する列データを取得する。
     * `open()` を使用せず、既に開かれているファイルオブジェクトを使用する。
     *
     * @param id 検索対象の ID（文字列）
     * @param label 取得したいデータの列名
     * @param file CSV ファイルオブジェクト（すでに開かれている状態）
     * @return 取得したデータ（該当なしの場合は空文字列）
     */
    String getFilePath(const String &id, const String &label, File &file);
};

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
bool containsWord(const String &str, const String &target);

#endif // CSVREADER_H
