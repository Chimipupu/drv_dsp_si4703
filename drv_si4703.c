/**
 * @file drv_si4703.c
 * @author Chimipupu(https://github.com/Chimipupu)
 * @brief DSPラジオIC Si4703 ドライバ
 * @version 0.1
 * @date 2026-05-15
 * @copyright Copyright (c) 2026 Chimipupu All Rights Reserved.
 */

#include "drv_si4703.h"

// -----------------------------------------------------------
#define DRV_TIMEOUT_MS           100

// [レジスタテーブル]
si4703_reg_data_t g_si4703_reg_data_tbl[] = {
    {SI4703_REG_DEVICEID,   0x0000},
    {SI4703_REG_CHIPID,     0x0000},
    {SI4703_REG_POWERCFG,   0x0000},
    {SI4703_REG_CHANNEL,    0x0000},
    {SI4703_REG_SYSCONFIG1, 0x0000},
    {SI4703_REG_SYSCONFIG2, 0x0000},
    {SI4703_REG_SYSCONFIG3, 0x0000},
    {SI4703_REG_TEST1,      0x0000},
    {SI4703_REG_TEST2,      0x0000},
    {SI4703_REG_BOOTCONFIG, 0x0000},
    {SI4703_REG_STATUSRSSI, 0x0000},
    {SI4703_REG_READCHAN,   0x0000},

    // [RDS/RBDSは未サポート]
    // NOTE: 日本国内ではRDS/RBDSの受信はできないため
#if 1
    {SI4703_REG_RDSA,       0x0000},
    {SI4703_REG_RDSB,       0x0000},
    {SI4703_REG_RDSC,       0x0000},
    {SI4703_REG_RDSD,       0x0000},
#endif
};

// [FMラジオ局テーブル]
const fm_station_freq_t g_fm_station_freq_tbl[] = {
#ifdef RADIO_AREA_TOKYO
    // [東京エリア]
    {80.0f,  40, "FM東京"},
    {81.3f,  53, "J-WAVE"},
    {82.5f,  65, "NHK FM東京"},
    {89.7f, 137, "InterFM897"},
    {90.5f, 145, "TBSラジオ"},
    {91.6f, 156, "文化放送"},
    {93.0f, 170, "ニッポン放送"},
#else
    // [大阪エリア]
    // NOTE: U8g2のフォントで大阪の'阪'が非対応なので'坂'で対処
    {76.5f,   5, "FM COCOLO"},
    {80.2f,  42, "FM802"},
    {85.1f,  91, "FM大坂"},
    {88.1f, 121, "NHK FM OSAKA"},
    {89.4f, 134, "a-STATION"},
    {89.9f, 139, "Kiss FM KOBE"},
    {90.6f, 146, "MBSラジオ"},
    {91.1f, 151, "ラジオ関西"},
    {91.9f, 159, "ラジオ大坂OBC"},
    {93.3f, 173, "ABCラジオ"},
#endif
};
const uint8_t FM_STATION_FREQ_TBL_SIZE = sizeof(g_fm_station_freq_tbl) / sizeof(g_fm_station_freq_tbl[0]);

static bool s_is_2_wire_enabled = false;
static kt0913_config_t s_drv_cfg;
// static kt0913_volume_ctrl_t s_vol_ctrl;
static uint16_t s_write_reg_buf[8];

static void _si4703_i2c_ctrl_enable(void);
static void _set_reg(uint8_t reg_addr, uint16_t reg_val);
static uint16_t _get_reg(uint8_t reg_addr);
// -----------------------------------------------------------
// [Static]

/**
 * @brief 2線式制御の有効化
 * @note Si4703のリセット時にSDAとRSTピンをGPIOでいじって2線式を有効化
 */
static void _si4703_i2c_ctrl_enable(void)
{
    // 1) Si4703のSDAピンとRSTピンをLow
    s_drv_cfg.p_sda_pin_ctrl(GPIO_LV_LOW);
    s_drv_cfg.p_rst_pin_ctrl(GPIO_LV_LOW);
    s_drv_cfg.p_delay_ms(2);

    // 2) RSTピン、SDAピンの順でHighに戻す
    s_drv_cfg.p_rst_pin_ctrl(GPIO_LV_HIGH);
    s_drv_cfg.p_delay_ms(2);
    s_drv_cfg.p_sda_pin_ctrl(GPIO_LV_HIGH);
    s_drv_cfg.p_delay_ms(10);

    // 3) I2C初期化
    s_drv_cfg.p_i2c_init();
    s_is_2_wire_enabled = true;
}

// NOTE: I2CでSi4703のレジスタをReadはAddr:0x0A〜0x0F、0x00〜0x09の順にバーストされる仕様
static void _read_all_reg(void)
{
    uint16_t read_buf[16];

    memset(read_buf, 0, sizeof(read_buf));
    s_drv_cfg.p_i2c_burst_read(&read_buf[0], 16 * 2);

    // バッファのデータはレジスタAddr:0x0A〜0x0F、0x00〜0x09の順なので対応
    g_si4703_reg_data_tbl[0x0A].reg_val = read_buf[0x00];  // Addr:0x0A
    g_si4703_reg_data_tbl[0x0B].reg_val = read_buf[0x01];  // Addr:0x0B
    g_si4703_reg_data_tbl[0x0C].reg_val = read_buf[0x02];  // Addr:0x0C
    g_si4703_reg_data_tbl[0x0D].reg_val = read_buf[0x03];  // Addr:0x0D
    g_si4703_reg_data_tbl[0x0E].reg_val = read_buf[0x04];  // Addr:0x0E
    g_si4703_reg_data_tbl[0x0F].reg_val = read_buf[0x05];  // Addr:0x0F

    g_si4703_reg_data_tbl[0x00].reg_val = read_buf[0x06];   // Addr:0x00
    g_si4703_reg_data_tbl[0x01].reg_val = read_buf[0x07];   // Addr:0x01
    g_si4703_reg_data_tbl[0x02].reg_val = read_buf[0x08];   // Addr:0x02
    g_si4703_reg_data_tbl[0x03].reg_val = read_buf[0x09];   // Addr:0x03
    g_si4703_reg_data_tbl[0x04].reg_val = read_buf[0x0A];   // Addr:0x04
    g_si4703_reg_data_tbl[0x05].reg_val = read_buf[0x0B];   // Addr:0x05
    g_si4703_reg_data_tbl[0x06].reg_val = read_buf[0x0C];   // Addr:0x06
    g_si4703_reg_data_tbl[0x07].reg_val = read_buf[0x0D];   // Addr:0x07
    g_si4703_reg_data_tbl[0x08].reg_val = read_buf[0x0E];   // Addr:0x08
    g_si4703_reg_data_tbl[0x09].reg_val = read_buf[0x0F];   // Addr:0x09
}

static void _set_reg(uint8_t reg_addr, uint16_t reg_val)
{
    uint8_t i;

    // 読み出し専用レジスタ: Addr 0x00、0x01、0x0A ~ 0x0F
    if((reg_addr < 0x02) || (reg_addr >= 0x0A)) {
        return;
    }

    // 先にレジスタを読み出しとく
    _read_all_reg();

    // 書き込めるレジスタ: Addr 0x02 ~ 0x09
    for(i = 0; i < 8; i++)
    {
        s_write_reg_buf[i] = g_si4703_reg_data_tbl[2 + i].reg_val;
    }
    s_write_reg_buf[reg_addr - 2] = reg_val;

    s_drv_cfg.p_i2c_burst_write(s_write_reg_buf, 8 * 2);
}

static uint16_t _get_reg(uint8_t reg_addr)
{
    uint16_t reg_val = 0xFFFF;

    _read_all_reg();

    if(reg_addr <= 0x0F) {
        reg_val = g_si4703_reg_data_tbl[reg_addr].reg_val;
    }

    return reg_val;
}

// -----------------------------------------------------------
// [API]

bool drv_si4703_init(kt0913_config_t *p_config)
{
    uint16_t reg_val;

    // 引数のNULLチェック
    if( p_config == NULL ) {
        return false;
    }

    s_drv_cfg = *p_config;

    // Si4703の制御方式を2線式のI2Cに変更
    _si4703_i2c_ctrl_enable();

#if 1
    // TEST1レジスタ(Addr:0x07)
    {
        reg_val = _get_reg(SI4703_REG_TEST1);
        reg_val |= 0x8000; // Bit15のXOSCENビットを立てる(水晶発振子の有効化)
        _set_reg(SI4703_REG_TEST1, reg_val);

        // NOTE: 水晶振動子の発振安定待ち
        s_drv_cfg.p_delay_ms(500);
    }
#endif

    // System Configuration 1 レジスタ(Addr:0x04)
    {
        reg_val = _get_reg(SI4703_REG_SYSCONFIG1);

        // [信号強調(De-emphasis)]: Bit11 DEビット = 1 (Japan: 50us)
        reg_val |= (0x0800);

        _set_reg(SI4703_REG_SYSCONFIG1, reg_val);
    }

    // System Configuration 2 レジスタ(Addr:0x05)
    {
        reg_val = _get_reg(SI4703_REG_SYSCONFIG2);

        // [周波数帯域 76~108MHz]: Bit[7:6] BANDビット = 0x01セット
        // [Spacing 100kHz]: Bit[5:4] SPACEビット = 0x01セット
        // [音量]: Bit[3:0] VOLUMEビット
        reg_val |= (0x0040 | 0x0010 | 0x000A);

        _set_reg(SI4703_REG_SYSCONFIG2, reg_val);
    }

    // POWERCFGレジスタ(Addr:0x02)
    {
        reg_val = _get_reg(SI4703_REG_POWERCFG);

        // [ミュート解除]: Bit14 DMUTEビットをセット
        // [ソフトミュート解除]: Bit13 SMUTEビットをセット]
        // [Seek Up Enable]: Bit12 SEEKUPビットをセット(0: Seek Down, 1: Seek Up)
        // [Power Up Enable]: Bit0 ENABLEビットをセット
        reg_val |= (0x4000 | 0x2000 | 0x1000 | 0x0001);

        _set_reg(SI4703_REG_POWERCFG, reg_val);
    }

    // ★超重要: Power Up完了待ち
    s_drv_cfg.p_delay_ms(130);

    // FM周波数の初期値を設定
    // NOTE: 受信地域: 東京 = FM東京(80.0MHz)、大阪 = FM大阪(85.1MHz)
#ifdef RADIO_AREA_TOKYO
    drv_si4703_set_fm_freq(FM_STATION_FM_TOKYO);
#else
    drv_si4703_set_fm_freq(FM_STATION_FM_OSAKA);
#endif

    // 初期化時の音量を小さくしておく
    drv_si4703_set_vol(0x08);

    return true;
}

void drv_si4703_set_vol(uint8_t vol_db)
{
    uint16_t reg_val;

    // SYSCONFIG2レジスタ(Addr:0x05)のBit[3:0] VOLUMEビットを設定
    reg_val = _get_reg(SI4703_REG_SYSCONFIG2);
    reg_val &= 0xFFF0; // Bit[3:0] VOLUMEビットをクリア
    reg_val |= (uint16_t)(vol_db & 0x0F);
    _set_reg(SI4703_REG_SYSCONFIG2, reg_val);
}

uint8_t drv_si4703_get_vol(void)
{
    uint8_t read_vol = 0;
    uint16_t reg_val;

    reg_val = _get_reg(SI4703_REG_SYSCONFIG2);
    read_vol = (uint8_t)(reg_val & 0x000F);

    return read_vol;
}

bool drv_si4703_set_fm_freq(uint8_t station)
{
    uint16_t reg_val;
    uint16_t status_reg;
    uint32_t timeout;

    // 引数チェック
    if(station >= FM_STATION_FREQ_TBL_SIZE) {
        return false;
    }

    // 引数のラジオ局のFM周波数をテーブルから引いてくる
    reg_val = g_fm_station_freq_tbl[station].set_reg_val;

    // 1) TUNEビットを0にしてFM周波数を設定
    _set_reg(SI4703_REG_CHANNEL, reg_val & ~0x8000);

    // 2) TUNEビットを1にして指定のFM周波数にTUNE開始
    _set_reg(SI4703_REG_CHANNEL, reg_val | 0x8000);

    // 3) TUNE完了待ち
    // NOTE: Status RSSIレジスタ(Addr:0x0AのBit14 STCビットが1になるまで
    timeout = DRV_TIMEOUT_MS;
    while(timeout > 0)
    {
        status_reg = _get_reg(SI4703_REG_STATUSRSSI);
        if((status_reg & 0x4000) != 0) {
            break; // STCが1になったら完了
        }
        s_drv_cfg.p_delay_ms(1);
        timeout--;
    }

    // 4) TUNEビットを0に戻す
    //  TUNEビットが立ってるときはミュートされるから
    _set_reg(SI4703_REG_CHANNEL, reg_val & ~0x8000);

    // 5) STCビットが0に戻るのを待つ
    timeout = DRV_TIMEOUT_MS;
    while(timeout > 0)
    {
        status_reg = _get_reg(SI4703_REG_STATUSRSSI);
        if((status_reg & 0x4000) == 0) {
            break; // STCが0に戻ったら次の操作が可能
        }
        s_drv_cfg.p_delay_ms(1);
        timeout--;
    }

    return true;
}

int8_t drv_si4703_get_fm_rssi(void)
{
    int8_t rssi_dB;
    uint8_t rssi_reg_val;
    uint16_t reg_val;

    // STATUSRSSIレジスタ(Addr:0x0A)のBit[7:0]のRSSIビット
    reg_val = _get_reg(SI4703_REG_STATUSRSSI);
    rssi_reg_val = (uint8_t)(reg_val& 0xFF);

    // RSSIを下位8bitから取り出す
    rssi_reg_val = (uint8_t)(reg_val & 0x00FF);

    // RSSIの最大値 75dBuVでマスク
    rssi_dB = (int8_t)(rssi_reg_val & SI4703_MAX_RSSI);

    return rssi_dB;
}

void drv_si4703_all_reg_dump(void)
{
    _read_all_reg();
}