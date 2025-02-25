# ESP32_LEDestPanel  

このプロジェクトは、**ESP32** を用いた **HUB75 LEDパネル** の行先表示システムです。  
Wi-Fi経由での制御が可能で、カスタマイズ性の高い表示システムを実現します。  

---

## **特徴**  

- **Wi-Fi制御対応**（WebSocket通信で、スマホやPCから簡単に表示切替）  
- **カスタマイズしやすいシステム設計**（ファイル差し替えだけで別路線に）  

---

## **バージョンとフォルダ**  

プロジェクトは **用途に応じた異なる方式** で開発が進行中です。  
詳細は、それぞれのフォルダ内の `README.md` を参照してください。  

| バージョン                 | 説明 | リンク |
|---------------------------|----------------------------------|---------------------------|
| **01_LittleFS_WebSocket** | LittleFS + WebSocket制御版 | [ReadMe](./01_LittleFS_WebSocket/README.md) |
| *02_(名称未定)* | (開発予定) |                                   |
| **aboutACR**                 | 本ソフトウェアのサンプル路線について | [ReadMe](./aboutACR/README.md) |
| **tools**                 | 画像変換・開発支援ツール | [ReadMe](./tools/README.md) |

---

## **プロジェクト情報**  

- **ライセンス**: MIT License  
- **開発者**: No Name(RChikamura)  
- **今後の開発予定**: MicroSD対応、自由スクロールなど  
