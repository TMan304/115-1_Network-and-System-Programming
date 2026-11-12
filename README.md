# 遠端系統監控客戶端-伺服器系統 (NSP)

> **課程名稱：** 115-1 網路與系統程式設計 (Network and System Programming) — 期中實作專案  
> **核心架構：** 基於 UNIX `fork()` 的多行程隔離架構（嚴格禁止使用 POSIX 執行緒）  
> **共享函式庫：** `libnsp.so` 動態函式庫（內嵌 RPATH `$ORIGIN`）  
> **通訊協定：** 自訂 8-byte 二進制幀 TCP 協定（魔術標頭 `0x4E53` / `"NS"`）  
> **建置工具：** CMake (C11 標準, `-Wall -Wextra -Werror -pedantic`)

---

## 專案在做什麼

本專案是一個以 C 語言撰寫的高效能、高魯棒性遠端系統監控客戶端-伺服器系統。伺服器透過 Linux 核心介面即時擷取主機硬體與系統負載資訊（主機名稱、OS 與核心版本、開機時間、1/5/15 分鐘平均負載、CPU 型號與核心數、可用記憶體），並透過自訂的二進制串流協定為多個客戶端提供即時查詢服務。

### 核心特性

1. **純行程並行架構（零執行緒）：** 伺服器主監聽程序在接收連線後，使用 `fork()` 產生專屬的子工作進程（Worker）處理客戶端，進程間記憶體完全隔離。全專案程式碼中完全不使用 `pthread`。
2. **共用動態函式庫 (`libnsp.so`)：** 將協定封包編解碼、可靠串流 I/O（處理 TCP 零碎讀寫與 `EINTR` 中斷重試）與除錯日誌引擎封裝為 `.so` 動態函式庫。執行檔透過內嵌 RPATH 自動搜尋同目錄之函式庫，毋須手動設定 `LD_LIBRARY_PATH`。
3. **支援 10+ 同時連線並行證明：** 能穩定並行處理 10 個以上客戶端，並透過 `ps --forest` 與 `ss -tnp` 提供行程樹與 Socket 狀態之即時執行期證明。
4. **二級除錯日誌控制：**
   - **編譯期控制：** 透過 CMake 選項（`NSP_DEBUG=ON/OFF`）在發布版本中直接將 `LOG_DEBUG` 展開為空操作，零效能與字串空間開銷。
   - **執行期控制：** 支援以命令列參數（`-l debug|info|warn|error`）或環境變數（`LOG_LEVEL`）動態過濾記錄等級，毋須重新編譯。
5. **四項可開關魯棒性機制（A/B 對照演示）：**
   - **殭屍進程回收 (Zombie Reaping)：** `SIGCHLD` 信號處理器以非阻塞 `waitpid()` 迴圈即時回收已結束子進程，避免進程表耗盡。
   - **異常與超長封包防護 (Frame Validation)：** 驗證魔術字首與長度上限（4 KiB），阻絕惡意攻擊與記憶體耗盡。
   - **客戶端閒置超時 (Idle Timeout)：** Socket 套用 `SO_RCVTIMEO`，逾時自動釋放閒置卡死的連線。
   - **最大連線上限防護 (Connection Limit)：** 限制並行連線數（`-m <max>`），超額連線直接以 `RESP_BUSY` 拒絕並關閉，防止 Fork 炸彈與 DoS 攻擊。

---

## 專案要怎麼用

### 1. 快速編譯與安裝

本專案依賴 Linux 系統環境、GCC/Clang、CMake 3.16+ 與標準 UNIX 工具。

```bash
# 1. 複製專案後進行除錯版本建置（預設開啟 DEBUG 日誌）
cmake -B build -DNSP_DEBUG=ON
cmake --build build

# 2. 亦可建置最佳化發布版本（完全移除 DEBUG 日誌字串）
cmake -B build-release -DNSP_DEBUG=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

#### 驗證動態函式庫鏈結 (`ldd`)

```bash
ldd build/server build/client build/evil_client
# 輸出確認鏈結至同目錄下的 ./libnsp.so
```

---

### 2. 啟動伺服器 (`server`)

```bash
# 預設於 port 9000 啟動，連線上限 16，閒置逾時 30 秒
./build/server

# 自訂參數啟動範例（指定連接埠 9000、最大連線數 10、逾時 15 秒、開啟詳細記錄）
./build/server -p 9000 -m 10 -t 15 -l info

# 常用參數列表：
#   -p, --port <port>       監聽連接埠（預設：9000）
#   -m, --max-clients <num> 最大並行客戶端連線數（預設：16）
#   -t, --timeout <sec>     客戶端通訊逾時秒數（預設：30）
#   -l, --log-level <level> 記錄層級 (debug, info, warn, error, off)
#   -v, --verbose           開啟除錯日誌 (相當於 -l debug)
#   --no-reap               關閉 SIGCHLD 殭屍進程回收 (A/B 演示)
#   --no-validate           關閉封包標頭與長度安全檢查 (A/B 演示)
#   --no-timeout            關閉 Socket 逾時中斷機制 (A/B 演示)
#   --no-limit              關閉最大連線上限檢查 (A/B 演示)
#   --no-robust             同時關閉上述四種魯棒性機制
```

---

### 3. 使用客戶端 (`client`)

#### A. 互動式命令列 (REPL 模式)

```bash
./build/client -p 9000
```

連線後可輸入以下指令：

- `sysinfo` — 查詢伺服器主機資訊（OS、核心版本、運行時間、平均負載、CPU、記憶體）。
- `echo <文字>` — 傳送文字並檢驗伺服器回傳內容。
- `time` — 查詢伺服器目前的時間戳記與格式化時間。
- `procs` — 檢視負責服務此連線的 Worker 子進程 PID、父進程 PID 與系統進程總數。
- `sleep <1-60>` — 讓伺服器 Worker 暫停指定秒數，用於檢驗多連線並行。
- `help` — 顯示支援指令清單。
- `quit` — 正常結束連線。

#### B. 單次指令非互動模式

適合自動化腳本呼叫，執行單一命令後立即結束並回傳結果：

```bash
./build/client -p 9000 -c "sysinfo"
./build/client -p 9000 -c "echo Hello NSP"
./build/client -p 9000 -c "procs"
```

---

### 4. 執行自動化演示腳本 (課堂演示與錄影必備)

專案附帶完整的自動化測試與演示腳本，具備彩色終端橫幅與清晰輸出：

```bash
# [推薦] 執行全功能一鍵演示套件（互動分步進行，非常適合 2 分鐘錄影展示）
./scripts/run_all_demos.sh

# 1. 10+ 客戶端並行執行證明（啟動 12 個客戶端，輸出 ps --forest 行程樹與 ss 連線列表）
./scripts/demo_concurrency.sh

# 2. 封包擷取與二進制協定解析（使用 tcpdump 解碼封包，高亮標示 0x4E53 魔術字首）
./scripts/demo_packet_capture.sh

# 3. 四種魯棒性機制 A/B 對照演示（逐一展示「無機制」vs「有機制」的行為差異）
./scripts/demo_robustness.sh all

# 4. 二級除錯日誌控制演示（展示編譯期前處理器過濾與執行期等級過濾）
./scripts/demo_logging.sh
```

---

### 5. 執行自動化測試套件 (CTest)

所有單元測試、整合測試與演示腳本皆已註冊至 CTest：

```bash
ctest --test-dir build --output-on-failure
```

測試涵蓋項目：

1. `test_framing`: 二進制標頭編解碼、端序轉換與 `socketpair` 零碎資料流測試。
2. `test_log`: 日誌等級門檻與前處理器開關單元測試。
3. `test_e2e_echo_quit`: 客戶端-伺服器端對端基礎通訊與結束測試。
4. `test_sysinfo_concurrency`: 系統資訊查詢與 12 客戶端並行獨立 PID 驗證。
5. `test_robustness`: 四項魯棒性機制之有/無回歸測試。
6. `test_demo_scripts`: 驗證所有演示腳本在自動化模式下正常運作。

---

## 詳細期中專案報告

如需參閱包含完整系統設計架構圖、核心程式碼解說、Wireshark 封包深度剖析、A/B 魯棒性實證數據、截圖放置指南與 2 分鐘影片逐字腳本的完整報告，請參閱：

👉 [docs/report.md](docs/report.md)
