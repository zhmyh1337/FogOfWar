// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFogOfWar, Log, All)

class UPostProcessComponent;
class UVisionComponent;

UCLASS(BlueprintType, Blueprintable)
class FOGOFWAR_API AFogOfWar : public AActor
{
	GENERATED_BODY()

public:
	AFogOfWar();

public:
	void RegisterVisionComponent(UVisionComponent* VisionComponent);

	void UnregisterVisionComponent(UVisionComponent* VisionComponent);

	UFUNCTION(BlueprintCallable)
	bool IsLocationVisible(FVector WorldLocation);

	UFUNCTION(BlueprintPure)
	UTexture* GetFinalVisibilityTexture();

	UFUNCTION(BlueprintCallable)
	void SetCommonMIDParameters(UMaterialInstanceDynamic* MID);

	UFUNCTION(BlueprintCallable)
	void Activate();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<ECollisionChannel> HeightScanCollisionChannel = ECC_Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UPostProcessComponent* PostProcess;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAutoActivate = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly)
	AVolume* GridVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float TileSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float VisionBlockingDeltaHeightThreshold = 200.0f;

	// The more the value, the less the impact of the new snapshot on the "history" will be and the smoother the transition will be.
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float ApproximateSecondsToAbsorbNewSnapshot = 0.1f;

	// All pixels with visibility less than this value will be zeroed out.
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.0f, UIMin = 0.0f, ClampMax = 1.0f, UIMax = 1.0f))
	float MinimalVisibility = 0.1f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.0f, UIMin = 0.0f, ClampMax = 1.0f, UIMax = 1.0f))
	float NotVisibleRegionBrightness = 0.1f;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Materials")
	UMaterialInterface* InterpolationMaterial;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Materials")
	UMaterialInterface* AfterInterpolationMaterial;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Materials")
	UMaterialInterface* SuperSamplingMaterial;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Materials")
	UMaterialInterface* PostProcessingMaterial;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "FogOfWar|Debug")
	bool bDebugStressTestIgnoreCache = false;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Debug")
	bool bDebugFilterNearest = false;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Debug")
	float DebugHeightmapLowestZ = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "FogOfWar|Debug")
	float DebugHeightmapHightestZ = 1000.0f;
#endif

protected:
	struct FTile
	{
		float Height;

		int VisibilityCounter = 0;
	};

	// some data for every vision unit, i.e. VisionComponent
	// for now we cache tiles states in the local area of the unit not to update them when the vision unit is not moving
	struct FVisionUnitData
	{
		enum class TileState : uint8
		{
			Unknown,
			NotVisible,
			Visible
		};

		const int LocalAreaTilesResolution;

		const float GridSpaceRadius;

		FIntVector2 LocalAreaCachedMinIJ;

		// I'm pretty sure this array can be made global and shared between all vision units. Needs some thinking
		TArray<TileState> LocalAreaTilesCachedStates;

		int CachedOriginGlobalIndex;

		bool bHasCachedData = false;

		FORCEINLINE_DEBUGGABLE bool HasCachedData() const { return bHasCachedData; }

		FORCEINLINE_DEBUGGABLE int GetLocalIndex(FIntVector2 IJ) const { return IJ.X * LocalAreaTilesResolution + IJ.Y; }

		FORCEINLINE_DEBUGGABLE FIntVector2 GetLocalIJ(int LocalIndex) { return { LocalIndex / LocalAreaTilesResolution, LocalIndex % LocalAreaTilesResolution }; }

		FORCEINLINE_DEBUGGABLE bool IsLocalIJValid(FIntVector2 IJ) { return (IJ.X >= 0) & (IJ.Y >= 0) & (IJ.X < LocalAreaTilesResolution) & (IJ.Y < LocalAreaTilesResolution); }

		FORCEINLINE_DEBUGGABLE TileState& GetLocalTileState(int LocalIndex) { return LocalAreaTilesCachedStates[LocalIndex]; }

		FORCEINLINE_DEBUGGABLE TileState& GetLocalTileState(FIntVector2 IJ) { checkSlow(IsLocalIJValid(IJ)); return GetLocalTileState(GetLocalIndex(IJ)); }

		FORCEINLINE_DEBUGGABLE FIntVector2 LocalToGlobal(FIntVector2 LocalIJ) const { return LocalAreaCachedMinIJ + LocalIJ; }

		FORCEINLINE_DEBUGGABLE FIntVector2 GlobalToLocal(FIntVector2 GlobalIJ) const { return GlobalIJ - LocalAreaCachedMinIJ; }
	};

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "FogOfWar", DisplayName = "RefreshVolume")
	void RefreshVolumeInEditor();

	virtual bool CanEditChange(const FProperty* InProperty) const override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void Tick(float DeltaSeconds) override;

protected:
	void Initialize();

	void ResetCachedVisibilities(FVisionUnitData& VisionUnitData);

	void UpdateVisibilities(const FVector3d& OriginWorldLocation, FVisionUnitData& VisionUnitData);

	void CalculateTileHeight(FTile& Tile, FIntVector2 TileIJ);

	FVisionUnitData CreateVisionUnitDataFromVisionComponent(UVisionComponent* VisionComponent);

	UTexture2D* CreateSnapshotTexture();

	UTextureRenderTarget2D* CreateRenderTarget();

#if WITH_EDITORONLY_DATA
	void WriteHeightmapDataToTexture(UTexture2D* Texture);
#endif

	void WriteVisionDataToTexture(UTexture2D* Texture);

	FORCEINLINE_DEBUGGABLE int GetGlobalIndex(FIntVector2 IJ) const { return IJ.X * GridResolution.Y + IJ.Y; }

	FORCEINLINE_DEBUGGABLE FIntVector2 GetTileIJ(int GlobalIndex) { return { GlobalIndex / GridResolution.Y, GlobalIndex % GridResolution.Y }; }

	FORCEINLINE_DEBUGGABLE FTile& GetGlobalTile(int GlobalIndex) { return Tiles[GlobalIndex]; }

	FORCEINLINE_DEBUGGABLE FTile& GetGlobalTile(FIntVector2 IJ) { checkSlow(IsGlobalIJValid(IJ)); return GetGlobalTile(GetGlobalIndex(IJ)); }

	FORCEINLINE_DEBUGGABLE bool IsGlobalIJValid(FIntVector2 IJ) { return (IJ.X >= 0) & (IJ.Y >= 0) & (IJ.X < GridResolution.X) & (IJ.Y < GridResolution.Y); }

	FORCEINLINE_DEBUGGABLE FVector2f ConvertWorldSpaceLocationToGridSpace(const FVector2D& WorldLocation);

	FORCEINLINE_DEBUGGABLE FVector2D ConvertTileIJToTileCenterWorldLocation(const FIntVector2& IJ);

	FORCEINLINE_DEBUGGABLE FIntVector2 ConvertGridLocationToTileIJ(const FVector2f& GridLocation);

	FORCEINLINE_DEBUGGABLE FIntVector2 ConvertWorldLocationToTileIJ(const FVector2D& WorldLocation);

	FORCEINLINE_DEBUGGABLE bool IsBlockingVision(float ObserverHeight, float PotentialObstacleHeight);

	FORCEINLINE_DEBUGGABLE void ExecuteDDAVisibilityCheck(float ObserverHeight, FIntVector2 LocalIJ, FIntVector2 OriginLocalIJ, FVisionUnitData& VisionUnitData);

protected:
	UPROPERTY(VisibleInstanceOnly)
	FVector2D GridSize = FVector2D::Zero();

	UPROPERTY(VisibleInstanceOnly)
	FIntVector2 GridResolution = {};

	UPROPERTY(VisibleInstanceOnly)
	FVector2D GridBottomLeftWorldLocation = FVector2D::Zero();

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleInstanceOnly, Category = "FogOfWar|Textures")
	UTexture2D* HeightmapTexture = nullptr;
#endif

	UPROPERTY(VisibleInstanceOnly, Category = "FogOfWar|Textures")
	UTexture2D* SnapshotTexture = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "FogOfWar|Textures")
	UTextureRenderTarget2D* VisibilityTextureRenderTarget = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "FogOfWar|Textures")
	UTextureRenderTarget2D* PreFinalVisibilityTextureRenderTarget = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "FogOfWar|Textures")
	UTextureRenderTarget2D* FinalVisibilityTextureRenderTarget = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* InterpolationMID;

	UPROPERTY()
	UMaterialInstanceDynamic* AfterInterpolationMID;

	UPROPERTY()
	UMaterialInstanceDynamic* SuperSamplingMID;

	UPROPERTY()
	UMaterialInstanceDynamic* PostProcessingMID;

	TArray<FTile> Tiles;

	TArray<uint8> TextureDataBuffer;

	TMap<UVisionComponent*, FVisionUnitData> RegisteredVisions;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleInstanceOnly)
	int RegisteredVisionsNum = 0;

	UPROPERTY(VisibleInstanceOnly)
	int64 TotalRegisteredVisionsCacheTilesNum = 0;
#endif

	// this is to avoid recursion overhead and this is not a local variable to avoid allocations overhead
	TArray<int> DDALocalIndexesStack;

	bool bFirstTick = true;

	bool bActivated = false;
};
