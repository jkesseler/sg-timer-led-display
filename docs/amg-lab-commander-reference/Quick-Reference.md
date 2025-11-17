# AMG Lab Commander - Quick Reference

**Device Type**: Shot timer with BLE connectivity
**Protocol**: Nordic UART Service (text commands + binary responses)
**Documentation Status**: 📋 Documented from [AmgLabCommander](https://github.com/DenisZhadan/AmgLabCommander)

---

## Connection Details

| Property | Value |
|----------|-------|
| **Device Name** | `AMG LAB COMM*` or `COMMANDER*` |
| **Service UUID** | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (Nordic UART) |
| **TX Characteristic** | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (Write commands) |
| **RX Characteristic** | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (Receive notifications) |
| **Descriptor UUID** | `00002902-0000-1000-8000-00805F9B34FB` (CCCD) |

---

## Text Commands (TX)

Send UTF-8 encoded strings to TX characteristic:

| Command | Purpose | Parameters |
|---------|---------|------------|
| `COM START` | Start timing session | None |
| `REQ STRING HEX` | Request shot list | None |
| `REQ SCREEN HEX` | Request screen data | None |
| `SET SENSITIVITY XX` | Adjust mic sensitivity | `XX` = 01-10 |

**Example:**
```cpp
pTxChar->writeValue("COM START", 9);
```

---

## Binary Responses (RX)

### Shot Event (Type 1, Event 3)
**Format:** 14 bytes
```
01 03 [SH_H] [SH_L] [TM_H] [TM_L] [SP_H] [SP_L] [FS_H] [FS_L] [??_H] [??_L] [SE_H] [SE_L]
```

| Bytes | Field | Description |
|-------|-------|-------------|
| 0 | Type | `0x01` = Timer event |
| 1 | Event | `0x03` = Shot detected |
| 2-3 | Shot # | 16-bit big-endian |
| 4-5 | Time | Centiseconds (BE) |
| 6-7 | Split | Centiseconds (BE) |
| 8-9 | First | First shot time (cs) |
| 10-11 | Unknown | Purpose unclear |
| 12-13 | Series | Session/batch number |

**Time Decoding:**
```cpp
int16_t cs = (highByte << 8) | lowByte;
if (lowByte <= 0) cs += 256;  // Sign correction
uint32_t ms = cs * 10;  // To milliseconds
```

### Timer Start (Type 1, Event 5)
```
01 05
```

### Timer Stop (Type 1, Event 8)
```
01 08
```

### Shot List Response (Type 10-26)
**Format:** Variable length
```
[TYPE] [COUNT] [SHOT1_H] [SHOT1_L] [SHOT2_H] [SHOT2_L] ...
```

- Type `0x0A` (10) = First packet (clear previous data)
- Type `0x0B-0x1A` (11-26) = Continuation packets
- Each shot = 2 bytes (centiseconds, big-endian)

---

## Implementation Checklist

### Phase 1: Basic Connection
- [ ] Scan for device name pattern
- [ ] Connect to Nordic UART Service
- [ ] Get TX and RX characteristics
- [ ] Enable notifications on RX
- [ ] Verify descriptor write

### Phase 2: Shot Detection
- [ ] Parse Type 1 Event 3 (shot events)
- [ ] Implement time decoding function
- [ ] Validate with known shot times
- [ ] Convert to `NormalizedShotData`
- [ ] Test split time accuracy

### Phase 3: Session Control
- [ ] Send `COM START` command
- [ ] Handle Type 1 Event 5 (start)
- [ ] Handle Type 1 Event 8 (stop)
- [ ] Track session ID from series field

### Phase 4: Advanced Features
- [ ] Implement shot list retrieval
- [ ] Parse multi-packet responses (Type 10-26)
- [ ] Add sensitivity control
- [ ] Test all command responses

---

## Key Differences from Other Timers

| Feature | AMG Lab Commander | SG Timer | Special Pie |
|---------|-------------------|----------|-------------|
| **Command Format** | Text (UTF-8) | Binary TLV | None |
| **Time Format** | Centiseconds (uint16 BE) | Milliseconds (uint32) | Seconds + CS |
| **Split Times** | ✅ Provided | ✅ Provided | ❌ Calculate client-side |
| **Remote Start** | ✅ Yes | ✅ Yes | ❌ No |
| **Shot List** | ✅ Yes | ✅ Yes | ❌ No |
| **Service** | Nordic UART | Custom | Custom |

---

## Common Pitfalls

1. **Sign Extension:** The Java code has unusual sign correction logic - needs validation
2. **Byte Order:** Big-endian (high byte first) unlike some ARM defaults
3. **Multi-packet Responses:** Type 10-26 messages may span multiple notifications
4. **Unknown Fields:** Bytes 10-11 in shot events - purpose unclear
5. **Text Encoding:** Commands must be UTF-8, not ASCII with terminator

---

## Testing Strategy

1. **Connect** to device and enable notifications
2. **Send** `COM START` command
3. **Fire** a shot and validate time decoding
4. **Compare** times with timer display
5. **Test** split time calculations
6. **Request** shot list with `REQ STRING HEX`
7. **Validate** multi-packet parsing

---

## References

- Full Protocol: `docs/amg-lab-commander-reference/BLE-Protocol-Analysis.md`
- Device Comparison: `docs/DEVICE_COMPARISON.md`
- Android Source: `docs/AMG Lab Commander/*.java`
- Nordic UART Spec: [Nordic Semiconductor Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/libraries/bluetooth_services/services/nus.html)

---

**Status**: Ready for implementation - requires physical device validation
