# Bug Fix TODO List

## Priority 1: Critical Issues

- [ ] Fix race condition in CSI callback mutex timeout (Issue #1)
  - Location: wifi_csi.c:122-126
  - Change `xSemaphoreTake(s_csi_mutex, 0)` to `xSemaphoreTake(s_csi_mutex, pdMS_TO_TICKS(1))`
  - Ensures brief wait for mutex instead of immediate failure

- [ ] Replace blocking printf with non-blocking output in WiFi callback (Issue #2)
  - Location: wifi_csi.c:136-149
  - Move JSON output to separate task to avoid blocking WiFi driver
  - Use queue for data passing between callback and output task

- [ ] Fix integer overflow in amplitude calculation (Issue #3)
  - Location: wifi_csi.c:87
  - Cast to float before multiplication: `sqrtf((float)I * I + (float)Q * Q)`
  - Prevents potential integer overflow

## Priority 2: High Priority Issues

- [ ] Fix timestamp comment mismatch (Issue #4)
  - Location: wifi_csi.h:39
  - Change comment from "Timestamp in microseconds" to "Timestamp in milliseconds"
  - Matches actual implementation

- [ ] Add proper NVS flash error recovery (Issue #5)
  - Location: main.c:211-218
  - Add explicit error check after NVS reinit
  - Provide better error message for second failure

- [ ] Document or fix event group lifecycle (Issue #6)
  - Location: main.c:97
  - Add comment noting event group is intentionally never deleted
  - Or add cleanup if appropriate

- [ ] Store WiFi event handler instances for cleanup (Issue #7)
  - Location: main.c:117-120
  - Make instances static/global for potential cleanup
  - Or document permanent registration
