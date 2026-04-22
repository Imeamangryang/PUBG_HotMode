# 🎮 Hot Drop Survival Mode - 2주 WBS

---

# 👨‍💻 신준섭 (Server / Dummy / Match)

## 📅 Week 1 - 기반 구축

### ✅ Day 1~2: Dedicated Server 기본
- [ ] Dedicated Server 빌드 환경 구축
- [ ] 서버 실행 / 클라 접속 확인
- [ ] Player 접속/퇴장 로그 출력
- [ ] NetMode (Listen / Dedicated) 이해

---

### ✅ Day 3: Player 연결 구조
- [ ] PlayerController / Pawn 서버 스폰 구조
- [ ] 서버에서 Pawn Spawn → Possess 확인
- [ ] PlayerState 생성 및 연결

---

### ✅ Day 4: Match System 기본
- [ ] Match State Enum 정의 (Waiting / Start / End)
- [ ] Match 시작 조건 구현 (인원 체크)
- [ ] GameMode에서 Match 상태 관리

---

### ✅ Day 5: 생존 판정
- [ ] Alive Player Count 관리
- [ ] Player 사망 시 Count 감소
- [ ] 1명 남으면 EndMatch() 호출

---

## 📅 Week 2 - 부하 & 안정화

### ✅ Day 6~7: Dummy Client
- [ ] Dummy Client 멀티 인스턴스 실행
- [ ] 자동 이동 로직 (랜덤 / 간단 AI)
- [ ] 공격 트리거 추가

---

### ✅ Day 8: 대량 접속 테스트
- [ ] 20 → 50명 접속 테스트
- [ ] 서버 로그 상태 확인
- [ ] 크래시 / 지연 체크

---

### ✅ Day 9: Replication 최적화
- [ ] NetUpdateFrequency 조정
- [ ] 불필요 변수 Replication 제거
- [ ] Owner Only / Multicast 구분

---

### ✅ Day 10: 안정화
- [ ] Match 반복 실행 테스트
- [ ] 메모리 누수 체크
- [ ] 서버 종료/재시작 안정성 확인

---

# 👨‍💻 정창원 (Player / Combat)

- 이동 시스템 Character
- [ ] WASD 이동 구현
- [ ] CharacterMovement 기반 (선택 서버 이동)
- [ ] 기본 Replication 확인 (Replication 쓰는 방법 익히)


- Player State
- [ ] HP 변수 추가
- [ ] RepNotify 구현
- [ ] 사망 상태 처리 (Dead Flag)

---

- Player Controller (공격입력)
- [ ] Input 바인딩 (Attack)
- [ ] Client → Server RPC 연결
- [ ] 공격 쿨타임 구현

---

-  히트 판정
- [ ] Sphere Trace 구현
- [ ] 서버에서만 판정
- [ ] Hit Actor 판별

---

-  데미지 처리
- [ ] ApplyDamage 구현
- [ ] HP 감소 처리
- [ ] 사망 Trigger 연결
- [ ] 타격 사운드와 몽타주 애니메이션 추가

---

- Edge Case
- [ ] 동시 공격 처리
- [ ] 사망 직전 공격 처리
- [ ] 중복 데미지 방지


---

# 👨‍💻 김건우 (UI / Inventory / DB)

## 📅 Week 1 - 최소 UI

### ✅ Day 1~2: 기본 UI
- [ ] HP UI (Progress Bar)
- [ ] Player Count UI
- [ ] Match 상태 표시 UI

---

### ✅ Day 3: 사망 UI
- [ ] Death 화면 표시
- [ ] 텍스트 기반 UI

---

### ✅ Day 4~5: Inventory (최소)
- [ ] 슬롯 UI (1~2칸)
- [ ] 아이템 구조 정의 (Dummy 가능)

---

## 📅 Week 2 - 연결 & 정리

### ✅ Day 6: UI 업데이트
- [ ] HP Replication 반영
- [ ] Match 상태 UI 연결

---

### ✅ Day 7: DB 연결 (최소)
- [ ] 테스트용 ID 생성
- [ ] 결과 저장 (생존 여부)

---

### ✅ Day 8~10: 안정화
- [ ] UI 버그 수정
- [ ] Widget 최소화 (성능 고려)
- [ ] 불필요 UI 제거

---

# 🔥 전체 통합 체크포인트

## 📅 Day 5 (1주차 종료)
- [ ] 2~3명 접속 테스트 성공
- [ ] 이동 / 공격 / 사망 동작
- [ ] Match 시작 / 종료 확인

---

## 📅 Day 8
- [ ] Dummy 포함 20명 테스트
- [ ] 서버 크래시 없음

---

## 📅 Day 10 (최종)
- [ ] 50명 접속 성공
- [ ] 전투 정상 동작
- [ ] 1명 생존 시 Match 종료

---