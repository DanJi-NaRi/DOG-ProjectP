////////////////////////////
//! \page MyGodPageWidget.h
//! \brief 신 상세 정보와 신 목록을 제공하는 전체 화면 God Page 선언 파일이다.
#pragma once

#include "God/MyGodPresentationTypes.h"
#include "GameplayTagContainer.h"
#include "Widget/MyActivatableWidget.h"
#include "MyGodPageWidget.generated.h"

class UButton;
class UMyGodDetailPanelWidget;
class UMyGodListEntryWidget;
class UVerticalBox;
class UWidget;

////////////////////////////
//! \class UMyGodPageWidget
//! \author 장효제
//! \brief Menu 레이어에서 신 선택과 상세 패널 갱신을 조율하는 전체 화면 위젯이다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyGodPageWidget : public UMyActivatableWidget
{
	GENERATED_BODY()

public:
	UMyGodPageWidget();

	//! \author 장효제
	//! \brief 지정한 신을 선택하고 목록 강조와 상세 패널을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "UI|GodPage", meta = (Categories = "God"))
	void SelectGod(FGameplayTag GodTag);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GodPage")
	TSubclassOf<UMyGodListEntryWidget> GodListEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GodPage")
	EMyGodPortraitStage DefaultPortraitStage = EMyGodPortraitStage::Full;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleGodEntrySelected(FGameplayTag GodTag);

	void BuildGodList();
	void RefreshSelection();
	void RestoreLastViewedGod();
	void StoreLastViewedGod() const;
	const FMyGodPresentationRow* FindPresentation(FGameplayTag GodTag) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UMyGodDetailPanelWidget> WBP_GodDetailPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> VB_GodList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|GodPage", meta = (AllowPrivateAccess = "true", Categories = "God"))
	FGameplayTag SelectedGodTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GodPage", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyGodPresentationRow"))
	TObjectPtr<UDataTable> GodPresentationTable = nullptr;

	UPROPERTY(Transient)
	TArray<FMyGodPresentationRow> GodPresentations;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMyGodListEntryWidget>> GodEntryWidgets;
};
