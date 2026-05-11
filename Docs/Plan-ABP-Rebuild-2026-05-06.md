# ABP 재구성 계획 - 2026-05-06

## 목표

- 스탠드, 크라우치, 엎드리기 상태가 자연스럽게 오가도록 정리한다.
- 비무장 / 무장 상태를 같은 구조 안에서 안정적으로 공존시킨다.
- 발사, 재장전, 투척, 피격, 사망, 낙하 같은 특수 동작은 locomotion 위에 겹치는 방식으로 정리한다.
- 초보자도 한 단계씩 따라가며 다시 만들 수 있는 구조로 단순화한다.

## 결론부터

- 지금 상태가 많이 엉켜 있다면 `ABP를 새로 만드는 쪽`이 더 빠르고 덜 헷갈린다.
- 특히 "상하체 분리" 자체는 맞는 방향이지만, 상태머신을 너무 많이 쪼개거나 조건을 여러 군데 중복으로 넣으면 금방 꼬인다.
- 이번에는 아래 원칙으로 다시 만든다.

## 핵심 원칙

1. 하체 locomotion은 "이동과 자세"만 책임진다.
2. 상체는 "무장 여부, 조준, 발사, 재장전, 투척"만 책임진다.
3. 상태 판단은 최대한 `ABG_Character`에서 끝내고, ABP는 그 값을 읽어서 재생만 한다.
4. `State Machine`은 적게, `Blend Poses`는 명확하게 쓴다.
5. 사망 / 낙하 / 피격처럼 우선순위가 높은 동작은 locomotion 바깥에서 덮어쓴다.

## 추천 최종 구조

1. `ABP_BG_Player_Combat` 같은 새 ABP를 만든다.
2. Event Graph에서 캐릭터 상태값을 읽는다.
3. AnimGraph는 아래 순서로 쌓는다.

   - Base Locomotion
   - Armed / Unarmed 상체 분기
   - Aim / Fire / Reload / Throw 오버레이
   - Hit React
   - Death

## 먼저 준비할 것

1. 새 ABP 생성
   - 기존 ABP를 바로 고치지 말고 복사본이나 새 ABP를 만든다.
   - 이유: 잘못 꼬이면 언제든 비교하거나 되돌릴 수 있어야 한다.

2. 애니메이션 자산 정리
   - 비무장 스탠드 BS
   - 비무장 크라우치 BS
   - 비무장 엎드리기 BS
   - 무장 스탠드 BS
   - 무장 크라우치 BS
   - 무장 엎드리기 BS
   - 발사 몽타주들
   - 재장전 몽타주들
   - 투척 몽타주들
   - 낙하 애니메이션
   - 사망 몽타주 또는 시퀀스

3. 슬롯 이름 정리
   - `UpperBody`
   - `CrouchUpperBody`
   - `ProneUpperBody`
   - 가능하면 이 세 개만 먼저 쓴다.

## 1단계 - ABP에서 읽을 변수만 정한다

ABP 안에 아래 변수만 둔다.

1. `Speed`
2. `Direction`
3. `bIsInAir`
4. `bIsAccelerating`
5. `bHasWeapon`
6. `bIsAiming`
7. `CharacterStance`
8. `CharacterState`
9. `EquippedWeaponPoseType`
10. `LeanDirection`
11. `bIsDead`

### 중요한 점

- `bIsCrouchingState`, `bIsProne`를 따로 써도 되지만, 최종적으로는 `CharacterStance` 하나로 모으는 편이 덜 꼬인다.
- 지금 코드에는 이미 `CharacterStance`, `CharacterState`가 있으니 그것을 중심값으로 쓰는 것을 추천한다.

## 2단계 - Event Graph를 아주 단순하게 만든다

`Event Blueprint Update Animation`에서 아래 순서로만 작업한다.

1. `Try Get Pawn Owner`
2. `Cast to ABG_Character`
3. 캐릭터가 유효하면 변수 세팅

세팅할 값:

1. `Speed = Velocity Size2D`
2. `Direction = Calculate Direction(Velocity, Actor Rotation)`
3. `bIsInAir = bIsFalling`
4. `bIsAccelerating = bIsAcceleration`
5. `bHasWeapon = bHasWeapon`
6. `bIsAiming = bIsAiming`
7. `CharacterStance = CharacterStance`
8. `CharacterState = CharacterState`
9. `EquippedWeaponPoseType = EquippedWeaponPoseType`
10. `LeanDirection = LeanDirection`
11. `bIsDead = bIsDead`

### 여기서 하지 말 것

- Event Graph에서 자세를 다시 추론하지 않는다.
- 속도로만 크라우치/엎드리기를 판단하지 않는다.
- 발사 중인지 직접 계산하지 않는다.

## 3단계 - Locomotion State Machine은 4개 상태만 둔다

새 State Machine 이름 예시:

- `SM_Locomotion_Base`

상태는 아래 4개만 시작한다.

1. `Grounded_Stand`
2. `Grounded_Crouch`
3. `Grounded_Prone`
4. `InAir`

### 각 상태 내용

1. `Grounded_Stand`
   - 아직은 여기서 무장/비무장을 나누지 않는다.
   - `Blend Poses by Bool(bHasWeapon)`으로
     - false: 비무장 스탠드 BS
     - true: 무장 스탠드 BS

2. `Grounded_Crouch`
   - `Blend Poses by Bool(bHasWeapon)`으로
     - false: 비무장 크라우치 BS
     - true: 무장 크라우치 BS

3. `Grounded_Prone`
   - `Blend Poses by Bool(bHasWeapon)`으로
     - false: 비무장 엎드리기 BS
     - true: 무장 엎드리기 BS

4. `InAir`
   - 기본 낙하 포즈 또는 낙하 BS
   - 비행기 낙하와 일반 점프 낙하를 나중에 세분화할 수 있다.

## 4단계 - 전이 규칙을 단순하게 만든다

### Any State -> InAir

- `bIsInAir == true`

### InAir -> Grounded_Stand

- `bIsInAir == false` AND `CharacterStance == Standing`

### InAir -> Grounded_Crouch

- `bIsInAir == false` AND `CharacterStance == Crouching`

### InAir -> Grounded_Prone

- `bIsInAir == false` AND `CharacterStance == Prone`

### Stand <-> Crouch

- `CharacterStance == Crouching`
- `CharacterStance == Standing`

### Stand <-> Prone

- `CharacterStance == Prone`
- `CharacterStance == Standing`

### Crouch <-> Prone

- `CharacterStance == Prone`
- `CharacterStance == Crouching`

### 블렌드 시간 추천

1. Stand <-> Crouch: 0.15 ~ 0.2
2. Crouch <-> Prone: 0.2 ~ 0.25
3. Stand <-> Prone: 0.25 ~ 0.35
4. Ground <-> InAir: 0.1 ~ 0.15

## 5단계 - 상체 분리는 State Machine 밖에서 한다

이 단계가 제일 중요하다.

`Final Pose` 직전에 `Layered Blend Per Bone`을 둔다.

### Base Pose

- 방금 만든 `SM_Locomotion_Base`

### Blend Pose 0

- 상체 전용 포즈 그래프

### Branch Filter 시작 뼈 추천

- `spine_01` 또는 프로젝트 스켈레톤 기준 상체 시작 뼈

## 6단계 - 상체 전용 그래프는 무장 상태만 책임지게 한다

상체 그래프는 대략 아래 순서로 만든다.

1. `Blend Poses by Bool(bHasWeapon)`
2. false면 Base Pose 그대로 통과
3. true면 무장 상체 포즈 그래프 사용

무장 상체 포즈 그래프 안에서는

1. `Blend Poses by Enum(EquippedWeaponPoseType)`
2. Pistol / Rifle / Shotgun 별 기본 상체 포즈를 나눈다
3. 그 위에 Aim Offset 또는 Aim Additive
4. 그 위에 Montage Slot

## 7단계 - 발사 / 재장전 / 투척은 몽타주로 처리한다

이건 State Machine 안에 넣지 않는 것을 강하게 추천한다.

### 이유

1. 자세가 스탠드인지 크라우치인지 엎드리기인지와 상관없이 재생하기 쉽다.
2. 네트워크 동기화와 우선순위 관리가 편하다.
3. 나중에 무기 추가가 쉬워진다.

### 추천 구조

1. 발사: `UpperBody`, `CrouchUpperBody`, `ProneUpperBody` 슬롯 몽타주
2. 재장전: 같은 슬롯 체계 사용
3. 투척: 상체 몽타주 사용
4. 사망: 전신 몽타주 또는 상태 분기에서 최종 덮어쓰기

## 8단계 - 조준은 Aim Offset 또는 Additive 중 하나만 먼저 쓴다

초보자 기준으로는 `Additive Pose`부터 추천한다.

### 쉬운 방법

1. 비조준 기본 무장 상체 포즈
2. 조준 상체 포즈
3. `Blend Poses by Bool(bIsAiming)`

### 더 좋은 방법

1. Aim Offset 자산 생성
2. Pitch/Yaw를 넣어 상체만 조준

처음에는 쉬운 방법으로 붙이고, 나중에 Aim Offset으로 교체하는 편이 낫다.

## 9단계 - Lean은 가장 마지막에 소량만 더한다

Lean까지 처음부터 같이 넣으면 그래프가 급격히 복잡해진다.

순서:

1. 스탠드/크라우치/엎드리기 완성
2. 비무장/무장 전환 완성
3. 발사/재장전/투척 완성
4. 조준 완성
5. 마지막에 Lean 추가

## 10단계 - 사망은 별도 우선순위로 처리한다

사망은 locomotion 안쪽에 섞기보다 최종 출력 직전에 강하게 덮는 편이 낫다.

방법 예시:

1. `Blend Poses by Bool(bIsDead)`
2. false: 기존 결과
3. true: Death Pose 또는 Death Montage 결과

## 11단계 - 비행기 낙하는 따로 빼는 게 낫다

비행기 낙하는 일반 이동 ABP와 목적이 다르다.

추천 방법:

1. 일반 플레이 ABP 안에 `InAir` 상태는 일반 낙하만 담당
2. 비행기 점프 직후 특수 낙하는
   - 별도 상태값 추가 후 같은 ABP에서 분기하거나
   - 아예 전용 ABP / 전용 상태 그래프로 분리

초보자 기준으로는 `같은 ABP 안에서 특수 상태 분기`가 더 관리하기 쉽다.

## 12단계 - 테스트 순서

아래 순서대로 하나씩 본다.

1. 비무장 스탠드 이동
2. 비무장 크라우치 이동
3. 비무장 엎드리기 이동
4. 무장 스탠드 이동
5. 무장 크라우치 이동
6. 무장 엎드리기 이동
7. 무장 스탠드 발사
8. 무장 크라우치 발사
9. 무장 엎드리기 발사
10. 재장전
11. 투척
12. 낙하
13. 피격
14. 사망

### 확인 포인트

1. 상태 전환 때 다리가 미끄러지지 않는가
2. 상체만 발사하고 하체 이동은 유지되는가
3. 조준 중 자세 전환이 끊기지 않는가
4. 엎드린 상태에서 발사 슬롯이 정상 재생되는가
5. 사망이 다른 몽타주보다 우선하는가

## 13단계 - 에디터에서 꼭 확인할 것

1. 각 BS의 축 설정
   - Speed
   - Direction

2. 캐릭터 스켈레톤 슬롯 존재 여부
   - `UpperBody`
   - `CrouchUpperBody`
   - `ProneUpperBody`

3. 몽타주 Slot Track 연결 상태

4. Layered Blend Per Bone의 시작 뼈가 올바른지

5. 프론 상태에서 캡슐 / 카메라 / 무기 소켓이 어색하지 않은지

## 14단계 - C++와 ABP 역할 분리

### C++가 맡을 것

1. `CharacterState`
2. `CharacterStance`
3. `bHasWeapon`
4. `bIsAiming`
5. `EquippedWeaponPoseType`
6. 발사 / 재장전 / 무기 교체 같은 이벤트 트리거

### ABP가 맡을 것

1. 이동 애니메이션 선택
2. 자세별 BS 선택
3. 상하체 블렌딩
4. 몽타주 슬롯 재생 결과 반영
5. 최종 포즈 합성

## 15단계 - 이번 작업에서 추천하는 실제 진행 순서

1. 새 ABP 생성
2. Event Graph 변수 연결
3. Base locomotion 4상태 완성
4. 비무장/무장 BS 연결
5. Layered Blend Per Bone 추가
6. 발사 몽타주 슬롯 연결
7. 재장전 몽타주 슬롯 연결
8. 투척 몽타주 슬롯 연결
9. 조준 포즈 연결
10. 사망 처리 연결
11. 낙하 특수분기 추가
12. Lean 추가

## 마지막 권장 사항

- 지금처럼 많이 꼬였을 때는 "수정"보다 "구조를 다시 세우기"가 더 빠르다.
- 특히 PUBG 스타일은 "자세", "무장 여부", "상체 액션"이 동시에 얽히므로, 한 그래프 안에서 모든 것을 State Machine으로 해결하려고 하면 거의 반드시 복잡해진다.
- 가장 안정적인 방식은 `하체 locomotion + 상체 overlay + 몽타주 슬롯` 구조다.
