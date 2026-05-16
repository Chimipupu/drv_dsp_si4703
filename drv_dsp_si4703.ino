/**
 * @file dev_dsp_kt0913.ino
 * @author Chimipupu(https://github.com/Chimipupu)
 * @brief DSPラジオ(Si4703)のアプリ for Arduino IDE
 * @note マイコン: RP2040
 * @version 0.1
 * @date 2026-05-16
 * @copyright Copyright (c) 2026 Chimipupu All Rights Reserved.
 */

// Arduino IDE
#include <Wire.h>

// DSPラジオ
#include "dsp_radio.h"

void setup()
{
    Serial.begin(115200);

    // DSPラジオ初期化
    dsp_radio_init();
}

void loop()
{
    // DSPラジオ メイン
    dsp_radio_main();
}