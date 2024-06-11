// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.


#include "VisibleComponent.h"

#include "FogOfWar.h"
#include "Utils/ManagerComponent.h"
#include "Utils/ManagerStatics.h"

UVisibleComponent::UVisibleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Preventing tick until FogOfWar is registered.
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVisibleComponent::BeginPlay()
{
	Super::BeginPlay();

	auto GameManager = UManagerStatics::GetGameManager(this);
	GameManager->WaitForRegistrationAsync<AFogOfWar>(FObjectRegisteredInManager::CreateWeakLambda(this,
		[this](UObject* Object)
		{
			FogOfWar = Cast<AFogOfWar>(Object);

			UpdateVisibility(true);
			PrimaryComponentTick.SetTickFunctionEnable(true);
		}));
}

void UVisibleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateVisibility();
}

void UVisibleComponent::UpdateVisibility(bool bForceChanged)
{
	SetIsVisible(FogOfWar->IsLocationVisible(GetOwner()->GetActorLocation()), bForceChanged);
}

void UVisibleComponent::SetIsVisible(bool bNewIsVisible, bool bForceChanged)
{
	bool bChanged = bForceChanged || bIsVisible != bNewIsVisible;
	bIsVisible = bNewIsVisible;

	if (bChanged)
	{
		if (bManageOwnerVisibility)
		{
			GetOwner()->GetRootComponent()->SetVisibility(bIsVisible, true);
		}

		OnVisibilityChanged.Broadcast(bIsVisible);
	}
}
