# Tools for ESP32_LEDestPanel

このフォルダには、ESP32_LEDestPanel の開発や運用を支援するツールが含まれています。

## 1. `convertBMP.py`

### 説明
ESP32のHUB75パネルで使用する **32ビットBMP** を **24ビットBMP** に変換するスクリプトです。

### 使用方法
Python 3.x がインストールされた環境で実行できます。

#### **単一の画像を変換**
```sh
python convertBMP.py -m single -i 入力画像.bmp -o 出力画像.bmp
```

#### **ディレクトリ内の画像を一括変換**
```sh
python convertBMP.py -m directory -i 入力フォルダ -o 出力フォルダ
```

#### **サブディレクトリを含めた一括変換**
```sh
python convertBMP.py -m recursive -i 入力フォルダ -o 出力フォルダ
```


## 2. `imagesplit.py`
### **概要**
画像を格子状に分割し、各タイルを **24ビットBMP形式** で保存するスクリプト。

### **使い方**
```sh
python imagesplit.py <入力画像> <出力フォルダ> <タイル幅> <タイル高さ> <隙間>
```

#### **例:**
```sh
python imagesplit.py input.png output_folder 32 32 2
```
- `input.png`: 入力画像ファイル（PNG）
- `output_folder`: 分割画像の保存先フォルダ
- `32`: タイルの幅（px）
- `32`: タイルの高さ（px）
- `2`: タイル間の隙間（px）

### **名前リストを使う場合**
ファイル名をリストで指定可能。
```sh
python imagesplit.py input.png output_folder 32 32 2 -n names.txt
```

`names.txt` には **ファイル名リスト** を記述。
```txt
train
bus
airplane
```
結果: `train.bmp`, `bus.bmp`, `airplane.bmp` として出力。

オプション:
| オプション | 説明 |
|------------|-----------------|
| `<入力画像>` | 入力する画像（PNG） |
| `<出力フォルダ>` | 分割した画像の保存先 |
| `<タイル幅>` | 1つのタイルの横幅（px） |
| `<タイル高さ>` | 1つのタイルの縦幅（px） |
| `<隙間>` | タイル間の隙間（px） |
| `-n <名前リスト>` | 名前リストファイル |
| `-m <line/char>` | 名前リストの分割モード (`line` or `char`) |


## 必要なライブラリ
このスクリプトを使用するには、以下のPythonライブラリが必要です。

```sh
pip install pillow
```


## 免責事項
- 本ツールはESP32用の行先表示器プロジェクト向けに開発されました。
- 使用によるいかなる問題も開発者は責任を負いません。
