#include "CYPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "AbilitySystem/CYAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CYVitalSet.h"
#include "AbilitySystem/CYCombatGameplayTags.h"
#include "Components/CYCharacterMovementComponent.h"
#include "Components/Items/CYInventoryComponent.h"
#include "Components/Items/CYItemInteractionComponent.h"
#include "Components/Items/CYWeaponComponent.h"
#include "Input/CYInputComponent.h"
#include "Input/CYInputGameplayTags.h"
#include "Player/CYPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Items/CYItemBase.h"
#include "Items/CYWeaponBase.h"

ACYPlayerCharacter::ACYPlayerCharacter(const FObjectInitializer& ObjectInitializer) 
	:	Super(ObjectInitializer.SetDefaultSubobjectClass<UCYCharacterMovementComponent>(CharacterMovementComponentName))
{
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(FName("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
}

void ACYPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	APlayerController* OwningPlayerController = GetController<APlayerController>();
	if (OwningPlayerController && OwningPlayerController->GetLocalPlayer())
	{
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem = OwningPlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (InputSubsystem)
		{
			InputSubsystem->RemoveMappingContext(DefaultMappingContext);
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ACYPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	SetupAbilitySystemComponent();
	
	// 서버에서만 어빌리티 세트를 초기화 시도
	TryInitializeAbilitySetsWithPawnData();

	UE_LOG(LogTemp, Warning, TEXT("PossessedBy called for %s - registering invisibility event"), *GetName());
	RegisterInvisibilityTagEvent();
}

void ACYPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACYPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 델리게이트 해제
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (InvisibilityTagDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(CYGameplayTags::State_Invisible, EGameplayTagEventType::NewOrRemoved)
				.Remove(InvisibilityTagDelegateHandle);
			InvisibilityTagDelegateHandle.Reset();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ACYPlayerCharacter::SetupAbilitySystemComponent()
{
	ACYPlayerState* PS = GetPlayerState<ACYPlayerState>();
	if (!PS)
	{
		return;
	}
    
	// ASC만 설정 (AbilitySet 초기화는 별도)
	CYAbilitySystemComponent = Cast<UCYAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (CYAbilitySystemComponent.IsValid())
	{
		CYAbilitySystemComponent->InitAbilityActorInfo(PS, this);
	}
}

void ACYPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	UCYInputComponent* CYInputComponent = Cast<UCYInputComponent>(PlayerInputComponent);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ACYPlayerCharacter::Input_Move, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACYPlayerCharacter::Input_Look, false);

	TArray<uint32> BindHandles;
	CYInputComponent->BindAbilityActions(DefaultInputConfig, this, &ThisClass::Input_AbilityInputTagStarted, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);

	// 아이템 상호작용 입력
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_Interact, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_Interact, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_Attack, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_Attack, false);
    
	// 인벤토리 슬롯 입력 (1~9번 키)
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot1, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot1, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot2, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot2, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot3, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot3, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot4, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot4, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot5, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot5, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot6, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot6, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot7, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot7, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot8, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot8, false);
	CYInputComponent->BindNativeAction(DefaultInputConfig, CYGameplayTags::InputTag_UseSlot9, ETriggerEvent::Started, this, &ACYPlayerCharacter::Input_UseSlot9, false);
	
}

void ACYPlayerCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	if (AController* LocalController = GetController())
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();
		const FRotator MovementRotation(0.0f, LocalController->GetControlRotation().Yaw, 0.0f);

		if (Value.X != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
			AddMovementInput(MovementDirection, Value.X);
		}

		if (Value.Y != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			AddMovementInput(MovementDirection, Value.Y);
		}
	}
}

void ACYPlayerCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		AddControllerPitchInput(Value.Y);
	}
}

void ACYPlayerCharacter::Input_AbilityInputTagStarted(FGameplayTag InputTag)
{
	if (!CYAbilitySystemComponent.IsValid())
	{
		return;
	}
	
	CYAbilitySystemComponent->AbilityInputTagStarted(InputTag);
}

void ACYPlayerCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (!CYAbilitySystemComponent.IsValid())
	{
		return;
	}

	CYAbilitySystemComponent->AbilityInputTagPressed(InputTag);
}

void ACYPlayerCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!CYAbilitySystemComponent.IsValid())
	{
		return;
	}

	CYAbilitySystemComponent->AbilityInputTagReleased(InputTag);
}

void ACYPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	SetupAbilitySystemComponent();

	UE_LOG(LogTemp, Warning, TEXT("OnRep_PlayerState for %s - registering invisibility event"), *GetName());
	RegisterInvisibilityTagEvent();
}

// 아이템 상호작용 입력
void ACYPlayerCharacter::Input_Interact(const FInputActionValue& InputActionValue)
{
    if (ItemInteractionComponent)
    {
        ItemInteractionComponent->InteractWithNearbyItem();
    }
}

void ACYPlayerCharacter::Input_Attack(const FInputActionValue& InputActionValue)
{
	// 현재 들고 있는 아이템이 있으면 아이템 사용
	if (InventoryComponent && InventoryComponent->CurrentHeldItem)
	{
		if (InventoryComponent->CurrentHeldItem->ItemCount <= 0)
		{
			return;
		}
		
		FText ItemName = InventoryComponent->CurrentHeldItem->ItemName;
		bool bUsedItem = InventoryComponent->UseHeldItem();
		
		if (bUsedItem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Used held item: %s"), *ItemName.ToString());
		}
		return;
	}
	
	// 무기가 장착되어 있으면 무기 공격 GA 활성화
	if (WeaponComponent && WeaponComponent->CurrentWeapon)
	{
		if (UCYAbilitySystemComponent* ASC = Cast<UCYAbilitySystemComponent>(GetAbilitySystemComponent()))
		{
			bool bActivated = ASC->TryActivateAbilityByTag(CYGameplayTags::Ability_Combat_WeaponAttack);
			if (bActivated)
			{
				UE_LOG(LogTemp, Warning, TEXT("Attacked with weapon: %s"), 
					  *WeaponComponent->CurrentWeapon->ItemName.ToString());
			}
		}
		return;
	}
	
	// 둘 다 없으면 인벤토리 디버그 표시
	//ShowInventoryDebug();
}

void ACYPlayerCharacter::ShowInventoryDebug()
{
    if (InventoryComponent)
    {
        InventoryComponent->ShowInventoryDebug();
    }
}

void ACYPlayerCharacter::Client_ShowRobberDetectedWarning_Implementation(bool bShow, AActor* DetectedThief)
{
}


// 인벤토리 슬롯 입력 (1~9번 키)
void ACYPlayerCharacter::Input_UseSlot1(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(1);
}

void ACYPlayerCharacter::Input_UseSlot2(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(2);
}

void ACYPlayerCharacter::Input_UseSlot3(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(3);
}

void ACYPlayerCharacter::Input_UseSlot4(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(4);
}

void ACYPlayerCharacter::Input_UseSlot5(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(5);
}

void ACYPlayerCharacter::Input_UseSlot6(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(6);
}

void ACYPlayerCharacter::Input_UseSlot7(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(7);
}

void ACYPlayerCharacter::Input_UseSlot8(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(8);
}

void ACYPlayerCharacter::Input_UseSlot9(const FInputActionValue& InputActionValue)
{
	if (InventoryComponent) InventoryComponent->HoldItem(9);
}

void ACYPlayerCharacter::RegisterInvisibilityTagEvent()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 기존 등록 해제
	if (InvisibilityTagDelegateHandle.IsValid())
	{
		ASC->RegisterGameplayTagEvent(CYGameplayTags::State_Invisible, EGameplayTagEventType::NewOrRemoved)
			.Remove(InvisibilityTagDelegateHandle);
	}

	// 새로 등록
	InvisibilityTagDelegateHandle = ASC->RegisterGameplayTagEvent(
		CYGameplayTags::State_Invisible, 
		EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &ACYPlayerCharacter::OnInvisibilityChanged);
}

void ACYPlayerCharacter::OnInvisibilityChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!IsValid(this))
	{
		return;
	}

	bool bIsInvisible = (NewCount > 0);
	MulticastHandleInvisibilityChanged(bIsInvisible);
}

void ACYPlayerCharacter::MulticastHandleInvisibilityChanged_Implementation(bool bIsInvisible)
{
	if (!IsValid(this))
	{
		return;
	}

	UpdateVisibilityForLocalPlayer(bIsInvisible);
}

void ACYPlayerCharacter::UpdateVisibilityForLocalPlayer(bool bIsInvisible)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	
	APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	if (!LocalPC) return;
    
	ACYPlayerCharacter* LocalCharacter = Cast<ACYPlayerCharacter>(LocalPC->GetPawn());
	if (!LocalCharacter) return;
    
	// 자기 자신은 항상 보임
	if (LocalCharacter == this)
	{
		SetMeshVisibility(GetMesh(), true);
		SetMeshVisibility(GetHelmetMesh(), true);
		SetMeshVisibility(GetEyewearMesh(), true);
		SetMeshVisibility(GetChestMesh(), true);
		SetMeshVisibility(GetLegsMesh(), true);
		SetMeshVisibility(GetFootwearMesh(), true);
		
		if (WeaponComponent && WeaponComponent->CurrentWeapon)
			SetMeshVisibility(WeaponComponent->CurrentWeapon->ItemMesh, true);
		if (InventoryComponent && InventoryComponent->CurrentHeldItem)
			SetMeshVisibility(InventoryComponent->CurrentHeldItem->ItemMesh, true);
		return;
	}
    
	// 팀 정보 가져오기
	ACYPlayerState* MyPS = GetPlayerState<ACYPlayerState>();
	ACYPlayerState* LocalPS = LocalCharacter->GetPlayerState<ACYPlayerState>();
	if (!MyPS || !LocalPS) return;
    
	if (bIsInvisible)
	{
		bool bSameTeam = (MyPS->GetTeamRole() == LocalPS->GetTeamRole());
        
		// 같은 팀: 보임 / 다른 팀: 안 보임
		SetMeshVisibility(GetMesh(), bSameTeam);
		SetMeshVisibility(GetHelmetMesh(), bSameTeam);
		SetMeshVisibility(GetEyewearMesh(), bSameTeam);
		SetMeshVisibility(GetChestMesh(), bSameTeam);
		SetMeshVisibility(GetLegsMesh(), bSameTeam);
		SetMeshVisibility(GetFootwearMesh(), bSameTeam);
        
		if (WeaponComponent && WeaponComponent->CurrentWeapon)
			SetMeshVisibility(WeaponComponent->CurrentWeapon->ItemMesh, bSameTeam);
            
		if (InventoryComponent && InventoryComponent->CurrentHeldItem)
			SetMeshVisibility(InventoryComponent->CurrentHeldItem->ItemMesh, bSameTeam);
	}
	else
	{
		// 투명 해제 - 모두 보임
		SetMeshVisibility(GetMesh(), true);
		SetMeshVisibility(GetHelmetMesh(), true);
		SetMeshVisibility(GetEyewearMesh(), true);
		SetMeshVisibility(GetChestMesh(), true);
		SetMeshVisibility(GetLegsMesh(), true);
		SetMeshVisibility(GetFootwearMesh(), true);
        
		if (WeaponComponent && WeaponComponent->CurrentWeapon)
			SetMeshVisibility(WeaponComponent->CurrentWeapon->ItemMesh, true);
            
		if (InventoryComponent && InventoryComponent->CurrentHeldItem)
			SetMeshVisibility(InventoryComponent->CurrentHeldItem->ItemMesh, true);
	}
}

void ACYPlayerCharacter::SetMeshVisibility(UMeshComponent* MeshComponent, bool bVisible)
{
	if (!MeshComponent) return;
    
	MeshComponent->SetVisibility(bVisible);
}