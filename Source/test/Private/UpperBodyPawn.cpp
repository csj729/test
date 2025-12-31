#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SkeletalMeshComponent.h" 

AUpperBodyPawn::AUpperBodyPawn()
{
	// [중요] 틱 그룹 설정: 모든 물리/이동 계산이 끝난 후 카메라를 갱신해야 떨림이 없습니다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FrontCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("FrontCameraBoom"));
	FrontCameraBoom->SetupAttachment(RootComponent);

	// =================================================================
	// [핵심 1] 절대 회전 사용 (Absolute Rotation)
	// 몸통(척추)이 애니메이션으로 흔들려도, 카메라는 마우스 방향을 굳건히 유지합니다.
	// =================================================================
	FrontCameraBoom->SetUsingAbsoluteRotation(true);

	FrontCameraBoom->bUsePawnControlRotation = true;
	FrontCameraBoom->bDoCollisionTest = false;
	FrontCameraBoom->TargetArmLength = 0.0f;

	FrontCameraBoom->bInheritPitch = true;
	FrontCameraBoom->bInheritYaw = true;
	FrontCameraBoom->bInheritRoll = false;

	// =================================================================
	// [핵심 2] 카메라 렉(Lag) 끄기
	// 공격 속도가 빠를 때 카메라가 뒤쳐져서 캐릭터가 화면 밖으로 나가는 현상을 방지합니다.
	// =================================================================
	FrontCameraBoom->bEnableCameraLag = false;
	FrontCameraBoom->bEnableCameraRotationLag = false;

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FrontCamera"));
	FrontCamera->SetupAttachment(FrontCameraBoom);
	FrontCamera->bUsePawnControlRotation = false;

	LastBodyYaw = 0.0f;
	ParentBodyCharacter = nullptr;
}

void AUpperBodyPawn::BeginPlay()
{
	Super::BeginPlay();

	// 입력 매핑 등록
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (UpperBodyMappingContext)
			{
				Subsystem->AddMappingContext(UpperBodyMappingContext, 0);
			}
		}
	}
}

void AUpperBodyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. 부모(몸통) 찾기 및 초기화 (유지)
	if (!ParentBodyCharacter)
	{
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());

		if (ParentBodyCharacter && Controller)
		{
			float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
			FRotator NewRotation = Controller->GetControlRotation();
			NewRotation.Yaw = CurrentBodyYaw + 180.0f;
			Controller->SetControlRotation(NewRotation);
			LastBodyYaw = CurrentBodyYaw;

			//USkeletalMeshComponent* ParentMesh = ParentBodyCharacter->GetMesh();
			//if (ParentMesh)
			//{
			//	ParentMesh->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);
			//}
			return;
		}
	}

	if (!ParentBodyCharacter || !Controller) return;

	// 2. 몸통 회전 동기화 (유지)
	float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
	float DeltaYaw = CurrentBodyYaw - LastBodyYaw;

	if (!FMath::IsNearlyZero(DeltaYaw))
	{
		FRotator CurrentRot = Controller->GetControlRotation();
		CurrentRot.Yaw += DeltaYaw;
		Controller->SetControlRotation(CurrentRot);
	}

	LastBodyYaw = CurrentBodyYaw;

	// -----------------------------------------------------------------
	// [각도 제한 로직 수정됨]
	// -----------------------------------------------------------------
	FRotator CurrentControlRot = Controller->GetControlRotation();

	// 3. 좌우(Yaw) 시야각 제한 (유지)
	float BodyFrontYaw = ParentBodyCharacter->GetActorRotation().Yaw + 180.0f;
	float RelativeYaw = FRotator::NormalizeAxis(CurrentControlRot.Yaw - BodyFrontYaw);
	float ClampedYaw = FMath::Clamp(RelativeYaw, -90.0f, 90.0f);

	// 4. [수정됨] 위아래(Pitch) 시야각 제한
	// 아까와 반대로 0.0f(수평선) ~ 90.0f(하늘) 사이로 제한합니다.
	// 이제 마우스를 내려도 0도(정면) 밑으로는 안내려갑니다.
	float CurrentPitch = FRotator::NormalizeAxis(CurrentControlRot.Pitch);
	float ClampedPitch = FMath::Clamp(CurrentPitch, 0.0f, 90.0f); // 여기를 수정했습니다!

	// 5. 제한된 각도 적용
	bool bYawChanged = !FMath::IsNearlyEqual(RelativeYaw, ClampedYaw);
	bool bPitchChanged = !FMath::IsNearlyEqual(CurrentPitch, ClampedPitch);

	if (bYawChanged || bPitchChanged)
	{
		CurrentControlRot.Yaw = BodyFrontYaw + ClampedYaw;
		CurrentControlRot.Pitch = ClampedPitch;
		Controller->SetControlRotation(CurrentControlRot);
	}
}

void AUpperBodyPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (LookAction)
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AUpperBodyPawn::Look);

		if (AttackAction)
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AUpperBodyPawn::Attack);
	}
}

void AUpperBodyPawn::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y * -1.0f); // 마우스 Y축 반전 (위로 올리면 위를 봄)

	if (ParentBodyCharacter)
	{
		// 애니메이션 블루프린트로 회전값을 전달 (허리 비틀기용)
		ParentBodyCharacter->SetUpperBodyRotation(GetControlRotation());
	}
}

void AUpperBodyPawn::Attack(const FInputActionValue& Value)
{
	if (!ParentBodyCharacter)
	{
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());
	}

	if (ParentBodyCharacter)
	{
		// 하체 캐릭터에게 공격 신호를 보냄
		ParentBodyCharacter->TriggerUpperBodyAttack();
	}
}
