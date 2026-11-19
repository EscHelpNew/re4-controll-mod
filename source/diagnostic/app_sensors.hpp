#pragma once
#include <switch.h>
#include "re4/installer_ui.hpp"

namespace re4 {
inline bool app_gyro_required(InstallerPage page) {
    return page==InstallerPage::Diagnostics||page==InstallerPage::GyroTest;
}
struct AppSensors {
    HidSixAxisSensorHandle handles[2]{};
    bool running[2]{};
    unsigned count = 0;
    u32 style = 0;
    u32 attributes = 0;
    HidNpadIdType selected_id=HidNpadIdType_No1;
    Result result = 0;
    void stop() {
        for (unsigned i = 0; i < 2; ++i) {
            if (running[i]) hidStopSixAxisSensor(handles[i]);
            running[i] = false;
        }
        count = 0;
        style = attributes = 0;
        selected_id = HidNpadIdType_No1;
        result = 0;
        selection_valid_ = false;
    }
    bool select(const PadState& pad,HidNpadIdType id,InstallerPage page) {
        if (!app_gyro_required(page)) {
            const bool changed = selection_valid_;
            stop();
            return changed;
        }
        const auto next = padGetStyleSet(&pad);
        const auto connected = padGetAttributes(&pad) &
            (HidNpadAttribute_IsConnected | HidNpadAttribute_IsLeftConnected | HidNpadAttribute_IsRightConnected);
        if(padIsHandheld(&pad))id=HidNpadIdType_Handheld;
        if (selection_valid_ && next == style && connected == attributes && id==selected_id) return false;
        stop(); style = next; attributes = connected; selected_id=id; result = 0; selection_valid_ = true;
        HidNpadStyleTag tag;
        int wanted = 1;
        if (!padIsConnected(&pad)) return true;
        if (padIsHandheld(&pad)) { id = HidNpadIdType_Handheld; tag = HidNpadStyleTag_NpadHandheld; }
        else if (style & HidNpadStyleTag_NpadFullKey) tag = HidNpadStyleTag_NpadFullKey;
        else if (style & HidNpadStyleTag_NpadJoyDual) { tag = HidNpadStyleTag_NpadJoyDual; wanted = 2; }
        else { result = MAKERESULT(Module_Libnx, LibnxError_NotFound); return true; }
        result = hidGetSixAxisSensorHandles(handles, wanted, id, tag);
        if (R_FAILED(result)) return true;
        for (int i = 0; i < wanted; ++i) {
            const auto started = hidStartSixAxisSensor(handles[i]);
            if (R_FAILED(started)) { result = started; continue; }
            running[i] = true;
            ++count;
        }
        return true;
    }
    size_t read(HidSixAxisSensorState* samples) {
        if (!count) return 0;
        // Prefer the right Joy-Con for split controllers, then the left.
        unsigned index = (style & HidNpadStyleTag_NpadJoyDual)
            && (attributes & HidNpadAttribute_IsRightConnected) && running[1] ? 1 : 0;
        if (!running[index]) index ^= 1;
        return hidGetSixAxisSensorStates(handles[index], samples, 16);
    }
private:
    bool selection_valid_ = false;
};
}
