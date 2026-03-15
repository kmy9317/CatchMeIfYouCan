#include "AbilitySystem/Attributes/CYCombatAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

UCYCombatAttributeSet::UCYCombatAttributeSet()
{
    InitMoveSpeed(400.0f);
    InitAttackPower(50.0f);
}

void UCYCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UCYCombatAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCYCombatAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
}

void UCYCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMoveSpeedAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
}

void UCYCombatAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMoveSpeedAttribute())
	{
		// 서버에서만 처리
		if (GetOwningActor() && GetOwningActor()->HasAuthority())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] PostAttributeChange MoveSpeed: %f -> %f"), 
				   OldValue, NewValue);
			HandleMoveSpeedChange();
		}
	}
}

void UCYCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AActor* Owner = GetOwningActor();

	if (Data.EvaluatedData.Attribute == GetMoveSpeedAttribute())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MoveSpeed attribute changed, calling HandleMoveSpeedChange"), 
			   Owner && Owner->HasAuthority() ? TEXT("Server") : TEXT("Client"));
        
		// 서버와 클라이언트 모두에서 이동속도 변경 처리
		HandleMoveSpeedChange();
	}
}

void UCYCombatAttributeSet::HandleMoveSpeedChange()
{
	float NewMoveSpeed = GetMoveSpeed();
	AActor* Owner = GetOwningActor();
    
	ACharacter* TargetCharacter = nullptr;
    
	// 직접 Character인 경우
	TargetCharacter = Cast<ACharacter>(GetOwningActor());
    
	// PlayerState가 Owner인 경우
	if (!TargetCharacter)
	{
		if (APlayerState* PS = Cast<APlayerState>(GetOwningActor()))
		{
			TargetCharacter = Cast<ACharacter>(PS->GetPawn());
		}
	}
    
	// Instigator를 통해 찾기
	if (!TargetCharacter)
	{
		if (APawn* Pawn = GetOwningActor() ? GetOwningActor()->GetInstigator() : nullptr)
		{
			TargetCharacter = Cast<ACharacter>(Pawn);
		}
	}
    
	// Character를 찾았으면 이동 속도 적용
	if (TargetCharacter)
	{
		ApplyMovementRestrictions(TargetCharacter, NewMoveSpeed);

		// 서버에서 네트워크 업데이트 강제
		if (TargetCharacter->HasAuthority())
		{
			TargetCharacter->ForceNetUpdate();
		}
	}
}

void UCYCombatAttributeSet::ApplyMovementRestrictions(ACharacter* Character, float Speed)
{
    if (!Character) return;
    
    UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
    if (!MovementComp) return;
    
    MovementComp->MaxWalkSpeed = Speed;
    
    if (Speed <= 0.0f)
    {
        MovementComp->StopMovementImmediately();
        MovementComp->MaxAcceleration = 0.0f;
        MovementComp->BrakingDecelerationWalking = 10000.0f;
        MovementComp->GroundFriction = 100.0f;
        MovementComp->JumpZVelocity = 0.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("IMMOBILIZED: %s"), *Character->GetName());
    }
    else if (Speed < 200.0f)
    {
        MovementComp->MaxAcceleration = 500.0f;
        MovementComp->BrakingDecelerationWalking = 1000.0f;
        MovementComp->JumpZVelocity = 0.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("SLOWED: %s to %f"), *Character->GetName(), Speed);
    }
    else if (Speed < 400.0f)
    {
        MovementComp->MaxAcceleration = 8192.0f;
        MovementComp->BrakingDecelerationWalking = 8192.0f;
        MovementComp->GroundFriction = 4.0f;
        MovementComp->JumpZVelocity = 600.0f;
        
        // 네트워크 오류 체크 완화
        if (Character->HasAuthority())
        {
            MovementComp->NetworkMaxSmoothUpdateDistance = 256.0f;
            MovementComp->NetworkNoSmoothUpdateDistance = 512.0f;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("SPEED BOOSTED: %s to %f (MaxAccel: %f)"), 
               *Character->GetName(), Speed, MovementComp->MaxAcceleration);
    }
    else
    {
        MovementComp->MaxAcceleration = 2048.0f;
        MovementComp->BrakingDecelerationWalking = 2000.0f;
        MovementComp->GroundFriction = 8.0f;
        MovementComp->JumpZVelocity = 600.0f;
        
        if (Character->HasAuthority())
        {
            MovementComp->NetworkMaxSmoothUpdateDistance = 92.0f;
            MovementComp->NetworkNoSmoothUpdateDistance = 140.0f;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("MOVEMENT RESTORED: %s to %f"), 
               *Character->GetName(), Speed);
    }
    
    MovementComp->bForceNextFloorCheck = true;
}

void UCYCombatAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	if (GetOwningAbilitySystemComponent())
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCYCombatAttributeSet, MoveSpeed, OldMoveSpeed);
		
		// 클라이언트에서 이동 속도 적용
		HandleMoveSpeedChange();
	}
}

void UCYCombatAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	if (GetOwningAbilitySystemComponent())
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCYCombatAttributeSet, AttackPower, OldAttackPower);
	}
}