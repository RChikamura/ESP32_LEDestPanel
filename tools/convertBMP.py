import os
import argparse
from PIL import Image

def convert_image(input_path, output_path):
    """
    32ビットBMPを24ビットBMPに変換して保存
    """
    with Image.open(input_path) as img:
        img = img.convert("RGB")  # 24ビットに変換
        os.makedirs(os.path.dirname(output_path), exist_ok=True)  # 必要ならディレクトリを作成
        img.save(output_path, format="BMP")
    print(f"Converted: {input_path} -> {output_path}")

def convert_directory(input_dir, output_dir):
    """
    ディレクトリ内のすべてのBMPファイルを変換
    """
    os.makedirs(output_dir, exist_ok=True)
    for filename in os.listdir(input_dir):
        if filename.endswith(".bmp"):
            input_path = os.path.join(input_dir, filename)
            output_path = os.path.join(output_dir, filename)
            convert_image(input_path, output_path)

def convert_directory_recursive(input_dir, output_dir):
    """
    サブディレクトリも含め、すべてのBMPファイルを変換
    """
    for root, _, files in os.walk(input_dir):
        for file in files:
            if file.endswith(".bmp"):
                input_path = os.path.join(root, file)
                # 出力ディレクトリの相対パスを保持
                relative_path = os.path.relpath(input_path, input_dir)
                output_path = os.path.join(output_dir, relative_path)
                convert_image(input_path, output_path)

def main():
    """
    コマンドライン引数を解析し、指定モードで変換を実行
    """
    parser = argparse.ArgumentParser(description="32ビットBMPを24ビットBMPに変換")
    parser.add_argument(
        "-m", "--mode", choices=["single", "directory", "recursive"], required=True,
        help="変換モード ('single': 単一画像, 'directory': ディレクトリ, 'recursive': サブディレクトリ含む)"
    )
    parser.add_argument(
        "-i", "--input", required=True, help="入力画像またはディレクトリのパス"
    )
    parser.add_argument(
        "-o", "--output", required=True, help="出力画像またはディレクトリのパス"
    )
    args = parser.parse_args()

    if args.mode == "single":
        # 単一画像の変換
        convert_image(args.input, args.output)
    elif args.mode == "directory":
        # ディレクトリ内の画像を変換
        convert_directory(args.input, args.output)
    elif args.mode == "recursive":
        # サブディレクトリも含めたすべての画像を変換
        convert_directory_recursive(args.input, args.output)

if __name__ == "__main__":
    main()
