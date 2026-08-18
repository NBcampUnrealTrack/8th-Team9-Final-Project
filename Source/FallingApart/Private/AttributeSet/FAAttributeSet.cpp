// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeSet/FAAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UFAAttributeSet::UFAAttributeSet()
{
	// 매직 넘버링 차후에 수정할 것
	// 이동
	InitMoveSpeed(600.0f);
	InitSprintMultiplier(1.5f);
	InitGroundFriction(8.0f);
	InitAirControl(0.35f);
	InitTurnRateScale(1.0f);
	InitWaterMoveSpeedMultiplier(1.0f);
	InitJumpZVelocity(420.0f);

	// 스태미나
	InitMaxStamina(5.0f);
	InitStamina(GetMaxStamina());

	// 시야
	InitVisionRange(6000.0f);
	InitFieldOfView(90.0f);

	// 포획 및 구출
	InitGrabRange(200.0f);
	// 10.0 → 5.0. 이제 이 값은 '인질이 전혀 저항하지 않았을 때' 의 감염 시간이다.
	// 저항(WASD 연타)이 최대면 UFAGA_Grab::MaxResistTimeMultiplier 만큼 늘어난다 — 기본 3배라 15초.
	InitInfectDuration(5.0f);
	InitRescueDuration(0.0f);
	InitContestGauge(50.0f);
}

void UFAAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute,float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,2000.0f);
	}
	else if (Attribute == GetSprintMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue,1.0f,5.0f);
	}
	else if (Attribute == GetGroundFrictionAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,20.0f);
	}
	else if (Attribute == GetAirControlAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,1.0f);
	}
	else if (Attribute == GetTurnRateScaleAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,5.0f);
	}
	else if (Attribute ==GetWaterMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,5.0f);
	}
	else if (Attribute == GetStaminaAttribute())
	{
		// 현재 스태미나 는 최대 스태미나에서 0 사이로 설정
		NewValue = FMath::Clamp(NewValue,0.0f,GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		// 최대 기본값 5
		NewValue = FMath::Max(NewValue,5.0f);
	}
	else if (Attribute == GetVisionRangeAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,6000.0f);
	}
	else if (Attribute == GetFieldOfViewAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,90.0f);
	}
	else if (Attribute == GetGrabRangeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 50.0f, 1000.0f);
	}
	else if (Attribute == GetInfectDurationAttribute())
	{
		// 상한은 기본값(5)보다 위여야 한다 — 같으면 '감염 시간 증가' 계열 룰이 통째로 무효가 된다.
		// 여기서 다루는 건 저항 0 기준값이라, 실제 최대 감염 시간은 여기에 저항 배수가 한 번 더 곱해진다.
		NewValue = FMath::Clamp(NewValue, 0.5f, 15.0f);
	}
	else if (Attribute == GetRescueDurationAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 5.0f);
	}
	else if (Attribute == GetContestGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
	}
	else if (Attribute == GetJumpZVelocityAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f,2000.0f);
	}
}

void UFAAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& ChangedAttribute =Data.EvaluatedData.Attribute;

	if (ChangedAttribute == GetMoveSpeedAttribute())
	{
		SetMoveSpeed(FMath::Clamp(GetMoveSpeed(),0.0f,2000.0f));
	}
	else if (ChangedAttribute ==GetSprintMultiplierAttribute())
	{
		SetSprintMultiplier(FMath::Clamp(GetSprintMultiplier(),1.0f,5.0f));
	}
	else if (ChangedAttribute ==GetGroundFrictionAttribute())
	{
		SetGroundFriction(FMath::Clamp(GetGroundFriction(),0.0f,20.0f));
	}
	else if (ChangedAttribute ==GetAirControlAttribute())
	{
		SetAirControl(FMath::Clamp(GetAirControl(),0.0f,1.0f));
	}
	else if (ChangedAttribute ==GetTurnRateScaleAttribute())
	{
		SetTurnRateScale(FMath::Clamp(GetTurnRateScale(),0.0f,5.0f));
	}
	else if (ChangedAttribute ==GetWaterMoveSpeedMultiplierAttribute())
	{
		SetWaterMoveSpeedMultiplier(FMath::Clamp(GetWaterMoveSpeedMultiplier(),0.0f,5.0f));
	}
	else if (ChangedAttribute ==GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(),0.0f,GetMaxStamina()));
	}
	else if (ChangedAttribute ==GetMaxStaminaAttribute())
	{
		const float ClampedMaxStamina =FMath::Max(GetMaxStamina(),0.0f);
		SetMaxStamina(ClampedMaxStamina);

		// 최대값이 줄었을 경우에만 현재값 제한
		if (GetStamina() > ClampedMaxStamina)
		{
			SetStamina(ClampedMaxStamina);
		}
	}
	else if (ChangedAttribute ==GetVisionRangeAttribute())
	{
		SetVisionRange(FMath::Clamp(GetVisionRange(),0.0f,6000.0f));
	}
	else if (ChangedAttribute ==GetFieldOfViewAttribute())
	{
		SetVisionRange(FMath::Clamp(GetFieldOfView(),0.0f,6000.0f));
	}
	else if (ChangedAttribute == GetGrabRangeAttribute())
	{
		SetGrabRange(FMath::Clamp(GetGrabRange(), 50.0f, 1000.0f));
	}
	else if (ChangedAttribute == GetInfectDurationAttribute())
	{
		SetInfectDuration(FMath::Clamp(GetInfectDuration(), 0.5f, 15.0f));
	}
	else if (ChangedAttribute == GetRescueDurationAttribute())
	{
		SetRescueDuration(FMath::Clamp(GetRescueDuration(), 0.0f, 5.0f));
	}
	else if (ChangedAttribute == GetJumpZVelocityAttribute())
	{
		SetJumpZVelocity(FMath::Clamp(GetJumpZVelocity(), 0.0f, 2000.0f));
	}
}

void UFAAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,MoveSpeed,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,SprintMultiplier,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,GroundFriction,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,AirControl,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,TurnRateScale,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,WaterMoveSpeedMultiplier,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,Stamina,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,MaxStamina,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet,VisionRange,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet, GrabRange, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet, InfectDuration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet, RescueDuration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet, ContestGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFAAttributeSet, JumpZVelocity, COND_None, REPNOTIFY_Always);
}

void UFAAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,MoveSpeed,OldValue);
}

void UFAAttributeSet::OnRep_SprintMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,SprintMultiplier,OldValue);
}

void UFAAttributeSet::OnRep_GroundFriction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,GroundFriction,OldValue);
}

void UFAAttributeSet::OnRep_AirControl(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,AirControl,OldValue);
}

void UFAAttributeSet::OnRep_TurnRateScale(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,TurnRateScale,OldValue);
}

void UFAAttributeSet::OnRep_WaterMoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,WaterMoveSpeedMultiplier,OldValue);
}

void UFAAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,Stamina,OldValue);
}

void UFAAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,MaxStamina,OldValue);
}
	void UFAAttributeSet::OnRep_VisionRange(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,VisionRange,OldValue);
}

void UFAAttributeSet::OnRep_FieldOfView(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet,FieldOfView,OldValue);
}

void UFAAttributeSet::OnRep_GrabRange(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet, GrabRange, OldValue);
}

void UFAAttributeSet::OnRep_InfectDuration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet, InfectDuration, OldValue);
}

void UFAAttributeSet::OnRep_RescueDuration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet, RescueDuration, OldValue);
}

void UFAAttributeSet::OnRep_ContestGauge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet, ContestGauge, OldValue);
}

void UFAAttributeSet::OnRep_JumpZVelocity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFAAttributeSet, JumpZVelocity, OldValue);
}
