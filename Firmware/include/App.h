#pragma once
#include <Arduino.h>
#include "MacroManager.h"

// ------------------------------------------------------------------
// アプリ全体で共有するオブジェクト (実体は main.cpp)
// ------------------------------------------------------------------
extern MacroManager macroManager;

// USB接続が完了し、LED/入力処理を開始してよいか (core0 が書き込み、core1 が読む)
extern volatile bool isConnected;