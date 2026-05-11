// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_ItemDataRegistrySubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

// --- Lifecycle ---

void UBG_ItemDataRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadRegistry();
}

void UBG_ItemDataRegistrySubsystem::Deinitialize()
{
	CachedRegistry = nullptr;
	Super::Deinitialize();
}

// --- Data Access ---

bool UBG_ItemDataRegistrySubsystem::LoadRegistry()
{
	if (!CachedRegistry)
	{
		CachedRegistry = LoadRegistryFromAssetManager();
		if (!CachedRegistry)
		{
			LOGE(TEXT("UBG_ItemDataRegistrySubsystem failed to load item data registry."));
			return false;
		}
	}

	if (!CachedRegistry->ValidateRegistry())
	{
		LOGE(TEXT("Item data registry %s loaded but validation failed."),
			*GetNameSafe(CachedRegistry));
		return false;
	}

	LOGD(TEXT("Item data registry %s loaded successfully."), *GetNameSafe(CachedRegistry));
	return true;
}

UBG_ItemDataRegistry* UBG_ItemDataRegistrySubsystem::GetRegistry() const
{
	return CachedRegistry.Get();
}

bool UBG_ItemDataRegistrySubsystem::IsRegistryLoaded() const
{
	return CachedRegistry != nullptr;
}

bool UBG_ItemDataRegistrySubsystem::ValidateRegistry() const
{
	if (!CachedRegistry)
	{
		LOGE(TEXT("UBG_ItemDataRegistrySubsystem cannot validate because CachedRegistry is null."));
		return false;
	}

	return CachedRegistry->ValidateRegistry();
}

bool UBG_ItemDataRegistrySubsystem::ValidateWeaponFireSpecs() const
{
	if (!CachedRegistry)
	{
		LOGE(TEXT("UBG_ItemDataRegistrySubsystem cannot validate weapon fire specs because CachedRegistry is null."));
		return false;
	}

	return CachedRegistry->ValidateWeaponFireSpecs();
}

// --- Item Data Queries ---

bool UBG_ItemDataRegistrySubsystem::HasValidItemRow(EBG_ItemType ItemType, FGameplayTag ItemTag)
{
	return FindItemRow(ItemType, ItemTag) != nullptr;
}

bool UBG_ItemDataRegistrySubsystem::HasValidItemRowByName(EBG_ItemType ItemType, FName ItemTagName)
{
	UBG_ItemDataRegistry* Registry = GetLoadedRegistry();
	return Registry ? Registry->HasValidItemRowByName(ItemType, ItemTagName) : false;
}

const FBG_ItemDataRow* UBG_ItemDataRegistrySubsystem::FindItemRow(
	EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	FString* OutFailureReason)
{
	const UBG_ItemDataRegistry* Registry = GetLoadedRegistry(OutFailureReason);
	return Registry ? Registry->FindItemRow(ItemType, ItemTag, OutFailureReason) : nullptr;
}

bool UBG_ItemDataRegistrySubsystem::HasValidWeaponFireSpecRow(FGameplayTag WeaponItemTag)
{
	return FindWeaponFireSpecRow(WeaponItemTag) != nullptr;
}

const FBG_WeaponFireSpecRow* UBG_ItemDataRegistrySubsystem::FindWeaponFireSpecRow(
	const FGameplayTag& WeaponItemTag,
	FString* OutFailureReason)
{
	const UBG_ItemDataRegistry* Registry = GetLoadedRegistry(OutFailureReason);
	return Registry ? Registry->FindWeaponFireSpecRow(WeaponItemTag, OutFailureReason) : nullptr;
}

// --- Internal Helpers ---

UBG_ItemDataRegistry* UBG_ItemDataRegistrySubsystem::GetLoadedRegistry(FString* OutFailureReason)
{
	if (!CachedRegistry)
	{
		// asset 작성 중에는 에디터와 PIE 시작 순서가 달라질 수 있으므로 요청 시 한 번 더 시도
		LoadRegistry();
	}

	if (!CachedRegistry)
	{
		const FString FailureReason = TEXT("UBG_ItemDataRegistrySubsystem has no loaded item data registry.");
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		// Subsystem 로드 실패는 모든 인벤토리 검증을 막으므로 조용히 지나가면 안 됨
		LOGE(TEXT("%s"), *FailureReason);
		return nullptr;
	}

	return CachedRegistry.Get();
}

UBG_ItemDataRegistry* UBG_ItemDataRegistrySubsystem::LoadRegistryFromAssetManager() const
{
	UAssetManager& AssetManager = UAssetManager::Get();

	FPrimaryAssetId RegistryId;
	if (!TryResolveRegistryPrimaryAssetId(RegistryId))
		return nullptr;

	if (UBG_ItemDataRegistry* LoadedRegistry = AssetManager.GetPrimaryAssetObject<UBG_ItemDataRegistry>(RegistryId))
	{
		// 에디터 검색이나 이전 로드로 registry가 이미 메모리에 올라와 있을 수 있음
		return LoadedRegistry;
	}

	const FSoftObjectPath RegistryPath = AssetManager.GetPrimaryAssetPath(RegistryId);
	if (!RegistryPath.IsValid())
	{
		LOGE(TEXT("AssetManager returned an invalid path for item data registry id %s."),
			*RegistryId.ToString());
		return nullptr;
	}

	UObject* LoadedObject = AssetManager.GetStreamableManager().LoadSynchronous(RegistryPath, true);
	UBG_ItemDataRegistry* LoadedRegistry = Cast<UBG_ItemDataRegistry>(LoadedObject);
	if (!LoadedRegistry)
	{
		LOGE(TEXT("AssetManager loaded %s for item data registry id %s, but it is not UBG_ItemDataRegistry."),
			*GetNameSafe(LoadedObject), *RegistryId.ToString());
		return nullptr;
	}

	return LoadedRegistry;
}

bool UBG_ItemDataRegistrySubsystem::TryResolveRegistryPrimaryAssetId(FPrimaryAssetId& OutRegistryId) const
{
	UAssetManager& AssetManager = UAssetManager::Get();
	const FPrimaryAssetId DefaultRegistryId(
		UBG_ItemDataRegistry::GetRegistryPrimaryAssetType(),
		UBG_ItemDataRegistry::GetDefaultRegistryAssetName());

	// 검색 directory 안의 다른 registry asset이 실수로 활성화되지 않도록 명시적 id 우선
	if (AssetManager.GetPrimaryAssetPath(DefaultRegistryId).IsValid())
	{
		OutRegistryId = DefaultRegistryId;
		return true;
	}

	TArray<FPrimaryAssetId> RegistryIds;
	AssetManager.GetPrimaryAssetIdList(UBG_ItemDataRegistry::GetRegistryPrimaryAssetType(), RegistryIds);
	for (const FPrimaryAssetId& RegistryId : RegistryIds)
	{
		if (RegistryId.PrimaryAssetName == UBG_ItemDataRegistry::GetDefaultRegistryAssetName())
		{
			OutRegistryId = RegistryId;
			return true;
		}
	}

	LOGE(TEXT("AssetManager could not find primary asset id %s. Found %d registry asset(s) for type %s."),
		*DefaultRegistryId.ToString(),
		RegistryIds.Num(),
		*UBG_ItemDataRegistry::GetRegistryPrimaryAssetType().ToString());
	return false;
}
