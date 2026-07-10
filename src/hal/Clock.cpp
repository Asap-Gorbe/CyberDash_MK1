#include "Clock.h"
#include "app/Config.h"
#include <time.h>

namespace Clock {

    void begin() {
        configTzTime(Config::Timezone, "pool.ntp.org", "time.nist.gov");
    }

    bool isSynced() {
        time_t now = time(nullptr);
        return now > 1700000000;  // sometime in 2023 — well past any un-synced default
    }

    String nowFormatted() {
        if (!isSynced()) return "--:--";

        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        char buf[6];  // "HH:MM\0"
        snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        return String(buf);
    }

}  // namespace Clock