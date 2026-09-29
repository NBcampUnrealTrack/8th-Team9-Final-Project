// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/FAAbilitySystemComponent.h"

#include "GAS/FAGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Rule/FARuleSettings.h"
#include "Rule/FARuleTypes.h"
#include "AttributeSet/FAAttributeSet.h"

UFAAbilitySystemComponent::UFAAbilitySystemComponent()
{
	// 네트워크에 복제를 사용하도록 기본 설정
	SetIsReplicatedByDefault(true);

	// Mixed — GE 전체는 소유 클라이언트에게만 보내고, 나머지에게는 태그·Attribute 만.
	// 8인 파티 기준 룰 태그 복제는 30초에 1회뿐이라 이 모드로 충분.
	
	//GameplayEffect 를 어느정도까지 다른 클라이언트에게 알려주는 설정 중간치 정도 알려줌
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UFAAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 남의 보유 룰도 관전 UI·머리 위 아이콘에서 보여줘야 하므로 전원에게 복제.
	//Unreal에서 어떤 UPROPERTY를 네트워크로 복제할 것인지 등록하는 함수 
	DOREPLIFETIME(UFAAbilitySystemComponent, ActiveRuleIds);
}

bool UFAAbilitySystemComponent::ApplyRuleCard(FName RuleId)
{
	// 본인이 서버인지 아닌지 확인하는 함수 
	if (!IsOwnerActorAuthoritative())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rule] ApplyRuleCard 는 서버 전용이다 (RuleId=%s)"), *RuleId.ToString());
		return false;
	}

	// 중복 롤 있는지 함수 
	// 지금 활성화 된 룰 중에서 
	if (ActiveRules.Contains(RuleId))
	{
		return false;
	}

	//Rule 카드 중에서 찾고자 하는 룰이 있는지 확인하는 함수 
	// 해당 룰 이 없으면 종료 
	const FFARuleCardRow* Row = UFARuleSettings::FindRuleCard(RuleId);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rule] DT 에 없는 카드: %s"), *RuleId.ToString());
		return false;
	}

	// 찾은 룰이 에셋에 있는지 확인
	// H03/Z03은 기존 H03의 태그 부여 GE를 공유한다. 피로 동작은 Status에서 처리한다.
	// Z03의 구형 상시 감속 GE를 함께 적용하지 않는다.
	const bool bFatigueRule = RuleId == FName(TEXT("H03")) || RuleId == FName(TEXT("Z03"));
	const FFARuleCardRow* EffectRow = bFatigueRule ? UFARuleSettings::FindRuleCard(TEXT("H03")) : Row;
	UClass* EffectClass = EffectRow ? EffectRow->RuleEffect.LoadSynchronous() : nullptr;
	if (!EffectClass)
	{
		// 아직 GE 애셋을 안 만든 카드. 표에 행만 있고 애셋이 비어 있는 상태.
		UE_LOG(LogTemp, Warning, TEXT("[Rule] %s 의 RuleEffect 가 비어 있다 — GE 애셋 미제작"), *RuleId.ToString());
		return false;
	}

	if (const UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>())
	{
		if (EffectCDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[Rule] %s: GE(%s) 의 Duration Policy 가 %s 다 — 룰 GE 는 Infinite 여야 한다. ")
				TEXT("Instant 면 Base Value 가 영구히 바뀌어 감염 시 룰을 제거해도 디버프가 안 풀린다. ")
				TEXT("애셋을 열어 Duration Policy 를 Infinite 로 바꿀 것."),
				*RuleId.ToString(), *GetNameSafe(EffectClass),
				EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant ? TEXT("Instant") : TEXT("HasDuration"));
		}
	}

	//호출하면 현재 ASC를 기준으로 새로운 GameplayEffectContext를 만들어 줍니다.
	//Effect 가 발생한 정보를 담아 Context 에 넣고 
	FGameplayEffectContextHandle Context = MakeEffectContext();
	//관련 객체 등 "이 효과의 출처를 식별하기 위해 보관하고 싶은 UObject"를 넣는 공간
	//추가적인 SourceObject 정보를 넣고 싶을 때 사용하는 것
	Context.AddSourceObject(GetOwner());

	
	// MakeOutgoingSpec()이 “적용할 효과의 실행본을 준비하는 단계”
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	const FGameplayTag EffectiveRuleTag = bFatigueRule ? Tags.Rule_Move_NoSprint : Row->RuleTag;
	if (bFatigueRule)
	{
		Spec.DynamicGrantedTags.AddTag(EffectiveRuleTag);
	}

	// 공통 룰은 GE 애셋을 좀비/인간이 공유. 진영 구분은 애셋이 아니라 여기서 붙임.
	// DynamicGrantedTags 는 복제되므로 클라이언트도 "이건 인간 룰"임을 알 수 있음.
	FGameplayTag TeamTag;
	switch (Row->Team)
	{
	case EFATeam::Zombie:	TeamTag = Tags.Rule_Team_Zombie; break;
	case EFATeam::Human:	TeamTag = Tags.Rule_Team_Human;  break;
	default:				break;
	}

	if (TeamTag.IsValid())
	{
		Spec.DynamicGrantedTags.AddTag(TeamTag);
	}

	// 밸런싱 수치 주입. GE 가 SetByCaller 를 안 쓰면 그냥 무시.
	Spec.SetSetByCallerMagnitude(Tags.Data_Rule_Magnitude, EffectRow->Magnitude);

	// ApplyGameplayEffectSpecToSelf()는 그 효과를 실제로 ASC에 적용하는 단계 효과 적용
	const FActiveGameplayEffectHandle Handle = ApplyGameplayEffectSpecToSelf(Spec);
	
	if (const UFAAttributeSet* Attributes = GetSet<UFAAttributeSet>())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Rule] RuleId=%s 적용 직후 AirControl=%.2f"),
			*RuleId.ToString(),
			Attributes->GetAirControl());
	}
	// 해당 Handle 이 유효한지 확인
	if (!Handle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rule] %s GE 적용 실패"), *RuleId.ToString());
		return false;
	}

	// 적용된 
	FFAActiveRule& Entry = ActiveRules.Add(RuleId);
	Entry.Handle = Handle;
	Entry.TeamTag = TeamTag;

	// 활성화  된 Rule 에 추가
	ActiveRuleIds.Add(RuleId);

	// 상한 초과 시 최선입부터 소멸 (밸런싱 워크북 0_기준 · 활성 룰 상한 4장).
	// 카드가 계속 쌓이면 6회차쯤엔 아무도 움직일 수 없게 된다 — 상한이 게임을 지탱.
	// 룰 최대치 확인해서 활성화된 룰이 넘지 않도록 
	const int32 MaxRules = UFARuleSettings::GetMaxActiveRules();
	while (ActiveRuleIds.Num() > MaxRules)
	{
		const FName Oldest = ActiveRuleIds[0];
		UE_LOG(LogTemp, Log, TEXT("[Rule] 상한(%d장) 초과 — 최선입 %s 소멸"), MaxRules, *Oldest.ToString());
		// 맨 처음에 들어왔던 룰 카드 제거 
		RemoveRuleCard(Oldest);
	}

	UE_LOG(LogTemp, Log, TEXT("[Rule] %s 적용 (보유 %d장)"), *RuleId.ToString(), ActiveRuleIds.Num());

	// 태그를 부여하지 않는 GE 는 아무 일도 하지 않는다 (CMC·어빌리티가 전부 태그만 본다).
	// 조용히 실패하면 "룰은 붙었다는데 게임은 그대로"가 되므로 여기서 크게 잡는다.
	if (EffectiveRuleTag.IsValid() && !HasMatchingGameplayTag(EffectiveRuleTag))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Rule] %s: GE(%s) 가 태그 %s 를 부여하지 않았다 — 애셋의 Components → Grant Tags to Target Actor → Add to Inherited 를 확인할 것"),
			*RuleId.ToString(), *GetNameSafe(EffectClass), *EffectiveRuleTag.ToString());
	}

	// 보유 룰 확인
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);
	UE_LOG(LogTemp, Log, TEXT("[Rule] 보유 태그: %s"), *OwnedTags.ToStringSimple());

	// 서버는 OnRep 이 안 불리므로 직접 알린다 (리슨 서버 호스트의 HUD 갱신).
	OnActiveRulesChanged.Broadcast();
	return true;
}

bool UFAAbilitySystemComponent::RemoveRuleCard(FName RuleId)
{
	// 활성화된 Rule 들 제거 
	// 서버인지 확인
	if (!IsOwnerActorAuthoritative())
	{
		return false;
	}

	// 황성화된 룰 찾기 이함수 진입시  
	const FFAActiveRule* Entry = ActiveRules.Find(RuleId);
	if (!Entry)
	{
		return false;
	}

	// 적용해서 활성 상태로 남아 있는 GameplayEffect를 제거하는 함수
	// 현제 있는 Rule 제거 
	RemoveActiveGameplayEffect(Entry->Handle);

	// 황성화된 Rule 도 해당 Vector 에서 제거 
	ActiveRules.Remove(RuleId);
	ActiveRuleIds.Remove(RuleId);

	// BoradCast 해서 UI 최신화 
	OnActiveRulesChanged.Broadcast();
	return true;
}

int32 UFAAbilitySystemComponent::RemoveRulesOfTeam(const FGameplayTag& TeamTag)
{
	//팀 테그 제거 
	if (!IsOwnerActorAuthoritative() || !TeamTag.IsValid())
	{
		return 0;
	}

	// 순회 중 제거하면 맵이 깨지므로 대상부터 모은다.
	TArray<FName> ToRemove;
	for (const TPair<FName, FFAActiveRule>& Pair : ActiveRules)
	{
		if (Pair.Value.TeamTag.MatchesTagExact(TeamTag))
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FName RuleId : ToRemove)
	{
		RemoveRuleCard(RuleId);
	}

	UE_LOG(LogTemp, Log, TEXT("[Rule] 진영 태그 %s 룰 %d장 제거"), *TeamTag.ToString(), ToRemove.Num());
	return ToRemove.Num();
}

void UFAAbilitySystemComponent::RemoveAllRules()
{
	// 모든 룰 제거 
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	TArray<FName> ToRemove;
	ActiveRules.GenerateKeyArray(ToRemove);
	//ActiveRules라는 TMap에 들어있는 모든 Key를 꺼내서 ToRemove라는 TArray에 복사하는 코드
	// 참조 생각하자
	for (const FName RuleId : ToRemove)
	{
		RemoveRuleCard(RuleId);
	}
}

void UFAAbilitySystemComponent::OnRep_ActiveRuleIds()
{
	// 네트워크에서 브로드 케스트 호출 
	OnActiveRulesChanged.Broadcast();
}
