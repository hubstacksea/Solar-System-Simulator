// OS ごとの保存先フォルダ
#pragma once
#include <string>

// スクリーンショットの保存先（ピクチャフォルダ内の SolarSystemVoyager）。末尾に区切り文字付き。
// 作れなければ空文字（カレントフォルダに保存する）
std::string ScreenshotDir();
