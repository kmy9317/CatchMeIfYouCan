#include "Components/Items/CYItemInteractionComponent.h"
#include "Items/CYItemBase.h"
#include "Items/Traps/CYTrapBase.h"
#include "Components/Items/CYInventoryComponent.h"
#include "Character/CYPlayerCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"

UCYItemInteractionComponent::UCYItemInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UCYItemInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
	// 위젯이 있을 때만 카메라 방향으로 회전
	if (CurrentInteractionWidget && IsValid(CurrentInteractionWidget))
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			FVector WidgetLocation = CurrentInteractionWidget->GetComponentLocation();
            
			// 위젯에서 카메라를 향하는 방향 벡터 계산
			FVector LookAtDirection = (CameraLocation - WidgetLocation).GetSafeNormal();
            
			// 해당 방향으로 회전값 생성
			FRotator LookAtRotation = LookAtDirection.Rotation();
            
			// 위젯을 카메라 방향으로 회전
			CurrentInteractionWidget->SetWorldRotation(LookAtRotation);
		}
	}
}

void UCYItemInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    
	GetWorld()->GetTimerManager().SetTimer(
		ItemCheckTimer,
		this,
		&UCYItemInteractionComponent::CheckForNearbyItems,
		CheckInterval,
		true
	);
}

void UCYItemInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCYItemInteractionComponent, NearbyItem);
}

void UCYItemInteractionComponent::InteractWithNearbyItem()
{
    if (!NearbyItem) 
    {
        UE_LOG(LogTemp, Warning, TEXT("No nearby item to interact with"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Interacting with: %s"), *NearbyItem->GetName());
    ServerPickupItem(NearbyItem);
}

void UCYItemInteractionComponent::ServerPickupItem_Implementation(ACYItemBase* Item)
{
    if (!Item || !GetOwner()->HasAuthority() || Item->bIsPickedUp) return;

	ACYPlayerCharacter* Character = Cast<ACYPlayerCharacter>(GetOwner());
	if (!Character) return;
    
	// 팀 체크
	if (!Item->CanBePickedUpBy(Character))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot pickup item: Team restriction"));
		return;
	}
    
    UCYInventoryComponent* InventoryComp = GetOwner()->FindComponentByClass<UCYInventoryComponent>();
    if (!InventoryComp)
    {
        UE_LOG(LogTemp, Error, TEXT("No InventoryComponent found"));
        return;
    }
    
    // 인벤토리에 추가
    bool bAddedToInventory = InventoryComp->AddItem(Item);
    if (bAddedToInventory)
    {
        // 아이템 픽업 처리
        Item->OnPickup(Cast<ACYPlayerCharacter>(GetOwner()));
        UE_LOG(LogTemp, Warning, TEXT("Item picked up: %s"), *Item->ItemName.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to add item to inventory"));
    }
}

void UCYItemInteractionComponent::CheckForNearbyItems()
{
    if (!GetOwner()) return;

	ACYPlayerCharacter* Character = Cast<ACYPlayerCharacter>(GetOwner());
	if (!Character) return;
    
    FVector PlayerLocation = GetOwner()->GetActorLocation();
    ACYItemBase* ClosestItem = nullptr;
    float ClosestDistance = FLT_MAX;
    
    // 모든 아이템 찾기
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACYItemBase::StaticClass(), FoundActors);
    
    for (AActor* Actor : FoundActors)
    {
        ACYItemBase* Item = Cast<ACYItemBase>(Actor);
        if (!Item || Item->bIsPickedUp) continue;

    	// 팀 체크
    	if (!Item->CanBePickedUpBy(Character)) continue;
        
        // 트랩의 경우 맵에 배치된 것만 픽업 가능
        if (ACYTrapBase* Trap = Cast<ACYTrapBase>(Item))
        {
            if (Trap->TrapState != ETrapState::MapPlaced) continue;
        }
        
        float Distance = FVector::Dist(PlayerLocation, Item->GetActorLocation());
        if (Distance < InteractionRange && Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestItem = Item;
        }
    }
    
    // 서버에서 NearbyItem 업데이트
	if (GetOwner()->HasAuthority() && NearbyItem != ClosestItem)
	{
		NearbyItem = ClosestItem;
	}

	// 클라이언트 로컬용 (하이라이트)
	if (Character->IsLocallyControlled())
	{
		if (LocalNearbyItem != ClosestItem)
		{
			LocalNearbyItem = ClosestItem;
			UpdateLocalHighlight();
		}
	}
}

void UCYItemInteractionComponent::UpdateLocalHighlight()
{
	// 이전 하이라이트 제거
	if (CurrentHighlightedItem)
	{
		RemoveHighlight(CurrentHighlightedItem);
		RemoveInteractionWidget(); // UI 제거
	}
    
	// 새 하이라이트 적용
	if (LocalNearbyItem)
	{
		ApplyHighlight(LocalNearbyItem);
		CreateInteractionWidget(LocalNearbyItem); // UI 생성
	}
    
	CurrentHighlightedItem = LocalNearbyItem;
}

void UCYItemInteractionComponent::ApplyHighlight(ACYItemBase* Item)
{
	if (!Item)
	{
		return;
	}
    
	// OutlineMesh
	if (Item->OutlineMesh)
	{
		if (Item->OutlineMesh->GetStaticMesh())
		{
			Item->OutlineMesh->SetRenderCustomDepth(true);
			Item->OutlineMesh->SetCustomDepthStencilValue(255);
			return;
		}
	}
    
	// ItemMesh
	if (Item->ItemMesh)
	{
		Item->ItemMesh->SetRenderCustomDepth(true);
		Item->ItemMesh->SetCustomDepthStencilValue(255);
	}
}

void UCYItemInteractionComponent::RemoveHighlight(ACYItemBase* Item)
{
	if (!Item || !Item->ItemMesh) return;
    
	// 아웃라인 메시가 있으면 끔
	if (Item->OutlineMesh && Item->OutlineMesh->GetStaticMesh())
	{
		Item->OutlineMesh->SetRenderCustomDepth(false);
	}
	else if (Item->ItemMesh)
	{
		Item->ItemMesh->SetRenderCustomDepth(false);
	}
}

void UCYItemInteractionComponent::CreateInteractionWidget(ACYItemBase* Item)
{
    if (!Item || !InteractionWidgetClass) return;
    
    AActor* Owner = GetOwner();
    if (!Owner) return;
    
    // 기존 위젯 제거
    RemoveInteractionWidget();
    
    // 새 위젯 컴포넌트 생성
    CurrentInteractionWidget = NewObject<UWidgetComponent>(Item);
    CurrentInteractionWidget->SetWidgetClass(InteractionWidgetClass);
    CurrentInteractionWidget->SetWidgetSpace(EWidgetSpace::World);
	CurrentInteractionWidget->SetDrawSize(FVector2D(400.f, 200.f));
	CurrentInteractionWidget->SetPivot(FVector2D(0.5f, 1.0f)); 
    CurrentInteractionWidget->SetVisibility(true);
	CurrentInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CurrentInteractionWidget->RegisterComponent();
    
    // 아이템에 부착
    CurrentInteractionWidget->AttachToComponent(
        Item->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform
    );
    CurrentInteractionWidget->SetRelativeLocation(WidgetOffset);
    
    // 위젯에 아이템 정보 설정
    if (UUserWidget* Widget = CurrentInteractionWidget->GetWidget())
    {
        // 블루프린트에서 "SetItemInfo" 함수 구현 필요
        UFunction* SetItemInfoFunc = Widget->FindFunction(FName("SetItemInfo"));
        if (SetItemInfoFunc)
        {
            struct FSetItemInfoParams
            {
                FText ItemName;
                FText ItemDescription;
            };
            
            FSetItemInfoParams Params;
            Params.ItemName = Item->ItemNameKR;
            Params.ItemDescription = Item->ItemDescriptionKR;
            
            Widget->ProcessEvent(SetItemInfoFunc, &Params);
        }
    }
    
	UE_LOG(LogTemp, Warning, TEXT("Created interaction UI for: %s at location: %s"), 
		   *Item->ItemName.ToString(), *CurrentInteractionWidget->GetComponentLocation().ToString());
}

void UCYItemInteractionComponent::RemoveInteractionWidget()
{
    if (CurrentInteractionWidget)
    {
        CurrentInteractionWidget->DestroyComponent();
        CurrentInteractionWidget = nullptr;
    }
}

void UCYItemInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInteractionWidget();
    
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ItemCheckTimer);
    }
    
    Super::EndPlay(EndPlayReason);
}