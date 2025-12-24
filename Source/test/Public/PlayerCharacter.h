// PlayerCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "InputActionValue.h" // [중요] Enhanced Input 값 구조체
#include "PlayerCharacter.generated.h"

// [Rule] 클래스 전용 로그 매크로 선언
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerChar, Log, All);

UCLASS()
class TEST_API APlayerCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // [Rule] 단축 로그 매크로
#define PLAYER_LOG(Verbosity, Format, ...) UE_LOG(LogPlayerChar, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
    // --- Camera ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class UCameraComponent* FollowCamera;

    // --- Enhanced Input Assets ---
    // 에디터에서 할당할 입력 매핑 컨텍스트
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputMappingContext* DefaultMappingContext;

    // 이동 액션 (IA_Move: Vector2D)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* MoveAction;

    // 시선 처리 액션 (IA_Look: Vector2D)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* LookAction;

    // 점프 액션 (IA_Jump: Bool)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* JumpAction;

    // 공격 액션 (IA_Attack: Bool)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* AttackAction;

    // 공격 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* AttackMontage;

protected:
    // --- Input Functions ---
    // Enhanced Input은 값을 FInputActionValue로 전달받습니다.
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Attack(const FInputActionValue& Value);
};