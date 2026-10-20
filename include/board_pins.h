#pragma once
// LILYGO's T-Embed-CC1101 V1.0 pin map; Plus revision needs hardware QA.
// https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/docs/pinmap_cn.md
namespace Pins {
constexpr int Power = 15;
constexpr int Sck = 11;
constexpr int Miso = 10;
constexpr int Mosi = 9;
constexpr int RadioCs = 12;
constexpr int Gdo0 = 3;
constexpr int Gdo2 = 38;
constexpr int AntennaSw1 = 47;
constexpr int AntennaSw0 = 48;
constexpr int SdCs = 13;
constexpr int NrfCs = 44;
constexpr int NrfCe = 43;
constexpr int DisplayCs = 41;
constexpr int Backlight = 21;
constexpr int EncoderA = 4;
constexpr int EncoderB = 5;
constexpr int EncoderKey = 0;
constexpr int UserKey = 6;
constexpr int I2cSda = 8;
constexpr int I2cScl = 18;
constexpr int NfcReset = 45;
constexpr int NfcIrq = 17;
constexpr int IrTx = 2;
constexpr int IrRx = 1;
constexpr int Rgb = 14;
constexpr int RgbCount = 8;
constexpr int MicClock = 39;
constexpr int MicData = 42;
constexpr int SpeakerBck = 46;
constexpr int SpeakerWs = 40;
constexpr int SpeakerData = 7;
}
