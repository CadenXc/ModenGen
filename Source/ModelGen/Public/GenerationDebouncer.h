// Copyright (c) 2024. All rights reserved.

/**
 * @file GenerationDebouncer.h
 * @brief 生成调用的通用防抖与参数变更检测（纯头文件，无状态静态工具）
 */

#pragma once

#include "CoreMinimal.h"

namespace ModelGen
{
namespace Debounce
{

// 简单的时间间隔防抖（调用方可存一个 static 上次时间戳）
inline bool ShouldSkipByInterval(double& InOutLastTimeSeconds,
                                 double MinIntervalSeconds,
                                 bool bDisableDebounce)
{
    if (bDisableDebounce)
    {
        InOutLastTimeSeconds = FPlatformTime::Seconds();
        return false;
    }

    const double Now = FPlatformTime::Seconds();
    if (Now - InOutLastTimeSeconds < MinIntervalSeconds)
    {
        return true;
    }
    InOutLastTimeSeconds = Now;
    return false;
}

// 参数缓存比较工具：当 Equal(Last, Current) 为真且非首次时可跳过
template <typename TParams>
inline bool IsSameAsLastAndUpdate(TParams& InOutLast,
                                  const TParams& Current,
                                  bool& InOutIsFirst)
{
    if (!InOutIsFirst && InOutLast == Current)
    {
        return true; // 相同，且不是首次
    }
    InOutLast = Current;
    InOutIsFirst = false;
    return false;
}

} // namespace Debounce
} // namespace ModelGen

