import argparse
from PIL import Image
import os

def split_grid_image(input_file, output_folder, tile_width, tile_height, spacing, name_list_file=None, name_list_mode="line"):
    """
    分割する格子画像を処理する関数（長方形タイル対応、24ビットBMP形式保存）

    Parameters:
        input_file (str): 入力画像ファイルのパス
        output_folder (str): 分割画像を保存するフォルダのパス
        tile_width (int): タイルの幅（ピクセル単位）
        tile_height (int): タイルの高さ（ピクセル単位）
        spacing (int): タイル間の隙間のピクセル数
        name_list_file (str): 名前リストファイルのパス（オプション）
        name_list_mode (str): 名前リストの分割モード（"line"または"char"）
    """
    # 名前リストの読み込み
    name_list = []
    if name_list_file:
        with open(name_list_file, "r", encoding="utf-8") as f:
            if name_list_mode == "line":
                name_list = [line.strip() for line in f.readlines()]
            elif name_list_mode == "char":
                name_list = [char for line in f.readlines() for char in line.strip()]

    # 入力画像を開く
    img = Image.open(input_file)
    img_width, img_height = img.size

    # 出力フォルダを作成
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    # タイルを分割
    tile_num = 0
    for y in range(0, img_height, tile_height + spacing):
        for x in range(0, img_width, tile_width + spacing):
            # タイルが画像内に収まる場合のみ処理
            if x + tile_width <= img_width and y + tile_height <= img_height:
                tile = img.crop((x, y, x + tile_width, y + tile_height))
                # BMP形式を24ビットで保存
                tile = tile.convert("RGB")  # 必ずRGB（24ビット）形式に変換

                # 名前を決定
                if tile_num < len(name_list):
                    tile_name = name_list[tile_num]
                else:
                    tile_name = f"tile_{tile_num:03d}"

                # タイルを保存
                tile.save(os.path.join(output_folder, f"{tile_name}.bmp"), format="BMP")
                tile_num += 1

    print(f"{tile_num}個のタイルを保存しました！")

if __name__ == "__main__":
    # 引数のパーサを作成
    parser = argparse.ArgumentParser(description="格子状の画像を分割するスクリプト（長方形タイル対応、24ビットBMP形式保存）")
    parser.add_argument("input_file", type=str, help="入力画像のパス（PNG形式）")
    parser.add_argument("output_folder", type=str, help="分割画像の出力先フォルダ")
    parser.add_argument("tile_width", type=int, help="分割するタイルの幅（ピクセル単位）")
    parser.add_argument("tile_height", type=int, help="分割するタイルの高さ（ピクセル単位）")
    parser.add_argument("spacing", type=int, help="タイル間の隙間のピクセル数")
    parser.add_argument("-n", "--name_list_file", type=str, help="名前リストのファイルパス（オプション）", default=None)
    parser.add_argument("-m", "--name_list_mode", type=str, choices=["line", "char"], help="名前リストの分割モード（line: 1行1名前、char: 1文字ずつ）", default="line")

    # 引数を解析
    args = parser.parse_args()

    # 関数を実行
    split_grid_image(
        args.input_file,
        args.output_folder,
        args.tile_width,
        args.tile_height,
        args.spacing,
        args.name_list_file,
        args.name_list_mode
    )
