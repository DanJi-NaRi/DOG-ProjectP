// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MyGameInstance.generated.h"

/**
 * Map(Level) 변경과 관계없이 게임 전체에서 유지되는 객체를 관리하는 클래스.
 * 컨테이너
 */
UCLASS()
class PROJECTP_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()


public : 
	virtual void Init() override;
	virtual void Shutdown() override;

	bool IsChatBackgroundEnabled() const;
	void SetChatBackgroundEnabled(bool bInEnabled);
	int32 GetChatFontSizeLevel() const;
	void SetChatFontSizeLevel(int32 InLevel);

private:
	bool bChatBackgroundEnabled = false;
	int32 ChatFontSizeLevel = 1;

};
