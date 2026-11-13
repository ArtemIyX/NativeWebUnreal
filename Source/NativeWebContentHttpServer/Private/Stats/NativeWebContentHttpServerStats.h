/**
 * @file NativeWebContentHttpServer/Private/Stats/NativeWebContentHttpServerStats.h
 * @brief Runtime stats declarations for content HTTP server module.
 */
#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("NativeWebContentHttpServer"), STATGROUP_NativeWebContentHttpServer, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Cached Images"), STAT_NativeWebContentHttpServer_CachedImages, STATGROUP_NativeWebContentHttpServer);
