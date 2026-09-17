// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FALobbyMainWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "LoadingScreenSubsystem.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Game/FALobbyGameState.h"
#include "Game/FAMapSettings.h"
#include "Game/FAMapTypes.h"
#include "Player/FALobbyPlayerController.h"
#include "Player/FALobbyPlayerState.h"
#include "UI/FALobbyPlayerEntryWidget.h"

namespace FALobbyAudio
{
	static const FName Click(TEXT("UI_Click"));
	static const FName Hover(TEXT("UI_Hover"));

	/** 준비 버튼은 켜고 끄는 토글이라 클릭음이 아니라 토글음을 쓴다. */
	static const FName Toggle(TEXT("UI_Toggle"));

	static const FName PlayerJoin(TEXT("UI_PlayerJoin"));
	static const FName PlayerLeave(TEXT("UI_PlayerLeave"));
}


void UFALobbyMainWidget::MenuSetup()
{
}

void UFALobbyMainWidget::SetReadyInteractionEnabled(bool bEnable)
{
	if (Button_Ready)
		Button_Ready->SetIsEnabled(bEnable);
}

void UFALobbyMainWidget::ShowLoadingScreen()
{
	if (ULSKLoadingScreenSubsystem* LoadingScreen = ULSKLoadingScreenSubsystem::Get(this))
	{
		LoadingScreen->ShowWithTitle(
			NSLOCTEXT("FALobby", "TravelingToMatch", "게임에 입장하는 중..."));
	}
}

void UFALobbyMainWidget::InitializeLobby()
{
	AFALobbyGameState* LobbyGameState =
		GetWorld()->GetGameState<AFALobbyGameState>();

	if (!LobbyGameState)
	{
		return;
	}
	if (BoundedLobbyGameState && LobbyGameState != BoundedLobbyGameState)
	{
		
		LobbyGameState->OnLobbyStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleLobbyStateChanged);
	}
	//
	BoundedLobbyGameState = LobbyGameState;
	BoundedLobbyGameState->OnLobbyStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleLobbyStateChanged);

	RefreshLobby();
}

void UFALobbyMainWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Ready)
	{
		Button_Ready->OnClicked.AddDynamic(this, &UFALobbyMainWidget::OnReadybuttonClicked);
		Button_Ready->OnHovered.AddUniqueDynamic(this, &UFALobbyMainWidget::OnButtonHovered);
	}
	if (Button_Start)
	{
		// 일단은 리슨서버라면
		ENetMode NetMode = GetWorld()->GetNetMode();
		if (NetMode == ENetMode::NM_ListenServer)
		{
			Button_Start->SetVisibility(ESlateVisibility::Visible);
			Button_Start->OnClicked.AddDynamic(this, &UFALobbyMainWidget::OnStartbuttonClicked);
			Button_Start->OnHovered.AddUniqueDynamic(this, &UFALobbyMainWidget::OnButtonHovered);
		}else
		{
			Button_Start->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if (Button_PreviousMap)
	{
		Button_PreviousMap->OnClicked.AddDynamic(this, &UFALobbyMainWidget::OnPreviousMapClicked);
	}
	if (Button_NextMap)
	{
		Button_NextMap->OnClicked.AddDynamic(this, &UFALobbyMainWidget::OnNextMapClicked);
	}
}

void UFALobbyMainWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UFALobbyMainWidget::NativeDestruct()
{
	Super::NativeDestruct();
	if (BoundedLobbyGameState)
	{
		BoundedLobbyGameState->OnLobbyStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleLobbyStateChanged);
	}
	if (Button_Ready)
	{
		Button_Ready->OnClicked.RemoveAll(this);
	}
	if (Button_Start)
	{
		Button_Start->OnClicked.RemoveAll(this);
	}
}

void UFALobbyMainWidget::HandleLobbyStateChanged()
{
	RefreshLobby();
	bRosterAudioArmed = true;
}

void UFALobbyMainWidget::RefreshLobby()
{
	AFALobbyGameState* LobbyGameState = GetWorld()->GetGameState<AFALobbyGameState>();

	if (!LobbyGameState)
		return;

	UpdateRosterAudio(LobbyGameState->GetConnectedPlayerCount());

	Text_PlayerCount->SetText(
		FText::AsNumber(LobbyGameState->GetConnectedPlayerCount()));

	Text_ReadyCount->SetText(
		FText::AsNumber(LobbyGameState->GetReadyPlayerCount()));
	
	RefreshPlayerList(LobbyGameState->PlayerArray);
	RefreshInteractionState(LobbyGameState->GetLobbyPhase());
	
	const EFALobbyPhase LobbyPhase = LobbyGameState->GetLobbyPhase();
	if (LobbyPhase == EFALobbyPhase::Countdown)
	{
		StartCountdownUiTimer();
	}
	else
	{
		StopCountdownUiTimer();
	}
	RefreshMapInfo();
	RefreshMapSelectionButtons();
}

void UFALobbyMainWidget::RefreshPlayerList(TArray<TObjectPtr<APlayerState>> PlayerArray)
{
	if (!PlayerListView)
		return;
	PlayerListView->ClearListItems();
	
	for (APlayerState* PlayerState : PlayerArray)
	{
		AFALobbyPlayerState* LobbyPlayerState = Cast<AFALobbyPlayerState>(PlayerState);
		if (!LobbyPlayerState || LobbyPlayerState->IsOnlyASpectator() || LobbyPlayerState->IsInactive())
		{
			continue;
		}
		//UListView를 사용할때는 지정된 Entry 위젯을 지정받음.
		//해당 위젯은 IUserObjectListEntry를 상속받으며,
		//NativeOnListItemObjectSet함수에서 UObject를 통해 생성 및 갱신됨.
		PlayerListView->AddItem(LobbyPlayerState);
		
		
		// 아래처럼 직접 위젯을 넣는 방식이 아님.
		// UFALobbyPlayerEntryWidget* Entry = 
		// 	CreateWidget<UFALobbyPlayerEntryWidget>(
		// 		this, LobbyPlayerEntryWidgetClass);
		// if (Entry)
		// {
		// 	Entry->Setup(LobbyPlayerState);
		// 	PlayerListView->AddItem(Entry);
		// }
	}
}

void UFALobbyMainWidget::RefreshInteractionState(EFALobbyPhase LobbyPhase)
{
	AFALobbyPlayerController* LobbyController =	Cast<AFALobbyPlayerController>(GetOwningPlayer());

	AFALobbyGameState* LobbyState =	BoundedLobbyGameState.Get();
	
	const bool bIsHost = LobbyController && LobbyController->IsLobbyHost();
	
	if (Button_Start)
	{
		const bool bCanClickStart =
			bIsHost
			&& LobbyState
			&& LobbyState->CanHostStartMatch()
			&& LobbyPhase == EFALobbyPhase::WaitingForReady;

		Button_Start->SetIsEnabled(bCanClickStart);
	}
	if (Button_Ready)
	{
		const bool bCanChangeReady = LobbyPhase == EFALobbyPhase::WaitingForPlayers
			|| LobbyPhase == EFALobbyPhase::WaitingForReady
			|| LobbyPhase == EFALobbyPhase::Countdown;
		
		Button_Ready->SetIsEnabled(bCanChangeReady);
		
		// switch (LobbyPhase)
		// {
		// case EFALobbyPhase::Traveling:
		// case EFALobbyPhase::LockingSession:
		// 	Button_Ready->SetIsEnabled(false);
		// 	break;
		// case EFALobbyPhase::WaitingForPlayers:
		// case EFALobbyPhase::WaitingForReady:
		// case EFALobbyPhase::Countdown:
		// 	Button_Ready->SetIsEnabled(true);
		// 	break;
		// }
	}
	
}

void UFALobbyMainWidget::OnReadybuttonClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FALobbyAudio::Toggle);

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	AFALobbyPlayerState* LobbyPlayerState = Cast<AFALobbyPlayerState>(GetOwningPlayerState());
	if (PlayerController && LobbyPlayerState)
	{
		AFALobbyPlayerController* LobbyPlayerController = Cast<AFALobbyPlayerController>(PlayerController);
		if (LobbyPlayerController)
		{
			bool IsReady = LobbyPlayerState->IsLobbyReady();
			LobbyPlayerController->SetReadyGame(!IsReady);
		}
	}
}

void UFALobbyMainWidget::OnStartbuttonClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FALobbyAudio::Click);

	AFALobbyPlayerController* PlayerController = Cast<AFALobbyPlayerController>(GetOwningPlayer());
	if (!PlayerController)
		return;

	PlayerController->RequestStartCountdown();
}

void UFALobbyMainWidget::UpdateCountdownText()
{
	if (!BoundedLobbyGameState || !Text_Countdown)
	{
		return;
	}
	
	const int32 RemainingSeconds =
		BoundedLobbyGameState->GetRemainingCountdownSeconds();
	
	Text_Countdown->SetText(
		FText::Format(
			NSLOCTEXT(
				"FALobby",
				"MatchStartCountdown",
				"{0}초 후 게임 시작"),
			FText::AsNumber(RemainingSeconds)));
	
	Text_Countdown->SetVisibility(
		ESlateVisibility::HitTestInvisible);
}

void UFALobbyMainWidget::StartCountdownUiTimer()
{
	if (GetWorld()->GetTimerManager().IsTimerActive(CountdownUiTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		CountdownUiTimerHandle,
		this,
		&ThisClass::UpdateCountdownText,
		0.1f,
		true);

	UpdateCountdownText();

}

void UFALobbyMainWidget::StopCountdownUiTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(
		CountdownUiTimerHandle);

	if (Text_Countdown)
	{
		Text_Countdown->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

void UFALobbyMainWidget::OnButtonHovered()
{
	UFAAudioSubsystem::PlayFASound(this, FALobbyAudio::Hover);
}

void UFALobbyMainWidget::UpdateRosterAudio(int32 ConnectedCount)
{
	// 첫 복제 알림이 실어온 값이 진짜 기준선.
	//
	// InitializeLobby() 는 RefreshLobby() 를 직접 부르는데, 갓 접속한 클라이언트에서는 그 시점에
	// AFALobbyGameState 의 인원수가 아직 복제되지 않아 기본값(1)이 읽힘. 그걸 기준으로 삼으면
	// 곧이어 도착하는 진짜 값(예: 4)이 '3명 입장'으로 보여 입장음이 헛나김.
	if (!bRosterAudioArmed)
	{
		LastConnectedCount = ConnectedCount;
		return;
	}

	if (LastConnectedCount >= 0 && ConnectedCount != LastConnectedCount)
	{
		UFAAudioSubsystem::PlayFASound(this, ConnectedCount > LastConnectedCount
			                                     ? FALobbyAudio::PlayerJoin
			                                     : FALobbyAudio::PlayerLeave);
	}

	LastConnectedCount = ConnectedCount;
}

void UFALobbyMainWidget::RefreshMapInfo()
{
	const AFALobbyGameState* LobbyState = GetWorld()->GetGameState<AFALobbyGameState>();
	
	if (!LobbyState)
		return;
	
	//GameState에서 MapId를 가져오고 MapSettings에서 정보조회
	const FName MapId = LobbyState->GetSelectedMapId();
	const FFAMapDataRow* MapRow = UFAMapSettings::FindMap(MapId);
	
	if (!MapRow)
		return;
	
	if (Text_MapName)
	{
		// 혹시 DisplayName이 없다면 Id출력.
		Text_MapName->SetText(
			MapRow->DisplayName.IsEmpty()
				? FText::FromName(MapId)
				: MapRow->DisplayName);
	}
	if (Text_MapDescription)
	{
		Text_MapDescription->SetText(MapRow->Description);
	}
	
	if (Text_RecommendedPlayers)
	{
		Text_RecommendedPlayers->SetText(
			FText::Format(
				NSLOCTEXT(
					"FALobby",
					"RecommendedPlayers",
					"권장 인원 : {0}~{1}명"),
					FText::AsNumber(MapRow->RecommendedMinPlayers),
					FText::AsNumber(MapRow->RecommendedMaxPlayers))
					);
		
	}
	if (Image_MapThumbnail)
	{
		UTexture2D* Thumbnail =
			MapRow->Thumbnail.LoadSynchronous();

		Image_MapThumbnail->SetBrushFromTexture(Thumbnail);
	}
}

void UFALobbyMainWidget::RefreshMapSelectionButtons()
{
	const AFALobbyPlayerController* Controller = Cast<AFALobbyPlayerController>(GetOwningPlayer());

	const AFALobbyGameState* LobbyState = GetWorld()->GetGameState<AFALobbyGameState>();

	const bool bIsHost = Controller && Controller->IsLobbyHost();
	
	const bool bCanChange =
		bIsHost
		&& LobbyState
		&& !LobbyState->IsInteractionLocked()
		&& LobbyState->GetLobbyPhase()
			!= EFALobbyPhase::Countdown;
	
	//bCanChange 여부에 따라 변경 버튼을 On/Off
	if (Button_PreviousMap)
	{
		Button_PreviousMap->SetVisibility(
			bIsHost
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);

		Button_PreviousMap->SetIsEnabled(bCanChange);
	}

	if (Button_NextMap)
	{
		Button_NextMap->SetVisibility(
			bIsHost
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);

		Button_NextMap->SetIsEnabled(bCanChange);
	}
	if (Text_HostDescription)
	{
		Text_HostDescription->SetVisibility(
			bIsHost
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
}

void UFALobbyMainWidget::OnPreviousMapClicked()
{
	SelectRelativeMap(-1);
}

void UFALobbyMainWidget::OnNextMapClicked()
{
	SelectRelativeMap(1);
}

void UFALobbyMainWidget::SelectRelativeMap(int32 Direction)
{
	AFALobbyPlayerController* LobbyPlayerController = Cast<AFALobbyPlayerController>(GetOwningPlayer());
	
	const AFALobbyGameState* LobbyState = GetWorld()->GetGameState<AFALobbyGameState>();
	
	if (!LobbyPlayerController
		|| !LobbyPlayerController->IsLobbyHost()
		|| !LobbyState)
	{
		return;
	}
	
	const TArray<FName> MapIds = UFAMapSettings::GetSelectableMapIds();
	
	if (MapIds.IsEmpty())
		return;
	
	//IndexOfByKey를 통해 TArray에서 해당키의 인덱스를 가져옴.
	int32 CurrentIndex = MapIds.IndexOfByKey(LobbyState->GetSelectedMapId());
	
	if (CurrentIndex == INDEX_NONE)
		CurrentIndex = 0;
	
	//음수 또는 초과시 위치 전환을 위한 방식
	const int32 NextIndex = (CurrentIndex + Direction + MapIds.Num()) % MapIds.Num();
	
	LobbyPlayerController->RequestSelectMap(MapIds[NextIndex]);
}
