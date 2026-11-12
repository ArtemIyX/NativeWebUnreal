#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dispatch/NativeWebDispatchHandlerRegistry.h"
#include "Dispatch/NativeWebDispatchRouteKey.h"

namespace
{
	struct FTestRoute
	{
		uint8 Group = 0;
		FName Name;

		bool operator==(const FTestRoute&) const = default;
	};

	uint32 GetTypeHash(const FTestRoute& InRoute)
	{
		return HashCombine(::GetTypeHash(InRoute.Group), ::GetTypeHash(InRoute.Name.GetComparisonIndex().ToUnstableInt()));
	}

	struct FOtherRoute
	{
		uint32 Value = 0;
		bool operator==(const FOtherRoute&) const = default;
	};

	uint32 GetTypeHash(const FOtherRoute& InRoute)
	{
		return ::GetTypeHash(InRoute.Value);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebDispatchRouteKeyPrimitiveTest, "NativeWebDispatch.RouteKey.Primitive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNativeWebDispatchRouteKeyPrimitiveTest::RunTest(const FString& Parameters)
{
	const FNativeWebDispatchRouteKey scalar = MakeNativeWebDispatchRouteKey(7u);
	const FNativeWebDispatchRouteKey sameScalar = MakeNativeWebDispatchRouteKey(7u);
	const FNativeWebDispatchRouteKey otherScalar = MakeNativeWebDispatchRouteKey(8u);
	const FNativeWebDispatchRouteKey route = MakeNativeWebDispatchRouteKey(FTestRoute{2, TEXT("B_1_1")});
	const FNativeWebDispatchRouteKey sameRoute = MakeNativeWebDispatchRouteKey(FTestRoute{2, TEXT("B_1_1")});
	const FNativeWebDispatchRouteKey differentRoute = MakeNativeWebDispatchRouteKey(FTestRoute{2, TEXT("B_1_2")});
	const FNativeWebDispatchRouteKey differentType = MakeNativeWebDispatchRouteKey(FOtherRoute{7});

	TestTrue(TEXT("Scalar key is valid"), scalar.IsValid());
	TestTrue(TEXT("Equal scalar keys compare equal"), scalar == sameScalar);
	TestEqual(TEXT("Equal scalar keys have equal hashes"), GetTypeHash(scalar), GetTypeHash(sameScalar));
	TestFalse(TEXT("Different scalar values do not compare equal"), scalar == otherScalar);
	TestTrue(TEXT("Equal composite keys compare equal"), route == sameRoute);
	TestFalse(TEXT("Different composite fields do not compare equal"), route == differentRoute);
	TestFalse(TEXT("Different key types do not compare equal"), scalar == differentType);
	TestEqual(TEXT("Composite key can be inspected"), route.TryGet<FTestRoute>()->Name, FName(TEXT("B_1_1")));
	TestNull(TEXT("Wrong key type is rejected"), route.TryGet<FOtherRoute>());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebDispatchRouteKeyLifetimeTest, "NativeWebDispatch.RouteKey.CopyLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNativeWebDispatchRouteKeyLifetimeTest::RunTest(const FString& Parameters)
{
	FNativeWebDispatchRouteKey copiedKey;
	{
		FTestRoute source{3, TEXT("temporary")};
		copiedKey = MakeNativeWebDispatchRouteKey(source);
		source.Name = TEXT("changed");
	}

	const FTestRoute* retainedRoute = copiedKey.TryGet<FTestRoute>();
	TestNotNull(TEXT("Copied key retains owned value"), retainedRoute);
	if (retainedRoute)
	{
		TestEqual(TEXT("Copied key is independent of source"), retainedRoute->Name, FName(TEXT("temporary")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebDispatchRegistryRoutingTest, "NativeWebDispatch.Registry.Routing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNativeWebDispatchRegistryRoutingTest::RunTest(const FString& Parameters)
{
	FNativeWebDispatchRegistry registry;
	const FTestRoute routeValue{1, TEXT("A_1")};
	int32 factoryCalls = 0;
	const bool registered = registry.RegisterFactory(routeValue,
		[&factoryCalls](const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload, FString& OutError)
		{
			++factoryCalls;
			return MakeNativeWebDispatchValue(InRouteKey.TryGet<FTestRoute>()->Name);
		});

	TestTrue(TEXT("Composite factory registers"), registered);
	TestTrue(TEXT("Composite factory is found"), registry.HasFactory(FTestRoute{1, TEXT("A_1")}));
	TestFalse(TEXT("Duplicate factory is rejected"), registry.RegisterFactory(routeValue, FNativeWebDispatchRegistry::FNativeWebDispatchFactory()));

	TUniquePtr<INativeWebDispatchValue> value;
	FString error;
	TestEqual(TEXT("Factory decodes route"), registry.Decode(routeValue, {}, value, error), ENativeWebDispatchFactoryResult::Ok);
	TestEqual(TEXT("Factory receives complete route"), factoryCalls, 1);
	TestEqual(TEXT("Factory returns expected value"), NativeWebDispatchTryGetValue<FName>(*value)->GetValue(), FName(TEXT("A_1")));
	TestFalse(TEXT("Default key is not found"), registry.HasFactory(FNativeWebDispatchRouteKey()));
	TestEqual(TEXT("Invalid key is rejected by decode"), registry.Decode(FNativeWebDispatchRouteKey(), {}, value, error), ENativeWebDispatchFactoryResult::InvalidRouteKey);
	TestTrue(TEXT("Factory unregisters"), registry.UnregisterFactory(routeValue));
	TestEqual(TEXT("Factory count is zero"), registry.GetFactoryCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebDispatchHandlerRoutingTest, "NativeWebDispatch.Handler.Routing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNativeWebDispatchHandlerRoutingTest::RunTest(const FString& Parameters)
{
	FNativeWebDispatchHandlerRegistry handlers;
	const FTestRoute routeValue{4, TEXT("B_1_1")};
	bool reentered = false;
	TestTrue(TEXT("Typed handler registers"), handlers.RegisterTypedHandler<FString>(routeValue,
		[&handlers, &reentered](const FNativeWebDispatchRouteKey& InRouteKey, const FString& InValue)
		{
			reentered = handlers.RegisterTypedHandler<FString>(FTestRoute{4, TEXT("reentry")}, [](const FString&) {});
			return InRouteKey.TryGet<FTestRoute>() && InValue == TEXT("payload");
		}));

	const TUniquePtr<INativeWebDispatchValue> value = MakeNativeWebDispatchValue(FString(TEXT("payload")));
	FString error;
	TestEqual(TEXT("Handler receives route and value"), handlers.Handle(routeValue, *value, error), ENativeWebDispatchHandlerResult::Ok);
	TestTrue(TEXT("Handler may reenter registry after lookup"), reentered);
	TestEqual(TEXT("Wrong payload reports type mismatch"), handlers.Handle(routeValue, *MakeNativeWebDispatchValue(7u), error), ENativeWebDispatchHandlerResult::HandlerTypeMismatch);
	TestEqual(TEXT("Invalid handler key is rejected"), handlers.Handle(FNativeWebDispatchRouteKey(), *value, error), ENativeWebDispatchHandlerResult::InvalidRouteKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebDispatchMetadataRoutingTest, "NativeWebDispatch.Handler.Metadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNativeWebDispatchMetadataRoutingTest::RunTest(const FString& Parameters)
{
	FNativeWebDispatchHandlerRegistry handlers;
	const FTestRoute routeValue{5, TEXT("metadata")};
	const FTestRoute legacyRoute{5, TEXT("legacy")};
	FString receivedGuid;

	TestTrue(TEXT("Metadata handler registers"), handlers.RegisterTypedHandler<FString>(routeValue,
		[&receivedGuid](const FString& InValue, const INativeWebDispatchMetadata* InMetadata)
		{
			const FString* guid = NativeWebDispatchTryGetMetadata<FString>(InMetadata);
			if (guid)
			{
				receivedGuid = *guid;
			}
			return guid && InValue == TEXT("payload");
		}));
	TestTrue(TEXT("Legacy handler registers"), handlers.RegisterTypedHandler<FString>(legacyRoute,
		[](const FString& InValue)
		{
			return InValue == TEXT("payload");
		}));

	const TUniquePtr<INativeWebDispatchValue> value = MakeNativeWebDispatchValue(FString(TEXT("payload")));
	const TSharedPtr<const INativeWebDispatchMetadata> metadata = MakeNativeWebDispatchMetadata(FString(TEXT("test-guid")));
	FString error;

	TestEqual(TEXT("Metadata handler receives metadata"), handlers.Handle(routeValue, *value, metadata, error), ENativeWebDispatchHandlerResult::Ok);
	TestEqual(TEXT("Metadata value is preserved"), receivedGuid, FString(TEXT("test-guid")));
	TestEqual(TEXT("Legacy handler call without metadata succeeds"), handlers.Handle(legacyRoute, *value, error), ENativeWebDispatchHandlerResult::Ok);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
