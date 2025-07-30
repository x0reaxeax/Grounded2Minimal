// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_WINGUI_WRAPPER_HPP
#define _GROUNDED_WINGUI_WRAPPER_HPP

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

namespace WinGUI {

    // Callback functions (these are set before calling LaunchGUIThread)
    extern std::function<void(int32_t /*iPlayerId*/, const std::wstring& /*szItemName*/, const std::wstring& /*szDataTableName*/, int32_t /*iItemCount*/)> fnSpawnCallback;
    //extern std::function<void(const std::wstring& /*szItemName*/)> fnCullCallback;
    extern std::function<void()> fnGlobalC2CycleCallback;
    // Called when a DataTable is selected or deselected (empty string on deselect)
    extern std::function<void(const std::wstring& /*szTableName*/)> fnDataTableSelectedCallback;
    // Called when Summon button is clicked with selected class name
    extern std::function<void(const std::string& /*szClassName*/)> fnSummonCallback;

    bool Initialize(void);
    void Stop(void);
}

#endif // _GROUNDED_WINGUI_WRAPPER_HPP