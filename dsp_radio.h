/**
 * @file dsp_radio.h
 * @author Chimipupu(https://github.com/Chimipupu)
 * @brief DSPラジオアプリ
 * @version 0.1
 * @date 2026-05-16
 * @copyright Copyright (c) 2026 Chimipupu All Rights Reserved.
 */

#ifndef DSP_RADIO_H
#define DSP_RADIO_H

#include "drv_si4703.h"

// -----------------------------------------------------------
// [コンパイルスイッチ]
// #define DEBUG_DSP_RADIO

// -----------------------------------------------------------
void dsp_radio_fm_ch_chg(void);
void dsp_radio_vol_ctrl(bool is_vol_up);
void dsp_radio_init(void);
void dsp_radio_main(void);

#endif // DSP_RADIO_H