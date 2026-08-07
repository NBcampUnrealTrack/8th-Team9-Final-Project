// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/FAAbilitySystemComponent.h"

#include "GAS/FAGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Rule/FARuleSettings.h"
#include "Rule/FARuleTypes.h"

UFAAbilitySystemComponent::UFAAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);

	// Mixed — GE 전체는 소유 클라이언트에게만 보내고, 나머지에게는 태그·Attribute 만.
	// 8인 파티 기준 룰 태그 복제는 30초에 1회뿐이라 이 모드로 충분.
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UFAAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 남의 보유 룰도 관전 UI·머리 위 아이콘에서 보여줘야 하므로 전원에게 복제.
	DOREPLIFETIME(UFAAbilitySystemComponent, ActiveRuleIds);
}

bool UFAAbilitySystemComponent::ApplyRuleCard(FName RuleId)
{
	if (!IsOwnerActorAuthoritative())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rule] ApplyRuleCard 는 서버 전용이다 (RuleId=%s)"), *RuleId.ToString());
		return false;
	}

	if (ActiveRules.Contains(RuleId))
	{
		return false;
	}

	const FFARuleCardRow* Row = UFARuleSettings::FindRuleCard(RuleId);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rule] DT 에 없는 카드: %s"), *RuleId.ToString());
		return false;
	}

	UClass* EffectClass = Row->RuleEffect.LoadSynchronous();
	if (!EffectClass)
	{
		// 아직 GE 애셋을 안 만든 카드. 표에 행만 있고 애셋이 비어 있는 상태.
		UE_LOG(LogTemp, Warning, TEXT("[Rule] %s 의 RuleEffect 가 비어 있다 — GE 애셋 미제작"), *RuleId.ToString());
		return false;
	}

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetOwner());

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	const FAGameplayTags& Tags = FAGameplayTags::Get();

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
	Spec.SetSetByCallerMagnitude(Tags.Data_Rule_Magnitude, Row->Magnitude);

	const FActiveGameplayEffectHandle Handle = ApplyGameplayEffectSpecToSelf(Spec);
	if (!Handle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rule] %s GE 적용 실패"), *RuleId.ToString());
		return false;
	}

	FFAActiveRule& Entry = ActiveRules.Add(RuleId);
	Entry.Handle = Handle;
	Entry.TeamTag = TeamTag;

	ActiveRuleIds.Add(RuleId);

	// 상한 초과 시 최선입부터 소멸 (밸런싱 워크북 0_기준 · 활성 룰 상한 4장).
	// 카드가 계속 쌓이면 6회차쯤엔 아무도 움직일 수 없게 된다 — 상한이 게임을 지탱.
	const int32 MaxRules = UFARuleSettings::GetMaxActiveRules();
	while (ActiveRuleIds.Num() > MaxRules)
	{
		const FName Oldest = ActiveRuleIds[0];
		UE_LOG(LogTemp, Log, TEXT("[Rule] 상한(%d장) 초과 — 최선입 %s 소멸"), MaxRules, *Oldest.ToString());
		RemoveRuleCard(Oldest);
	}

	UE_LOG(LogTemp, Log, TEXT("[Rule] %s 적용 (보유 %d장)"), *RuleId.ToString(), ActiveRuleIds.Num());

	// 태그를 부여하지 않는 GE 는 아무 일도 하지 않는다 (CMC·어빌리티가 전부 태그만 본다).
	// 조용히 실패하면 "룰은 붙었다는데 게임은 그대로"가 되므로 여기서 크게 잡는다.
	if (Row->RuleTag.IsValid() && !HasMatchingGameplayTag(Row->RuleTag))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Rule] %s: GE(%s) 가 태그 %s 를 부여하지 않았다 — 애셋의 Components → Grant Tags to Target Actor → Add to Inherited 를 확인할 것"),
			*RuleId.ToString(), *GetNameSafe(EffectClass), *Row->RuleTag.ToString());
	}

	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);
	UE_LOG(LogTemp, Log, TEXT("[Rule] 보유 태그: %s"), *OwnedTags.ToStringSimple());

	// 서버는 OnRep 이 안 불리므로 직접 알린다 (리슨 서버 호스트의 HUD 갱신).
	OnActiveRulesChanged.Broadcast();
	return true;
}

bool UFAAbilitySystemComponent::RemoveRuleCard(FName RuleId)
{
	if (!IsOwnerActorAuthoritative())
	{
		return false;
	}

	const FFAActiveRule* Entry = ActiveRules.Find(RuleId);
	if (!Entry)
	{
		return false;
	}

	RemoveActiveGameplayEffect(Entry->Handle);

	ActiveRules.Remove(RuleId);
	ActiveRuleIds.Remove(RuleId);

	OnActiveRulesChanged.Broadcast();
	return true;
}

int32 UFAAbilitySystemComponent::RemoveRulesOfTeam(const FGameplayTag& TeamTag)
{
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
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	TArray<FName> ToRemove;
	ActiveRules.GenerateKeyArray(ToRemove);

	for (const FName RuleId : ToRemove)
	{
		RemoveRuleCard(RuleId);
	}
}

void UFAAbilitySystemComponent::OnRep_ActiveRuleIds()
{
	OnActiveRulesChanged.Broadcast();
}
