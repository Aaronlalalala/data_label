主要使用label_data.cpp 執行該windows桌面應用專案 

執行成果 (使用滑鼠進行sliding box標註，按下鍵盤s/S可進行標註儲存，並會輸出json 標注資料

![螢幕擷取畫面 2025-05-05 162051](https://github.com/user-attachments/assets/9bf6562f-ea74-47d5-b6df-38dc2973c724)

![螢幕擷取畫面 2025-05-05 162101](https://github.com/user-attachments/assets/0846c776-7e67-41c3-b0d4-d0285b8638ba)

標註完成後輸出格式類似如下，會輸出json檔案，後續將會增加圖片類別


    "annotations": [
        
        {
            "x1": 224,
            "x2": 773,
            "y1": 23,
            "y2": 526
        }
    ],
    "image": "shutterstock_94854301"
    
