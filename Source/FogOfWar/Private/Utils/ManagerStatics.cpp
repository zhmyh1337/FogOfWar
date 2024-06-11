// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.


#include "Utils/ManagerStatics.h"

#include "Utils/ManagerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

UManagerComponent* UManagerStatics::GetGameManager(const UObject* WorldContextObject)
{
	auto GS = UGameplayStatics::GetGameState(WorldContextObject);
	UManagerComponent* Manager = GS->GetComponentByClass<UManagerComponent>();
	checkf(Manager, TEXT("Manager not found. Add ManagerComponent to the GameState"));
	return Manager;
}
