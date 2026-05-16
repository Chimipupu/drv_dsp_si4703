/**
 * @file dsp_radio.cpp
 * @author Chimipupu(https://github.com/Chimipupu)
 * @brief DSPラジオアプリ
 * @version 0.1
 * @date 2026-05-16
 * @copyright Copyright (c) 2026 Chimipupu All Rights Reserved.
 */

#include "dsp_radio.h"

// Arduino IDE
#include <U8g2lib.h>
#include <Wire.h>

// Si4703ドライバ
#include "drv_si4703.h"

// -----------------------------------------------------------
// 基板のGPIO
#define I2C_SDA_PIN        4
#define I2C_SCL_PIN        5
#define DSP_RST_PIN        22
#define PCB_BTN_PIN        24

// OLEDディスプレイのI2Cアドレス
#define I2C_ADDR_OLED      0x3C

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C g_lcd(U8G2_R0, U8X8_PIN_NONE);
kt0913_config_t g_si4703_cfg;

static kt0913_volume_ctrl_t s_vol_ctrl;
static uint8_t s_fm_freq_tbl_idx = 0;
static float s_fm_freq = 76.5f; // 初期周波数
static int8_t s_fm_rssi = 0; // RSSI値

static void _gpio_init(void);
static void _i2c_init(void);
#if 0
static void _i2c_write(uint8_t reg_addr, uint16_t reg_val);
static uint16_t _i2c_read(uint8_t reg_addr);
#endif
static void _i2c_read_burst(uint16_t *p_reg, uint32_t read_byte_length);
static void _i2c_write_burst(uint16_t *p_buf, uint32_t write_byte_length);
static void _ui_draw_fm_freq(float freq_val, char *p_str);

#ifdef DEBUG_DSP_RADIO
static void _dbg_get_all_reg(void);
#endif // DEBUG_DSP_RADIO
// -----------------------------------------------------------
// [Static]

static void _btn_isr(void)
{
    dsp_radio_fm_ch_chg();
}

static void _gpio_init(void)
{
    // 基板のYD-RP2040のボタン(GPIO24)を割り込みに設定
    // NOTE: ボタンがONでFMのCH切り替えをコールバック
    pinMode(PCB_BTN_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PCB_BTN_PIN), _btn_isr, FALLING);
}

static void _rst_pin_ctrl(uint8_t onoff)
{
    static bool s_is_rst_pin_init = false;

    if(s_is_rst_pin_init != true) {
        pinMode(DSP_RST_PIN, OUTPUT);
        s_is_rst_pin_init = true;
    }

    digitalWrite(DSP_RST_PIN, (onoff & 0x01) ? HIGH : LOW);
}

static void _sda_pin_ctrl(uint8_t onoff)
{
    static bool s_is_sda_pin_init = false;

    if(s_is_sda_pin_init != true) {
        pinMode(I2C_SDA_PIN, OUTPUT);
        s_is_sda_pin_init = true;
    }

    digitalWrite(I2C_SDA_PIN, (onoff & 0x01) ? HIGH : LOW);
}

static void _i2c_init(void)
{
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);
    Wire.begin();
}

#if 0
static uint16_t _i2c_read(uint8_t reg_addr)
{
    uint16_t reg_val = 0xFFFF;

    Wire.beginTransmission(I2C_ADDR_SI4703);
    Wire.write(reg_addr);
    Wire.endTransmission(false); // リピートスタート

    if(Wire.requestFrom(I2C_ADDR_SI4703, 2) == 2) {
        reg_val = (Wire.read() << 8); // 上位バイト
        reg_val |= Wire.read();       // 下位バイト
    }

    return reg_val;
}

static void _i2c_write(uint8_t reg_addr, uint16_t reg_val)
{
    Wire.beginTransmission(I2C_ADDR_SI4703);
    Wire.write(reg_addr);
    Wire.write((reg_val >> 8) & 0xFF); // 上位バイト
    Wire.write(reg_val & 0xFF);        // 下位バイト
    Wire.endTransmission();
}
#endif

static void _i2c_read_burst(uint16_t *p_reg, uint32_t read_byte_length)
{
    uint8_t i;
    uint32_t word_length;

    word_length = read_byte_length / 2;

    if(Wire.requestFrom(I2C_ADDR_SI4703, read_byte_length) == read_byte_length) {
        for(i = 0; i < word_length; i++)
        {
            p_reg[i] = (Wire.read() << 8); // 上位バイト
            p_reg[i] |= Wire.read();       // 下位バイト
        }
    }
}

static void _i2c_write_burst(uint16_t *p_buf, uint32_t write_byte_length)
{
    uint32_t i;
    uint32_t word_length;

    word_length = write_byte_length / 2;

    Wire.beginTransmission(I2C_ADDR_SI4703);
    for(i = 0; i < word_length; i++)
    {
        Wire.write((p_buf[i] >> 8) & 0xFF); // 上位バイト
        Wire.write(p_buf[i] & 0xFF);        // 下位バイト
    }
    Wire.endTransmission();
}

static void _lcd_init(void)
{
    g_lcd.setI2CAddress(I2C_ADDR_OLED << 1);
    g_lcd.begin();
    g_lcd.enableUTF8Print();
}

/**
 * @brief UIにFM関連情報を表示
 * @note LCDサイズ: 0.91インチの128x32
 * @param freq_val FM周波数(MHz)
 * @param p_str FMラジオ局の日本語文字列ポインタ
 */
static void _ui_draw_fm_freq(float freq_val, char *p_str)
{
    char buf[128];

    /**
     * @brief LCDの表示例
     * ラジオ局: FM大阪
     * 85.1MHz -65dBm
     */

    g_lcd.clearBuffer();

    // 1行目: FMラジオ局名（日本語）
    g_lcd.setFont(u8g2_font_b12_t_japanese1);
    g_lcd.setCursor(0, 12);
    snprintf(buf, sizeof(buf), "ラジオ局: %s", p_str);
    g_lcd.print(buf);

    // 2行目: FM周波数[MHz]とRSSI[dBuV]
    g_lcd.setFont(u8g2_font_helvB12_tr);
    g_lcd.setCursor(0, 31);
    snprintf(buf, sizeof(buf), "%.1fMHz %ddBuV", freq_val, s_fm_rssi);
    g_lcd.print(buf);

    g_lcd.sendBuffer();
}

// -----------------------------------------------------------
// [API]

void dsp_radio_fm_ch_chg(void)
{
    // FM周波数をテーブルから選択
    drv_si4703_set_fm_freq((E_FM_STATION)s_fm_freq_tbl_idx);
    s_fm_freq = g_fm_station_freq_tbl[s_fm_freq_tbl_idx].fm_rerq_Mhz;

    // UIに周波数とラジオ局名を表示
    _ui_draw_fm_freq(s_fm_freq, g_fm_station_freq_tbl[s_fm_freq_tbl_idx].p_str);
    Serial.printf("FM Freq: %.1f MHz (%s)\r\n", s_fm_freq, g_fm_station_freq_tbl[s_fm_freq_tbl_idx].p_str);
    s_fm_rssi = drv_si4703_get_fm_rssi();
    Serial.printf("RSSI: %d dBuV\r\n", s_fm_rssi);

    s_fm_freq_tbl_idx = (s_fm_freq_tbl_idx + 1) % FM_STATION_FREQ_TBL_SIZE;
}

void dsp_radio_vol_ctrl(bool is_vol_up)
{
    s_vol_ctrl.volume_dB = drv_si4703_get_vol();

    if(is_vol_up) {
        s_vol_ctrl.volume_dB++;
    } else {
        s_vol_ctrl.volume_dB--;
    }

    drv_si4703_set_vol(s_vol_ctrl.volume_dB & 0x0F);
    Serial.printf("Volume: %d\r\n", s_vol_ctrl.volume_dB & 0x0F);
}

void dsp_radio_init(void)
{
    // Si4703ドライバにI2CのRead/Write関数を渡して初期化
    g_si4703_cfg.p_i2c_burst_read = _i2c_read_burst;
    g_si4703_cfg.p_i2c_burst_write = _i2c_write_burst;
    g_si4703_cfg.p_rst_pin_ctrl = _rst_pin_ctrl;
    g_si4703_cfg.p_sda_pin_ctrl = _sda_pin_ctrl;
    g_si4703_cfg.p_i2c_init = _i2c_init;
    g_si4703_cfg.p_delay_ms = delay;
    drv_si4703_init(&g_si4703_cfg);

    // GPIO初期化
    _gpio_init();

    // LCD初期化
    _lcd_init();

    // LCDのUIにFM関連情報を表示
    _ui_draw_fm_freq(g_fm_station_freq_tbl[s_fm_freq_tbl_idx].fm_rerq_Mhz,
                    g_fm_station_freq_tbl[s_fm_freq_tbl_idx].p_str);

#ifdef DEBUG_DSP_RADIO
    // [DEBUG] DSPの全レジスタを読み出し
    _dbg_get_all_reg();
#endif // DEBUG_DSP_RADIO
}

void dsp_radio_main(void)
{
    char c;

    // シリアルの受信でDSPラジオを制御
    if (Serial.available() > 0) {
        c = Serial.read();

        // アルファベットのみ処理
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            // 'n'を受信: FMラジオのCHを切り替え
            if (c == 'n') {
                dsp_radio_fm_ch_chg();
            }
            // 'u'を受信: 音量アップ
            else if (c == 'u') {
                dsp_radio_vol_ctrl(true);
            }
            // 'd'を受信: 音量ダウン
            else if (c == 'd') {
                dsp_radio_vol_ctrl(false);
            }

#ifdef DEBUG_DSP_RADIO
        // [DEBUG] DSPの全レジスタを読み出し
        _dbg_get_all_reg();
#endif // DEBUG_DSP_RADIO
        }
    }
}

// -----------------------------------------------------------
// [DEBUG]

#ifdef DEBUG_DSP_RADIO
static void _dbg_get_all_reg(void)
{
    uint8_t i;

    drv_si4703_all_reg_dump();
    Serial.println("[DEBUG] DSP(SI4703) All Register Read Dump:");

    for(i = 0; i < 16; i++)
    {
        Serial.printf("[DEBUG] Reg[0x%02X]: 0x%04X\r\n", i, g_si4703_reg_data_tbl[i].reg_val);
    }
}
#endif // DEBUG_DSP_RADIO