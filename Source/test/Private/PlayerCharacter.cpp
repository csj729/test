// PlayerCharacter.cpp
#include "PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Enhanced Input 헤더들
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY(LogPlayerChar);

APlayerCharacter::APlayerCharacter()
{
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

    // Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
    // instead of recompiling to adjust them
    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

    // 카메라 설정
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // [Refactor] 초기화
    bIsAttacking = false;
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // [Enhanced Input] Mapping Context 추가
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
                PLAYER_LOG(Log, TEXT("DefaultMappingContext Added"));
            }
            else
            {
                PLAYER_LOG(Error, TEXT("DefaultMappingContext is missing!"));
            }
        }
    }
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // [Enhanced Input] 바인딩
    // 기존 InputComponent를 UEnhancedInputComponent로 캐스팅해야 합니다.
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 이동 (Triggered: 누르고 있는 동안 계속 실행)
        if (MoveAction)
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

        // 시선 (Triggered)
        if (LookAction)
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);

        // 점프 (Started: 누르는 순간)
        if (JumpAction)
        {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        }

        // 공격 (Started: 누르는 순간)
        if (AttackAction)
            EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &APlayerCharacter::Attack);
    }
    else
    {
        PLAYER_LOG(Error, TEXT("Failed to cast to UEnhancedInputComponent! Check Project Settings."));
    }
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{

    // 입력값 가져오기 (Vector2D)
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // 카메라가 바라보는 방향 알아내기
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        // 전후 이동 (W/S)
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        AddMovementInput(ForwardDirection, MovementVector.Y);

        // 좌우 이동 (A/D)
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
    // 마우스/스틱 입력값 (Vector2D)
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void APlayerCharacter::Attack(const FInputActionValue& Value)
{
    // 1. 몽타주가 할당되어 있는지 확인
    if (!AttackMontage)
    {
        PLAYER_LOG(Warning, TEXT("No Attack Montage Assigned!"));
        return;
    }

    // [Refactor] 이미 공격 중이면 중복 실행 방지
    if (bIsAttacking) return;

    // 2. 애니메이션 인스턴스 가져오기
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (!AnimInstance) return;

    // 3. 몽타주 재생
    AnimInstance->Montage_Play(AttackMontage);

    // [Refactor] 상태 업데이트 및 종료 델리게이트 바인딩    
    bIsAttacking = true;

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &APlayerCharacter::OnAttackMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);

    PLAYER_LOG(Log, TEXT("Attack Started (State Set to True)"));

    // 참고: 여기서 Weapon->StartAttack()을 직접 호출하지 않습니다.
    // BaseWeapon의 StartAttack은 몽타주 내부의 'Anim Notify'가 호출해줘야
    // 애니메이션 동작과 데미지 타이밍이 정확하게 일치합니다.
}

void APlayerCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // [Refactor] 공격 종료 처리
    if (Montage == AttackMontage)
    {
        bIsAttacking = false;
        PLAYER_LOG(Log, TEXT("Attack Ended (State Set to False)"));
    }
}