
// GolfLogCategories.h - ���� ���� ����
#pragma once

#include "CoreMinimal.h"

// ���� �α� ī�װ�� ���� (���� ���Ͽ��� ������ ���)
//DECLARE_LOG_CATEGORY_EXTERN(LogGolfGame, Log, All);
//DECLARE_LOG_CATEGORY_EXTERN(LogGameMode, Log, All);
//DECLARE_LOG_CATEGORY_EXTERN(LogGolfPlayer, Log, All);
//DECLARE_LOG_CATEGORY_EXTERN(LogGolfCamera, Log, All);

// ���� ��ũ�� ����
#define GOLF_LOG(Category, Verbosity, Format, ...) \
    UE_LOG(Category, Verbosity, TEXT("[%s:%d] " Format), \
           *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#define GOLF_LOG_FUNC(Category, Verbosity, Format, ...) \
    UE_LOG(Category, Verbosity, TEXT("[%s] " Format), \
           *FString(__FUNCTION__), ##__VA_ARGS__)

// ���Ǻ� �α� ��ũ��
#define GOLF_LOG_IF(Condition, Category, Verbosity, Format, ...) \
    UE_CLOG(Condition, Category, Verbosity, TEXT("[%s] " Format), \
            *FString(__FUNCTION__), ##__VA_ARGS__)

// ���� ���� ��ũ��
#define GOLF_SCOPED_TIMER(Name) \
    SCOPE_CYCLE_COUNTER(STAT_##Name);