// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.


#include "Utils/ManagerComponent.h"

DEFINE_LOG_CATEGORY(LogManagerComponent);

void UManagerComponent::Register(TSubclassOf<UObject> Class, UObject* Object)
{
	check(Object);

	if (RegisteredMap.Contains(Class))
	{
		UE_LOG(LogManagerComponent, Log, TEXT("Class %s is already registered with the %s."), *Class->GetName(), *GetName());
		return;
	}

	RegisteredMap.Add(Class, Object);

	if (auto Callbacks = RegistrationCallbacks.Find(Class))
	{
		for (const FObjectRegisteredInManager& Callback : *Callbacks)
		{
			Callback.ExecuteIfBound(Object);
		}

		RegistrationCallbacks.Remove(Class);
	}
}

UObject* UManagerComponent::Resolve(TSubclassOf<UObject> Class) const
{
	UObject* Object = RegisteredMap.FindRef(Class);
	if (!Object)
	{
		return nullptr;
	}

	return Object;
}

void UManagerComponent::WaitForRegistrationAsync(TSubclassOf<UObject> Class, const FObjectRegisteredInManager& Callback)
{
	if (UObject* Object = Resolve(Class))
	{
		Callback.ExecuteIfBound(Object);
		return;
	}

	RegistrationCallbacks.FindOrAdd(Class).Add(Callback);
}
