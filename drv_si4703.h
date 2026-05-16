/**
 * @file drv_si4703.h
 * @author Chimipupu(https://github.com/Chimipupu)
 * @brief DSPラジオIC Si4703 ドライバ
 * @note Si4703の制御方式: 2線式のI2C (3線式は未対応 @永遠に)
 * @version 0.1
 * @date 2026-05-15
 * @copyright Copyright (c) 2026 Chimipupu All Rights Reserved.
 */

#ifndef DRV_SI4703_H
#define DRV_SI4703_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------
// [コンパイルスイッチ]
#define DEBUG_DRV_SI4703

// -----------------------------------------------------------
// [Define]

// Si4703のレジスタアドレス
#define I2C_ADDR_SI4703           0x10
// #define I2C_ADDR_SI4703           (0x10 << 1)

// CHIPIDレジスタの期待値
#define SI4703_CHIP_ID             0x1242

// Si4703の最大RSSI = 75[dBuV]
#define SI4703_MAX_RSSI            0x4B

// FM周波数
#define SI4703_FM_FREQ_MHZ_MIN     76.0f
#define SI4703_FM_FREQ_MHZ_MAX     108.0f

// 受信地域
// #define RADIO_AREA_TOKYO           0 // 受信地域: 東京
#define RADIO_AREA_OSAKA           1 // 受信地域: 大阪

#define GPIO_LV_LOW                0
#define GPIO_LV_HIGH               1
// -----------------------------------------------------------
// Si4703レジスタ
typedef enum {
    SI4703_REG_DEVICEID = 0x00,
    SI4703_REG_CHIPID,
    SI4703_REG_POWERCFG,
    SI4703_REG_CHANNEL,
    SI4703_REG_SYSCONFIG1,
    SI4703_REG_SYSCONFIG2,
    SI4703_REG_SYSCONFIG3,
    SI4703_REG_TEST1,
    SI4703_REG_TEST2,
    SI4703_REG_BOOTCONFIG,
    SI4703_REG_STATUSRSSI,
    SI4703_REG_READCHAN,

    // [RDS/RBDSは未サポート]
    // NOTE: 日本国内ではRDS/RBDSの受信はできないため
#if 1
    SI4703_REG_RDSA,
    SI4703_REG_RDSB,
    SI4703_REG_RDSC,
    SI4703_REG_RDSD
#endif
} SI4703_REG;

// FMラジオ局構造体
typedef struct {
    float fm_rerq_Mhz;    // FM周波数(MHz)
    uint16_t set_reg_val; // SI4703のレジスタ値 @10bit
    char *p_str;          // FMラジオ局名
} fm_station_freq_t;
extern const fm_station_freq_t g_fm_station_freq_tbl[];
extern const uint8_t FM_STATION_FREQ_TBL_SIZE;

typedef enum {
#ifdef RADIO_AREA_TOKYO
    // 東京エリア
    FM_STATION_FM_TOKYO = 0,        // FM東京: 80.0MHz
    FM_STATION_JWAVE,               // J-WAVE: 81.3MHz
    FM_STATION_NHK_FM_TOKYO,        // NHK FM東京: 82.5MHz
    FM_STATION_INTERFM897,          // InterFM897: 89.7MHz
    FM_STATION_TBS_WIDEFM,          // TBSラジオ(ワイドFM): 90.5MHz
    FM_STATION_BUNKA_WIDEFM,        // 文化放送(ワイドFM): 91.6MHz
    FM_STATION_NIPPON_WIDEFM,       // ニッポン放送(ワイドFM): 93.0MHz
#else
    // 大阪エリア
    FM_STATION_FM_COCOLO,           // FM COCOLO: 76.5MHz
    FM_STATION_FM802,               // FM802: 80.2MHz
    FM_STATION_FM_OSAKA,            // FM大阪: 85.1MHz
    FM_STATION_NHK_FM_OSAKA,        // NHK FM大阪: 88.1MHz
    FM_STATION_ALPHA_STATION,       // α-STATION(京都): 89.4MHz
    FM_STATION_KISS_FM_KOBE,        // Kiss FM KOBE（神戸）: 89.9MHz
    FM_STATION_MBS_WIDEFM,          // MBSラジオ(ワイドFM): 90.6MHz
    FM_STATION_OBC_WIDEFM,          // ラジオ大阪OBC(ワイドFM): 91.9MHz
    FM_STATION_ABC_WIDEFM,          // ABCラジオ(ワイドFM): 93.3MHz
    FM_STATION_RADIO_KANSAI_WIDEFM, // ラジオ関西(ワイドFM): 91.1MHz
#endif
} E_FM_STATION;

// Si4703ボリューム制御構造体
typedef struct {
    bool is_stereo;    // true: ステレオ, false: モノラル
    bool is_vol_ext;   // 拡張音量範囲の有効有無
    uint8_t volume_dB; // 音量dB (デフォ:0dB ~ -28dB、拡張音量:-30dB ~ -58dB)
} kt0913_volume_ctrl_t;

// レジスタデータ
typedef struct {
    uint8_t reg_addr;
    uint16_t reg_val;
} si4703_reg_data_t;
extern si4703_reg_data_t g_si4703_reg_data_tbl[];

// Si4703のRSTピンのON/OFF関数ポインタ
typedef void (*rst_pin_ctrl_func_t)(uint8_t);

// Si4703のSDAピン(SDIOピン)のON/OFF関数ポインタ
typedef void (*sda_pin_ctrl_func_t)(uint8_t);

// I2Cの初期化関数ポインタ
// NOTE: 期待値: Arduino IDE環境ならWire.begin()のラッパーの関数ポインタ
typedef void (*i2c_init_func_t)(void);

// I2CのバーストWrite関数ポインタ
typedef void (*i2c_burst_write_func_t)(uint16_t *, uint32_t);

// I2CのバーストRead関数ポインタ
typedef void (*i2c_burst_read_func_t)(uint16_t *, uint32_t);

// Delay関数ポインタ
// NOTE: 期待値: Arduino IDE環境ならdelay(ms)の関数ポインタ
typedef void (*delay_ms_func_t)(unsigned long);

// Si4703ドライバ初期化構造体
typedef struct {
    kt0913_volume_ctrl_t vol_cfg;

    // [関数ポインタ]
    rst_pin_ctrl_func_t p_rst_pin_ctrl;
    sda_pin_ctrl_func_t p_sda_pin_ctrl;
    i2c_init_func_t p_i2c_init;
    i2c_burst_write_func_t p_i2c_burst_write;
    i2c_burst_read_func_t p_i2c_burst_read;
    delay_ms_func_t p_delay_ms;
} kt0913_config_t;

// -----------------------------------------------------------
// [API]
bool drv_si4703_init(kt0913_config_t *p_config);
void drv_si4703_set_vol(uint8_t vol_db);
bool drv_si4703_set_fm_freq(uint8_t station);
int8_t drv_si4703_get_fm_rssi(void);
void drv_si4703_all_reg_dump(void);

#ifdef __cplusplus
}
#endif

#endif // DRV_SI4703_H