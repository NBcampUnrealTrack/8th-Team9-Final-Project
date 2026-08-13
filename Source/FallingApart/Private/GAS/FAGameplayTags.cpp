#include "GAS/FAGameplayTags.h"

#include "GameplayTagsManager.h"

FAGameplayTags FAGameplayTags::Instance;

void FAGameplayTags::InitializeNativeTags()
{
	Instance.AddAllTags();
}

FGameplayTag FAGameplayTags::AddTag(const TCHAR* TagName, const TCHAR* DevComment)
{
	return UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(DevComment));
}

void FAGameplayTags::AddAllTags()
{
	//~ 진영
	Rule_Team_Zombie = AddTag(TEXT("Rule.Team.Zombie"), TEXT("좀비 진영이 보유한 룰 GE 에 붙는 태그. 감염 시 일괄 제거 대상 식별용."));
	Rule_Team_Human  = AddTag(TEXT("Rule.Team.Human"),  TEXT("인간 진영이 보유한 룰 GE 에 붙는 태그. 감염 시 이 태그를 가진 GE 전부 제거."));

	//~ 룰 — Move 계열
	Rule_Move_NoJump       = AddTag(TEXT("Rule.Move.NoJump"),       TEXT("Z01/H01 점프 불가. GA_Jump 의 ActivationBlockedTags."));
	Rule_Move_SpeedDown_S  = AddTag(TEXT("Rule.Move.SpeedDown.S"),  TEXT("Z02/H02 이동속도 -15%."));
	Rule_Move_LowFriction  = AddTag(TEXT("Rule.Move.LowFriction"),  TEXT("Z04/H04 지면 마찰 -70%. 관성이 커져 미끄러진다."));
	Rule_Move_ForceForward = AddTag(TEXT("Rule.Move.ForceForward"), TEXT("Z05/H05 자동 전진. 멈출 수 없고 좌우 조향만 가능."));
	Rule_Move_Seizure      = AddTag(TEXT("Rule.Move.Seizure"),      TEXT("Z07/H06 경련·발작. 6초마다 1.5초 State.Stunned."));
	Rule_Move_NoAriControl = AddTag(TEXT("Rule.Move.NoAriControl"),  TEXT("Z09/H08 공중 마비. 공중 제어 0 "));
	Rule_Move_LandStun	   = AddTag(TEXT("Rule.Move.Seizure"),      TEXT("Z11/H09 착지 경직 . 착지 시 1초 정지 "));
	
	//~ 룰 - Input
	Rule_Input_FlipLR      = AddTag(TEXT("Rule.Input.FlipLR"),TEXT("Z13/H10 . 5초 마다 좌우 입력 반전 "));
	
	//~ 룰 - Vision
	Rule_Vision_NarrowFOV  = AddTag(TEXT("Rule.Vision.NarrowFov"), TEXT("Z16/H17. 시야각 -35% "));
	
	Rule_Vision_Desautrate = AddTag(TEXT("Rule.Vision.Desautrate"), TEXT("Z17/H18 흑백.  시야거리 8m "));

	//~ 상태
	State_Human         = AddTag(TEXT("State.Human"),         TEXT("현재 인간. 진영 전환 시 서버가 갈아끼운다."));
	State_Zombie        = AddTag(TEXT("State.Zombie"),        TEXT("현재 좀비."));
	State_Stunned       = AddTag(TEXT("State.Stunned"),       TEXT("이동·점프 전면 봉인. 경련/착지 경직/QTE 패배 경직 공용."));
	State_Invulnerable  = AddTag(TEXT("State.Invulnerable"),  TEXT("감염 직후 2초 무적. 연속 드래프트 중 즉사 방지."));
	State_Drafting      = AddTag(TEXT("State.Drafting"),      TEXT("드래프트 UI 활성 중."));
	State_Grabbing      = AddTag(TEXT("State.Grabbing"),      TEXT("좀비가 포획 중 (설계문서 §3.1)."));
	State_Grabbed       = AddTag(TEXT("State.Grabbed"),       TEXT("인간이 포획당한 상태."));
	State_Contesting    = AddTag(TEXT("State.Contesting"),    TEXT("구출 QTE 참여 중 — 이동·점프 봉인 (설계문서 §3.3)."));

	//~ 어빌리티
	Ability_Jump = AddTag(TEXT("Ability.Jump"),						TEXT("GA_Jump 식별 태그."));
	Ability_Grab = AddTag(TEXT("Ability.Grab"),						TEXT("GA_Grab 좀비 포획 어빌리티 식별 태그."));
	Ability_Rescue = AddTag(TEXT("Ability.Rescue"),					TEXT("GA_Rescue 인간 구출 어빌리티 식별 태그."));
	Ability_RescueContest = AddTag(TEXT("Ability.RescueContest"),	TEXT("GA_RescueContest QTE 줄다리기 어빌리티 식별 태그."));
	Ability_ContestMash = AddTag(TEXT("Ability.ContestMash"),		TEXT("GA_ContestMash QTE 스페이스바 연타 어빌리티 식별 태그."));

	//~ 이벤트
	Event_Contest_Mash = AddTag(TEXT("Event.Contest.Mash"), TEXT("QTE 줄다리기 스페이스바 연타 1회 이벤트."));

	//~ SetByCaller
	Data_Rule_Magnitude = AddTag(TEXT("Data.Rule.Magnitude"), TEXT("룰 GE Modifier 의 SetByCaller 수치. DataTable 의 Magnitude 열이 주입한다."));
}
