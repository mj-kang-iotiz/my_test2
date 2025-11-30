# RTCM to LoRa Transmission - Test Guide

## Overview
This guide provides test scenarios and validation steps for RTCM to LoRa transmission functionality.

## Test Environment Requirements
- STM32F405 board with FreeRTOS
- GPS module (UM982 or F9P) configured for RTCM output
- RAK4270 LoRa module (SF7, BW125, CR4/5)
- Serial monitor for log output

## Quick Validation Checklist

### ✅ 1. Compilation Check
```bash
# Ensure code compiles without errors
# Check for:
# - No type mismatches
# - No undefined references
# - No buffer overflow warnings
```

### ✅ 2. LoRa Initialization Check
```
Expected logs:
[LORA_APP] LoRa TX Task started
[LORA_APP] LoRa TX Task ready
[LORA_APP] LoRa RX Task started
[LORA_APP] LoRa RX Task ready
[LORA_APP] Both TX and RX tasks ready, starting LoRa initialization
[LORA_APP] LoRa init complete - now accepting P2P data
```

### ✅ 3. RTCM Transmission Test Cases

#### Test Case 1: Normal Even-Byte Packet (236 bytes)
**Setup:**
- GPS sends RTCM1074 (GPS MSM4)
- Expected size: ~120-150 bytes (even)

**Expected Logs:**
```
[RTCM] Sending RTCM to LoRa: type=1074, len=120, padded_len=120, ToA=212ms
[RTCM] RTCM TX complete: OK_time=10ms, wait=202ms, total=212ms
[RTCM] RTCM sent successfully (type=1074)
```

**Validation:**
- ✅ No padding added (even bytes)
- ✅ ToA calculated correctly: (120/236)*350*1.2 = 212ms
- ✅ Total time = OK response + wait = ToA

#### Test Case 2: Normal Odd-Byte Packet (235 bytes)
**Setup:**
- GPS sends RTCM1084 (GLONASS MSM4)
- Expected size: ~115-145 bytes (odd)

**Expected Logs:**
```
[RTCM] RTCM odd-byte padding: 125 -> 126 bytes
[RTCM] Sending RTCM to LoRa: type=1084, len=125, padded_len=126, ToA=223ms
[RTCM] RTCM TX complete: OK_time=11ms, wait=212ms, total=223ms
[RTCM] RTCM sent successfully (type=1084)
```

**Validation:**
- ✅ 0x00 padding added
- ✅ Padded length = 126 (even)
- ✅ ToA based on padded length

#### Test Case 3: Maximum Size (236 bytes)
**Expected Logs:**
```
[RTCM] Sending RTCM to LoRa: type=XXXX, len=236, padded_len=236, ToA=420ms
[RTCM] RTCM TX complete: OK_time=12ms, wait=408ms, total=420ms
[RTCM] RTCM sent successfully
```

**Validation:**
- ✅ No overflow error
- ✅ Maximum ToA: 420ms

#### Test Case 4: Oversized Packet (237 bytes, odd)
**Expected Logs:**
```
[RTCM] RTCM odd-byte padding: 237 -> 238 bytes
[RTCM] RTCM length too large after padding: 238 > 236 (max)
```

**Validation:**
- ✅ Error detected AFTER padding
- ✅ Transmission aborted
- ✅ Returns false

#### Test Case 5: Small Packet (50 bytes)
**Expected Logs:**
```
[RTCM] Sending RTCM to LoRa: type=1006, len=50, padded_len=50, ToA=88ms
[RTCM] RTCM TX complete: OK_time=8ms, wait=80ms, total=88ms
[RTCM] RTCM sent successfully (type=1006)
```

**Validation:**
- ✅ Fast transmission (~88ms)
- ✅ Minimal wait time

#### Test Case 6: Rapid Consecutive Transmissions
**Scenario:** GPS sends 5 RTCM packets in quick succession

**Expected Behavior:**
- Each packet waits for its ToA before next transmission
- No packet collision
- Total time = Sum of all ToAs

**Monitor for:**
- ❌ "RTCM TX took longer than ToA" warnings (indicates blocking issue)
- ✅ Consistent timing between packets

### ✅ 4. Error Condition Tests

#### Test 4a: LoRa Not Initialized
**Setup:** Call `rtcm_send_to_lora()` before LoRa init completes

**Expected:**
```
[LORA_APP] LoRa not initialized
```
Returns false gracefully.

#### Test 4b: Zero-Length Packet
**Expected:**
```
[RTCM] RTCM length is zero
```

#### Test 4c: GPS Handle NULL
**Expected:**
```
[RTCM] GPS handle is NULL
```

## Code Analysis Results

### ✅ Memory Safety
- `padded_data[RTCM_MAX_LORA_SIZE + 1]` = 237 bytes buffer ✓
- `hex_str[RTCM_MAX_LORA_SIZE * 2 + 3]` = 475 bytes buffer ✓
- `GPS_PAYLOAD_SIZE` = 256 bytes (sufficient for 236 max) ✓

### ✅ Logic Validation

#### Padding Logic
```c
// Correct order:
1. Calculate actual_len with padding
2. Check if actual_len > 236
3. Proceed if valid
```
✓ Prevents buffer overflow

#### ToA Calculation
```c
ToA = (bytes * 350 / 236) * (100 + 20) / 100
    = (bytes * 350 / 236) * 1.2
```
Examples:
- 236 bytes: (236 * 350 / 236) * 1.2 = 420ms ✓
- 118 bytes: (118 * 350 / 236) * 1.2 = 210ms ✓
- 50 bytes: (50 * 350 / 236) * 1.2 = 88ms ✓

#### Timing Control
```c
1. Record start_tick
2. Send command (get OK response)
3. Calculate elapsed_ms
4. Wait (toa_ms - elapsed_ms)
```
✓ Ensures total delay = ToA

### ⚠️ Potential Issues Found

#### Issue 1: Integer Division Truncation
```c
uint32_t toa_ms = (bytes * LORA_TOA_BASE_MS / LORA_TOA_BASE_BYTES);
// bytes * 350 / 236
// For small values, may lose precision
```

**Impact:** Minor timing inaccuracy for small packets (< 10ms difference)
**Severity:** LOW
**Fix if needed:** Use floating point or pre-multiply

#### Issue 2: configTICK_RATE_HZ Dependency
```c
uint32_t elapsed_ms = elapsed_tick * 1000 / configTICK_RATE_HZ;
```

**Impact:** Assumes FreeRTOS tick rate is defined
**Severity:** LOW (standard FreeRTOS config)
**Validation:** Check FreeRTOSConfig.h for `configTICK_RATE_HZ`

#### Issue 3: Stack Usage
Total stack variables:
- `padded_data[237]` = 237 bytes
- `hex_str[475]` = 475 bytes
- **Total: ~712 bytes**

**Impact:** High stack usage in GPS event handler
**Severity:** MEDIUM
**Recommendation:** Check task stack size (should be >= 1024 bytes)

## Performance Benchmarks

### Transmission Times (20% margin included)

| Packet Size | Padded Size | ToA (ms) | Notes |
|------------|-------------|----------|-------|
| 1 byte     | 2 bytes     | 1 ms     | Minimum |
| 50 bytes   | 50 bytes    | 88 ms    | Small packet |
| 100 bytes  | 100 bytes   | 177 ms   | Medium packet |
| 150 bytes  | 150 bytes   | 266 ms   | Large packet |
| 200 bytes  | 200 bytes   | 354 ms   | Very large |
| 235 bytes  | 236 bytes   | 420 ms   | Max odd |
| 236 bytes  | 236 bytes   | 420 ms   | Maximum |

### Typical RTCM Message Sizes
- **1006** (Station ARP): ~25 bytes
- **1033** (Descriptor): ~50 bytes
- **1074** (GPS MSM4): ~120-150 bytes
- **1084** (GLONASS MSM4): ~115-145 bytes
- **1094** (Galileo MSM4): ~120-140 bytes
- **1124** (BDS MSM4): ~115-135 bytes

### Update Rate Calculations
If GPS sends all MSM4 messages at 1Hz:
- Total data: ~500-600 bytes/sec
- After padding: ~502-602 bytes/sec
- Total ToA: ~900-1050ms
- **Achievable rate: ~1Hz with margin**

## Recommendations

### Stack Size
```c
// In gps_app.c or wherever GPS task is created
xTaskCreate(gps_task, "gps",
            2048,  // Increase from 1024 to 2048
            ...);
```

### Monitoring
Add runtime monitoring:
```c
// Track max ToA exceeded count
static uint32_t toa_exceeded_count = 0;

if (elapsed_ms >= toa_ms) {
    toa_exceeded_count++;
    if (toa_exceeded_count > 10) {
        LOG_ERR("ToA consistently exceeded - check system load");
    }
}
```

### Optimization (if needed)
For very high throughput:
1. Reduce margin from 20% to 10%
2. Use async transmission (non-blocking)
3. Implement transmission queue

## Sign-Off Checklist

Before deployment:
- [ ] All test cases pass
- [ ] No memory corruption detected
- [ ] Stack usage within limits
- [ ] Timing accuracy validated
- [ ] Error handling verified
- [ ] Long-duration stability test (24 hours)
- [ ] Range testing completed
- [ ] Packet loss rate < 1%

## Troubleshooting

### "RTCM TX took longer than ToA"
**Cause:** System too busy, delayed processing
**Fix:** Increase GPS task priority or reduce system load

### "Failed to send RTCM via LoRa"
**Cause:** LoRa not initialized or command queue full
**Fix:** Check LoRa initialization logs

### Packet corruption on receiver
**Cause:** Rapid transmission without proper spacing
**Fix:** Verify ToA timing is working correctly

### Odd-byte packets failing
**Cause:** Padding not working or receiver issue
**Fix:** Check receiver can handle padded data
