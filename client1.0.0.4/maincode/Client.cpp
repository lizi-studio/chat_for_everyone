#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winuser.h>
#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "User32.lib")

#define PORT "23456"
#define SERVER_ADDRESS "47.95.179.78"

SOCKET sock = INVALID_SOCKET;
WSADATA wsaData;
HWND hwndEditMessage;
HWND hwndListBoxMessages;
HWND hwndEditUsername;
std::string username;
bool running = true;

void SendMessageToServer(const std::string& message) {
    std::string msgToSend = message + "\n";
    send(sock, msgToSend.c_str(), msgToSend.size(), 0);
}

void UpdateListBox(const std::string& message) {
    SendMessageA(hwndListBoxMessages, LB_ADDSTRING, 0, (LPARAM)message.c_str());
}

void ReceiveMessagesFromServer() {
    char buffer[1024];
    while (running) {
        int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string receivedMessage = buffer;
            PostMessage(hwndListBoxMessages, WM_USER + 1, 0, (LPARAM)new std::string(receivedMessage));
        }
        else if (bytesReceived == 0) {
            // 连接已关闭
            PostMessage(hwndListBoxMessages, WM_USER + 1, 0, (LPARAM)new std::string("Server disconnected."));
            break;
        }
        else {
            // 发生错误
            int errorCode = WSAGetLastError();
            std::string errorMessage = "Error receiving message. Code: " + std::to_string(errorCode);
            PostMessage(hwndListBoxMessages, WM_USER + 1, 0, (LPARAM)new std::string(errorMessage));
            break;
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static bool connected = false;
    static std::string inputMessage;

    switch (uMsg) {
    case WM_CREATE: {
        // 初始化 Winsock
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            MessageBoxA(hwnd, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
            PostQuitMessage(0);
            return 0;
        }

        // 创建套接字并连接到服务器
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(atoi(PORT));
        inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr);

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            MessageBoxA(hwnd, "socket failed", "Error", MB_OK | MB_ICONERROR);
            WSACleanup();
            PostQuitMessage(0);
            return 0;
        }

        if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            MessageBoxA(hwnd, "connect failed, 可能是连接错误或者是服务器异常，你能做的只有：检查网络连接（确保网络畅通）或重启软件，不然就只能等到服务器恢复了", "Error", MB_OK | MB_ICONERROR);
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        else {
            connected = true;
        }

        // 创建接收消息的线程
        if (connected) {
            std::thread receiveThread(ReceiveMessagesFromServer);
            receiveThread.detach();
        }

        // 创建用户名输入框
        hwndEditUsername = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 10, 200, 25, hwnd, (HMENU)1, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        // 创建消息输入框
        hwndEditMessage = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 50, 200, 25, hwnd, (HMENU)2, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        // 创建发送按钮
        CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 50, 80, 25, hwnd, (HMENU)IDOK, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        // 创建退出按钮
        CreateWindowA("BUTTON", "Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 10, 80, 25, hwnd, (HMENU)IDCANCEL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        // 创建消息列表框
        hwndListBoxMessages = CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_HASSTRINGS | LBS_NOTIFY,
            10, 100, 300, 150, hwnd, (HMENU)3, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        break;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDOK && connected) { // 发送按钮的ID是IDOK
            char buffer[1024];
            GetWindowTextA(hwndEditMessage, buffer, sizeof(buffer));
            SendMessageToServer(buffer);
            SetWindowTextA(hwndEditMessage, ""); // 清空输入区域
        }
        else if (LOWORD(wParam) == IDCANCEL) { // 退出按钮的ID是IDCANCEL
            int response = MessageBoxA(hwnd, "Are you sure you want to exit?", "Exit Confirmation", MB_YESNO | MB_ICONQUESTION);
            if (response == IDYES) {
                PostQuitMessage(0);
            }
        }
        break;
    }

    case WM_CLOSE:
    case WM_DESTROY: {
        running = false; // 停止接收线程
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        WSACleanup();
        PostQuitMessage(0); // 确保进程退出
        break;
    }

    case WM_USER + 1: { // 处理从接收线程发送的消息
        std::string* receivedMessage = reinterpret_cast<std::string*>(lParam);
        if (receivedMessage) {
            UpdateListBox(*receivedMessage);
            delete receivedMessage; // 释放动态分配的内存
        }
        break;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "chat_window";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "chat",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // 运行消息循环
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}