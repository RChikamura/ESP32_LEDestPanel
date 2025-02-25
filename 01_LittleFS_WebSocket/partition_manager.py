import tkinter as tk
from tkinter import messagebox, filedialog
import csv

# アプリのメインウィンドウ
root = tk.Tk()
root.title("ESP32 パーティションテーブルメーカー")
root.geometry("600x500")

# パーティションデータを保存するリスト
partition_data = []

# オフセットを自動設定する関数
def calculate_next_offset():
    if partition_data:
        last_partition = partition_data[-1]
        last_offset = int(last_partition[3], 16) if last_partition[3] else 0x10000
        last_size = int(last_partition[4].replace("M", "000000").replace("K", "000"), 16)
        return hex(last_offset + last_size)
    return "0x10000"  # 最初のオフセット

# 必須のパーティションを追加
def add_required_partitions():
    required_partitions = [
        ["nvs", "data", "nvs", "0x9000", "0x5000", ""],
        ["phy_init", "data", "phy", "0xf000", "0x1000", ""]
    ]
    for partition in required_partitions:
        if partition not in partition_data:
            partition_data.append(partition)
    refresh_list()

# factoryパーティションを追加
def add_factory_partition():
    size = entry_factory_size.get()
    if size:
        factory_partition = ["factory", "app", "factory", calculate_next_offset(), size, ""]
        if factory_partition not in partition_data:
            partition_data.append(factory_partition)
        refresh_list()
    else:
        messagebox.showerror("エラー", "factoryパーティションのサイズを入力してください！")

# OTAに必要なパーティションを追加
def add_ota_partitions():
    ota_partitions = [
        ["otadata", "data", "ota", calculate_next_offset(), "0x2000", ""],
        ["ota_0", "app", "ota_0", calculate_next_offset(), "1M", ""],
        ["ota_1", "app", "ota_1", calculate_next_offset(), "1M", ""]
    ]
    for partition in ota_partitions:
        if partition not in partition_data:
            partition_data.append(partition)
    refresh_list()

# SPIFFSパーティションを追加
def add_spiffs_partition():
    size = entry_spiffs_size.get()
    if size:
        spiffs_partition = ["spiffs", "data", "spiffs", calculate_next_offset(), size, ""]
        if spiffs_partition not in partition_data:
            partition_data.append(spiffs_partition)
        refresh_list()
    else:
        messagebox.showerror("エラー", "SPIFFSパーティションのサイズを入力してください！")

# カスタムパーティションを追加
def add_custom_partition():
    name = entry_name.get()
    p_type = entry_type.get()
    subtype = entry_subtype.get()
    offset = entry_offset.get() or calculate_next_offset()  # 自動オフセット設定
    size = entry_size.get()
    flags = entry_flags.get()

    if name and p_type and subtype and size:
        custom_partition = [name, p_type, subtype, offset, size, flags]
        if custom_partition not in partition_data:
            partition_data.append(custom_partition)
            refresh_list()
            clear_entries()
        else:
            messagebox.showerror("エラー", "同じパーティションが既に存在します！")
    else:
        messagebox.showerror("エラー", "すべての必須フィールドを入力してください！")

# 選択したパーティションを削除
def delete_partition():
    selected = partition_list.curselection()
    if selected:
        partition_data.pop(selected[0])
        refresh_list()
    else:
        messagebox.showerror("エラー", "削除するパーティションを選択してください！")

# 入力フィールドをクリア
def clear_entries():
    entry_name.delete(0, tk.END)
    entry_type.delete(0, tk.END)
    entry_subtype.delete(0, tk.END)
    entry_offset.delete(0, tk.END)
    entry_size.delete(0, tk.END)
    entry_flags.delete(0, tk.END)

# パーティションリストを更新
def refresh_list():
    partition_list.delete(0, tk.END)
    for partition in partition_data:
        partition_list.insert(tk.END, f"{partition}")

# パーティションテーブルをCSVに保存
def save_to_csv():
    if not partition_data:
        messagebox.showerror("エラー", "パーティションがありません！")
        return
    file_path = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV files", "*.csv")])
    if file_path:
        with open(file_path, mode="w", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(["# ESP32 Partition Table"])
            writer.writerow(["# Name", "Type", "SubType", "Offset", "Size", "Flags"])
            for partition in partition_data:
                writer.writerow(partition)
        messagebox.showinfo("保存完了", "パーティションテーブルがCSVに保存されました！")

# ウィジェットの配置
frame = tk.Frame(root)
frame.pack(pady=10)

tk.Label(frame, text="パーティション名:").grid(row=0, column=0)
entry_name = tk.Entry(frame, width=15)
entry_name.grid(row=0, column=1)

tk.Label(frame, text="タイプ:").grid(row=1, column=0)
entry_type = tk.Entry(frame, width=15)
entry_type.grid(row=1, column=1)

tk.Label(frame, text="サブタイプ:").grid(row=2, column=0)
entry_subtype = tk.Entry(frame, width=15)
entry_subtype.grid(row=2, column=1)

tk.Label(frame, text="オフセット:").grid(row=3, column=0)
entry_offset = tk.Entry(frame, width=15)
entry_offset.grid(row=3, column=1)

tk.Label(frame, text="サイズ:").grid(row=4, column=0)
entry_size = tk.Entry(frame, width=15)
entry_size.grid(row=4, column=1)

tk.Label(frame, text="フラグ:").grid(row=5, column=0)
entry_flags = tk.Entry(frame, width=15)
entry_flags.grid(row=5, column=1)

tk.Button(frame, text="必須パーティション追加", command=add_required_partitions).grid(row=6, column=0, columnspan=2, pady=5)

tk.Label(frame, text="factoryパーティションサイズ:").grid(row=7, column=0)
entry_factory_size = tk.Entry(frame, width=15)
entry_factory_size.grid(row=7, column=1)
tk.Button(frame, text="factoryパーティション追加", command=add_factory_partition).grid(row=8, column=0, columnspan=2, pady=5)

tk.Button(frame, text="OTAパーティション追加", command=add_ota_partitions).grid(row=9, column=0, columnspan=2, pady=5)

tk.Label(frame, text="SPIFFSパーティションサイズ:").grid(row=10, column=0)
entry_spiffs_size = tk.Entry(frame, width=15)
entry_spiffs_size.grid(row=10, column=1)
tk.Button(frame, text="SPIFFSパーティション追加", command=add_spiffs_partition).grid(row=11, column=0, columnspan=2, pady=5)

tk.Button(frame, text="カスタムパーティション追加", command=add_custom_partition).grid(row=12, column=0, columnspan=2, pady=5)
tk.Button(frame, text="選択したパーティション削除", command=delete_partition).grid(row=13, column=0, columnspan=2, pady=5)

partition_list = tk.Listbox(root, width=80, height=10)
partition_list.pack(pady=10)

tk.Button(root, text="CSVに保存", command=save_to_csv).pack()

# メインループ
root.mainloop()
