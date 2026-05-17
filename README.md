# Si4703ドライバ

## 接続

| Si4703 | マイコン(RP2040) | 備考 |
|---|---|---|
| VCC  | 3.3V |  |
| GND  | GND |  |
| SDIO | GPIO 4 (I2C SDA) | プルアップ必須 (S/W or H/W) |
| SCL  | GPIO 5 (I2C SCL) | プルアップ必須 (S/W or H/W) |
| RST | GPIO 22 |  |
| SEN | (未接続) | プルアップ必須 (H/W) |
