#pragma once

#define APP_TITLE       "rawBit — Native Win11 BitTorrent"
#define APP_COMPANY     "SynoSoftware"
#define APP_VERSION     "1.0.0"
#define APP_DESCRIPTION "Built Old School, Out of Curiosity. With Maximum Effort for Maximum Speed."
#define APP_COPYRIGHT   "Copyright (C) 2025 SynoSoftware. All rights reserved."

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

#define APP_TITLE_W       WIDEN(APP_TITLE)
#define APP_COMPANY_W     WIDEN(APP_COMPANY)
#define APP_VERSION_W     WIDEN(APP_VERSION)
#define APP_DESCRIPTION_W WIDEN(APP_DESCRIPTION)
#define APP_COPYRIGHT_W   WIDEN(APP_COPYRIGHT)
