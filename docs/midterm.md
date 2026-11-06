# 中期

實現一個使用 C 語言編寫的客戶端-服務器應用程序，並滿足以下要求：

    服務器必須使用 `fork()` 系統調用來創建用於處理每個客戶端連接的子進程（不允許使用線程）。

    服務器必須能夠同時處理至少 10 個客戶端，並且需要提供運行時的驗證證明。

    提供數據包捕獲和分析功能，使用 `tcpdump` 捕獲本地網絡流量，或使用 `Wireshark` 分析不同計算機之間的通信。

    該程序必須支持兩種調試日誌控制方式：一種是在編譯時，另一種是在運行時。

    客戶端和服務器共享的通用函數應被封裝成一個動態共享庫（.so），並由雙方都鏈接。

    客戶要求從服務器獲取系統信息，這是您的一項功能（您還可以提供其他功能...）。

    您需要設計和實施機制，以應對各種異常或特殊情況。

提供至少三個示例，展示在應用這些魯棒性機制後，程序行為的差異。

    該項目必須使用 Makefile 或 CMake 構建。

提交報告應包含：

    系統設計說明

    關鍵代碼的描述

    執行截圖

    完整源代碼

    此外：包含 2 分鐘的視頻演示

在期中考試後的課程中，學生需要在自己的電腦上獨立運行和演示他們的程序。

---

Implement a client–server application in C with the following requirements:

    The server must use fork() to create a child process for handling each client connection (threads are not allowed).

    The server must be able to handle at least 10 clients concurrently, and a runtime proof must be provided.

    Provide a packet capture and analysis, using tcpdump for loopback traffic or Wireshark for communication between different computers.

    The program must support two levels of debug log control: one at compile time and one at runtime.

    Utility functions shared by both the client and server must be encapsulated into a dynamic shared library (.so) and linked by both sides.

    The client requests system information from the server as a one of your features design ( you can have many other features...)

    You must design and implement mechanisms for robustness to handle various abnormal or exceptional conditions.

Provide at least three execution examples demonstrating the difference in program behavior with and without these robustness mechanisms implemented.

    The project must be built using Makefile or CMake.

Submission report should include:

    System design explanation

    Description of key source code

    Execution screenshots

    Complete source code

    Plus:  2 minutes video illustration

During the class session after the midterm week, students must run and demonstrate their program on their own computer.
