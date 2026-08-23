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
	Rule_Move_NoJump       = AddTag(TEXT("Rule.Move.NoJump"),       TEXT("Z01/H01 점프 불가 . GA_Jump 의 ActivationBlockedTags."));
	Rule_Move_SpeedDown_S  = AddTag(TEXT("Rule.Move.SpeedDown.S"),  TEXT("Z02/H02 이동속도 -15%."));
	Rule_Move_LowFriction  = AddTag(TEXT("Rule.Move.LowFriction"),  TEXT("Z04/H04 지면 마찰 -70%. 관성이 커져 미끄러진다."));
	Rule_Move_ForceForward = AddTag(TEXT("Rule.Move.ForceForward"), TEXT("Z05/H05 자동 전진 . 멈출 수 없고 좌우 조향만 가능."));
	Rule_Move_Seizure      = AddTag(TEXT("Rule.Move.Seizure"),      TEXT("Z07/H06 경련·발작 . 6초마다 1.5초 State.Stunned."));
	Rule_Move_NoAriControl = AddTag(TEXT("Rule.Move.NoAriControl"), TEXT("Z09/H08 공중 마비 . 공중 제어 0 "));
	Rule_Move_LandStun	   = AddTag(TEXT("Rule.Move.LandStun"),     TEXT("Z11/H09 착지 경직 . 착지 시 1초 정지 "));
	Rule_Move_NoMantle	   = AddTag(TEXT("Rule.Move.NoMantle"),		TEXT("Z13/H12 등반 불가 . 오르기·넘기 봉인 "));

	//~ 룰 - Input
	Rule_Input_FlipLR      = AddTag(TEXT("Rule.Input.FlipLR"),TEXT("Z13/H10 . 5초 마다 좌우 입력 반전 "));

	//~ 룰 - Vision
	Rule_Vision_NarrowFOV  = AddTag(TEXT("Rule.Vision.NarrowFov"), TEXT("Z16/H17. 시야각 -35% "));
	Rule_Vision_Desaturate = AddTag(TEXT("Rule.Vision.Desautrate"), TEXT("Z17/H18 흑백 — 로컬 PostProcess 채도 0. "));
	Rule_Vision_Fog       = AddTag(TEXT("Rule.Vision.Fog"),       TEXT("Z18/H19 안개 — VisionRange 800uu + PostProcess 안개."));
	Rule_Vision_Blackout  = AddTag(TEXT("Rule.Vision.Blackout"),  TEXT("Z19/H20 명멸/암전 — Periodic GE + PostProcess 암전."));
	Rule_Vision_ForceFPS  = AddTag(TEXT("Rule.Vision.ForceFPS"),  TEXT("Z20/H21 1인칭 강제 — 토글 잠금 + FOV -10%."));
	Rule_Vision_FlipY     = AddTag(TEXT("Rule.Vision.FlipY"),     TEXT("Z21/H22 상하 반전 — 입력 Y축 반전."));

	//~ 상태
	State_Human         = AddTag(TEXT("State.Human"),         TEXT("현재 인간. 진영 전환 시 서버가 갈아끼운다."));
	State_Zombie        = AddTag(TEXT("State.Zombie"),        TEXT("현재 좀비."));
	State_Stunned       = AddTag(TEXT("State.Stunned"),       TEXT("이동·점프 전면 봉인. 경련/착지 경직/QTE 패배 경직 공용."));
	State_Invulnerable  = AddTag(TEXT("State.Invulnerable"),  TEXT("감염 직후 2초 무적. 연속 드래프트 중 즉사 방지."));
	State_Drafting      = AddTag(TEXT("State.Drafting"),      TEXT("드래프트 UI 활성 중."));
	State_Grabbing      = AddTag(TEXT("State.Grabbing"),      TEXT("좀비가 포획 중 (설계문서 §3.1)."));
	State_Grabbed       = AddTag(TEXT("State.Grabbed"),       TEXT("인간이 포획당한 상태."));
	State_Contesting    = AddTag(TEXT("State.Contesting"),    TEXT("구출 QTE 참여 중 — 이동·점프 봉인 (설계문서 §3.3)."));
	State_Input_Flipped = AddTag(TEXT("State.Input.Flipped"), TEXT("현재 앞뒤좌우 이동 입력이 반전된 구간."));
	State_Climbing      = AddTag(TEXT("State.Climbing"),      TEXT("사다리 등반 중. 이동 모드가 Flying 이라 등반 여부를 태그로 구분한다."));

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

#pragma region Zombie Rule

	//~ 룰 - Move
	Rule_Move_SpeedDown_L  = AddTag(TEXT("Rule.Move.SpeedDown.L"),  TEXT("Z03 이동속도 -30%."));
	Rule_Move_BackwardOnly  = AddTag(TEXT("Rule.Move.BackwardOnly"),  TEXT("Z06 후진만 가능"));
	Rule_Move_LowJump  = AddTag(TEXT("Rule.Move.LowJump"),  TEXT("Z12 점프 높이 -60%"));

	//~ 룰 - Bite
	Rule_Bite_TimeUp = AddTag(TEXT("Rule.Bite.TimeUp"), TEXT("Z26 무딘 이빨 — 감염 시간 3초 → 5초"));
	Rule_Bite_RangedDown = AddTag(TEXT("Rule.Bite.RangedDown"), TEXT("Z27 짧은 팔 — 포획 사거리 2.0m → 1.2m"));
	Rule_Bite_FailStun = AddTag(TEXT("Rule.Bite.FailStun"), TEXT("Z28 빗나감 — 포획 실패 시 5초 경직"));
	Rule_Bite_RootedGrab = AddTag(TEXT("Rule.Bite.RootedGrab"), TEXT("Z29 고정 턱 — 포획 중 완전 이동 불가"));
	Rule_Bite_RescueStun = AddTag(TEXT("Rule.Bite.RescueStun"), TEXT("Z30 구조 트라우마 — QTE 패배 시 8초 기절"));
	Rule_Bite_RescueStun = AddTag(TEXT("Rule.Bite.RescueStun"), TEXT("Z30 구조 트라우마 — QTE 패배 시 8초 기절"));

	//~ 룰 - Trade
	Rule_Trade_Frenzy = AddTag(TEXT("Rule.Trade.Frenzy"), TEXT("Z43 광폭 — 이동속도 +35%, 감염 시간 6초"));
	Rule_Trade_Ghost = AddTag(TEXT("Rule.Trade.Ghost"), TEXT("Z44 유령 — 반투명, 이동속도 -35%"));

#pragma endregion

#pragma region Human Rule

	//~ 룰 - Move
	Rule_Move_NoSprint = AddTag(TEXT("Rule.Move.NoSprint"), TEXT("H03 전력질주 봉인"));
	Rule_Move_CrouchOnly = AddTag(TEXT("Rule.Move.CrouchOnly"), TEXT("H07 웅크린 상태로만 이동"));
	Rule_Move_Burden = AddTag(TEXT("Rule.Move.Burden"), TEXT("H11 이동속도-25%,점프높이 -40%"));
	Rule_Move_SlowTurn = AddTag(TEXT("Rule.Move.SlowTurn"), TEXT("H14 카메라 선회속도 -45%"));

	//~ 룰 - Rescue
	Rule_Rescue_Disabled = AddTag(TEXT("Rule.Rescue.Disabled"), TEXT("H32 겁쟁이 — 구출 입력 봉인"));

#pragma endregion
}
