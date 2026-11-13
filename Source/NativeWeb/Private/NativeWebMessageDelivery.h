#pragma once

#include "CoreMinimal.h"

/** Delivers messages only while the owner remains usable before and after each callback. */
template <typename TMessage, typename TIsAlive, typename TCallback>
bool NativeWebDeliverMessagesWhileAlive(const TArray<TMessage>& Messages, TIsAlive&& IsAlive,
	                                    TCallback&& Callback)
{
	for (const TMessage& Message : Messages)
	{
		if (!IsAlive()) { return false; }
		Callback(Message);
		if (!IsAlive()) { return false; }
	}
	return true;
}
