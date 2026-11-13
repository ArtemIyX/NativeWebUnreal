/**
 * @file Source/NativeWeb/Public/NativeWeb.h
 * @brief Declares native web for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"

// Public categories are exported for game modules that consume the NativeWeb API.
/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWeb Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWeb, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWeb, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebSubsystem Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebSubsystem, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebSubsystem, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebView Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebView, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebView, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebAssets Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebAssets, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebAssets, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebScript Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebScript, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebScript, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebMessaging Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebMessaging, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebMessaging, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebInput Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebInput, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebInput, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebViewport Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebViewport, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebViewport, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebAffineUi Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebAffineUi, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebAffineUi, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebRender Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebRender, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebRender, Log, All);

/**
 * @brief Performs the declare log category extern operation.
* @param LogNativeWebTick Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebTick, Log, All);
 * @endcode
 */

NATIVEWEB_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebTick, Log, All);
