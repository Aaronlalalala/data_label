// label_data.cpp : 定義應用程式的進入點。
//

#include "framework.h"
#include "label_data.h"
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <filesystem>
#include <shlwapi.h>
#include <codecvt>
#include <locale>
#include <fstream>
#include <ShlObj.h>  
#pragma comment(lib, "Shell32.lib")  
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")

using namespace Gdiplus;

#define MAX_LOADSTRING 100

// 全域變數:
HINSTANCE hInst;                                // 目前執行個體
WCHAR szTitle[MAX_LOADSTRING];                  // 標題列文字
WCHAR szWindowClass[MAX_LOADSTRING];            // 主視窗類別名稱
const std::wstring IMAGE_DIRECTORY = L"..\\image\\Dogs";
std::vector<std::wstring> imageFiles;
int currentImageIndex = 0;
Image* currentImage = nullptr;

struct Annotation {
    int x1, y1, x2, y2;
};

std::vector<Annotation> annotations;
bool isDrawing = false;
POINT startPoint, endPoint;

// 函數宣告
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void                LoadImageFiles();
void                LoadCurrentImage();
void                DrawImage(HDC hdc, HWND hWnd);
std::wstring        GetFileExtension(const std::wstring& filename);
void                SaveAnnotations();

std::wstring GetOutputFileName() {
    LPCWSTR filePath = imageFiles[currentImageIndex].c_str();
    LPWSTR fileNamePtr = PathFindFileNameW(filePath);

    std::wstring fileName(fileNamePtr);
    return IMAGE_DIRECTORY + L"\\" + fileName + L".txt";
}

#include "json.hpp"
using json = nlohmann::json;
#include <filesystem>  // C++17 std::filesystem

bool CreateDirectoryIfNeeded(const std::wstring& directory) {
    try {
        // 檢查目錄是否已存在
        if (std::filesystem::exists(directory)) {
            return true; // 目錄已存在
        }

        // 嘗試創建目錄
        return std::filesystem::create_directories(directory);
    }
    catch (const std::filesystem::filesystem_error& e) {
        // 轉換錯誤訊息
        std::string errorMsg = "無法創建目錄: " + std::string(e.what());
        MessageBoxA(NULL, errorMsg.c_str(), "錯誤", MB_OK | MB_ICONERROR);
        return false;
    }
}

#include <Windows.h>

// UTF-8 → wide string (wstring)
std::wstring utf8_to_wstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    wstr.resize(size_needed - 1);  
    return wstr;
}

// wide string → UTF-8
std::string wstring_to_utf8(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, NULL, NULL);
    str.resize(size_needed - 1);  
    return str;
}

void SaveAnnotations() {
    try {
        // 設定儲存路徑（改用用戶目錄或桌面，避免權限問題）
        wchar_t userProfilePath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, userProfilePath))) {
            std::wstring outputDirectory = std::wstring(userProfilePath) + L"\\LabelOutput\\";

            // 嘗試創建目錄並檢查是否成功
            if (!CreateDirectoryIfNeeded(outputDirectory)) {
                throw std::runtime_error("無法創建輸出目錄");
            }

            // 取得檔案名稱（去除副檔名）
            std::wstring imageNameW = PathFindFileNameW(imageFiles[currentImageIndex].c_str());

            // 移除副檔名
            size_t pos = imageNameW.find_last_of(L".");
            if (pos != std::wstring::npos) {
                imageNameW = imageNameW.substr(0, pos);  // 去掉副檔名
            }

            // 產生 .json 檔案名稱
            std::wstring jsonFileName = outputDirectory + imageNameW + L".json";

            // 檢查檔案是否已存在且無法寫入
            DWORD fileAttributes = GetFileAttributesW(jsonFileName.c_str());
            if (fileAttributes != INVALID_FILE_ATTRIBUTES) {
                if (fileAttributes & FILE_ATTRIBUTE_READONLY) {
                    // 嘗試移除唯讀屬性
                    SetFileAttributesW(jsonFileName.c_str(), fileAttributes & ~FILE_ATTRIBUTE_READONLY);
                }
            }

            // 準備JSON數據
            nlohmann::json jsonOutput;
            jsonOutput["image"] = wstring_to_utf8(imageNameW);
            for (const auto& ann : annotations) {
                jsonOutput["annotations"].push_back({
                    {"x1", ann.x1},
                    {"y1", ann.y1},
                    {"x2", ann.x2},
                    {"y2", ann.y2}
                    });
            }

            // 使用寬字符API直接打開文件，避免UTF-8轉換問題
            HANDLE hFile = CreateFileW(
                jsonFileName.c_str(),
                GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE) {
                DWORD error = GetLastError();
                wchar_t errorMsg[256];
                swprintf_s(errorMsg, L"無法開啟檔案：%s\n錯誤代碼: %d", jsonFileName.c_str(), error);
                throw std::runtime_error(wstring_to_utf8(errorMsg));
            }

            // 將JSON轉換為字符串
            std::string jsonStr = jsonOutput.dump(4);

            // 寫入檔案
            DWORD bytesWritten;
            BOOL writeResult = WriteFile(
                hFile,
                jsonStr.c_str(),
                static_cast<DWORD>(jsonStr.size()),
                &bytesWritten,
                NULL
            );

            // 關閉檔案句柄
            CloseHandle(hFile);

            if (!writeResult || bytesWritten != jsonStr.size()) {
                throw std::runtime_error("寫入檔案時發生錯誤");
            }

            // 顯示成功訊息
            std::wstring successMsg = L"已儲存標記至:\n" + jsonFileName;
            MessageBoxW(NULL, successMsg.c_str(), L"儲存成功", MB_OK | MB_ICONINFORMATION);
        }
        else {
            throw std::runtime_error("無法獲取桌面路徑");
        }
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "錯誤", MB_OK | MB_ICONERROR);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化 GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // 將全域字串初始化
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LABELDATA, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 載入圖片檔案列表
    LoadImageFiles();

    // 執行應用程式初始化:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LABELDATA));

    MSG msg;

    // 主訊息迴圈:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (currentImage) {
        delete currentImage;
    }

    GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}

// ------------------------------------------------------------------------------------------------ //
void LoadImageFiles() {
    imageFiles.clear();

    try {
        for (const auto& entry : std::filesystem::directory_iterator(IMAGE_DIRECTORY)) {
            if (entry.is_regular_file()) {
                std::wstring extension = GetFileExtension(entry.path().wstring());
                // 支援jpg , jpeg, png 圖片檔案
                if (_wcsicmp(extension.c_str(), L".jpg") == 0 ||
                    _wcsicmp(extension.c_str(), L".jpeg") == 0 ||
                    _wcsicmp(extension.c_str(), L".png") == 0) {
                    imageFiles.push_back(entry.path().wstring());
                }
            }
        }
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Error", MB_OK | MB_ICONERROR);
    }

    // 如果有找到圖片，則載入第一張
    if (!imageFiles.empty()) {
        LoadCurrentImage();
    }
}

std::wstring GetFileExtension(const std::wstring& filename) {
    size_t dotPos = filename.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        return filename.substr(dotPos);
    }
    return L"";
}

void LoadCurrentImage() {
    if (currentImage) {
        delete currentImage;
        currentImage = nullptr;
    }
    annotations.clear();
    if (imageFiles.empty()) return;
    if (currentImageIndex >= static_cast<int>(imageFiles.size())) {
        currentImageIndex = 0;
    }
    currentImage = new Image(imageFiles[currentImageIndex].c_str());
}

// 繪製圖片
void DrawImage(HDC hdc, HWND hWnd) {
    if (!currentImage) return;
    Graphics graphics(hdc);
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    UINT originalWidth = currentImage->GetWidth();
    UINT originalHeight = currentImage->GetHeight();
    INT windowWidth = clientRect.right - clientRect.left;
    INT windowHeight = clientRect.bottom - clientRect.top;
    float scale = min((float)windowWidth / originalWidth, (float)windowHeight / originalHeight);
    INT scaledWidth = (INT)(originalWidth * scale);
    INT scaledHeight = (INT)(originalHeight * scale);
    INT posX = (windowWidth - scaledWidth) / 2;
    INT posY = (windowHeight - scaledHeight) / 2;
    graphics.DrawImage(currentImage, posX, posY, scaledWidth, scaledHeight);

    Pen pen(Color(255, 255, 0, 0), 2);
    for (const auto& ann : annotations) {
        graphics.DrawRectangle(&pen, ann.x1, ann.y1, ann.x2 - ann.x1, ann.y2 - ann.y1);
    }
    if (isDrawing) {
        graphics.DrawRectangle(&pen, (INT)startPoint.x, (INT)startPoint.y, (INT)(endPoint.x - startPoint.x), (INT)(endPoint.y - startPoint.y));
    }
}

//
//  函式: MyRegisterClass()
//
//  用途: 註冊視窗類別。
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LABELDATA));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_LABELDATA);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

//
//   函式: InitInstance(HINSTANCE, int)
//
//   用途: 儲存執行個體控制代碼並且建立主視窗
//
//   註解:
//
//        在這個函式中，我們將執行個體控制代碼儲存在全域變數中，
//        並建立及顯示主程式視窗。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

//
//  函式: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  用途: 處理主視窗的訊息。
//
//  WM_COMMAND  - 處理應用程式功能表
//  WM_PAINT    - 繪製主視窗
//  WM_DESTROY  - 張貼結束訊息然後傳回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    } break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawImage(hdc, hWnd);
        EndPaint(hWnd, &ps);
    } break;
    case WM_LBUTTONDOWN:
        isDrawing = true;
        startPoint.x = LOWORD(lParam);
        startPoint.y = HIWORD(lParam);
        endPoint = startPoint;
        break;
    case WM_MOUSEMOVE:
        if (isDrawing) {
            endPoint.x = LOWORD(lParam);
            endPoint.y = HIWORD(lParam);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    case WM_LBUTTONUP:
        if (isDrawing) {
            endPoint.x = LOWORD(lParam);
            endPoint.y = HIWORD(lParam);
            annotations.push_back({ startPoint.x, startPoint.y, endPoint.x, endPoint.y });
            isDrawing = false;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_DELETE:
            if (!annotations.empty()) {
                annotations.pop_back();
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;
        case 'S':
        case 's':
            SaveAnnotations();
            /*MessageBox(hWnd, L"標註已儲存！", L"提示", MB_OK);*/
            break;
        case VK_RIGHT:
        case VK_DOWN:
            currentImageIndex = (currentImageIndex + 1) % (int)imageFiles.size();
            LoadCurrentImage();
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        case VK_LEFT:
        case VK_UP:
            currentImageIndex = (currentImageIndex - 1 + (int)imageFiles.size()) % (int)imageFiles.size();
            LoadCurrentImage();
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


// [關於] 方塊的訊息處理常式。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    if (message == WM_COMMAND) {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
    }
    return FALSE;
}