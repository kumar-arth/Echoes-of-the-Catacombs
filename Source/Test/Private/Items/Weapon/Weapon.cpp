// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Weapon/Weapon.h"
#include "Character/BaseCharacter.h"
#include "Character/MyCharacter.h"
#include "Combat/CombatHitResolver.h"
#include "Enemy/Enemy.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Utils/DebugDrawHelper.h"
#include "NiagaraComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

// ==================== 生命周期 ====================

AWeapon::AWeapon()
{
	BoxTrace = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxTrace"));
	BoxTrace->SetupAttachment(GetMesh());
	BoxTrace->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxTrace->SetHiddenInGame(true);
}

// ==================== 装备/拾取 ====================

bool AWeapon::AttachMeshToSocket(USceneComponent* Parent, const FName& SocketName)
{
	if (!Parent || SocketName == NAME_None || !Parent->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to attach weapon: parent or socket '%s' is invalid."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	if (!AttachToComponent(Parent, AttachmentRules, SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to attach weapon to socket '%s'."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	// 叠加装备旋转偏移，修正不同武器模型的本地朝向差异
	if (!EquipRotationOffset.IsNearlyZero())
	{
		SetActorRelativeRotation(EquipRotationOffset);
	}

	return true;
}

bool AWeapon::Equip(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner, APawn* NewInstigator,
	bool bPlayEquipSound)
{
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);

	if (!AttachMeshToSocket(Parent, SocketName))
	{
		return false;
	}

	ItemState = EItemState::EIS_Equipped;
	DisablePickupCollision();

	if (GetEffect())
	{
		GetEffect()->Deactivate();
	}

	// 非常关键：绑定武器的主人，避免砍中自己
	if (Parent && Parent->GetOwner())
	{
		SetOwner(Parent->GetOwner());
	}

	if (bPlayEquipSound)
	{
		PlayEquipSound();
	}

	return true;
}

void AWeapon::PlayEquipSound() const
{
	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquipSound, GetActorLocation());
	}
}

bool AWeapon::TryGrantPickup(AMyCharacter* Picker, USoundBase*& OutPickupSound)
{
	return TryClaimPersistentWorldPickup(Picker, OutPickupSound);
}

// ==================== 武器碰撞检测 ====================

void AWeapon::StartWeaponTrace()
{
	check(BoxTrace);
	TraceCenterOld = BoxTrace->GetComponentLocation();
	TraceRotationOld = BoxTrace->GetComponentRotation();
}

void AWeapon::ExecuteWeaponTrace()
{
	check(BoxTrace);
	if (const AEnemy* OwnerEnemy = Cast<AEnemy>(GetOwner()); OwnerEnemy && OwnerEnemy->IsEncounterDormant())
	{
		return;
	}

	FVector CurrentCenter = BoxTrace->GetComponentLocation();
	FRotator TraceRotation = BoxTrace->GetComponentRotation();
	FVector BoxHalfExtent = BoxTrace->GetScaledBoxExtent();

	TArray<AActor*> ActorsToIgnore;
	BuildIgnoreList(ActorsToIgnore);

	const EDrawDebugTrace::Type DrawDebugTrace = EDrawDebugTrace::None;

	// Trace both Pawn (bosses, enemies) and WorldDynamic
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	TArray<FHitResult> AllHits;

	// 1. Box trace on the weapon blade
	UKismetSystemLibrary::BoxTraceMultiForObjects(
		this, TraceCenterOld, CurrentCenter, BoxHalfExtent, TraceRotation,
		ObjectTypes, false, ActorsToIgnore, DrawDebugTrace,
		AllHits, true, FLinearColor::Red, FLinearColor::Green, 3.f);

	// 2. Generous forward sphere sweep from character when player is swinging (guarantees 100% hit on bosses)
	if (AActor* AttackerActor = GetOwner())
	{
		const FVector Fwd = AttackerActor->GetActorForwardVector();
		const FVector SweepStart = AttackerActor->GetActorLocation() - (Fwd * 30.0f) + FVector(0.0f, 0.0f, 20.0f);
		const FVector SweepEnd = AttackerActor->GetActorLocation() + (Fwd * 270.0f) + FVector(0.0f, 0.0f, 20.0f);

		TArray<FHitResult> ForwardHits;
		UKismetSystemLibrary::SphereTraceMultiForObjects(
			this, SweepStart, SweepEnd, 100.0f, ObjectTypes, false, ActorsToIgnore,
			DrawDebugTrace, ForwardHits, true);

		for (const FHitResult& FHit : ForwardHits)
		{
			if (FHit.GetActor() && !AllHits.ContainsByPredicate([&](const FHitResult& H){ return H.GetActor() == FHit.GetActor(); }))
			{
				AllHits.Add(FHit);
			}
		}
	}

	// Record positions for next frame
	TraceCenterOld = CurrentCenter;
	TraceRotationOld = TraceRotation;

	for (const FHitResult& HitPoint : AllHits)
	{
		AActor* HitActor = HitPoint.GetActor();
		if (!HitActor || IgnoreActors.Contains(HitActor)) continue;

		float IncomingDamage = Damage;
		float PoiseDamage = 0.f;
		bool bApplyPoiseDamage = false;
		float BlockStaminaDamageMultiplier = 1.f;
		bool bCanBeParried = true;

		if (ABaseCharacter* Attacker = Cast<ABaseCharacter>(GetOwner()))
		{
			IncomingDamage *= Attacker->GetAttackDamageMultiplier();
			PoiseDamage = Attacker->GetCurrentPoiseDamage();
			bApplyPoiseDamage = true;
			BlockStaminaDamageMultiplier = Attacker->GetBlockStaminaDamageMultiplier();
			bCanBeParried = Attacker->CanCurrentAttackBeParried();
		}

		FCombatHitRequest Request;
		Request.Attacker = GetOwner();
		Request.DamageCauser = this;
		Request.EventInstigator = GetInstigatorController();
		Request.HitActor = HitActor;
		Request.HitResult = HitPoint;
		Request.IncomingDamage = IncomingDamage;
		Request.PoiseDamage = PoiseDamage;
		Request.bApplyPoiseDamage = bApplyPoiseDamage;
		Request.BlockStaminaDamageMultiplier = BlockStaminaDamageMultiplier;
		Request.bCanBeParried = bCanBeParried;

		const FCombatHitResult Result = FCombatHitResolver::ResolveAndApply(Request);
		if (Result.bSuppressed)
		{
			IgnoreActors.AddUnique(HitActor);
			continue;
		}

		// Directly apply damage so all bosses and pawns reliably take damage
		if (!Result.bSameTeam)
		{
			UGameplayStatics::ApplyDamage(HitActor, IncomingDamage, GetInstigatorController(), this, UDamageType::StaticClass());
		}

		CameraShake();
		SetEnableHitStop(true);
		HitStop(HitActor);
		IgnoreActors.AddUnique(HitActor);
	}
}

void AWeapon::BuildIgnoreList(TArray<AActor*>& OutActors)
{
	OutActors.Add(this);

	if (GetOwner())
	{
		OutActors.AddUnique(GetOwner());
	}

	for (AActor* ToIgnore : IgnoreActors)
	{
		OutActors.AddUnique(ToIgnore);
	}
}

// ==================== 卡肉感 ====================

void AWeapon::RestoreTimeDilation(AActor* Attacker, AActor* Victim)
{
	if (Attacker)
	{
		Attacker->CustomTimeDilation = 1.0f;
	}
	if (Victim)
	{
		Victim->CustomTimeDilation = 1.0f;
	}
}

void AWeapon::HitStop(AActor* HitActor)
{
	if (bEnableHitStop)
	{
		AActor* Attacker = GetOwner();
		if (Attacker && HitActor)
		{
			Attacker->CustomTimeDilation = HitStopTimeDilation;
			HitActor->CustomTimeDilation = HitStopTimeDilation;

			FTimerHandle HitStopTimer;
			FTimerDelegate TimerDel;
			TimerDel.BindUFunction(this, FName("RestoreTimeDilation"), Attacker, HitActor);
			GetWorld()->GetTimerManager().SetTimer(HitStopTimer, TimerDel, HitStopDuration, false);
		}
	}
}

void AWeapon::CameraShake()
{
	if (HitCameraShake && GetInstigatorController())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetInstigatorController()))
		{
			PlayerController->ClientStartCameraShake(HitCameraShake);
		}
	}
}
